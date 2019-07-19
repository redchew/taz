#define taz_TESTING
#include "../taz_index.c"
#include "../taz_record.c"
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

begin_test( create_record, SETUP_ENGINE_AND_BARRIER )
    struct {
        tazE_Bucket base;
        tazR_TVal   idx;
        tazR_TVal   rec;
    } buc;
    tazE_addBucket( eng, &buc, 2 );

    tazR_Idx* idx = tazR_makeIdx( eng );
    buc.idx = tazR_idxVal( idx );

    tazR_Rec* rec = tazR_makeRec( eng, idx );
    buc.rec = tazR_recVal( rec );

    tazE_collect( eng, true );

    tazE_remBucket( eng, &buc );

    tazE_collect( eng, true );
end_test( create_record, TEARDOWN_ENGINE_AND_BARRIER )


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

begin_test( record_fields, SETUP_ENGINE_AND_BARRIER )
    struct {
        tazE_Bucket base;
        tazR_TVal   idx;
        tazR_TVal   rec;
        tazR_TVal   key;
        tazR_TVal   val1;
        tazR_TVal   val2;
    } buc;
    tazE_addBucket( eng, &buc, 5 );

    tazR_Idx* idx = tazR_makeIdx( eng );
    buc.idx = tazR_idxVal( idx );

    tazR_Rec* rec = tazR_makeRec( eng, idx );
    buc.rec = tazR_recVal( rec );

    for( unsigned i = 0 ; i < 1000 ; i++ ) {
        randVal( eng, &buc.key );
        randVal( eng, &buc.val1 );
        randVal( eng, &buc.val2 );

        tazR_recDef( eng, rec, buc.key, buc.val1 );
        check( tazR_valEqual( tazR_recGet( eng, rec, buc.key ), buc.val1 ) );

        tazR_recSet( eng, rec, buc.key, buc.val2 );
        check( tazR_valEqual( tazR_recGet( eng, rec, buc.key ), buc.val2 ) );
    }
    
    tazE_remBucket( eng, &buc );
end_test( record_fields, TEARDOWN_ENGINE_AND_BARRIER )


begin_test( record_iteration, SETUP_ENGINE_AND_BARRIER )
    struct {
        tazE_Bucket base;
        tazR_TVal   idx;
        tazR_TVal   rec;
        tazR_TVal   iter;
    } buc;
    tazE_addBucket( eng, &buc, 3 );

    tazR_Idx* idx = tazR_makeIdx( eng );
    buc.idx = tazR_idxVal( idx );

    tazR_Rec* rec = tazR_makeRec( eng, idx );
    buc.rec = tazR_recVal( rec );

    unsigned vals[1000] = { 0 };
    for( unsigned i = 0 ; i < 1000 ; i++ )
        tazR_recDef( eng, rec, tazR_intVal( i ), tazR_intVal( 1000 - i ) );
    
    tazR_RecIter* iter = tazR_makeRecIter( eng, rec );
    buc.iter = tazR_stateVal( (tazR_State*)iter );

    tazR_TVal key, val;
    while( tazR_recIterNext( eng, iter, &key, &val ) )
        vals[tazR_getValInt( key )] = tazR_getValInt( val );
    
    for( unsigned i = 0 ; i < 1000 ; i++ )
        check( vals[i] == 1000 - i );
    
    tazE_remBucket( eng, &buc );
end_test( record_iteration, TEARDOWN_ENGINE_AND_BARRIER )



begin_test( fail_on_set_from_udf, SETUP_ENGINE_AND_BARRIER )
    struct {
        tazE_Bucket base;
        tazR_TVal   idx;
        tazR_TVal   rec;
        tazR_TVal   iter;
    } buc;
    tazE_addBucket( eng, &buc, 3 );

    tazR_Idx* idx = tazR_makeIdx( eng );
    buc.idx = tazR_idxVal( idx );

    tazR_Rec* rec = tazR_makeRec( eng, idx );
    buc.rec = tazR_recVal( rec );

    tazR_recDef( eng, rec, tazR_intVal( 123 ), tazR_intVal( 321 ) );

    if( setjmp( bar.errorDst ) ) {
        pass();
    }
    tazR_recSet( eng, rec, tazR_intVal( 123 ), tazR_udf );
    fail();
end_test( fail_on_set_from_udf, TEARDOWN_ENGINE )

