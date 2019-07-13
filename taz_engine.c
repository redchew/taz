#include "taz_engine.h"
#include <string.h>
#include <limits.h>

typedef struct EngineFull EngineFull;
typedef struct StrPool    StrPool;

struct EngineFull {
    tazE_Engine view;
    taz_Alloc   alloc;
    
    tazE_Barrier*  barriers;
    tazR_Obj*      objects;
    taz_StrLoan*   loans;
    
    bool isGCRunning;
    bool isFullCycle;
    
    unsigned nGCCycles;
    
    size_t memUsed;
    size_t memLimit;
    double memGrowth;
    
    unsigned   gcStackTop;
    tazR_Obj** gcStackBuf;
    bool       gcDisabled;
    
    tazR_Obj* gcFirstStackBuf[taz_CONFIG_GC_STACK_SEGMENT_SIZE];
    
    tazR_TVal errvalBadAlloc;
    
    StrPool* strPool;
};

/********************** Memory Management Helpers *****************************/

static void collect( EngineFull* eng, size_t nsz, bool full );

static void* reallocMem( EngineFull* eng, void* old, size_t osz, size_t nsz ) {
    assert( !eng->isGCRunning || nsz == 0 );
    
    if( eng->memUsed - osz + nsz > eng->memLimit  )
        collect( eng, nsz, false );
    
    void* mem = eng->alloc( old, osz, nsz );
    if( !mem && nsz > 0 ) {
        collect( eng, nsz, false );
        if( !mem )
            tazE_error( (tazE_Engine*)eng, taz_ErrNum_FATAL, eng->errvalBadAlloc );
    }
    
    eng->memUsed -= osz;
    eng->memUsed += nsz;
    return mem;
}

static void* mallocMem( EngineFull* eng, size_t nsz ) {
    return reallocMem( eng, NULL, 0, nsz );
}

static void  freeMem( EngineFull* eng, void* old, size_t osz ) {
    reallocMem( eng, old, osz, 0 );
}

#define tazR_finlIdx( ENG, OBJ )
#define tazR_finlRec( ENG, OBJ )
#define tazR_finlCode( ENG, OBJ )
#define tazR_finlFun( ENG, OBJ )
#define tazR_finlFib( ENG, OBJ )
#define tazR_finlBox( ENG, OBJ )

