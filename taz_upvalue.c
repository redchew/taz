#include "taz_upvalue.h"
#include "taz_engine.h"

tazR_Upv* tazR_makeUpv( tazE_Engine* eng, tazR_TVal val ) {
    tazE_ObjAnchor upvA;
    tazR_Upv* upv = tazE_mallocObj( eng, &upva, sizeof(tazR_Upv), tazR_Type_UPV );
    upv->val = val;
    tazE_commitObj( eng, &upvA );

    return upv;
}