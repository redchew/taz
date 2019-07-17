#ifndef taz_record_h
#define taz_record_h
#include "taz_common.h"

typedef struct tazR_RecIter tazR_RecIter;

tazR_Rec* tazR_makeRec( tazE_Engine* eng, tazR_Idx* idx );
void      tazR_recDef( tazE_Engine* eng, tazR_Rec* rec, tazR_TVal key, tazR_TVal val );
void      tazR_recSet( tazE_Engine* eng, tazR_Rec* rec, tazR_TVal key, tazR_TVal val );
tazR_TVal tazR_recGet( tazE_Engine* eng, tazR_Rec* rec, tazR_TVal key );
void      tazR_recSep( tazE_Engine* eng, tazR_Rec* rec );

tazR_RecIter* tazR_makeRecIter( tazE_Engine* eng, tazR_Rec* rec );
bool          tazR_recIterNext( tazE_Engine* eng, tazR_RecIter* iter, tazR_TVal* key, tazR_TVal* val );

#define tazR_scanRec _tazR_scanRec
void _tazR_scanRec( tazE_Engine* eng, tazR_Rec* rec, bool full );

#define tazR_sizeofRec _tazR_sizeofRec
size_t _tazR_sizeofRec( tazE_Engine* eng, tazR_Rec* rec );

#define tazR_finlRec _tazR_finlRec
void _tazR_finlRec( tazE_Engine* eng, tazR_Rec* rec );

#endif