static void destructObj( EngineFull* eng, tazR_Obj* obj ) {
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

static void releaseObj( EngineFull* eng, tazR_Obj* obj ) {
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

static void scanObj( EngineFull* eng, tazR_Obj* obj ) {
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

static void clearBarrier( EngineFull* eng, tazE_Barrier* bar ) {
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

static void adjustHeap( EngineFull* eng, size_t nsz ) {
    assert( eng->memGrowth > 0.0 && eng->memGrowth <= 1.0 );
    double mul = 1.0 + eng->memGrowth;
    eng->memLimit = (double)(eng->memLimit + nsz) * mul;
}

static void startStringGC( tazE_Engine* eng );
static void finishStringGC( tazE_Engine* eng );

static void collect( EngineFull* eng, size_t nsz, bool full ) {
    if( eng->gcDisabled ) {
        adjustHeap( eng, nsz );
        return;
    }
    
    eng->isGCRunning = true;
    
    if( eng->nGCCycles++ % taz_CONFIG_GC_FULL_CYCLE_INTERVAL == 0 || full ) {
        eng->isFullCycle = true;
        startStringGC( (tazE_Engine*)eng );
    }
    
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
    
    // Finish up.
    adjustHeap( eng, nsz );
    
    eng->isGCRunning = false;
    
    if( eng->isFullCycle ) {
        eng->isFullCycle = false;
        finishStringGC( (tazE_Engine*)eng );
    }
}

/**************************** String Pooling **********************************/


typedef struct StrNode       StrNode;
typedef struct StrNodeLong  StrNodeLong;
typedef struct StrNodeMedium StrNodeMedium;

struct StrNode {
    unsigned  hash  : 30;
    unsigned  mark  : 1;
    unsigned  large : 1;
    unsigned  id;
};

struct StrNodeLong {
    StrNode base;
    size_t  len;
    char    buf[];
};

struct StrNodeMedium {
    StrNode  base;
    
    StrNodeMedium*  next;
    StrNodeMedium** link;
    
    size_t len;
    char   buf[];
};

typedef StrNode* StrNodeBlock[sizeof(unsigned)];

struct StrPool {
    
    // This part makes up the hashmap used for interning medium
    // length strings.
    unsigned        hcap;
    unsigned        hcnt;
    StrNodeMedium** hmap;
    
    // This keeps a direct mapping from string handles to nodes,
    // it's used for both medium and long strings.
    size_t        ncap;
    StrNodeBlock* nmap;
    
    // This is a bitmap to keep track of which slots in nmap are
    // being used and which are free.  It'll always have the
    // same capacity as nmap.
    unsigned* bmap;
};

static StrPool* makeStrPool( tazE_Engine* eng ) {
    tazE_RawAnchor poolA, hmapA, nmapA, bmapA;
    
    unsigned        hcap = 21;
    StrNodeMedium** hmap = tazE_zallocRaw( eng, &hmapA, sizeof(StrNodeMedium*)*hcap );
    
    unsigned      ncap = 1;
    StrNodeBlock* nmap = tazE_zallocRaw( eng, &nmapA, sizeof(StrNodeBlock)*ncap );
    unsigned*     bmap = tazE_zallocRaw( eng, &bmapA, sizeof(unsigned)*ncap );
    
    StrPool* pool = tazE_mallocRaw( eng, &poolA, sizeof(StrPool) );
    pool->hcap   = hcap;
    pool->hcnt   = 0;
    pool->hmap   = hmap;
    pool->ncap   = ncap;
    pool->nmap   = nmap;
    pool->bmap   = bmap;
    
    tazE_commitRaw( eng, &poolA );
    tazE_commitRaw( eng, &hmapA );
    tazE_commitRaw( eng, &nmapA );
    tazE_commitRaw( eng, &bmapA );
    return pool;
}

static void freeStrPool( tazE_Engine* eng, StrPool* pool ) {
    for( unsigned i = 0 ; i < pool->ncap ; i++ ) {
        for( unsigned j = 0 ; j < elemsof( pool->nmap[i] ) ; j++ ) {
            StrNode* node = pool->nmap[i][j];
            if( node ) {
                if( node->large ) {
                    StrNodeLong* nodeL = (StrNodeLong*)node;
                    tazE_freeRaw( eng, pool->nmap[i][j], sizeof(StrNodeLong) + nodeL->len + 1 );
                }
                else {
                    StrNodeMedium* nodeM = (StrNodeMedium*)node;
                    tazE_freeRaw( eng, pool->nmap[i][j], sizeof(StrNodeMedium) + nodeM->len + 1 );
                }
            }
        }
    }
    
    tazE_freeRaw( eng, pool->hmap, sizeof(StrNodeMedium*)*pool->hcap );
    tazE_freeRaw( eng, pool->nmap, sizeof(StrNodeBlock)*pool->ncap );
    tazE_freeRaw( eng, pool->bmap, sizeof(unsigned)*pool->ncap );
    tazE_freeRaw( eng, pool, sizeof(StrPool) );
}

/* Note: String Format
Strings can be either short, medium, or long; and are represented in a integral
type (tazR_Str) differently depending on the type.  The topmost byte of the
6 bytes the string is allowed to use (only 6 since the top 2 are used for value
meta data) is reserved for string meta data.  The topmost two bits indicate the
string type.  If the type matches STR_SHORT then the rest of the bits give the
short string's size (the number of bytes it uses), any unused bytes (at the lower
end of the integral) must be zero.  If for other string types the rest of the
meta bits should be cleared.
*/

#define STR_SIZE_MASK  (0x3FLLU << 40)
#define STR_SIZE_SHIFT (40)
#define STR_TYPE_MASK  (0x3LLU << 46)
#define STR_SHORT      (0x2LLU << 46)
#define STR_MEDIUM     (0x1LLU << 46)
#define STR_LONG       (0x0LLU << 46)

#define SHORT_STR_MAX_LEN  (5)
#define MEDIUM_STR_MAX_LEN (16)

static tazR_Str makeShortStr( tazE_Engine* eng, StrPool* pool, char const* str, size_t len ) {
    tazR_Str ss = 0;
    for( unsigned i = 0 ; i < len ; i++ )
        ss = (ss << 8) | str[i];
    
    ss |= STR_SHORT | (len << STR_SIZE_SHIFT);
    return ss;
}

static tazR_Str makeStrId( tazE_Engine* eng, StrPool* pool ) {
    for( unsigned i = 0 ; i < pool->ncap ; i++ ) {
        
        unsigned unit = pool->bmap[i];
        
        // This checks if the full unit of spaces if occupied.
        if(  unit ^ UINT_MAX )
            continue;
        
        for( unsigned j = 0 ; j < sizeof(unsigned) ; j++ ) {
            
            // If the next byte doens't have any empty bits.
            if( (unit & 0xFF) == 0xFF ) {
                unit >>= 8;
                continue;
            }
            
            for( unsigned k = 0 ; k < 8 ; k++ ) {
                if( unit & 0x1 ) {
                    unit >>= 1;
                    continue;
                }
                
                unsigned arrayOffset = i;
                unsigned bitOffset   = j*8 + k;
                pool->bmap[arrayOffset] |= 1U << bitOffset;
                return arrayOffset*sizeof(unsigned) + bitOffset;
            }
        }
    }
    
    // Couldn't find a place for the new string, so grow the pool.
    unsigned        id   = pool->ncap * sizeof(unsigned);
    unsigned        ncap = pool->ncap + 1;
    tazE_RawAnchor  nmapA = { .raw = pool->nmap, .sz = sizeof(StrNodeBlock)*pool->ncap };
    tazE_RawAnchor  bmapA = { .raw = pool->bmap, .sz = sizeof(unsigned)*pool->ncap };
    
    pool->nmap = tazE_reallocRaw( eng, &nmapA, sizeof(StrNodeBlock)*ncap );
    pool->bmap = tazE_reallocRaw( eng, &bmapA, sizeof(unsigned)*ncap );
    memset( pool->nmap[pool->ncap], 0, sizeof(StrNodeBlock) );
    pool->bmap[pool->ncap] = 0;
    
    pool->bmap[pool->ncap] |= 1;
    pool->ncap = ncap;
    
    tazE_commitRaw( eng, &nmapA );
    tazE_commitRaw( eng, &bmapA );
    return id;
}

static unsigned hash( char const* str, size_t len ) {
    unsigned h = 0;
    for( unsigned i = 0 ; i < len && i < 16 ; i++ )
        h = h*37 + str[i];
    return h;
}

static tazR_Str makeMediumStr( tazE_Engine* eng, StrPool* pool, char const* str, size_t len ) {
    unsigned h = hash( str, len ) >> 2;
    unsigned i = h % pool->hcap;
    StrNodeMedium* it = pool->hmap[i];
    while( it ) {
        if( it->len == len && it->base.hash == h && !memcmp( it->buf, str, len ) )
            return it->base.id | STR_MEDIUM;
        it = it->next;
    }
    
    tazR_Str id = makeStrId( eng, pool );
    
    unsigned  arrayOffset = id / sizeof(unsigned);
    unsigned  blockOffset = id % sizeof(unsigned);
    StrNode** place       = &pool->nmap[arrayOffset][blockOffset];
    
    tazE_RawAnchor nodeA;
    StrNodeMedium* node = tazE_mallocRaw( eng, &nodeA, sizeof(StrNodeMedium) + len + 1 );
    node->base.hash  = h;
    node->base.mark  = 0;
    node->base.large = 0;
    node->base.id    = id;
    node->len = len;
    memcpy( node->buf, str, len + 1 );
    
    *place = (StrNode*)node;
    tazR_linkWithNextAndLink( &pool->hmap[i], node );
    
    tazE_commitRaw( eng, &nodeA );
    return id | STR_MEDIUM;
}

static tazR_Str makeLongStr( tazE_Engine* eng, StrPool* pool, char const* str, size_t len ) {
    tazR_Str id = makeStrId( eng, pool );
    
    unsigned  arrayOffset = id / sizeof(unsigned);
    unsigned  blockOffset = id % sizeof(unsigned);
    StrNode** place       = &pool->nmap[arrayOffset][blockOffset];
    
    tazE_RawAnchor nodeA;
    StrNodeLong* node = tazE_mallocRaw( eng, &nodeA, sizeof(StrNodeLong) + len + 1 );
    node->base.hash  = hash( str, len ) >> 2;
    node->base.mark  = 0;
    node->base.large = 1;
    node->base.id    = id;
    node->len = len;
    memcpy( node->buf, str, len + 1 );
    
    *place = (StrNode*)node;
    
    tazE_commitRaw( eng, &nodeA );
    
    return id | STR_LONG;
}

static tazR_Str makeStr( tazE_Engine* eng, StrPool* pool, char const* str, size_t len ) {
    if( len <= SHORT_STR_MAX_LEN )
        return makeShortStr( eng, pool, str, len );
    if( len <= MEDIUM_STR_MAX_LEN )
        return makeMediumStr( eng, pool, str, len );
    
    return makeLongStr( eng, pool, str, len );
}

static StrNode* getStrNode( tazE_Engine* eng, StrPool* pool, tazR_Str id ) {
    unsigned  arrayOffset = id / sizeof(unsigned);
    unsigned  blockOffset = id % sizeof(unsigned);
    return pool->nmap[arrayOffset][blockOffset];
}

static void borrowShortStr( tazE_Engine* eng, StrPool* pool, tazR_Str s, taz_StrLoan* loan ) {
    size_t len = (s & STR_SIZE_MASK) >> STR_SIZE_SHIFT;
    
    tazE_RawAnchor strA;
    char*  str = tazE_mallocRaw( eng, &strA, len + 1 );
    size_t end = len;
    str[end--] = '\0';
    for( unsigned i = 0 ; i < len ; i++ ) {
        str[end--] = s & 0xFF;
        s >>= 8;
    }
    
    loan->next = NULL;
    loan->link = NULL;
    loan->str  = str;
    loan->len  = len;
    
    tazE_commitRaw( eng, &strA );
}

static void borrowLongStr( tazE_Engine* eng, StrPool* pool, tazR_Str id, taz_StrLoan* loan ) {
    StrNodeLong* node = (StrNodeLong*)getStrNode( eng, pool, id );
    assert( node );
    
    tazR_linkWithNextAndLink( &((EngineFull*)eng)->loans, loan );
    loan->len = node->len;
    loan->str = node->buf;
}

static void borrowMediumStr( tazE_Engine* eng, StrPool* pool, tazR_Str id, taz_StrLoan* loan ) {
    StrNodeMedium* node = (StrNodeMedium*)getStrNode( eng, pool, id );
    assert( node );
    
    tazR_linkWithNextAndLink( &((EngineFull*)eng)->loans, loan );
    loan->len = node->len;
    loan->str = node->buf;
}


static void startStringGC( tazE_Engine* _eng ) {
    EngineFull* eng = (EngineFull*)_eng;
    
    // Copy out all the loaned strings.
    taz_StrLoan* loanIt = eng->loans;
    while( loanIt ) {
        tazE_stealStr( _eng, loanIt );
        loanIt = loanIt->next;
    }
    eng->loans = NULL;
}

static void collectNode( tazE_Engine* eng, StrPool* pool, StrNode* node ) {
    unsigned arrayOffset = node->id / sizeof(unsigned);
    unsigned blockOffset = node->id % sizeof(unsigned);
    pool->nmap[arrayOffset][blockOffset] = NULL;
    pool->bmap[arrayOffset] &= ~(1 << blockOffset);
    
    if( node->large ) {
        StrNodeLong* nodeL = (StrNodeLong*)node;
        tazE_freeRaw( eng, nodeL, sizeof(StrNodeLong) + nodeL->len + 1 );
    }
    else {
        StrNodeMedium* nodeM = (StrNodeMedium*)node;
        tazR_unlinkWithNextAndLink( nodeM );
        tazE_freeRaw( eng, nodeM, sizeof(StrNodeMedium) + nodeM->len + 1 );
    }
}

static void finishStringGC( tazE_Engine* _eng ) {
    EngineFull* eng  = (EngineFull*)_eng;
    StrPool*    pool = eng->strPool;
    
    // Figure out how many blocks we actually need to scan.
    unsigned end = pool->ncap;
    while( end > 0 && pool->bmap[end-1] == 0 )
        end--;
    
    for( unsigned i = 0 ; i < end ; i++ ) {
        for( unsigned j = 0 ; j < elemsof(pool->nmap[i]) ; j++ ) {
            StrNode* node = pool->nmap[i][j];
            if( !node )
                continue;
            if( node && node->mark == 0 )
                collectNode( _eng, pool, node );
            else
                node->mark = 0;
        }
    }
}

/*************************** API Functions ************************************/

tazE_Engine* tazE_makeEngine( taz_Config const* cfg ) {
    taz_Alloc        alloc = cfg->alloc;
    EngineFull* eng   = alloc( NULL, 0, sizeof(EngineFull) );
    
    eng->view.modPool  = NULL;
    eng->view.fmtState = NULL;
    eng->view.apiState = NULL;
    eng->alloc         = alloc;
    eng->barriers      = NULL;
    eng->objects       = NULL;
    eng->loans         = NULL;
    eng->isGCRunning   = false;
    eng->isFullCycle   = false;
    eng->nGCCycles     = 0;
    eng->memUsed       = sizeof(EngineFull);
    eng->memLimit      = 1024;
    eng->memGrowth     = 0.5;
    eng->gcStackTop    = 0;
    eng->gcStackBuf    = eng->gcFirstStackBuf;
    eng->gcDisabled    = true;
    
    /// Replace this with a string at some point.
    eng->errvalBadAlloc = tazR_udf;
    
    // This should come last, as it relies on the engine being
    // semi-functional.
    tazE_Barrier bar = { 0 };
    if( setjmp( bar.errorDst ) || setjmp( bar.yieldDst ) ) {
        eng->strPool = NULL;
        tazE_freeEngine( (tazE_Engine*)eng );
        return NULL;
    }
    
    tazE_pushBarrier( (tazE_Engine*)eng, &bar );
    eng->strPool = makeStrPool( (tazE_Engine*)eng );
    tazE_popBarrier( (tazE_Engine*)eng, &bar );
    
    eng->gcDisabled = false;
    
    return (tazE_Engine*)eng;
}

void tazE_freeEngine( tazE_Engine* _eng ) {
    EngineFull* eng = (EngineFull*)_eng;
    
    // This should come first, as it relies on having a functional engine.
    if( eng->strPool )
        freeStrPool( _eng, eng->strPool );
    
    // Now cleanup everything else.
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
    
    eng->alloc( eng, sizeof(EngineFull), 0 );
}

static void scanWithNewStack( EngineFull* eng, tazR_Obj* obj ) {
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
    EngineFull* eng = (EngineFull*)_eng;
    
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
    tazR_Str type = str & STR_TYPE_MASK;
    if( type == STR_SHORT )
        return;
    
    tazR_Str id   = str & ~STR_TYPE_MASK;
    StrNode* node = getStrNode( eng, ((EngineFull*)eng)->strPool, id );
    assert( node );
    
    node->mark = 1;
}

void tazE_collect( tazE_Engine* eng, bool full ) {
    collect( (EngineFull*)eng, 0, full );
}

void* tazE_mallocObj( tazE_Engine* _eng, tazE_ObjAnchor* anchor, size_t sz, tazR_Type type ) {
    EngineFull* eng = (EngineFull*)_eng;
    
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
    freeMem( (EngineFull*)eng, anchor->obj, anchor->sz );
}

void  tazE_commitObj( tazE_Engine* _eng, tazE_ObjAnchor* anchor ) {
    EngineFull* eng = (EngineFull*)_eng;
    tazR_unlinkWithNextAndLink( anchor );
    
    tazR_Obj* obj = anchor->obj;
    obj->next_and_tag = tazR_makeTPtr( tazR_getPtrTag( obj->next_and_tag ), eng->objects );
    eng->objects = obj;
}

void* tazE_mallocRaw( tazE_Engine* _eng, tazE_RawAnchor* anchor, size_t sz ) {
    EngineFull* eng = (EngineFull*)_eng;
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
    EngineFull* eng = (EngineFull*)_eng;
    
    void* ptr = reallocMem( eng, anchor->raw, anchor->sz, sz );
    anchor->raw = ptr;
    anchor->sz  = sz;
    
    assert( eng->barriers );
    tazR_linkWithNextAndLink( &eng->barriers->rawAnchors, anchor );
    return ptr;
}

void tazE_freeRaw( tazE_Engine* eng, void* raw, size_t sz ) {
    freeMem( (EngineFull*)eng, raw, sz );
}

void tazE_cancelRaw( tazE_Engine* _eng, tazE_RawAnchor* anchor ) {
    EngineFull* eng = (EngineFull*)_eng;
    
    freeMem( eng, anchor->raw, anchor->sz );
    tazR_unlinkWithNextAndLink( anchor );
}

void tazE_commitRaw( tazE_Engine* eng, tazE_RawAnchor* anchor ) {
    tazR_unlinkWithNextAndLink( anchor );
}

void tazE_addBucket( tazE_Engine* _eng, void* _buc, unsigned size ) {
    EngineFull* eng = (EngineFull*)_eng;
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
    EngineFull* eng = (EngineFull*)_eng;
    
    barrier->prev = eng->barriers;
    eng->barriers = barrier;
    
    barrier->objAnchors = NULL;
    barrier->rawAnchors = NULL;
    barrier->buckets    = NULL;
    barrier->errnum     = taz_ErrNum_NONE;
    barrier->errval     = tazR_udf;
}

void tazE_popBarrier( tazE_Engine* _eng, tazE_Barrier* barrier ) {
    EngineFull* eng = (EngineFull*)_eng;
    assert( eng->barriers == barrier );
    
    clearBarrier( eng, barrier );
    eng->barriers = eng->barriers->prev;
}

void tazE_error( tazE_Engine* _eng, taz_ErrNum errnum, tazR_TVal errval ) {
    EngineFull* eng = (EngineFull*)_eng;
    
    tazE_Barrier* bar = eng->barriers;
    assert( bar );
    eng->barriers = bar->prev;
    
    clearBarrier( eng, bar );
    
    assert( errnum != taz_ErrNum_NONE );
    bar->errnum = errnum;
    bar->errval = errval;
    
    if( bar->errorFun )
        bar->errorFun( _eng, bar );
    longjmp( bar->errorDst, 1 );
}

void tazE_yield( tazE_Engine* _eng ) {
    EngineFull* eng = (EngineFull*)_eng;
    
    tazE_Barrier* bar = eng->barriers;
    assert( bar );
    eng->barriers = bar->prev;
    
    clearBarrier( eng, bar );
    
    bar->errnum = taz_ErrNum_NONE;
    bar->errval = tazR_udf;
    
    if( bar->yieldFun )
        bar->yieldFun( _eng, bar );
    longjmp( bar->yieldDst, 1 );
}

tazR_Str tazE_makeStr( tazE_Engine* eng, char const* str, size_t len ) {
    return makeStr( eng, ((EngineFull*)eng)->strPool, str, len );
}

void tazE_borrowStr( tazE_Engine* eng, tazR_Str str, taz_StrLoan* loan ) {
    StrPool* pool = ((EngineFull*)eng)->strPool;
    tazR_Str type = str & STR_TYPE_MASK;
    tazR_Str id   = str & ~STR_TYPE_MASK & ~STR_SIZE_MASK;
    if( type == STR_SHORT )
        borrowShortStr( eng, pool, str, loan );
    else
    if( type == STR_MEDIUM )
        borrowMediumStr( eng, pool, id, loan );
    else
        borrowLongStr( eng, pool, id, loan );
}

void tazE_returnStr( tazE_Engine* eng, taz_StrLoan* loan ) {
    if( loan->link ) {
        tazR_unlinkWithNextAndLink( loan );
        return;
    }
    
    tazE_freeRaw( eng, (void*)loan->str, loan->len + 1 );
}

void tazE_stealStr( tazE_Engine* eng, taz_StrLoan* loan ) {
    if( !loan->link )
        return;
    
    tazE_RawAnchor cpyA;
    char* cpy = tazE_mallocRaw( eng, &cpyA, loan->len + 1 );
    memcpy( cpy, loan->str, loan->len + 1 );
    loan->str = cpy;
    
    tazR_unlinkWithNextAndLink( loan );
    loan->link = NULL;
    loan->next = NULL;
    
    tazE_commitRaw( eng, &cpyA );
}

unsigned tazE_strHash( tazE_Engine* eng, tazR_Str str ) {
    StrPool* pool = ((EngineFull*)eng)->strPool;
    tazR_Str type = str & STR_TYPE_MASK;
    tazR_Str id   = str & ~STR_TYPE_MASK & ~STR_SIZE_MASK;
    
    if( type == STR_SHORT )
        return str | str >> 32;
    StrNode* node = getStrNode( eng, pool, id );
    assert( node );
    
    return node->hash;
}

bool tazE_strEqual( tazE_Engine* eng, tazR_Str str1, tazR_Str str2 ) {
    StrPool* pool = ((EngineFull*)eng)->strPool;
    tazR_Str type1 = str1 & STR_TYPE_MASK;
    tazR_Str type2 = str2 & STR_TYPE_MASK;
    tazR_Str id1   = str1 & ~STR_TYPE_MASK & ~STR_SIZE_MASK;
    tazR_Str id2   = str2 & ~STR_TYPE_MASK & ~STR_SIZE_MASK;
    
    if( type1 != type2 )
        return false;
    if( str1 == str2 )
        return true;
    
    if( type1 == STR_SHORT || type1 == STR_MEDIUM )
        return str1 == str2;
    
    StrNodeLong* node1 = (StrNodeLong*)getStrNode( eng, pool, id1 );
    StrNodeLong* node2 = (StrNodeLong*)getStrNode( eng, pool, id2 );
    assert( node1 && node2 );
    
    if( node1->len != node2->len )
        return false;
    
    return !memcmp( node1->buf, node2->buf, node1->len );
}