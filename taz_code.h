#ifndef taz_code_h
#define taz_code_h

typedef enum   tazR_CodeType   tazR_CodeType;
typedef struct tazR_ByteCode   tazR_ByteCode;
typedef struct tazR_NativeCode tazR_NativeCode;
typedef struct tazR_Label      tazR_Label;

enum tazR_CodeType {
    tazR_CodeType_NATIVE,
    tazR_CodeType_BYTE
};

struct tazR_Code {
    tazR_CodeType type;
    tazR_Str      name;

    unsigned numFixedParams;
    bool     hasVarParam;

    tazR_Idx* upvalIdx;
    unsigned  numUpvals;

    tazR_Idx* localIdx;
    unsigned  numLocals;
};

struct tazR_NativeCode {
    tazR_Code base;

    size_t    stateSize;
    taz_FunCb callback;
};

struct tazR_Label {
    ulongest* addr;
    unsigned  byte;
};

struct tazR_ByteCode {
    taz_Code base;

    tazR_TVal* constBuf;
    unsigned   numConsts;

    tazR_Label* labelBuf;
    unsigned    numLabels;

    size_t   len;
    ulongest code[];
};


tazC_Assembler*  tazR_makeAssembler( tazE_Engine* eng );
tazR_ByteCode*   tazR_makeByteCode( tazE_Engine* eng, tazC_Assembler* );
tazR_NativeCode* tazR_makeNativeCode( tazE_Engine* eng, taz_FunCb cb, char const* name, char const** params, char const** upvals );

void tazR_dumpCode( tazE_Engine* eng, tazR_Code* code, taz_Writer* w );

#define tazR_scanCode _tazR_scanCode
void _tazR_scanCode( tazE_Engine* eng, tazR_Code* code, bool full );

#define tazR_sizeofCode( ENG, CODE ) (                              \
    ((tazR_Code*)(CODE))->type == tazR_CodeType_NATIVE              \
        ? sizeof(tazR_NativeCode)                                   \
        : sizeof(tazR_ByteCode) + ((tazR_Bytecode*)(CODE))->len     \
)

#define tazR_finlCode _tazR_finlCode
void _tazR_finlCode( tazE_Engine* eng, tazR_Code* code );

#endif