begin_test( fail_on_set_to_udf, SETUP_ENGINE_AND_BARRIER )
    struct {
        tazE_Bucket base;
        tazR_TVal   idx;
        tazR_TVal   rec;
        tazR_TVal   iter;
    } buc;
    tazE_addBucket( eng, &buc, 3 );

    tazR_Idx* idx = tazR_makeIdx( eng );
    buc.idx = tazR_idxVal( idx );

    tazR_Rec* rec = tazR_makeRec( eng, idx );
    buc.rec = tazR_recVal( rec );

    if( setjmp( bar.errorDst ) ) {
        pass();
    }
    tazR_recSet( eng, rec, tazR_intVal( 123 ), tazR_intVal( 321 ) );
    fail();
end_test( fail_on_set_to_udf, TEARDOWN_ENGINE )

begin_test( record_comparison, SETUP_ENGINE_AND_BARRIER )
    struct {
        tazE_Bucket base;
        tazR_TVal   idx;
        tazR_TVal   rec1;
        tazR_TVal   rec2;
        tazR_TVal   child1;
        tazR_TVal   child2;
    } buc;
    tazE_addBucket( eng, &buc, 5 );

    tazR_Idx* idx = tazR_makeIdx( eng );
    buc.idx = tazR_idxVal( idx );

    tazR_Rec* rec1 = tazR_makeRec( eng, idx );
    buc.rec1 = tazR_recVal( rec1 );

    tazR_Rec* child1 = tazR_makeRec( eng, idx );
    buc.child1 = tazR_recVal( child1 );

    tazR_Rec* rec2 = tazR_makeRec( eng, idx );
    buc.rec2 = tazR_recVal( rec2 );

    tazR_Rec* child2 = tazR_makeRec( eng, idx );
    buc.child2 = tazR_recVal( child2 );

    check( tazR_recEqual( eng, rec1, rec2 ) );
    check( !tazR_recLess( eng, rec1, rec2 ) );
    check( tazR_recLessOrEqual( eng, rec1, rec2 ) );

    tazR_recDef( eng, rec2, tazR_intVal( 0 ), tazR_intVal( 123 ) );

    check( !tazR_recEqual( eng, rec1, rec2 ) );
    check( tazR_recLess( eng, rec1, rec2 ) );
    check( tazR_recLessOrEqual( eng, rec1, rec2 ) );

    tazR_recDef( eng, rec1, tazR_intVal( 0 ), tazR_intVal( 123 ) );
    check( tazR_recEqual( eng, rec1, rec2 ) );
    check( !tazR_recLess( eng, rec1, rec2 ) );
    check( tazR_recLessOrEqual( eng, rec1, rec2 ) );


    tazR_recDef( eng, rec1, tazR_intVal( 1 ), buc.child1 );
    tazR_recDef( eng, rec2, tazR_intVal( 1 ), buc.child2 );
    check( tazR_recEqual( eng, rec1, rec2 ) );
    check( !tazR_recLess( eng, rec1, rec2 ) );
    check( tazR_recLessOrEqual( eng, rec1, rec2 ) );

    tazR_recDef( eng, child1, tazR_intVal( 0 ), tazR_intVal( 321 ) );
    check( !tazR_recEqual( eng, rec1, rec2 ) );
    check( !tazR_recLess( eng, rec1, rec2 ) );
    check( !tazR_recLessOrEqual( eng, rec1, rec2 ) );

    tazR_recDef( eng, child2, tazR_intVal( 0 ), tazR_intVal( 321 ) );
    check( tazR_recEqual( eng, rec1, rec2 ) );
    check( !tazR_recLess( eng, rec1, rec2 ) );
    check( tazR_recLessOrEqual( eng, rec1, rec2 ) );

    tazE_remBucket( eng, &buc );
end_test( record_comparison, TEARDOWN_ENGINE )

begin_suite( record_tests )
    with_test( create_record )
    with_test( record_fields )
    with_test( record_iteration )
    with_test( fail_on_set_from_udf )
    with_test( fail_on_set_to_udf )
    with_test( record_comparison )
end_suite( record_tests )

int main( void ) {
    if( run_suite( record_tests ) ) {
        printf( "PASSED\n" );
        return 0;
    }
    else {
        printf( "FAILED\n" );
        return 1;
    }
}