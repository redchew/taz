#include "taz_engine.h"
#include "taz_record.h"

/* Note: Record Structure
The record structure takes advantage of tagged pointers to save
space on 64bit platforms; allowing us to encode the record's
meta info in only two 64bit words.  While this reduction would
be trivial when compared to the bucket array of normal hashmap
implementations, Taz's use of shared indices allows us to keep
the memory overhead of records themselves extremely small, to
the point that the record's value array may only need to allocate
as many slots as the record has values; for example 2 slots for
a linked list node.  These two optimizations allow for very cheap,
as far as memory goes, record instances.

The combined `index_and_flags` field houses both a pointer to the
record's index, and 16 bits of flag bits; of which only the lowest
2 bits are used.  The bit masked by `SEP_FLAG_MASK` indicates that the
record has been marked for 'separation' from its index; so its
current index pointer will be replaced with a new index when the
next definition to the record is requested.  The `RCU_FLAG_MASK` is
used as a marker for cycle detection when comparing records or doing
other recursive operations.


The combined `vals_and_row` field houses a pointer to the value
array, as well as the array's size as a `row` into the `valsCapTable`
*/
struct tazR_Rec {
    tazR_TPtr index_and_flags;
    #define SEP_FLAG_MASK (0x1)
    #define RCU_FLAG_MASK (0x2)

    tazR_TPtr vals_and_row;
};

#define REC_NUM_ROWS (28)
static unsigned valsCapTable[REC_NUM_ROWS] = {
    1,          2,          3,          4,
    5,          7,          10,         20,
    40,         80,         160,        320,
    640,        1280,       2560,       5120,
    10240,      20480,      40960,      81920,
    163840,     327680,     655360,     1310720,
    2621440,    5242880,    10485760,   20971520
};

tazR_Rec* tazR_makeRec( tazE_Engine* eng, tazR_Idx* idx ) {
    unsigned cap = tazR_idxNumKeys( eng, idx );
    unsigned row = 0;
    while( valsCapTable[row] < cap ) {
        row++;
        assert( row < REC_NUM_ROWS );
    }
    cap = valsCapTable[row];

    tazE_RawAnchor valsA;
    tazR_TVal* vals = tazE_mallocRaw( eng, &valsA, sizeof(tazR_TVal)*cap );
    for( unsigned i = 0 ; i < cap ; i++ )
        vals[i] = tazR_udf;
    
    tazE_ObjAnchor recA;
    tazR_Rec* rec = tazE_mallocObj( eng, &recA, sizeof(tazR_Rec), tazR_Type_REC );
    rec->index_and_flags = tazR_makeTPtr( 0, idx );
    rec->vals_and_row    = tazR_makeTPtr( row, vals );

    tazE_commitRaw( eng, &valsA );
    tazE_commitObj( eng, &recA );

    return rec;
}

static void separateRec( tazE_Engine* eng, tazR_Rec* rec ) {
    tazR_Idx*  idx  = tazR_getPtrAddr( rec->index_and_flags );
    unsigned   row  = tazR_getPtrTag( rec->vals_and_row );
    unsigned   cap  = valsCapTable[row];
    tazR_TVal* vals = tazR_getPtrAddr( rec->vals_and_row );

    bool select[cap];
    for( unsigned i = 0 ; i < cap ; i++ )
        select[i] = (tazR_getValType( vals[i] ) != tazR_Type_UDF);
    
    long locs[cap];
    idx = tazR_subIdx( eng, idx, cap, select, locs );

    unsigned ncap = 0;
    for( unsigned i = 0 ; i < cap ; i++ ) {
        if( select[i] && locs[i] >= ncap )
            ncap = locs[i] + 1;
    }

    tazE_RawAnchor nvalsA;
    tazR_TVal* nvals = tazE_mallocRaw( eng, &nvalsA, sizeof(tazR_TVal)*ncap );
    for( unsigned i = 0 ; i < cap ; i++ ) {
        if( select[i] )
            nvals[locs[i]] = vals[i];
    }
    tazE_commitRaw( eng, &nvalsA );
    tazE_freeRaw( eng, vals, sizeof(tazR_TVal)*cap );

    while( row > 0 && valsCapTable[row-1] >= ncap )
        row--;

    rec->index_and_flags = tazR_makeTPtr(
        tazR_getPtrTag( rec->index_and_flags ) & ~SEP_FLAG_MASK,
        tazR_getPtrAddr( rec->index_and_flags )
    );
    rec->vals_and_row = tazR_makeTPtr( row, nvals );
}

