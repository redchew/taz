#ifndef taz_h
#define taz_h
#include <stddef.h>
#include <stdbool.h>

typedef struct taz_Instance taz_Instance;
typedef struct taz_Config   taz_Config;
typedef struct taz_Var      taz_Var;
typedef struct taz_StrBuf   taz_StrBuf;
typedef enum   taz_ErrNum   taz_ErrNum;
typedef enum   taz_Scope    taz_Scope;

typedef void* (*taz_Alloc)( void* old, size_t osz, size_t nsz );

struct taz_Config {
    taz_Alloc alloc;
};

struct taz_Var {
    void**   _base;
    unsigned _offset;
};

struct taz_StrBuf {
    void* _node;
    
    char const* str;
    size_t      len;
};

enum taz_ErrNum {
    taz_ErrNum_NONE,
    taz_ErrNum_FATAL,
    taz_ErrNum_PANIC
};

#endif