#include "taz_engine.h"
#include <string.h>

typedef struct tazE_EngineFull tazE_EngineFull;

struct tazE_EngineFull {
    tazE_Engine view;
    taz_Alloc   alloc;
    
    tazE_Barrier*  barriers;
    tazR_Obj*      objects;
    
    bool isGCRunning;
    bool isFullCycle;
    
    unsigned nGCCycles;
    
    size_t memUsed;
    size_t memLimit;
    double memGrowth;
    
    unsigned   gcStackTop;
    tazR_Obj** gcStackBuf;
    tazR_Obj*  gcFirstStackBuf[taz_CONFIG_GC_STACK_SEGMENT_SIZE];
    
    tazR_TVal errvalBadAlloc;
};

/********************** Memory Management Helpers *****************************/

static void collect( tazE_EngineFull* eng, size_t nsz );

static void* reallocMem( tazE_EngineFull* eng, void* old, size_t osz, size_t nsz ) {
    assert( !eng->isGCRunning || nsz == 0 );
    
    if( eng->memUsed - osz + nsz > eng->memLimit  )
        collect( eng, nsz );
    
    void* mem = eng->alloc( old, osz, nsz );
    if( !mem && nsz > 0 ) {
        collect( eng, nsz );
        if( !mem )
            tazE_error( (tazE_Engine*)eng, taz_ErrNum_FATAL, eng->errvalBadAlloc );
    }
    
    eng->memUsed -= osz;
    eng->memUsed += nsz;
    return mem;
}

static void* mallocMem( tazE_EngineFull* eng, size_t nsz ) {
    return reallocMem( eng, NULL, 0, nsz );
}

static void  freeMem( tazE_EngineFull* eng, void* old, size_t osz ) {
    reallocMem( eng, old, osz, 0 );
}

#define tazR_finlIdx( ENG, OBJ )
#define tazR_finlRec( ENG, OBJ )
#define tazR_finlCode( ENG, OBJ )
#define tazR_finlFun( ENG, OBJ )
#define tazR_finlFib( ENG, OBJ )
#define tazR_finlBox( ENG, OBJ )

static void destructObj( tazE_EngineFull* eng, tazR_Obj* obj ) {
    void* data = tazR_getObjData( obj );
    switch( tazR_getObjType( obj ) ) {
        case tazR_Type_IDX:
            tazR_finlIdx( (tazE_Engine*)eng, data );
        break;
        case tazR_Type_REC:
            tazR_finlRec( (tazE_Engine*)eng, data );
        break;
        case tazR_Type_CODE:
            tazR_finlCode( (tazE_Engine*)eng, data );
        break;
        case tazR_Type_FUN:
            tazR_finlFun( (tazE_Engine*)eng, data );
        break;
        case tazR_Type_FIB:
            tazR_finlFib( (tazE_Engine*)eng, data );
        break;
        case tazR_Type_BOX:
            tazR_finlBox( (tazE_Engine*)eng, data );
        break;
        case tazR_Type_STATE:
            if( ((tazR_State*)data)->finl )
                ((tazR_State*)data)->finl( (tazE_Engine*)eng, data );
        break;
        default:
            assert( false );
        break;
    }
}

#define tazR_sizeofIdx( ENG, OBJ )  0
#define tazR_sizeofRec( ENG, OBJ )  0
#define tazR_sizeofCode( ENG, OBJ ) 0
#define tazR_sizeofFun( ENG, OBJ )  0
#define tazR_sizeofFib( ENG, OBJ )  0
#define tazR_sizeofBox( ENG, OBJ )  0

