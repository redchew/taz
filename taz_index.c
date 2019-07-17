#include "taz_index.h"
#include "taz_engine.h"
#include <string.h>
#include <limits.h>
#include <assert.h>

/* Note: Index Structure
While indices are basically just hashmaps between keys and array slots, the
implementation may look a bit unfamiliar since it isn't a very common design.

The index is formatted as an open addressed hashmap with incremental linear
polling for best cache performance.  But we also use a few additional
optimizations to ensure best performance; indices have a pretty strong role
in the workings of the rest of the runtime, so good performance is important.

The first major optimization is the polling limit.  An index lookup will only
poll up to N slots away from a given key's ideal bucket, where N is the known
max distance between any key and its ideal index in the map.  In addition,
this N will always tend toward a specific L, where L is some ideal limit
computed as something like log2( MAP_SIZE ), as the index map array will grow
whenever N > L.

The second major optimization is in the use of a bitmap to optimize the key
lookup process.  Since bit operations can be done very efficiently, and in
parallel, they can make lookups go a lot faster.  This bitmap approach also
allows us to avoid a lot of memory fetches, which are fairly expensive, and
keeps what fetches we do make fairly localized to make best use of the cache
hierarchy.  The bitmap consists of an array of `long long` words (to optimize
memory fetching) with each byte of each word representing a bucket slot.
This byte will contain the bottom most 8 bits of the bucket's key, if occupied,
or 0 otherwise.  For any two equal values of atomic types these bottom-most
bits (which encode the value type and a few bits of the payload) will be
equal, so we can use this equality as a fast qualifier before checking the
full value.  For equal string values these bits will also be equal for short
and medium string, but not long strings which must be treated specially, but
should be fairly rare as record keys.
*/

typedef struct {
    tazR_TVal key;
    unsigned  loc;
} KeyLoc;

struct tazR_Idx {
    
    // The actual array of map buckets, normally we'd allocate the keys
    // and locs in separate buckets for best cache performance, but since
    // most of our lookup time will be spent in the bitmap instead, it'll
    // likely be more advantageous to have key and loc in cache together
    // than multiple keys, since when we actually access the bucket there's
    // a pretty good chance of a match.  We use a table of capacity values
    // and keep track of the current row here to allow for a more flexible
    // (and map friendly) sizing system.
    unsigned row;
    KeyLoc*  buf;
    
    // Next loc to allocate.
    unsigned loc;
    
    // Bitmap used for lookup optimization, we use the longest standard
    // integral type to allow for optimized memory fetches for systems
    // that support fetching larger words, but the bitmap is scanned on
    // a byte-per-byte basis.  Also, since the `bitmap` and `buf` will
    // always be resized together, the bitmap doens't get its own memory
    // allocation, it's just tacked onto the end of `buf`.
    ulongest* bitmap;
    
    // These keeps track of the current step limit and deviation.  The
    // deviation is tracked as the sum of the differences between the
    // real bucket offets of keys and their ideal offset, minus the ideal
    // step limit; for any keys whose bucket offset surpasses the ideal
    // step limit.  If the deviation surpasses the current level of
    // tolerance then the index will grow to accomodate.  The current
    // step limit will always be the largest distance between a key and
    // its ideal bucket.
    unsigned stepLimit;
    unsigned stepLimitDeviation;
    
    // We can do some optimization depending on which types of value are
    // contained in the index.  For example if there are not long strings
    // then we don't need to do the extra steps necessary for dealing
    // with them, and if there are no strings at all then no need to scan
    // the index.  We defer these checks to completely to definition
    // time to make lookups and other operations more efficient by putting
    // the appropriately optimized function pointers here.  While this
    // would add significant memory overhead in normal hashmap implementations,
    // indices are generally expected to have a one-to-many relationship with
    // their respective external units (this is true for functions and records,
    // which is the main place indices are used en mass), so the practical
    // overhead shouldn't be significant.
    long     (*lookup)( tazE_Engine* eng, tazR_Idx* idx, tazR_TVal key );
    unsigned (*insert)( tazE_Engine* eng, tazR_Idx* idx, tazR_TVal key );
    void     (*scan)( tazE_Engine* eng, tazR_Idx* idx );
};

#define IDX_ROW_CAP (28)

/* Note: Prime Numbers
These numbers mostly come from: https://planetmath.org/goodhashtableprimes;
with a few numbers added at beginning for a smaller initial index size.
*/
static unsigned const bufCapTable[IDX_ROW_CAP] = {
    17,         31,         53,         97,         193,
    389,        769,        1543,       3079,       6151,
    12289,      24593,      49157,      98317,      196613,
    393241,     786433,     1572869,    3145739,    6291469,
    12582917,   25165843,   50331653,   100663319,  201326611,
    402653189,  805306457,  1610612741
};

