#include "taz_function.h"
#include "taz_engine.h"
#include "taz_code.h"

tazR_Fun* tazR_makeFun( tazE_Engine* eng, tazR_Code* code ) {
    size_t stateSize = 0;
    if( code->type == tazR_CodeType_HOST )
        stateSize = ((tazR_HostCode*)code)->fSize;
    
    tazE_RawAnchor upvsA;
    tazR_Upv** upvs = tazE_zallocRaw( eng, &upvsA, sizeof(tazR_Upv*)*code->numUpvals );

    tazE_ObjAnchor funA;
    tazR_Fun* fun = tazE_mallocObj( eng, &funA, sizeof(tazR_Fun) + stateSize, tazR_Type_FUN );
    fun->code = code;
    fun->upvs = upvs;
    
    tazE_commitRaw( eng, &upvsA );
    tazE_commitObj( eng, &funA );
    return fun;
}

void tazR_scanFun( tazE_Engine* eng, tazR_Fun* fun, bool full ) {
    for( unsigned i = 0 ; i < fun->code->numUpvals ; i++ ) {
        if( fun->upvs[i] )
            tazE_markVal( eng, fun->upvs[i]->val );
    }
    tazE_markObj( eng, fun->code );
}

size_t tazR_sizeofFun( tazE_Engine* eng, tazR_Fun* fun ) {
    size_t stateSize = 0;
    if( fun->code->type == tazR_CodeType_HOST )
        stateSize = ((tazR_HostCode*)fun->code)->stateSize;
    return sizeof(tazR_Fun) + stateSize;
}

void tazR_finlFun( tazE_Engine* eng, tazR_Fun* fun ) {
    tazE_freeRaw( eng, fun->upvs, sizeof(tazR_Upv*)*fun->code->numUpvals );
}