static void releaseObj( tazE_EngineFull* eng, tazR_Obj* obj ) {
    size_t size = sizeof(tazR_Obj);
    void*  data = tazR_getObjData( obj );
    switch( tazR_getObjType( obj ) ) {
        case tazR_Type_IDX:
            size += tazR_sizeofIdx( (tazE_Engine*)eng, data );
        break;
        case tazR_Type_REC:
            size += tazR_sizeofRec( (tazE_Engine*)eng, data );
        break;
        case tazR_Type_CODE:
            size += tazR_sizeofCode( (tazE_Engine*)eng, data );
        break;
        case tazR_Type_FUN:
            size += tazR_sizeofFun( (tazE_Engine*)eng, data );
        break;
        case tazR_Type_FIB:
            size += tazR_sizeofFib( (tazE_Engine*)eng, data );
        break;
        case tazR_Type_BOX:
            size += tazR_sizeofBox( (tazE_Engine*)eng, data );
        break;
        case tazR_Type_STATE:
            if( ((tazR_State*)data)->size )
                size += ((tazR_State*)data)->size( (tazE_Engine*)eng, data );
        break;
        default:
            assert( false );
        break;
    }
    freeMem( eng, obj, size );
}


#define tazR_scanIdx( ENG, OBJ )
#define tazR_scanRec( ENG, OBJ )
#define tazR_scanCode( ENG, OBJ )
#define tazR_scanFun( ENG, OBJ )
#define tazR_scanFib( ENG, OBJ )
#define tazR_scanBox( ENG, OBJ )

static void scanObj( tazE_EngineFull* eng, tazR_Obj* obj ) {
    void*  data = tazR_getObjData( obj );
    switch( tazR_getObjType( obj ) ) {
        case tazR_Type_IDX:
            tazR_scanIdx( (tazE_Engine*)eng, data );
        break;
        case tazR_Type_REC:
            tazR_scanRec( (tazE_Engine*)eng, data );
        break;
        case tazR_Type_CODE:
            tazR_scanCode( (tazE_Engine*)eng, data );
        break;
        case tazR_Type_FUN:
            tazR_scanFun( (tazE_Engine*)eng, data );
        break;
        case tazR_Type_FIB:
            tazR_scanFib( (tazE_Engine*)eng, data );
        break;
        case tazR_Type_BOX:
            tazR_scanBox( (tazE_Engine*)eng, data );
        break;
        case tazR_Type_STATE:
            if( ((tazR_State*)data)->scan )
                ((tazR_State*)data)->scan( (tazE_Engine*)eng, data );
        break;
        default:
            assert( false );
        break;
    }
}

static void clearBarrier( tazE_EngineFull* eng, tazE_Barrier* bar ) {
    tazE_ObjAnchor* oIt = bar->objAnchors;
    while( oIt ) {
        tazE_ObjAnchor* anc = oIt;
        oIt = oIt->next;
        
        tazE_cancelObj( (tazE_Engine*)eng, anc );
    }
    
    tazE_RawAnchor* rIt = bar->rawAnchors;
    while( rIt ) {
        tazE_RawAnchor* anc = rIt;
        rIt = rIt->next;
        
        tazE_cancelRaw( (tazE_Engine*)eng, anc );
    }
    
    bar->buckets = NULL;
}

static struct {
    tazE_Bucket base;
    tazR_TVal   val1;
    tazR_TVal   val2;
} const DMY_BUCKET;
static size_t const FIRST_BUCKET_OFFSET = (void*)&DMY_BUCKET.val1 - (void*)&DMY_BUCKET;
static size_t const NEXT_BUCKET_OFFSET  = (void*)&DMY_BUCKET.val2 - (void*)&DMY_BUCKET.val1;