void tazR_recDef( tazE_Engine* eng, tazR_Rec* rec, tazR_TVal key, tazR_TVal val ) {
    tazR_Type keyType = tazR_getValType( key );
    if( keyType == tazR_Type_UDF || keyType >= tazR_Type_FIRST_OBJECT )
        tazE_error( eng, taz_ErrNum_KEY_TYPE );
    
    if( (tazR_getPtrTag( rec->index_and_flags ) & SEP_FLAG_MASK) != 0 )
        separateRec( eng, rec );
    
    tazR_Idx*  idx  = tazR_getPtrAddr( rec->index_and_flags );
    unsigned   row  = tazR_getPtrTag( rec->vals_and_row );
    unsigned   cap  = valsCapTable[row];
    tazR_TVal* vals = tazR_getPtrAddr( rec->vals_and_row );

    unsigned loc = tazR_idxInsert( eng, idx, key );
    if( loc >= cap ) {
        tazE_RawAnchor valsA = { .raw = vals, sizeof(tazR_TVal)*cap };

        unsigned ncap = loc;
        while( valsCapTable[row] <= ncap ) {
            row++;
            assert( row < REC_NUM_ROWS );
        }
        ncap = valsCapTable[row];

        vals = tazE_reallocRaw( eng, &valsA, sizeof(tazR_TVal)*ncap );
        for( unsigned i = cap ; i < ncap ; i++ )
            vals[i] = tazR_udf;
        cap = ncap;

        tazE_commitRaw( eng, &valsA );
        rec->vals_and_row = tazR_makeTPtr( row, vals );
    }

    vals[loc] = val;
}

void tazR_recSet( tazE_Engine* eng, tazR_Rec* rec, tazR_TVal key, tazR_TVal val ) {
    tazR_Type keyType = tazR_getValType( key );
    if( keyType == tazR_Type_UDF || keyType >= tazR_Type_FIRST_OBJECT )
        tazE_error( eng, taz_ErrNum_KEY_TYPE );
    tazR_Type valType = tazR_getValType( val );
    if( valType == tazR_Type_UDF )
        tazE_error( eng, taz_ErrNum_SET_TO_UDF );

    tazR_Idx*  idx  = tazR_getPtrAddr( rec->index_and_flags );
    unsigned   row  = tazR_getPtrTag( rec->vals_and_row );
    unsigned   cap  = valsCapTable[row];
    tazR_TVal* vals = tazR_getPtrAddr( rec->vals_and_row );
    
    long loc = tazR_idxLookup( eng, idx, key );
    if( loc < 0 || loc >= cap || tazR_getValType( vals[loc] ) == tazR_Type_UDF )
        tazE_error( eng, taz_ErrNum_SET_UNDEFINED );
    
    vals[loc] = val;
}

tazR_TVal tazR_recGet( tazE_Engine* eng, tazR_Rec* rec, tazR_TVal key ) {
    tazR_Type keyType = tazR_getValType( key );
    if( keyType == tazR_Type_UDF || keyType >= tazR_Type_FIRST_OBJECT )
        tazE_error( eng, taz_ErrNum_KEY_TYPE );

    tazR_Idx*  idx  = tazR_getPtrAddr( rec->index_and_flags );
    unsigned   row  = tazR_getPtrTag( rec->vals_and_row );
    unsigned   cap  = valsCapTable[row];
    tazR_TVal* vals = tazR_getPtrAddr( rec->vals_and_row );
    
    long loc = tazR_idxLookup( eng, idx, key );
    if( loc < 0 || loc >= cap || tazR_getValType( vals[loc] ) == tazR_Type_UDF )
        return tazR_udf;
    
    return vals[loc];
}