static unsigned bitCapTable[IDX_ROW_CAP] = { 0 };

static void bitCapTableInit( void ) {
    for( unsigned i = 0 ; i < IDX_ROW_CAP ; i++ )
        bitCapTable[i] = (bufCapTable[i] + sizeof(ulongest))/sizeof(ulongest);
}

static unsigned ulog2( unsigned u ) {
    // This computes the truncated log2( u )
    // as the target step count.  Using a log
    // of the cap ensures that the step size
    // will grow, but not unreasonably fast, so
    // we still have some lookup efficiency.
    uint log = 0;
    uint msk = UINT_MAX;
    while( msk & u ) {
        msk <<= 1;
        log++;
    }
    
    return log;
}

static unsigned stepLimitToleranceTable[IDX_ROW_CAP] = { 0 };

static void stepLimitToleranceTableInit( void ) {
    for( unsigned i = 0 ; i < IDX_ROW_CAP ; i++ ) {
        stepLimitToleranceTable[i] = ulog2( bufCapTable[i] )*taz_CONFIG_INDEX_STEP_LIMIT_TOLERANCE_KNOB;
    }
}

static unsigned idealStepLimitTable[IDX_ROW_CAP] = { 0 };

static void idealStepLimitTableInit( void ) {
    for( unsigned i = 0 ; i < IDX_ROW_CAP ; i++ ) {
        idealStepLimitTable[i] = ulog2( bufCapTable[i] )*taz_CONFIG_INDEX_IDEAL_STEP_LIMIT_KNOB;
    }
}

static void ensureTables( void ) {
    static volatile bool done = false;
    if( done )
        return;
    
    bitCapTableInit();
    stepLimitToleranceTableInit();
    idealStepLimitTableInit();
    done = true;
}

static long     lookupWhenNoStrings( tazE_Engine* eng, tazR_Idx* idx, tazR_TVal key );
static unsigned insertWhenNoStrings( tazE_Engine* eng, tazR_Idx* idx, tazR_TVal key );
static void     scanWhenNoStrings( tazE_Engine* eng, tazR_Idx* idx );


tazR_Idx* tazR_makeIdx( tazE_Engine* eng ) {
    ensureTables();
    
    tazE_ObjAnchor idxA;
    tazE_RawAnchor rawA;
    
    tazR_Idx* idx = tazE_mallocObj( eng, &idxA, sizeof(tazR_Idx), tazR_Type_IDX );
    
    size_t bufSize = bufCapTable[0]*sizeof(KeyLoc);
    size_t bitSize = bitCapTable[0]*sizeof(ulongest);
    
    void* raw = tazE_zallocRaw( eng, &rawA, bufSize + bitSize );
    
    KeyLoc*   buf    = raw;
    ulongest* bitmap = raw + bufSize;
    
    idx->row     = 0;
    idx->buf     = buf;
    idx->bitmap  = bitmap;
    
    idx->loc = 0;
    
    idx->stepLimit          = idealStepLimitTable[0];
    idx->stepLimitDeviation = 0;
    
    idx->lookup = lookupWhenNoStrings;
    idx->insert = insertWhenNoStrings;
    idx->scan   = scanWhenNoStrings;
    
    tazE_commitRaw( eng, &rawA );
    tazE_commitObj( eng, &idxA );
    
    return idx;
}

void _tazR_scanIdx( tazE_Engine* eng, tazR_Idx* idx, bool full ) {
    idx->scan( eng, idx );
}

size_t tazR_sizeofIdx( tazE_Engine* eng, tazR_Idx* idx ) {
    return sizeof(tazR_Idx);
}

static long lookupString( tazE_Engine* eng, tazR_Idx* idx, tazR_Str str ) {
    unsigned hash = tazE_strHash( eng, str );
    unsigned step = 0;
    
    unsigned i = hash % bufCapTable[idx->row];
    unsigned j = i / sizeof(ulongest);
    unsigned k = i % sizeof(ulongest);
    
    ulongest u = idx->bitmap[j] >> k*8;
    while( true ) {
        unsigned n = sizeof(ulongest);
        if( j == bitCapTable[idx->row] - 1 )
            n = bufCapTable[idx->row] % sizeof(ulongest);
        
        while( k < n ) {
            if( step++ > idx->stepLimit )
                return -1;
            
            if( (u & 0xF) == tazR_Type_STR ) {
                unsigned i = j*sizeof(ulongest) + k;
                
                KeyLoc* kp = &idx->buf[i];
                if( tazE_strEqual( eng, str, tazR_getValStr( kp->key ) ) )
                    return kp->loc;
            }
            
            u >>= 8; k++;
        }
        k = 0; j = (j + 1) % bitCapTable[idx->row];
        u = idx->bitmap[j];
    }
    
    return -1;
}