static void collect( tazE_EngineFull* eng, size_t nsz ) {
    
    eng->isGCRunning = true;
    
    if( eng->nGCCycles++ % taz_CONFIG_GC_FULL_CYCLE_INTERVAL == 0 )
        eng->isFullCycle = true;
    
    // Scan.
    tazE_markVal( (tazE_Engine*)eng, eng->errvalBadAlloc );
    if( eng->view.modPool )
        tazE_markObj( (tazE_Engine*)eng, eng->view.modPool );
    if( eng->view.fmtState )
        tazE_markObj( (tazE_Engine*)eng, eng->view.fmtState );
    if( eng->view.apiState )
        tazE_markObj( (tazE_Engine*)eng, eng->view.apiState );
    
    tazE_Barrier* barIt = eng->barriers;
    while( barIt ) {
        tazE_Barrier* bar = barIt;
        barIt = barIt->prev;
        
        tazE_Bucket* bucIt = bar->buckets;
        while( bucIt ) {
            tazE_Bucket* buc = bucIt;
            bucIt = bucIt->next;
            
            void* ptr = (void*)buc + FIRST_BUCKET_OFFSET;
            for( unsigned i = 0 ; i < buc->size ; i++, ptr += NEXT_BUCKET_OFFSET ) {
                tazR_TVal* val = ptr;
                tazE_markVal( (tazE_Engine*)eng, *val );
            }
        }
        
        tazE_markVal( (tazE_Engine*)eng, bar->errval );
    }

    while( eng->gcStackTop > 0 )
        scanObj( eng, eng->gcStackBuf[--eng->gcStackTop] );
    
    // Sweep.
    tazR_Obj* alive = NULL;
    tazR_Obj* dead  = NULL;
    
    tazR_Obj* it = eng->objects;
    while( it ) {
        tazR_Obj* obj = it;
        it = tazR_getPtrAddr( it->next_and_tag );
        
        if( tazR_isObjMarked( obj ) ) {
            obj->next_and_tag = tazR_makeTPtr(
                tazR_getPtrTag( obj->next_and_tag ) & ~tazR_OBJ_TAG_MARK_MASK,
                alive
            );
            alive = obj;
        }
        else {
            destructObj( eng, obj );
            obj->next_and_tag = tazR_makeTPtr(
                tazR_getPtrTag( obj->next_and_tag ) | tazR_OBJ_TAG_DEAD_MASK,
                dead
            );
            dead = obj;
        }
    }
    
    it = dead;
    while( it ) {
        tazR_Obj* obj = it;
        it = tazR_getPtrAddr( it->next_and_tag );
        
        releaseObj( eng, obj );
    }
    
    eng->objects = alive;
    
    // Adjust heap limit.
    assert( eng->memGrowth > 0.0 && eng->memGrowth <= 1.0 );
    double mul = 1.0 + eng->memGrowth;
    eng->memLimit = (double)(eng->memLimit + nsz) * mul;
    
    // Finish up.
    eng->isGCRunning = false;
    
    if( eng->isFullCycle )
        eng->isFullCycle = false;
}

/*************************** API Functions ************************************/

tazE_Engine* tazE_makeEngine( taz_Config const* cfg ) {
    taz_Alloc        alloc = cfg->alloc;
    tazE_EngineFull* eng   = alloc( NULL, 0, sizeof(tazE_EngineFull) );
    
    eng->view.modPool  = NULL;
    eng->view.fmtState = NULL;
    eng->view.apiState = NULL;
    eng->alloc         = alloc;
    eng->barriers      = NULL;
    eng->objects       = NULL;
    eng->isGCRunning   = false;
    eng->isFullCycle   = false;
    eng->nGCCycles     = 0;
    eng->memUsed       = sizeof(tazE_EngineFull);
    eng->memLimit      = 1024;
    eng->memGrowth     = 0.5;
    eng->gcStackTop    = 0;
    eng->gcStackBuf    = eng->gcFirstStackBuf;
    
    /// Replace this with a string at some point.
    eng->errvalBadAlloc = tazR_udf;
    
    return (tazE_Engine*)eng;
}

