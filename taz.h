#ifndef taz_h
#define taz_h
#include <stddef.h>
#include <stdbool.h>

typedef struct taz_Interface taz_Interface;
typedef struct taz_Config    taz_Config;
typedef struct taz_Var       taz_Var;
typedef struct taz_StrLoan   taz_StrLoan;
typedef struct taz_Trace     taz_Trace;
typedef struct taz_LocInfo   taz_LocInfo;
typedef struct taz_FunInfo   taz_FunInfo;
typedef enum   taz_FibState  taz_FibState;
typedef enum   taz_ErrNum    taz_ErrNum;
typedef enum   taz_Scope     taz_Scope;


typedef struct taz_Tup  taz_Tup;
typedef struct taz_Tup1 taz_Tup1;
typedef struct taz_Tup2 taz_Tup2;
typedef struct taz_Tup3 taz_Tup3;

typedef struct taz_Reader taz_Reader;
typedef struct taz_Writer taz_Writer;

typedef void*    (*taz_MemCb)( void* old, size_t osz, size_t nsz );
typedef taz_Tup* (*taz_FunCb)( taz_Interface* taz, taz_Tup* args );

struct taz_Config {
    taz_MemCb alloc;
};

struct taz_Var {
    void**   _base;
    unsigned _offset;
};

struct taz_Tup {
    unsigned size;
    taz_Var  vars[];
};

struct taz_Tup1 {
    unsigned size;
    taz_Var  vars[1];
};

struct taz_Tup2 {
    unsigned size;
    taz_Var  vars[2];
};
struct taz_Tup3 {
    unsigned size;
    taz_Var  vars[3];
};

struct taz_StrLoan {
    taz_StrLoan*  next;
    taz_StrLoan** link;
    
    char const* str;
    size_t      len;
};

struct taz_LocInfo {
    char const* file;
    char const* unit;
    unsigned    line;
    unsigned    column;
};

struct taz_Trace {
    taz_Trace*  next;
    taz_Trace** link;
    taz_LocInfo where;
};

enum taz_ErrNum {
    taz_ErrNum_NONE,
        taz_ErrNum_KEY_TYPE,
        taz_ErrNum_NUM_LOCALS,
        taz_ErrNum_NUM_UPVALS,
        taz_ErrNum_NUM_CONSTS,
        taz_ErrNum_PARAM_NAME,
        taz_ErrNum_UPVAL_NAME,
        taz_ErrNum_EXTRA_PARAMS,
        taz_ErrNum_SET_TO_UDF,
        taz_ErrNum_SET_UNDEFINED,
        taz_ErrNum_FORMAT_SPEC,
        taz_ErrNum_CYCLIC_RECORD,
        taz_ErrNum_FIB_NOT_STOPPED,
        taz_ErrNum_PANIC,
        taz_ErrNum_OTHER,
    taz_ErrNum_FATAL,
        taz_ErrNum_MEMORY,
    taz_ErrNum_LAST
};

enum taz_FibState {
    taz_FibState_CURRENT,
    taz_FibState_FAILED,
    taz_FibState_PAUSED,
    taz_FibState_STOPPED,
    taz_FibState_FINISHED
};

enum taz_Scope {
    taz_Scope_GLOBAL,
    taz_SCOPE_LOCAL
};

struct taz_Writer {
    bool (*write)( taz_Writer* self, char chr );
};

struct taz_Reader {
    char (*read)( taz_Reader* self );
};

struct taz_FunInfo {
    char const* name;
    char const* params;
    char const* upvals;
    size_t      cSize;
    size_t      fSize;
    taz_FunCb   callback;
};

#endif