static long lookupNonString( tazE_Engine* eng, tazR_Idx* idx, tazR_TVal key ) {
    unsigned hash = tazR_getValHash( key );
    uchar    byte = tazR_getValByte( key );
    unsigned step = 0;
    
    unsigned i = hash % bufCapTable[idx->row];
    unsigned j = i / sizeof(ulongest);
    unsigned k = i % sizeof(ulongest);
    
    ulongest u = idx->bitmap[j] >> k*8;
    while( true ) {
        unsigned n = sizeof(ulongest);
        if( j == bitCapTable[idx->row] - 1 )
            n = bufCapTable[idx->row] % sizeof(ulongest);
        
        while( k < n ) {
            if( step++ > idx->stepLimit )
                return -1;
            if( (u & 0xFF) == byte ) {
                unsigned i = j*sizeof(ulongest) + k;
                
                KeyLoc* kp = &idx->buf[i];
                if( tazR_valEqual( key, kp->key ) )
                    return kp->loc;
            }
            u >>= 8; k++;
        }
        k = 0; j = (j + 1) % bitCapTable[idx->row];
        u = idx->bitmap[j];
    }
    
    return -1;
}

static unsigned insertInEmpty( tazE_Engine* eng, tazR_Idx* idx, tazR_TVal key ) {
    uchar    byte = tazR_getValByte( key );
    unsigned step = 0;
    
    
    unsigned hash;
    if( tazR_getValType( key ) == tazR_Type_STR )
        hash = tazE_strHash( eng, tazR_getValStr( key ) );
    else
        hash = tazR_getValHash( key );
    
    unsigned i = hash % bufCapTable[idx->row];
    unsigned j = i / sizeof(ulongest);
    unsigned k = i % sizeof(ulongest);

    ulongest u = idx->bitmap[j] >> k*8;
    
    // Assume there will always be a free slot in the index, so
    // this loop will terminate.  This assumption needs to be
    // enforced elsewhere.
    while( true ) {
        while( k < sizeof(ulongest) ) {
            if( (u & 0xFF) == 0 ) {
                unsigned i = j*sizeof(ulongest) + k;
                
                // The bitmap may have a few more bytes than the bucket array
                // has buckets since it's allocated in terms of multi-byte
                // words.  These extra bytes will always be zero, so when we
                // encounter them just skip to the next bitmap entry.
                if( i >= bufCapTable[idx->row] ) {
                    step++;
                    break;
                }
                
                KeyLoc* kp = &idx->buf[i];
                kp->key = key;
                kp->loc = idx->loc++;
                
                if( step > idx->stepLimit ) {
                    idx->stepLimit = step;
                
                    idx->stepLimitDeviation += step - idealStepLimitTable[idx->row];
                }
                
                idx->bitmap[j] = (idx->bitmap[j] & ~(0xFFLLU << k*8)) | (ulongest)byte << k*8;
                return kp->loc;
            }
            u >>= 8; k++;
            step++;
        }
        k = 0; j = (j + 1) % bitCapTable[idx->row];
        u = idx->bitmap[j];
    }
}

static unsigned insertWhenHasLongStrings( tazE_Engine* eng, tazR_Idx* idx, tazR_TVal key );
static unsigned insertWhenNoLongStrings( tazE_Engine* eng, tazR_Idx* idx, tazR_TVal key );
static long     lookupWhenHasLongStrings( tazE_Engine* eng, tazR_Idx* idx, tazR_TVal key );
static long     lookupWhenNoLongStrings( tazE_Engine* eng, tazR_Idx* idx, tazR_TVal key );
static void     scanWhenHasStrings( tazE_Engine* eng, tazR_Idx* idx );


static unsigned insertWhenNoStrings( tazE_Engine* eng, tazR_Idx* idx, tazR_TVal key ) {
    if( tazR_getValType( key ) == tazR_Type_STR ) {
        tazR_Str str = tazR_getValStr( key );
        if( tazE_strIsLong( eng, str ) ) {
            idx->insert = insertWhenHasLongStrings;
            idx->lookup = lookupWhenHasLongStrings;
        }
        else {
            idx->insert = insertWhenNoLongStrings;
            idx->lookup = lookupWhenNoLongStrings;
        }
        idx->scan = scanWhenHasStrings;
        
        // There are no strings in the index, so put
        // in an empty bucket.
        return insertInEmpty( eng, idx, key );
    }
    
    long loc = lookupNonString( eng, idx, key );
    if( loc >= 0 )
        return loc;
    
    return insertInEmpty( eng, idx, key );
}

