#ifndef taz_compiler_h
#define taz_compiler_h

#include "taz_common.h"

typedef struct tazC_Compiler tazC_Compiler;
typedef struct tazC_Constant tazC_Constant;
typedef struct tazC_Variable tazC_Variable;
typedef struct tazC_Field    tazC_Field;
typedef struct tazC_Fragment tazC_Fragment;
typedef struct tazC_Pattern  tazC_Pattern;
typedef struct tazC_Location tazC_Location;
typedef enum   tazC_Flags    tazC_Flags;

struct tazC_Coord {
    char const* file;
    char const* func;
    uint        line;
    uint        column;
};

enum tazC_Profile {
    tazC_Flags_DEBUG,
    tazC_Flags_FAST,
    tazC_Flags_SMALL
};

tazC_Compiler* tazC_newCompiler( tazE_Engine* eng, char const* unit, tazC_Profile prof );
bool           tazC_testCompiler( void );

tazC_Fragment* tazC_makeClosure( tazC_Compiler* com, tazC_Location* loc, tazC_Pattern* pat, tazC_Fragment* context );
tazC_Fragment* tazC_makeRecord( tazC_Compiler* com, tazC_Location* loc );
tazC_Fragment* tazC_makeTuple( tazC_Compiler* com, tazC_Location* loc );
tazC_Fragment* tazC_makeDoFor( tazC_Compiler* com, tazC_Location* loc );
tazC_Fragment* tazC_makeIfElse( tazC_Compiler* com, tazC_Location* loc );
tazC_Fragment* tazC_makeWhenIn( tazC_Compiler* com, tazC_Location* loc );
tazC_Fragment* tazC_makeBinary( tazC_Compiler* com, int oper, tazC_Fragment* arg1, tazC_Fragment* arg2 );
tazC_Fragment* tazC_makeUnary( tazC_Compiler* com, int oper, tazC_Fragment* arg );
tazC_Fragment* tazC_makeVariableRef( tazC_Compiler* com, tazC_Variable* var );
tazC_Fragment* tazC_makeConstantRef( tazC_Compiler* com, tazC_Constant* con );
tazC_Fragment* tazC_makeFieldRef( tazC_Compiler* com, tazC_Field* field );

tazC_Pattern* tazC_makeFieldTupPattern( tazC_Compiler* com, tazC_Fragment* recordFrag );
tazC_Pattern* tazC_makeFieldRecPattern( tazC_Compiler* com, tazC_Fragment* recordFrag );
tazC_Pattern* tazC_makeVariableTupPattern( tazC_Compiler* com );
tazC_Pattern* tazC_makeVariableRecPattern( tazC_Compiler* com );

void tazC_addDstVar( tazC_Compiler* com, tazC_Pattern* pat, tazC_Variable* var );
void tazC_addDstKey( tazC_Compiler* com, tazC_Pattern* pat, tazC_Fragment* key );
void tazC_addDstVarKeyPair( tazC_Compiler* com, tazC_Pattern* pat, tazS_Variable* var, tazC_Fragment* key );
void tazC_addDstKeyKeyPair( tazC_Compiler* com, tazC_Pattern* pat, tazC_Fragment* dst, tazC_Fragment* src );
void tazC_setVariadicPattern( tazC_Compiler* com, tazC_Pattern* pat );

tazC_Variable* tazC_getVariable( tazC_Compiler* com, tazC_Fragment* frag, tazR_Str name );
tazC_Constant* tazC_getConstant( tazC_Compiler* com, tazC_Fragment* frag, tazR_TVal value );
tazC_Field*    tazC_getField( tazC_Compiler* com, tazC_Fragment* frag, tazC_Variable* rec, tazR_Value key );

void tazC_putExpr( tazC_Compiler* com, tazC_Fragment* dst, tazC_Fragment* frag );
void tazC_putPair( tazC_Compiler* com, tazC_Fragment* dst, tazC_Fragment* frag1, tazC_Fragment* frag2 );
void tazC_putDef( tazC_Compiler* com, tazC_Pattern* pat, tazC_Fragment* valueFrag );
void tazC_putSet( tazC_Compiler* com, tazC_Pattern* pat, tazC_Fragment* valueFrag );

void tazC_compile( tazC_Compiler* com, tazC_Assembler* assem );

#endif