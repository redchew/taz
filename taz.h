#ifndef taz_h
#define taz_h
#include <stddef.h>
#include <stdbool.h>

typedef struct taz_Instance taz_Instance;
typedef struct taz_Config   taz_Config;
typedef struct taz_Var      taz_Var;
typedef struct taz_Call     taz_Call;
typedef struct taz_StrLoan  taz_StrLoan;
typedef enum   taz_ErrNum   taz_ErrNum;
typedef enum   taz_Scope    taz_Scope;

typedef struct taz_Reader taz_Reader;
typedef struct taz_Writer taz_Writer;

typedef void* (*taz_MemCb)( void* old, size_t osz, size_t nsz );
typedef void  (*taz_FunCb)( taz_Instance* taz, taz_Call* call );

struct taz_Config {
    taz_MemCb alloc;
};

struct taz_Var {
    void**   _base;
    unsigned _offset;
};

struct taz_StrLoan {
    taz_StrLoan*  next;
    taz_StrLoan** link;
    
    char const* str;
    size_t      len;
};

enum taz_ErrNum {
    taz_ErrNum_NONE,
    taz_ErrNum_FATAL,
    taz_ErrNum_PANIC,
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

#endif