#ifndef taz_fiber_h
#define taz_fiber_h
#include "taz_common.h"

tazR_Fib*    tazR_makeFib( tazE_Engine* eng, tazR_Fun* fun );
tazR_TVal    tazR_getFibErrVal( tazE_Engine* eng, tazR_Fib* fib );
taz_ErrNum   tazR_getFibErrNum( tazE_Engine* eng, tazR_Fib* fib );
taz_Trace*   tazR_getFibTrace( tazE_Engine* eng, tazR_Fib* fib );
taz_FibState tazR_getFibState( tazE_Engine* eng, tazR_Fib* fib );

void tazR_cont( tazE_Engine* eng, tazR_Fib* fib, taz_Tup* args, taz_Tup* rets, taz_LocInfo const* loc );
void tazR_call( tazE_Engine* eng, taz_Tup* args, taz_Tup* rets, taz_LocInfo const* loc );
void tazR_yield( tazE_Engine* eng, taz_Tup* vals, taz_LocInfo const* loc );
void tazR_panic( tazE_Engine* eng, tazR_TVal errval, taz_LocInfo const* loc );

taz_Var tazR_push( tazE_Engine* eng, tazR_Fib* fib );
void    tazR_pop( tazE_Engine* eng, tazR_Fib* fib, taz_Var* var );

#define tazR_scanFib   _tazR_scanFib
#define tazR_sizeofFib _tazR_sizeofFib
#define tazR_finlFib   _tazR_finlFib

#endif