static long lookupWhenNoStrings( tazE_Engine* eng, tazR_Idx* idx, tazR_TVal key ) {
    if( tazR_getValType( key ) == tazR_Type_STR )
        return -1;
    else
        return lookupNonString( eng, idx, key );
}

static void scanWhenNoStrings( tazE_Engine* eng, tazR_Idx* idx ) {
    // NADA
}

static unsigned insertWhenNoLongStrings( tazE_Engine* eng, tazR_Idx* idx, tazR_TVal key ) {
    long loc = 0;
    if( tazR_getValType( key ) == tazR_Type_STR ) {
        tazR_Str str = tazR_getValStr( key );
        if( tazE_strIsLong( eng, str ) ) {
            idx->insert = insertWhenHasLongStrings;
            idx->lookup = lookupWhenHasLongStrings;
            
            // There are no long strings in index, so put
            // in empty bucket.
            return insertInEmpty( eng, idx, key );
        }
        loc = lookupString( eng, idx, str );
    }
    else {
        loc = lookupNonString( eng, idx, key );
    }
    if( loc >= 0 )
        return loc;
    
    return insertInEmpty( eng, idx, key );
}

static long lookupWhenNoLongStrings( tazE_Engine* eng, tazR_Idx* idx, tazR_TVal key ) {
    if( tazR_getValType( key ) == tazR_Type_STR ) {
        tazR_Str str = tazR_getValStr( key );
        if( tazE_strIsLong( eng, str ) )
            return -1;
        return lookupString( eng, idx, str );
    }
    else {
        return lookupNonString( eng, idx, key );
    }
}


static unsigned insertWhenHasLongStrings( tazE_Engine* eng, tazR_Idx* idx, tazR_TVal key ) {
    long loc = 0;
    if( tazR_getValType( key ) == tazR_Type_STR )
        loc = lookupString( eng, idx, tazR_getValStr( key ) );
    else
        loc = lookupNonString( eng, idx, key );
    if( loc >= 0 )
        return loc;
    
    return insertInEmpty( eng, idx, key );
}

static long lookupWhenHasLongStrings( tazE_Engine* eng, tazR_Idx* idx, tazR_TVal key ) {
    if( tazR_getValType( key ) == tazR_Type_STR )
        return lookupString( eng, idx, tazR_getValStr( key ) );
    else
        return lookupNonString( eng, idx, key );
}

static void scanWhenHasStrings( tazE_Engine* eng, tazR_Idx* idx ) {
    unsigned const bitCap = bitCapTable[idx->row];
    for( unsigned i = 0 ; i < bitCap ; i++ ) {
        unsigned k = 0;
        unsigned u = idx->bitmap[i];
        while( u ) {
            if( (u & 0xF) == tazR_Type_STR ) {
                unsigned i = i*sizeof(ulongest) + k;
                
                tazR_Str str = tazR_getValStr( idx->buf[i].key );
                if( tazE_strIsGCed( eng, str ) )
                    tazE_markStr( eng, str );
            }
            k++;
            u >>= 8;
        }
    }
}

static void growIdx( tazE_Engine* eng, tazR_Idx* idx ) {
    assert( idx->row + 1 < IDX_ROW_CAP );
    unsigned orow    = idx->row;
    KeyLoc*  obuf    = idx->buf;
    
    
    idx->row++;
    tazE_RawAnchor rawA;
    size_t bufSize = bufCapTable[idx->row]*sizeof(KeyLoc);
    size_t bitSize = bitCapTable[idx->row]*sizeof(ulongest);
    
    void* raw = tazE_zallocRaw( eng, &rawA, bufSize + bitSize );
    
    idx->buf    = raw;
    idx->bitmap = raw + bufSize;
    
    idx->stepLimit          = idealStepLimitTable[idx->row];
    idx->stepLimitDeviation = 0;
    
    idx->lookup = lookupWhenNoStrings;
    idx->insert = insertWhenNoStrings;
    idx->scan   = scanWhenNoStrings;
    
    unsigned ocap = bufCapTable[orow];
    unsigned oloc = idx->loc;
    for( unsigned i = 0 ; i < ocap ; i++ ) {
        if( tazR_getValType( obuf[i].key ) == tazR_Type_NONE )
            continue;
        
        idx->loc = obuf[i].loc;
        idx->insert( eng, idx, obuf[i].key );
    }
    idx->loc = oloc;
    
    tazE_commitRaw( eng, &rawA );
    
    size_t oBufSize = bufCapTable[orow]*sizeof(KeyLoc);
    size_t oBitSize = bitCapTable[orow]*sizeof(ulongest);
    tazE_freeRaw( eng, obuf, oBufSize + oBitSize );
}


