#ifndef taz_function_h
#define taz_function_h
#include "taz_common.h"

struct tazR_Fun {
    tazR_Code* code;
    tazR_Upv** upvs;
    char       state[];
};

tazR_Fun* tazR_makeFun( tazE_Engine* eng, tazR_Code* code );

#define tazR_scanFun    _tazR_scanFun
#define tazR_sizeofFun  _tazR_sizeofFun
#define tazR_finlFun    _tazR_finlFun

void   tazR_scanFun( tazE_Engine* eng, tazR_Fun* fun, bool full );
size_t tazR_sizeofFun( tazE_Engine* eng, tazR_Fun* fun );
void   tazR_finlFun( tazE_Engine* eng, tazR_Fun* fun );

#endif