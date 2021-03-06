#define taz_TESTING
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

begin_test( make_and_free_engine, SETUP_ENGINE )
    check( eng->envState == NULL );
    check( eng->apiState == NULL );
end_test( make_and_free_engine, TEARDOWN_ENGINE )


typedef struct Cell Cell;

struct Cell {
    tazR_State base;
    int        car;
    Cell*      cdr;
};

static void cellScan( tazE_Engine* eng, tazR_State* self, bool full ) {
    Cell* cell = (Cell*)self;
    
    if( cell->cdr )
        tazE_markObj( eng, cell->cdr );
}

static size_t cellSize( tazE_Engine* eng, tazR_State* self ) {
    return sizeof(Cell);
}

Cell* cons( tazE_Engine* eng, int car, Cell* cdr ) {
    tazE_ObjAnchor anc;
    Cell* cell = tazE_mallocObj( eng, &anc, sizeof(Cell), tazR_Type_STATE );
    cell->base.finl = NULL;
    cell->base.scan = cellScan;
    cell->base.size = cellSize;
    
    cell->car = car;
    cell->cdr = cdr;
    
    tazE_commitObj( eng, &anc );
    return cell;
}

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

begin_test( malloc_and_collect_objects, SETUP_ENGINE_AND_BARRIER )
    struct {
        tazE_Bucket  base;
        tazR_TVal    cell;
    } buc;
    tazE_addBucket( eng, &buc, 1 );
    
    Cell* cell = cons( eng, 0, NULL );
    buc.cell = tazR_stateVal( cell );
    
    for( int i = 1 ; i < 10000 ; i++ ) {
        cell = cons( eng, i, cell );
        buc.cell = tazR_stateVal( cell );
    }
    
    tazE_remBucket( eng, &buc );
    
    check( ((EngineFull*)eng)->nGCCycles > 0 );
    
    // Touch all cells to make sure they're still alive, issues here
    // may not be caught by the test itself, but will be caught by
    // valgrind and similar tools.
    for( int i = 9999 ; i >= 0 ; i-- ) {
        check( cell->car == i );
        cell = cell->cdr;
    }
    
    tazE_collect( eng, false );
end_test( malloc_and_collect_objects, TEARDOWN_ENGINE_AND_BARRIER )

begin_test( zalloc_and_cancel_objects, SETUP_ENGINE_AND_BARRIER )
    
    tazE_ObjAnchor anc;
    Cell* cell = tazE_zallocObj( eng, &anc, sizeof(Cell), tazR_Type_STATE );
    Cell  zero = { 0 };
    
    bool isZero = !memcmp( cell, &zero, sizeof(Cell) );
    
    tazE_cancelObj( eng, &anc );
    
    check( isZero );
end_test( zalloc_and_cancel_objects, TEARDOWN_ENGINE_AND_BARRIER )

begin_test( raw_memory_management, SETUP_ENGINE_AND_BARRIER )
    
    tazE_RawAnchor anc;
    char  str[] = "Hello, World!";
    char* raw   = tazE_mallocRaw( eng, &anc, sizeof(str) + 1 );
    strcpy( raw, str );
    tazE_commitRaw( eng, &anc );
    check( !strcmp( raw, str ) );
    tazE_freeRaw( eng, raw, sizeof(str) + 1 );
    
    Cell  zero = { 0 };
    Cell* cell = tazE_zallocRaw( eng, &anc, sizeof(Cell) );
    check( !memcmp( cell, &zero, sizeof(Cell) ) );
    tazE_cancelRaw( eng, &anc );
end_test( raw_memory_management, TEARDOWN_ENGINE_AND_BARRIER )


static bool calledErrorFun = false;
static void errorFun( tazE_Engine* eng, tazE_Barrier* bar ) {
    calledErrorFun = true;
}
static void doError( tazE_Engine* eng ) {
    tazE_error( eng, taz_ErrNum_OTHER );
}
begin_test( error_handling, SETUP_ENGINE )
    tazE_Barrier bar = { .errorFun = errorFun };
    if( setjmp( bar.errorDst ) ) {
        check( calledErrorFun );
        check( bar.errnum == taz_ErrNum_OTHER );
        pass();
    }
    tazE_pushBarrier( eng, &bar );
    
    doError( eng );
    fail();
end_test( error_handling, TEARDOWN_ENGINE )


static bool calledPanicFun = false;
static void panicFun( tazE_Engine* eng, tazE_Barrier* bar ) {
    calledPanicFun = true;
}
static void doPanic( tazE_Engine* eng ) {
    tazE_panic( eng, tazR_nil );
}

begin_test( panic_handling, SETUP_ENGINE )
    tazE_Barrier bar = { .errorFun = panicFun };
    if( setjmp( bar.errorDst ) ) {
        check( calledPanicFun );
        check( bar.errnum == taz_ErrNum_PANIC );
        check( tazR_valEqual( bar.errval, tazR_nil ) );
        pass();
    }
    tazE_pushBarrier( eng, &bar );
    
    doPanic( eng );
    fail();
end_test( panic_handling, TEARDOWN_ENGINE )

static bool calledYieldFun = false;
static void yieldFun( tazE_Engine* eng, tazE_Barrier* bar ) {
    calledYieldFun = true;
}
static void doYield( tazE_Engine* eng ) {
    tazE_yield( eng );
}
begin_test( yield_handling, SETUP_ENGINE )
    tazE_Barrier bar = { .yieldFun = yieldFun };
    if( setjmp( bar.yieldDst ) ) {
        check( calledYieldFun );
        check( bar.errnum == taz_ErrNum_NONE );
        check( !memcmp( &bar.errval, &tazR_udf, sizeof(tazR_TVal) ) );
        pass();
    }
    tazE_pushBarrier( eng, &bar );
    
    doYield( eng );
    fail();