unsigned tazR_idxInsert( tazE_Engine* eng, tazR_Idx* idx, tazR_TVal key ) {
    assert( tazR_getValType( key ) < tazR_Type_FIRST_OBJECT );
    
    unsigned loc = idx->insert( eng, idx, key );
    if( idx->loc >= bufCapTable[idx->row] || idx->stepLimitDeviation >stepLimitToleranceTable[idx->row] )
        growIdx( eng, idx );
    
    return loc;
}

long tazR_idxLookup( tazE_Engine* eng, tazR_Idx* idx, tazR_TVal key ) {
    return idx->lookup( eng, idx, key );
}

unsigned tazR_idxNumKeys( tazE_Engine* eng, tazR_Idx* idx ) {
    return idx->loc;
}

tazR_Idx* tazR_subIdx( tazE_Engine* eng, tazR_Idx* idx, unsigned n, bool* select, long* locs ) {
    struct {
        tazE_Bucket base;
        tazR_TVal   sub;
        tazR_TVal   iter;
    } buc;
    tazE_addBucket( eng, &buc, 2 );
    
    tazR_Idx* sub = tazR_makeIdx( eng );
    buc.sub = tazR_idxVal( sub );
    
    tazR_IdxIter* iter = tazR_makeIdxIter( eng, idx );
    buc.iter = tazR_stateVal( (tazR_State*)iter );
    
    tazR_TVal key;
    unsigned  loc;
    while( tazR_idxIterNext( eng, iter, &key, &loc ) ) {
        if( loc >= n )
            continue;
        
        if( select[loc] )
            locs[loc] = tazR_idxInsert( eng, sub, key );
        else
            locs[loc] = -1;
    }
    tazE_remBucket( eng, &buc );
    
    return sub;
}

void _tazR_finlIdx( tazE_Engine* eng, tazR_Idx* idx ) {
    size_t bufSize = bufCapTable[idx->row]*sizeof(KeyLoc);
    size_t bitSize = bitCapTable[idx->row]*sizeof(ulongest);
    tazE_freeRaw( eng, idx->buf, bufSize + bitSize );
}

struct tazR_IdxIter {
    tazR_State base;
    tazR_Idx*  idx;
    unsigned   i;
};

static void idxIterScan( tazE_Engine* eng, tazR_State* self, bool full ) {
    tazR_IdxIter* iter = (tazR_IdxIter*)self;
    tazE_markObj( eng, iter );
}

static size_t idxIterSize( tazE_Engine* eng, tazR_State* self ) {
    return sizeof(tazR_IdxIter);
}

tazR_IdxIter* tazR_makeIdxIter( tazE_Engine* eng, tazR_Idx* idx ) {
    tazE_ObjAnchor iterA;
    tazR_IdxIter*  iter = tazE_mallocObj( eng, &iterA, sizeof(tazR_IdxIter), tazR_Type_STATE );
    iter->base.scan = idxIterScan;
    iter->base.size = idxIterSize;
    iter->base.finl = NULL;
    
    iter->idx = idx;
    iter->i   = 0;
    
    tazE_commitObj( eng, &iterA );
    
    return iter;
}

bool tazR_idxIterNext( tazE_Engine* eng, tazR_IdxIter* iter, tazR_TVal* key, unsigned* loc ) {
    if( iter->i >= bufCapTable[iter->idx->row] )
        return false;
    
    unsigned j = iter->i / sizeof(ulongest);
    unsigned k = iter->i % sizeof(ulongest);
    ulongest u = iter->idx->bitmap[j] >> k*8;
    
    while( true ) {
        while( u ) {
            if( (u & 0xFF) != 0 ) {
                unsigned i = j*sizeof(ulongest) + k;
                iter->i = i + 1;
                
                *key = iter->idx->buf[i].key;
                *loc = iter->idx->buf[i].loc;
                return true;
            }
            u >>= 8; k++;
        }
        k = 0; j++;
        if( j >= bitCapTable[iter->idx->row] ) {
            iter->i = j*sizeof(ulongest);
            return false;
        }
        u = iter->idx->bitmap[j];
    }
}