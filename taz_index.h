#ifndef taz_index_h
#define taz_index_h
#include "taz_common.h"

/* Note: Indices
Here we implement the `taz_Idx` type, which serves as a general purpose hashmap
between key-loc pairs, where a `loc` is an index into some other data buffer.
We implement the index independently from particular hashmap implementations
both to allow for broader reuse of the functionallity, and to support Taz's
unique take on records, which allow particular index instances to be shared
between multiple record instances for better memory efficiency.

Keep in mind that indices only allow atomic and string types for keys.  The
decision behind this is based on the fact that there's really not much use
in storing other types in a hashmap, and for the few legitimate cases where
allowing such would be useful, a better solution would usually be to use the
ID of a particular object instance as a key.  The benefit of disallowing
arbitrary key types is that we only have to scan indices for full GC cycles
(to scan for strings) since an index will never hold object references.  Other
benefits also manifest in the implementation and optimization of the index
as a data structure.
*/

typedef struct tazR_IdxIter tazR_IdxIter;

tazR_Idx* tazR_makeIdx( tazE_Engine* eng );
unsigned  tazR_idxInsert( tazE_Engine* eng, tazR_Idx* idx, tazR_TVal key );
long      tazR_idxLookup( tazE_Engine* eng, tazR_Idx* idx, tazR_TVal key );
unsigned  tazR_idxNumKeys( tazE_Engine* eng, tazR_Idx* idx );
tazR_TVal tazR_idxGetKey( tazE_Engine* eng, tazR_Idx* idx, unsigned loc );


tazR_Idx* tazR_subIdx( tazE_Engine* eng, tazR_Idx* idx, unsigned n, bool* select, long* locs );

tazR_IdxIter* tazR_makeIdxIter( tazE_Engine* eng, tazR_Idx* idx );
bool          tazR_idxIterNext( tazE_Engine* eng, tazR_IdxIter* iter, tazR_TVal* key, unsigned* loc );

#define tazR_scanIdx( ENG, IDX, FULL ) do {                                    \
    if( (FULL) )                                                               \
        _tazR_scanIdx( (ENG), (IDX), (FULL) );                                 \
} while( 0 )
void _tazR_scanIdx( tazE_Engine* eng, tazR_Idx* idx, bool full );

#define tazR_sizeofIdx _tazR_sizeofIdx
size_t _tazR_sizeofIdx( tazE_Engine* eng, tazR_Idx* idx );

#define tazR_finlIdx _tazR_finlIdx
void _tazR_finlIdx( tazE_Engine* eng, tazR_Idx* idx );

#endif