end_test( yield_handling, TEARDOWN_ENGINE )

char const* randLongStr( void ) {
    static char buf[32];
    for( unsigned i = 0 ; i < elemsof(buf) - 1 ; i++ )
        buf[i] = rand()%(127 - '!') + '!';
    buf[31] = '\0';
    
    return buf;
}
char const* randMediumStr( void ) {
    static char buf[16];
    for( unsigned i = 0 ; i < elemsof(buf) - 1 ; i++ )
        buf[i] = rand()%(127 - '!') + '!';
    buf[15] = '\0';
    
    return buf;
}
char const* randShortStr( void ) {
    static char buf[6];
    for( unsigned i = 0 ; i < elemsof(buf) - 1 ; i++ )
        buf[i] = rand()%(127 - '!') + '!';
    buf[5] = '\0';
    
    return buf;
}

begin_test( long_strings, SETUP_ENGINE_AND_BARRIER )
    for( unsigned i = 0 ; i < 1000 ; i++ ) {
        char const* rnd = randLongStr();
        tazR_Str    str = tazE_makeStr( eng, rnd, strlen( rnd ) );
        
        taz_StrLoan ln;
        tazE_borrowStr( eng, str, &ln );
        check( !strcmp( rnd, ln.str ) );
        tazE_returnStr( eng, &ln );
    }
    tazE_collect( eng, true );
end_test( large_strings, TEARDOWN_ENGINE_AND_BARRIER )

begin_test( medium_strings, SETUP_ENGINE_AND_BARRIER )
    for( unsigned i = 0 ; i < 1000 ; i++ ) {
        char const* rnd = randMediumStr();
        tazR_Str    str = tazE_makeStr( eng, rnd, strlen( rnd ) );
        
        taz_StrLoan ln;
        tazE_borrowStr( eng, str, &ln );
        check( !strcmp( rnd, ln.str ) );
        tazE_returnStr( eng, &ln );
    }
    tazE_collect( eng, true );
end_test( medium_strings, TEARDOWN_ENGINE_AND_BARRIER )

begin_test( short_strings, SETUP_ENGINE_AND_BARRIER )
    for( unsigned i = 0 ; i < 1000 ; i++ ) {
        char const* rnd = randShortStr();
        tazR_Str    str = tazE_makeStr( eng, rnd, strlen( rnd ) );
        
        taz_StrLoan ln;
        tazE_borrowStr( eng, str, &ln );
        check( !strcmp( rnd, ln.str ) );
        tazE_returnStr( eng, &ln );
    }
    tazE_collect( eng, true );
end_test( medium_strings, TEARDOWN_ENGINE_AND_BARRIER )

begin_test( string_comparison, SETUP_ENGINE_AND_BARRIER )
    for( unsigned i = 0 ; i < 1000 ; i++ ) {
        char const* shortRnd  = randShortStr();
        tazR_Str    shortStr1 = tazE_makeStr( eng, shortRnd, strlen( shortRnd ) );
        tazR_Str    shortStr2 = tazE_makeStr( eng, shortRnd, strlen( shortRnd ) );
        check( tazE_strEqual( eng, shortStr1, shortStr2 ) );
        check( tazE_strLessOrEqual( eng, shortStr1, shortStr2 ) );
        
        char const* mediumRnd  = randMediumStr();
        tazR_Str    mediumStr1 = tazE_makeStr( eng, mediumRnd, strlen( mediumRnd ) );
        tazR_Str    mediumStr2 = tazE_makeStr( eng, mediumRnd, strlen( mediumRnd ) );
        check( tazE_strEqual( eng, mediumStr1, mediumStr2 ) );
        check( tazE_strLessOrEqual( eng, mediumStr1, mediumStr2 ) );
        
        char const* longRnd  = randShortStr();
        tazR_Str    longStr1 = tazE_makeStr( eng, longRnd, strlen( longRnd ) );
        tazR_Str    longStr2 = tazE_makeStr( eng, longRnd, strlen( longRnd ) );
        check( tazE_strEqual( eng, longStr1, longStr2 ) );
        check( tazE_strLessOrEqual( eng, longStr1, longStr2 ) );
    }
    for( unsigned i = 0 ; i < 1000 ; i++ ) {
        char const* rnd  = randShortStr();
        tazR_Str    str1 = tazE_makeStr( eng, rnd, strlen( rnd ) );
        tazR_Str    str2 = tazE_makeStr( eng, rnd, strlen( rnd )/2 );
        check( tazE_strLess( eng, str2, str1 ) );
    }

    tazR_Str str1 = tazE_makeStr( eng, "abc", 3 );
    tazR_Str str2 = tazE_makeStr( eng, "cba", 3 );
    check( tazE_strLess( eng, str1, str2 ) );

end_test( string_comparison, TEARDOWN_ENGINE_AND_BARRIER )

begin_suite( engine_tests )
    with_test( make_and_free_engine )
    with_test( malloc_and_collect_objects )
    with_test( zalloc_and_cancel_objects )
    with_test( raw_memory_management )
    with_test( error_handling );
    with_test( panic_handling );
    with_test( yield_handling );
    with_test( long_strings );
    with_test( medium_strings );
    with_test( short_strings );
    with_test( string_comparison );
end_suite( engine_tests )

int main( void ) {
    if( run_suite( engine_tests ) ) {
        printf( "PASSED\n" );
        return 0;
    }
    else {
        printf( "FAILED\n" );
        return 1;
    }
}