#ifndef taz_parser_h
#define taz_parser_h
#include "taz_common.h"
#include "taz_compiler.h"

typedef struct tazC_Parser tazC_Parser;

tazC_Parser* tazC_newParser( tazE_Engine* eng, char const* name, tazU_Reader* src );
bool         tazC_testParser( void ); 
void         tazC_parse( tazC_Parser* par, tazC_Compiler* com );

#endif