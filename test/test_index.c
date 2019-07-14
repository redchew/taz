#define taz_TESTING
#include "../taz_index.c"
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

begin_test( index_construction, SETUP_ENGINE_AND_BARRIER )
    tazR_Idx* idx = tazR_makeIdx( eng );
    check( idx != NULL );
end_test( index_construction, TEARDOWN_ENGINE_AND_BARRIER )

static void randStr( tazE_Engine* eng, tazR_TVal* val ) {
    static char buf[64];
    
    unsigned len = rand() % (sizeof(buf) - 1);
    for( unsigned i = 0 ; i < len ; i++ )
        buf[i] = rand()%(127 - '!') + '!';
    buf[len] = '\0';
    
    tazR_Str str = tazE_makeStr( eng, buf, len );
    *val = tazR_strVal( str );
}

static void randInt( tazE_Engine* eng, tazR_TVal* val ) {
    *val = tazR_intVal( rand() );
}

static void randDec( tazE_Engine* eng, tazR_TVal* val ) {
    *val = tazR_decVal( (1.0/RAND_MAX)*rand() );
}

static void randVal( tazE_Engine* eng, tazR_TVal* val ) {
    
    switch( rand() % 3 ) {
        case 0:
            randStr( eng, val );
        break;
        case 1:
            randInt( eng, val );
        break;
        case 2:
            randDec( eng, val );
        break;
    }
    
}

begin_test( index_insert_and_lookup, SETUP_ENGINE_AND_BARRIER )
    struct {
        tazE_Bucket base;
        tazR_TVal   idx;
        tazR_TVal   key;
    } buc;
    tazE_addBucket( eng, &buc, 2 );
    
    tazR_Idx* idx = tazR_makeIdx( eng );
    buc.idx = tazR_idxVal( idx );
    
    for( unsigned i = 0 ; i < 10000 ; i++ ) {
        randVal( eng, &buc.key );
        unsigned insertLoc = tazR_idxInsert( eng, idx, buc.key );
        long     lookupLoc = tazR_idxLookup( eng, idx, buc.key );
        check( lookupLoc >= 0 );
        check( lookupLoc == insertLoc );
    }
    
    tazE_remBucket( eng, &buc );
end_test( index_insert_and_lookup, TEARDOWN_ENGINE_AND_BARRIER )

begin_test( index_iteration, SETUP_ENGINE_AND_BARRIER )
    struct {
        tazE_Bucket base;
        tazR_TVal   idx;
        tazR_TVal   iter;
    } buc;
    tazE_addBucket( eng, &buc, 2 );
    
    tazR_Idx* idx = tazR_makeIdx( eng );
    buc.idx = tazR_idxVal( idx );
    
    for( unsigned i = 0 ; i < 1000 ; i++ )
        tazR_idxInsert( eng, idx, tazR_intVal( i ) );
    
    tazR_TVal keys[1000];
    
    tazR_IdxIter* iter = tazR_makeIdxIter( eng, idx );
    buc.iter = tazR_stateVal( (tazR_State*)iter );
    
    tazR_TVal key; unsigned loc;
    while( tazR_idxIterNext( eng, iter, &key, &loc ) )
        keys[loc] = key;
    
    for( unsigned i = 0 ; i < 1000 ; i++ )
        check( tazR_getValInt( keys[i] ) == i );

end_test( index_iteration, TEARDOWN_ENGINE_AND_BARRIER )

begin_test( sub_index, SETUP_ENGINE_AND_BARRIER )
    struct {
        tazE_Bucket base;
        tazR_TVal   idx;
        tazR_TVal   sub;
    } buc;
    tazE_addBucket( eng, &buc, 2 );
    
    tazR_Idx* idx = tazR_makeIdx( eng );
    buc.idx = tazR_idxVal( idx );
    
    for( unsigned i = 0 ; i < 1000 ; i++ )
        tazR_idxInsert( eng, idx, tazR_intVal( i ) );
    
    bool select[] = { true, true, false, true, false, true, false, false };
    long locs[elemsof(select)];
    tazR_Idx* sub = tazR_subIdx( eng, idx, elemsof(select), select, locs );
    
    unsigned loc = 0;
    for( unsigned i = 0 ; i < elemsof(locs) ; i++ ) {
        if( select[i] )
            check( locs[i] == loc++ );
        else
            check( locs[i] == -1 );
    }
    
    check( tazR_idxLookup( eng, sub, tazR_intVal( 0 ) ) == 0 );
    check( tazR_idxLookup( eng, sub, tazR_intVal( 1 ) ) == 1 );
    check( tazR_idxLookup( eng, sub, tazR_intVal( 2 ) ) == -1 );
    check( tazR_idxLookup( eng, sub, tazR_intVal( 3 ) ) == 2 );
    check( tazR_idxLookup( eng, sub, tazR_intVal( 4 ) ) == -1 );
    check( tazR_idxLookup( eng, sub, tazR_intVal( 5 ) ) == 3 );
    check( tazR_idxLookup( eng, sub, tazR_intVal( 6 ) ) == -1 );
    check( tazR_idxLookup( eng, sub, tazR_intVal( 7 ) ) == -1 );

    tazE_remBucket( eng, &buc );
end_test( sub_index, TEARDOWN_ENGINE_AND_BARRIER )

begin_suite( index_tests )
    with_test( index_construction )
    with_test( index_insert_and_lookup )
    with_test( index_iteration )
    with_test( sub_index )
end_suite( index_tests )

int main( void ) {
    if( run_suite( index_tests ) ) {
        printf( "PASSED\n" );
        return 0;
    }
    else {
        printf( "FAILED\n" );
        return 1;
    }
}