void tazR_recSep( tazE_Engine* eng, tazR_Rec* rec ) {
    tazR_Idx* idx = tazR_getPtrAddr( rec->index_and_flags );
    unsigned  tag = tazR_getPtrTag ( rec->index_and_flags );
    rec->index_and_flags = tazR_makeTPtr( tag | SEP_FLAG_MASK, idx );
}


void _tazR_scanRec( tazE_Engine* eng, tazR_Rec* rec, bool full ) {
    tazR_Idx*  idx  = tazR_getPtrAddr( rec->index_and_flags );
    unsigned   row  = tazR_getPtrTag( rec->vals_and_row );
    unsigned   cap  = valsCapTable[row];
    tazR_TVal* vals = tazR_getPtrAddr( rec->vals_and_row );

    tazE_markObj( eng, idx );
    for( unsigned i = 0 ; i < cap ; i++ )
        tazE_markVal( eng, vals[i] );
}

size_t _tazR_sizeofRec( tazE_Engine* eng, tazR_Rec* rec ) {
    return sizeof(tazR_Rec);
}

void _tazR_finlRec( tazE_Engine* eng, tazR_Rec* rec ) {
    unsigned   row  = tazR_getPtrTag( rec->vals_and_row );
    unsigned   cap  = valsCapTable[row];
    tazR_TVal* vals = tazR_getPtrAddr( rec->vals_and_row );

    tazE_freeRaw( eng, vals, sizeof(tazR_TVal)*cap );
}

struct tazR_RecIter {
    tazR_State    base;
    tazR_Rec*     rec;
    tazR_IdxIter* iter;
};

static void scanRecIter( tazE_Engine* eng, tazR_State* self, bool full ) {
    tazR_RecIter* iter = (tazR_RecIter*)self;
    tazE_markObj( eng, iter->rec );
    tazE_markObj( eng, iter->iter );
}

static size_t sizeofRecIter( tazE_Engine* eng, tazR_State* self ) {
    return sizeof(tazR_RecIter);
}

tazR_RecIter* tazR_makeRecIter( tazE_Engine* eng, tazR_Rec* rec ) {
    struct {
        tazE_Bucket base;
        tazR_TVal   idxIter;
    } buc;
    tazE_addBucket( eng, &buc, 1 );

    tazR_IdxIter* idxIter = tazR_makeIdxIter( eng, tazR_getPtrAddr( rec->index_and_flags ) );
    buc.idxIter = tazR_stateVal( (tazR_State*)idxIter );

    tazE_ObjAnchor recIterA;
    tazR_RecIter* recIter = tazE_mallocObj( eng, &recIterA, sizeof(tazR_RecIter), tazR_Type_STATE );
    recIter->base.scan = scanRecIter;
    recIter->base.size = sizeofRecIter;
    recIter->base.finl = NULL;

    recIter->rec  = rec;
    recIter->iter = idxIter;

    tazE_commitObj( eng, &recIterA );
    tazE_remBucket( eng, &buc );

    return recIter;
}

bool tazR_recIterNext( tazE_Engine* eng, tazR_RecIter* iter, tazR_TVal* key, tazR_TVal* val ) {
    unsigned   cap  = valsCapTable[ tazR_getPtrTag( iter->rec->vals_and_row ) ];
    tazR_TVal* vals = tazR_getPtrAddr( iter->rec->vals_and_row );

    unsigned loc = cap;
    while( loc >= cap || tazR_getValType( vals[loc] ) == tazR_Type_UDF ) {
        if( !tazR_idxIterNext( eng, iter->iter, key, &loc ) )
            return false;
    }
    *val = vals[loc];
    return true;
}

