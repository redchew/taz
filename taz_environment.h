#ifndef taz_environment_h
#define taz_environment_h
#include "taz_common.h"

/* Note: The Environment
The Taz environment is responsible for maintaining a pool of global
variables, module import strategies, and operator overloads.
*/

void tazR_initEnv( tazE_Engine* eng );

unsigned   tazR_getGlobalLoc( tazE_Engine* eng, tazR_Str name );
taz_Var    tazR_getGlobalVar( tazE_Engine* eng, tazR_Str name );
tazR_TVal* tazR_getGlobalVal( tazE_Engine* eng, tazR_Str name );
taz_Var    tazR_getGlobalVarByLoc( tazE_Engine* eng, unsigned loc );
tazR_TVal* tazR_getGlobalValByLoc( tazE_Engine* eng, unsigned loc );

void tazR_setImportStrategy( tazE_Engine* eng, tazR_Str name, tazR_Fun* load, tazR_Fun* trans );
void tazR_getImportStrategy( tazE_Engine* eng, tazR_Str name, tazR_Fun** load, tazR_Fun** trans );

void      tazR_setOperatorFunction( tazE_Engine* eng, tazR_Str opr, tazR_Fun* fun );
tazR_Fun* tazR_getOperatorFunction( tazE_Engine* eng, tazR_Str opr );

#endif