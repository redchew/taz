#ifndef taz_upvalue_h
#define taz_upvalue_h
#include "taz_common.h"

struct tazR_Upv {
    tazR_TVal val;
};

tazR_Upv* tazR_makeUpv( tazE_Engine* eng, tazR_TVal val );

#define tazR_scanUpv( ENG, UPV, FULL ) tazE_markVal( (ENG), ((tazR_Upv*)(UPV))->val )
#define tazR_sizeofUpv( ENG, UPV )     sizeof(tazR_Upv)

#endif