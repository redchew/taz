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
typedef enum   taz_ErrNum    taz_ErrNum;
typedef enum   taz_Scope     taz_Scope;


typedef struct taz_Tup  taz_Tup;
typedef struct taz_Tup1 taz_Tup1;
typedef struct taz_Tup2 taz_Tup2;
typedef struct taz_Tup3 taz_Tup3;

typedef struct taz_Reader taz_Reader;
typedef struct taz_Writer taz_Writer;

typedef void*   (*taz_MemCb)( void* old, size_t osz, size_t nsz );
typedef taz_Tup (*taz_FunCb)( taz_Interface* taz, taz_Tup* args );

struct taz_Config {
    taz_MemCb alloc;
};

struct taz_Var {
    void**   _base;
    unsigned _offset;
};

struct taz_Tup {
    unsigned size;
};

struct taz_Tup1 {
    taz_Tup  base;
    taz_Var  _0;
};

struct taz_Tup2 {
    taz_Tup  base;
    taz_Var  _0;
    taz_Var  _1;
};

struct taz_Tup3 {
    taz_Tup  base;
    taz_Var  _0;
    taz_Var  _1;
    taz_Var  _2;
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
    taz_ErrNum_FATAL,
    taz_ErrNum_PANIC,
    taz_ErrNum_COMPILE,
    taz_ErrNum_FORMAT,
    taz_ErrNum_OTHER
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