void tazE_freeEngine( tazE_Engine* _eng ) {
    tazE_EngineFull* eng = (tazE_EngineFull*)_eng;
    
    tazR_Obj* objIter = eng->objects;
    while( objIter ) {
        tazR_Obj* obj = objIter;
        objIter = tazR_getObjNext( obj );
        
        destructObj( eng, obj );
    }
    objIter = eng->objects;
    while( objIter ) {
        tazR_Obj* obj = objIter;
        objIter = tazR_getObjNext( obj );
        
        releaseObj( eng, obj );
    }
    
    while( eng->barriers ) {
        clearBarrier( eng, eng->barriers );
        eng->barriers = eng->barriers->prev;
    }
    
    eng->alloc( eng, sizeof(tazE_EngineFull), 0 );
}

static void scanWithNewStack( tazE_EngineFull* eng, tazR_Obj* obj ) {
    tazR_Obj** obuf = eng->gcStackBuf;
    unsigned   otop = eng->gcStackTop;
    
    tazR_Obj* buf[taz_CONFIG_GC_STACK_SEGMENT_SIZE];
    
    eng->gcStackBuf = buf;
    eng->gcStackTop = 0;
    
    scanObj( eng, obj );
    while( eng->gcStackTop > 0 )
        scanObj( eng, eng->gcStackBuf[--eng->gcStackTop] );
    
    eng->gcStackBuf = obuf;
    eng->gcStackTop = otop;
}

void tazE_markObj( tazE_Engine* _eng, void* ptr ) {
    tazE_EngineFull* eng = (tazE_EngineFull*)_eng;
    
    tazR_Obj* obj = tazR_toObj( ptr );
    if( tazR_isObjMarked( obj ) )
        return;
    
    obj->next_and_tag = tazR_makeTPtr(
        tazR_getPtrTag( obj->next_and_tag ) | tazR_OBJ_TAG_MARK_MASK,
        tazR_getPtrAddr( obj->next_and_tag )
    );
    if( eng->gcStackTop < taz_CONFIG_GC_STACK_SEGMENT_SIZE && eng->gcStackBuf != NULL ) {
        eng->gcStackBuf[eng->gcStackTop++] = obj;
        return;
    }
    
    scanWithNewStack( eng, obj );
}

void tazE_markStr( tazE_Engine* eng, tazR_Str str ) {
    // TODO
    assert( false );
}

void tazE_collect( tazE_Engine* eng ) {
    collect( (tazE_EngineFull*)eng, 0 );
}

void* tazE_mallocObj( tazE_Engine* _eng, tazE_ObjAnchor* anchor, size_t sz, tazR_Type type ) {
    tazE_EngineFull* eng = (tazE_EngineFull*)_eng;
    
    size_t    osz = sizeof(tazR_Obj) + sz;
    tazR_Obj* obj = mallocMem( eng,  osz );
    obj->next_and_tag = tazR_makeTPtr(
        (type << tazR_OBJ_TAG_TYPE_SHIFT) & tazR_OBJ_TAG_TYPE_MASK,
        NULL
    );
    
    anchor->obj = obj;
    anchor->sz  = osz;
    
    assert( eng->barriers );
    tazR_linkWithNextAndLink( &eng->barriers->objAnchors, anchor );
    
    return tazR_getObjData( obj );
}

void* tazE_zallocObj( tazE_Engine* eng, tazE_ObjAnchor* anchor, size_t sz, tazR_Type type ) {
    void* ptr = tazE_mallocObj( eng, anchor, sz, type );
    memset( ptr, 0, sz );
    return ptr;
}

void tazE_cancelObj( tazE_Engine* eng, tazE_ObjAnchor* anchor ) {
    tazR_unlinkWithNextAndLink( anchor );
    freeMem( (tazE_EngineFull*)eng, anchor->obj, anchor->sz );
}

void  tazE_commitObj( tazE_Engine* _eng, tazE_ObjAnchor* anchor ) {
    tazE_EngineFull* eng = (tazE_EngineFull*)_eng;
    tazR_unlinkWithNextAndLink( anchor );
    
    tazR_Obj* obj = anchor->obj;
    obj->next_and_tag = tazR_makeTPtr( tazR_getPtrTag( obj->next_and_tag ), eng->objects );
    eng->objects = obj;
}

