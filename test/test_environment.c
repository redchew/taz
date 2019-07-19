#define taz_TESTING
#include "../taz_index.c"
#include "../taz_record.c"
#include "../taz_environment.c"
#include "../taz_engine.c"
#include "test.h"
#include <string.h>
#include <stdlib.h>

static void* alloc( void* old, size_t osz, size_t nsz ) {
    if( nsz > 0 )
        return realloc( old, nsz );
    free( old );
    return NULL;
}

#define SETUP_ENGINE                                                        \
    taz_Config   cfg = { .alloc = alloc };                                  \
    tazE_Engine* eng = tazE_makeEngine( &cfg );
#define TEARDOWN_ENGINE                                                     \
    tazE_freeEngine( eng );

#define SETUP_ENGINE_AND_BARRIER                                            \
    taz_Config   cfg = { .alloc = alloc };                                  \
    tazE_Engine* eng = tazE_makeEngine( &cfg );                             \
    tazE_Barrier bar = { 0 };                                               \
    if( setjmp( bar.errorDst ) )                                            \
        fail();                                                             \
    if( setjmp( bar.yieldDst ) )                                            \
        fail();                                                             \
    tazE_pushBarrier( eng, &bar );
    
#define TEARDOWN_ENGINE_AND_BARRIER                                         \
    tazE_popBarrier( eng, &bar );                                           \
    tazE_freeEngine( eng );


begin_test( test_globals, SETUP_ENGINE_AND_BARRIER )
    tazR_initEnv( eng );

    struct {
        tazE_Bucket base;
        tazR_TVal   key;
    } buc;
    tazE_addBucket( eng, &buc, 1 );

    tazR_TVal* val = NULL;
    unsigned   loc = 0;

    buc.key = tazR_strVal( tazE_makeStr( eng, "key1", 4 ) );
    val = tazR_getGlobalVal( eng, tazR_getValStr( buc.key ) );
    *val = tazR_intVal( 123 );

    buc.key = tazR_strVal( tazE_makeStr( eng, "key2", 4 ) );
    val = tazR_getGlobalVal( eng, tazR_getValStr( buc.key ) );
    *val = tazR_intVal( 321 );

    buc.key = tazR_strVal( tazE_makeStr( eng, "key1", 4 ) );
    loc = tazR_getGlobalLoc( eng, tazR_getValStr( buc.key ) );
    check( tazR_valEqual( *tazR_getGlobalValByLoc( eng, loc ), tazR_intVal( 123 ) ) );

    buc.key = tazR_strVal( tazE_makeStr( eng, "key2", 4 ) );
    loc = tazR_getGlobalLoc( eng, tazR_getValStr( buc.key ) );
    check( tazR_valEqual( *tazR_getGlobalValByLoc( eng, loc ), tazR_intVal( 321 ) ) );
end_test( test_globals, TEARDOWN_ENGINE_AND_BARRIER )

begin_suite( environment_tests )
    with_test( test_globals );
end_suite( environment_tests )

int main( void ) {
    if( run_suite( environment_tests ) ) {
        printf( "PASSED\n" );
        return 0;
    }
    else {
        printf( "FAILED\n" );
        return 1;
    }
}