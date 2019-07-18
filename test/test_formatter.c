#define taz_TESTING
#include "../taz_index.c"
#include "../taz_record.c"
#include "../taz_formatter.c"
#include "../taz_engine.c"
#include "test.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static void* alloc( void* old, size_t osz, size_t nsz ) {
    if( nsz > 0 )
        return realloc( old, nsz );
    free( old );
    return NULL;
}

#define SETUP_ENGINE                                                        \
    taz_Config   cfg = { .alloc = alloc };                                  \
    tazE_Engine* eng = tazE_makeEngine( &cfg );
#define TEARDOWN_ENGINE                                                     \
    tazE_freeEngine( eng );

#define SETUP_ENGINE_AND_BARRIER                                            \
    taz_Config   cfg = { .alloc = alloc };                                  \
    tazE_Engine* eng = tazE_makeEngine( &cfg );                             \
    tazE_Barrier bar = { 0 };                                               \
    if( setjmp( bar.errorDst ) )                                            \
        fail();                                                             \
    if( setjmp( bar.yieldDst ) )                                            \
        fail();                                                             \
    tazE_pushBarrier( eng, &bar );
    
#define TEARDOWN_ENGINE_AND_BARRIER                                         \
    tazE_popBarrier( eng, &bar );                                           \
    tazE_freeEngine( eng );


typedef struct {
    taz_Writer base;
    char       buf[512];
    unsigned   loc;
} StringWriter;

static bool swWrite( taz_Writer* w, char c ) {
    StringWriter* sw = (StringWriter*)w;
    if( sw->loc < sizeof(sw->buf) - 1 ) {
        sw->buf[sw->loc++] = c;
        return true;
    }
    return false;
}

begin_test( format_atomic, SETUP_ENGINE_AND_BARRIER )
    StringWriter sw1 = { .base = { .write = swWrite } };
    tazR_fmt( eng, (taz_Writer*)&sw1, "{}", tazR_intVal( 123 ) );
    check( !strcmp( sw1.buf, "123" ) );

    StringWriter sw2 = { .base = { .write = swWrite } };
    tazR_fmt( eng, (taz_Writer*)&sw2, "{}", tazR_decVal( 123.0 ) );
    check( !strcmp( sw2.buf, "123.0" ) );

    StringWriter sw3 = { .base = { .write = swWrite } };
    tazR_fmt( eng, (taz_Writer*)&sw3, "{} {} {}", tazR_nil, tazR_udf, tazR_decVal( INFINITY ) );
    check( !strcmp( sw3.buf, "nil udf inf" ) );

    StringWriter sw4 = { .base = { .write = swWrite } };
    tazR_fmt( eng, (taz_Writer*)&sw4, "{} {}", tazR_logVal( true ), tazR_logVal( false ) );
    check( !strcmp( sw4.buf, "true false" ) );
end_test( format_atomic, TEARDOWN_ENGINE_AND_BARRIER )

begin_test( alt_base_numbers, SETUP_ENGINE_AND_BARRIER )
    StringWriter sw1 = { .base = { .write = swWrite } };
    tazR_fmt( eng, (taz_Writer*)&sw1, "{H}", tazR_intVal( 0xABC ) );
    check( !strcmp( sw1.buf, "ABC" ) );

    StringWriter sw2 = { .base = { .write = swWrite } };
    tazR_fmt( eng, (taz_Writer*)&sw2, "{H}", tazR_decVal( 2748.0 ) );
    check( !strcmp( sw2.buf, "ABC.0" ) );

    StringWriter sw3 = { .base = { .write = swWrite } };
    tazR_fmt( eng, (taz_Writer*)&sw3, "{B}", tazR_intVal( 0xF ) );
    check( !strcmp( sw3.buf, "1111" ) );

    StringWriter sw4 = { .base = { .write = swWrite } };
    tazR_fmt( eng, (taz_Writer*)&sw4, "{H}", tazR_decVal( 15.9375 ) );
    check( !strcmp( sw4.buf, "F.F" ) );
end_test( alt_base_numbers, TEARDOWN_ENGINE_AND_BARRIER )

begin_test( format_strings, SETUP_ENGINE_AND_BARRIER )
    struct {
        tazE_Bucket base;
        tazR_TVal   str1;
        tazR_TVal   str2;
    } buc;
    tazE_addBucket( eng, &buc, 2 );

    buc.str1 = tazR_strVal( tazE_makeStr( eng, "Hello", 5 ) );
    buc.str2 = tazR_strVal( tazE_makeStr( eng, "thing1\nthing2", 13 ) );

    StringWriter sw1 = { .base = { .write = swWrite } };
    tazR_fmt( eng, (taz_Writer*)&sw1, "{}, World!", buc.str1 );
    check( !strcmp( sw1.buf, "Hello, World!" ) );

    StringWriter sw2 = { .base = { .write = swWrite } };
    tazR_fmt( eng, (taz_Writer*)&sw2, "{Q}", buc.str2 );
    check( !strcmp( sw2.buf, "'(thing1\nthing2)'" ) );

end_test( format_strings, TEARDOWN_ENGINE_AND_BARRIER )

begin_test( format_records, SETUP_ENGINE_AND_BARRIER )
    struct {
        tazE_Bucket base;
        tazR_TVal   idx;
        tazR_TVal   rec;
        tazR_TVal   key;
    } buc;
    tazE_addBucket( eng, &buc, 3 );

    tazR_Idx* idx = tazR_makeIdx( eng );
    buc.idx = tazR_idxVal( idx );
    tazR_Rec* rec = tazR_makeRec( eng, idx );
    buc.rec = tazR_recVal( rec );

    StringWriter sw1 = { .base = { .write = swWrite } };
    tazR_fmt( eng, (taz_Writer*)&sw1, "{}", buc.rec );
    check( !strcmp( sw1.buf, "{}" ) );

    tazR_recDef( eng, rec, tazR_intVal( 0 ), tazR_intVal( 0 ) );
    tazR_recDef( eng, rec, tazR_intVal( 1 ), tazR_intVal( 1 ) );

    StringWriter sw2 = { .base = { .write = swWrite } };
    tazR_fmt( eng, (taz_Writer*)&sw2, "{}", buc.rec );
    check( !strcmp( sw2.buf, "{ 0, 1 }" ) );

    buc.key = tazR_strVal( tazE_makeStr( eng, "key", 3 ) );
    tazR_recDef( eng, rec, buc.key, tazR_intVal( 2 ) );

    StringWriter sw3 = { .base = { .write = swWrite } };
    tazR_fmt( eng, (taz_Writer*)&sw3, "{}", buc.rec );
    check( !strcmp( sw3.buf, "{ 0, 1, .key: 2 }" ) );

end_test( format_records, TEARDOWN_ENGINE_AND_BARRIER )

begin_suite( record_tests )
    with_test( format_atomic )
    with_test( alt_base_numbers )
    with_test( format_strings )
    with_test( format_records )
end_suite( record_tests )

int main( void ) {
    if( run_suite( record_tests ) ) {
        printf( "PASSED\n" );
        return 0;
    }
    else {
        printf( "FAILED\n" );
        return 1;
    }
}