void* tazE_mallocRaw( tazE_Engine* _eng, tazE_RawAnchor* anchor, size_t sz ) {
    tazE_EngineFull* eng = (tazE_EngineFull*)_eng;
    void* ptr = mallocMem( eng, sz );
    anchor->raw = ptr;
    anchor->sz  = sz;
    
    assert( eng->barriers );
    tazR_linkWithNextAndLink( &eng->barriers->rawAnchors, anchor );
    
    return ptr;
}

void* tazE_zallocRaw( tazE_Engine* eng, tazE_RawAnchor* anchor, size_t sz ) {
    void* ptr = tazE_mallocRaw( eng, anchor, sz );
    memset( ptr, 0, sz );
    return ptr;
}

void* tazE_reallocRaw( tazE_Engine* _eng, tazE_RawAnchor* anchor, size_t sz ) {
    tazE_EngineFull* eng = (tazE_EngineFull*)_eng;
    
    void* ptr = reallocMem( eng, anchor->raw, anchor->sz, sz );
    anchor->raw = ptr;
    anchor->sz  = sz;
    
    assert( eng->barriers );
    tazR_linkWithNextAndLink( &eng->barriers->rawAnchors, anchor );
    return ptr;
}

void tazE_freeRaw( tazE_Engine* eng, void* raw, size_t sz ) {
    freeMem( (tazE_EngineFull*)eng, raw, sz );
}

void tazE_cancelRaw( tazE_Engine* _eng, tazE_RawAnchor* anchor ) {
    tazE_EngineFull* eng = (tazE_EngineFull*)_eng;
    
    freeMem( eng, anchor->raw, anchor->sz );
    tazR_unlinkWithNextAndLink( anchor );
}

void tazE_commitRaw( tazE_Engine* eng, tazE_RawAnchor* anchor ) {
    tazR_unlinkWithNextAndLink( anchor );
}

void tazE_addBucket( tazE_Engine* _eng, void* _buc, unsigned size ) {
    tazE_EngineFull* eng = (tazE_EngineFull*)_eng;
    tazE_Bucket*     buc = _buc;
    assert( eng->barriers != NULL );
    
    tazR_linkWithNextAndLink( &eng->barriers->buckets, buc );
    buc->size = size;
    
    void* ptr = (void*)buc + FIRST_BUCKET_OFFSET;
    for( unsigned i = 0 ; i < buc->size ; i++, ptr += NEXT_BUCKET_OFFSET ) {
        tazR_TVal* val = ptr;
        *val = tazR_udf;
    }
}

void tazE_remBucket( tazE_Engine* eng, void* _buc ) {
    tazE_Bucket* buc = _buc;
    tazR_unlinkWithNextAndLink( buc );
}

void tazE_pushBarrier( tazE_Engine* _eng, tazE_Barrier* barrier ) {
    tazE_EngineFull* eng = (tazE_EngineFull*)_eng;
    
    barrier->prev = eng->barriers;
    eng->barriers = barrier;
    
    barrier->objAnchors = NULL;
    barrier->rawAnchors = NULL;
    barrier->buckets    = NULL;
    barrier->errnum     = taz_ErrNum_NONE;
    barrier->errval     = tazR_udf;
}

void tazE_popBarrier( tazE_Engine* _eng, tazE_Barrier* barrier ) {
    tazE_EngineFull* eng = (tazE_EngineFull*)_eng;
    assert( eng->barriers == barrier );
    
    clearBarrier( eng, barrier );
    eng->barriers = eng->barriers->prev;
}

void tazE_error( tazE_Engine* eng, taz_ErrNum errnum, tazR_TVal errval ) {
    // TODO
    assert( false );
}

void tazE_yield( tazE_Engine* eng ) {
    // TODO
    assert( false );
}