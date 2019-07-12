#include "../taz_engine.c"
#include "test.h"

static void* alloc( void* old, size_t osz, size_t nsz ) {
    if( nsz > 0 )
        return realloc( old, nsz );
    free( old );
    return NULL;
}

#define SETUP                                                               \
    taz_Config   cfg = { .alloc = alloc };                                  \
    tazE_Engine* eng = tazE_makeEngine( &cfg );
#define TEARDOWN                                                            \
    tazE_freeEngine( eng );

begin_test( makeAndFreeEngine, "Engine Construction" )
    check( eng->modPool == NULL );
    check( eng->fmtState == NULL );
    check( eng->apiState == NULL );
end_test( makeAndFreeEngine )


typedef struct Cell Cell;

struct Cell {
    tazR_State base;
    int        car;
    Cell*      cdr;
};

static void cellScan( tazE_Engine* eng, tazR_State* self ) {
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

begin_test( objectAllocAndCollection, "Object Allocation And Collection" )
    tazE_Barrier bar;
    if( setjmp( bar.errorDst ) )
        fail();
    if( setjmp( bar.yieldDst ) )
        fail();
    tazE_pushBarrier( eng, &bar );
    
    
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
    tazE_popBarrier( eng, &bar );
    
    // Touch all cells to make sure they're still alive, issues here
    // may not be caught by the test itself, but will be caught by
    // valgrind and similar tools.
    for( int i = 9999 ; i >= 0 ; i-- ) {
        check( cell->car == i );
        cell = cell->cdr;;
    }
    
    check( ((tazE_EngineFull*)eng)->nGCCycles > 0 );
end_test( objectAllocAndCollection )

begin_suite( taz_Engine, "taz_Engine tests" )
    with_test( makeAndFreeEngine )
    with_test( objectAllocAndCollection )
end_suite( taz_Engine )

int main( void ) {
    return !run_suite( taz_Engine );
}