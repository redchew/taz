#ifndef test_h
#define test_h
#include <stdbool.h>
#include <stdio.h>
#include <string.h>


typedef struct {
    char const* desc;
    bool        result;
    bool        (*test)( void );
} test_Case;

#define begin_test( NAME, SETUP )                                              \
static bool test_ ## NAME( void );                                             \
static test_Case info_ ## NAME = {                                             \
    .result = false,                                                           \
    .test   = test_ ## NAME                                                    \
};                                                                             \
static bool test_ ## NAME( void ) { SETUP;

#define end_test( NAME, TEARDOWN )                                             \
    pass: { TEARDOWN }; return true;                                           \
    fail: { TEARDOWN }; return false;                                          \
}


static inline void pad( unsigned has, char with, unsigned column ) {
    while( has++ < column )
        putc( with, stdout );
}

#define begin_suite( NAME )                                                    \
static bool suite_ ## NAME( void ) {                                           \
    unsigned passed = 0;                                                       \
    unsigned failed = 0;                                                       \
    unsigned total  = 0;                                                       \
    printf( "\n%s\n", #NAME );                                                \
    printf( "============================================================\n" );

#define with_test( NAME )                                                      \
    {                                                                          \
        test_Case*  test = &info_ ## NAME;                                     \
        unsigned has = printf( "Running Test: %s", #NAME );                    \
        test->result = test->test();                                           \
                                                                               \
        total++;                                                               \
                                                                               \
        pad( has, ' ', 60 );                                                   \
        if( test->result ) {                                                   \
            printf( "PASSED\n" );                                              \
            passed++;                                                          \
        }                                                                      \
        else {                                                                 \
            printf( "FAILED\n" );                                              \
            failed++;                                                          \
        }                                                                      \
    }

#define end_suite( NAME )                                                      \
    printf( "---\n" );\
    printf( "Total: %u, Passed: %u, Failed: %u\n", total, passed, failed );    \
    if( failed == 0 )                                                          \
        printf( "Passed All Tests\n" );                                        \
    else                                                                       \
        printf( "Failed Some Tests\n" );                                       \
    printf( "============================================================\n" );\
    return failed == 0;                                                        \
}

#define run_suite( NAME ) suite_ ## NAME()

#define check( COND ) do {                                                     \
    if( !(COND) ) goto fail;                                                   \
} while( 0 )

#define pass() goto pass
#define fail() goto fail

#endif