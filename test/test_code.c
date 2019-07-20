#define taz_TESTING
#include "../taz_index.c"
#include "../taz_code.c"
#include "../taz_record.c"
#include "../taz_environment.c"
#include "../taz_engine.c"
#include "test.h"
#include <string.h>
#include <stdlib.h>

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

static void foo( taz_Interface* taz, taz_Call* call ) {
    // NADA
}

begin_test( create_host_code, SETUP_ENGINE_AND_BARRIER )
    struct {
        tazE_Bucket base;
        tazR_TVal   code;
    } buc;
    tazE_addBucket( eng, &buc, 1 );

    tazR_Code* code = tazR_makeHostCode( eng, foo, 123, "foo",
        (char const*[]){ "a", "b", NULL },
        (char const*[]){ "x", "y", NULL }
    );
    buc.code = tazR_codeVal( code );

    tazE_collect( eng, true );

    tazE_remBucket( eng, &buc );

    tazE_collect( eng, true );
end_test( create_host_code, TEARDOWN_ENGINE_AND_BARRIER )

begin_test( create_assembler, SETUP_ENGINE_AND_BARRIER )
    struct {
        tazE_Bucket base;
        tazR_TVal   name;
        tazR_TVal   as;
    } buc;
    tazE_addBucket( eng, &buc, 1 );

    tazR_Str name = tazE_makeStr( eng, "foo", 3 );
    buc.name = tazR_strVal( name );

    tazC_Assembler* as = tazR_makeAssembler( eng, name, taz_SCOPE_LOCAL );
    buc.as = tazR_stateVal( (tazR_State*)as );

    tazE_collect( eng, true );

    tazE_remBucket( eng, &buc );

    tazE_collect( eng, true );
end_test( create_assembler, TEARDOWN_ENGINE_AND_BARRIER )

begin_test( assemble_code, SETUP_ENGINE_AND_BARRIER )
    struct {
        tazE_Bucket base;
        tazR_TVal   name;
        tazR_TVal   code;
        tazR_TVal   tmp;
        tazR_TVal   as;
    } buc;
    tazE_addBucket( eng, &buc, 1 );

    tazR_Str name = tazE_makeStr( eng, "foo", 4 );
    buc.name = tazR_strVal( name );

    tazC_Assembler* as = tazR_makeAssembler( eng, name, taz_SCOPE_LOCAL );
    buc.as = tazR_stateVal( (tazR_State*)as );

    tazR_Ref k1 = as->addConst( as, tazR_intVal( 123 ) );
    tazR_Ref k2 = as->addConst( as, tazR_intVal( 321 ) );

    check( k1.bits.type == tazR_RefType_CONST );
    check( k2.bits.type == tazR_RefType_CONST );

    buc.tmp = tazR_strVal( tazE_makeStr( eng, "v1", 2 ) );
    tazR_Ref v1 = as->addLocal( as, tazR_getValStr( buc.tmp ) );
    buc.tmp = tazR_strVal( tazE_makeStr( eng, "v2", 2 ) );
    tazR_Ref v2 = as->addLocal( as, tazR_getValStr( buc.tmp ) );

    check( v1.bits.type == tazR_RefType_LOCAL );
    check( v2.bits.type == tazR_RefType_LOCAL );

    buc.tmp = tazR_strVal( tazE_makeStr( eng, "u1", 2 ) );
    tazR_Ref u1 = as->addUpval( as, tazR_getValStr( buc.tmp ) );
    buc.tmp = tazR_strVal( tazE_makeStr( eng, "u2", 2 ) );
    tazR_Ref u2 = as->addUpval( as, tazR_getValStr( buc.tmp ) );

    check( u1.bits.type == tazR_RefType_UPVAL );
    check( u2.bits.type == tazR_RefType_UPVAL );


    as->addInstr( as, tazR_OpCode_GET_CONST_A, 0, k1.bits.which );
    as->addInstr( as, tazR_OpCode_GET_CONST_B, 0, k2.bits.which );
    as->addInstr( as, tazR_OpCode_ADD, 0, 0 );

    tazR_Code* code = as->makeCode( as );
    buc.code = tazR_codeVal( code );

    check( code->type == tazR_CodeType_BYTE );

    tazR_ByteCode* bcode = (tazR_ByteCode*)code;
    ulongest w = bcode->wordBuf[0];
    check( (w & 0xFF) == (tazR_OpCode_GET_CONST_A | k1.bits.which << 5) );
    w >>= 8;
    check( (w & 0xFFFF) == (tazR_OpCode_GET_CONST_B | k2.bits.which << 7) );

    tazE_remBucket( eng, &buc );
end_test( assemble_code, TEARDOWN_ENGINE_AND_BARRIER )

begin_suite( code_tests )
    with_test( create_host_code )
    with_test( create_assembler )
    with_test( assemble_code )
end_suite( code_tests )

int main( void ) {
    if( run_suite( code_tests ) ) {
        printf( "PASSED\n" );
        return 0;
    }
    else {
        printf( "FAILED\n" );
        return 1;
    }
}