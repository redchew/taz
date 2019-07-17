#ifndef taz_code_h
#define taz_code_h
#include "taz_common.h"

typedef enum   tazR_CodeType tazR_CodeType;
typedef struct tazR_ByteCode tazR_ByteCode;
typedef struct tazR_HostCode tazR_HostCode;
typedef struct tazR_Label    tazR_Label;

enum tazR_CodeType {
    tazR_CodeType_HOST,
    tazR_CodeType_BYTE
};

struct tazR_Code {
    tazR_CodeType type;
    taz_Scope     scope;
    tazR_Str      name;

    unsigned numFixedParams;
    bool     hasVarParams;

    tazR_Idx* upvalIdx;
    unsigned  numUpvals;

    tazR_Idx* localIdx;
    unsigned  numLocals;
};

struct tazR_HostCode {
    tazR_Code base;

    size_t    stateSize;
    taz_FunCb callback;
};

struct tazR_Label {
    ulongest* addr;
    unsigned  shift;
};

struct tazR_ByteCode {
    tazR_Code base;

    tazR_TVal const* constBuf;
    unsigned numConsts;

    tazR_Label const* labelBuf;
    unsigned numLabels;

    ulongest const* wordBuf;
    unsigned numWords;
};


tazC_Assembler* tazR_makeAssembler( tazE_Engine* eng, tazR_Str name, taz_Scope scope );
tazR_Code* tazR_makeHostCode( tazE_Engine* eng, taz_FunCb cb, size_t sz, char const* name, char const** params, char const** upvals );

void tazR_dumpCode( tazE_Engine* eng, tazR_Code* code, taz_Writer* w );

#define tazR_scanCode _tazR_scanCode
void _tazR_scanCode( tazE_Engine* eng, tazR_Code* code, bool full );

#define tazR_sizeofCode( ENG, CODE ) (                                                  \
    ((tazR_Code*)(CODE))->type == tazR_CodeType_HOST                                    \
        ? sizeof(tazR_HostCode)                                                         \
        : sizeof(tazR_ByteCode) + ((tazR_ByteCode*)(CODE))->numWords*sizeof(ulongest)   \
)

#define tazR_finlCode _tazR_finlCode
void _tazR_finlCode( tazE_Engine* eng, tazR_Code* code );

#endif