unsigned tazR_recCount( tazE_Engine* eng, tazR_Rec* rec ) {
    unsigned   cap  = valsCapTable[ tazR_getPtrTag( rec->vals_and_row ) ];
    tazR_TVal* vals = tazR_getPtrAddr( rec->vals_and_row );
    
    unsigned count = 0;
    for( unsigned i = 0 ; i < cap ; i++ ) {
        if( tazR_getValType( vals[i] ) != tazR_Type_UDF )
            count++;
    }

    return count;
}

static bool areEqual( tazE_Engine* eng, tazR_Rec* rec1, tazR_Rec* rec2, bool* cyclic );

static bool isSubset( tazE_Engine* eng, tazR_Rec* rec1, tazR_Rec* rec2, bool* cyclic ) {
    unsigned tag = tazR_getPtrTag( rec1->index_and_flags );
    if( tag & RCU_FLAG_MASK ) {
        *cyclic = true;
        return false;
    }
    
    struct {
        tazE_Bucket base;
        tazR_TVal   iter;
    } buc;
    tazE_addBucket( eng, &buc, 1 );
    

    tazR_RecIter* iter = tazR_makeRecIter( eng, rec1 );
    buc.iter = tazR_stateVal( (tazR_State*)iter );

    bool isSub = true;

    tazR_TVal key, val;
    while( tazR_recIterNext( eng, iter, &key, &val ) ) {
        tazR_TVal oth = tazR_recGet( eng, rec2, key );

        tazR_Type type = tazR_getValType( oth );
        if( type != tazR_getValType( val ) ) {
            isSub = false;
            break;
        }
        if( type == tazR_Type_STR ) {
            if( !tazE_strEqual( eng, tazR_getValStr( val ), tazR_getValStr( oth ) ) ) {
                isSub = false;
                break;
            }
        }
        else
        if( !tazR_valEqual( val, oth ) ) {
            if( type == tazR_Type_REC ) {
                
                rec1->index_and_flags = tazR_makeTPtr( tag | RCU_FLAG_MASK, tazR_getPtrAddr( rec1->index_and_flags ) );
                isSub = areEqual( eng, tazR_getValRec( val ), tazR_getValRec( oth ), cyclic );
                rec1->index_and_flags = tazR_makeTPtr( tag & !RCU_FLAG_MASK, tazR_getPtrAddr( rec1->index_and_flags ) );
                if( !isSub )
                    break;
            }
        }
    }

    tazE_remBucket( eng, &buc );
    return isSub;
}

static bool areEqual( tazE_Engine* eng, tazR_Rec* rec1, tazR_Rec* rec2, bool* cyclic ) {
    bool isSub = isSubset( eng, rec1, rec2, cyclic );
    if( *cyclic )
        return false;
    
    return isSub && tazR_recCount( eng, rec1 ) == tazR_recCount( eng, rec2 );
}

bool tazR_recEqual( tazE_Engine* eng, tazR_Rec* rec1, tazR_Rec* rec2 ) {
    bool cyclic = false;
    bool equal  = areEqual( eng, rec1, rec2, &cyclic );
    if( cyclic )
        tazE_error( eng, taz_ErrNum_CYCLIC_RECORD );
    
    return equal;
}

bool tazR_recLess( tazE_Engine* eng, tazR_Rec* rec1, tazR_Rec* rec2 ) {
    bool cyclic = false;
    bool subset = isSubset( eng, rec1, rec2, &cyclic );
    if( cyclic )
        tazE_error( eng, taz_ErrNum_CYCLIC_RECORD );
    
    return subset && tazR_recCount( eng, rec1 ) < tazR_recCount( eng, rec2 );
}

bool tazR_recLessOrEqual( tazE_Engine* eng, tazR_Rec* rec1, tazR_Rec* rec2 ) {
    bool cyclic = false;
    bool subset = isSubset( eng, rec1, rec2, &cyclic );
    if( cyclic )
        tazE_error( eng, taz_ErrNum_CYCLIC_RECORD );
    
    return subset && tazR_recCount( eng, rec1 ) <= tazR_recCount( eng, rec2 );
}