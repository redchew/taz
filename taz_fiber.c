#include "taz_fiber.h"
#include "taz_engine.h"
#include "taz_function.h"
#include "taz_code.h"

#include <assert.h>

typedef struct BaseAR BaseAR;
typedef struct ByteAR ByteAR;
typedef struct HostAR HostAR;
typedef enum   ARType ARType;
typedef union  AR     AR;
typedef struct Regs   Regs;

enum ARType {
    BYTE_AR,
    HOST_AR
};

struct BaseAR {
    ARType     type;
    BaseAR*    prev;
    tazR_Fun*  fun;
    tazR_TVal* sb;
};

struct ByteAR {
    BaseAR     base;
    ulongest*  wp;
    unsigned   ws;
};

struct HostAR {
    BaseAR      base;
    taz_LocInfo loc;
    long        cp;
    char        state[];
};

struct tazR_Fib {
    struct {
        tazR_TVal* buf;
        tazR_TVal* top;
        unsigned   cap;
    } vstack;

    struct {
        void*   buf;
        BaseAR* top;
        size_t  cap;
    } cstack;

    tazR_Fib*    parent;
    tazR_Fun*    entry;
    tazR_TVal    errval;
    taz_ErrNum   errnum;
    taz_Trace*   trace;
    taz_FibState state;
};


tazR_Fib* tazR_makeFib( tazE_Engine* eng, tazR_Fun* fun ) {
    tazE_ObjAnchor fibA;
    tazR_Fib* fib = tazE_malloc( eng, &fibA, sizeof(tazR_Fib), tazR_Type_FIB );

    tazE_RawAnchor vbufA;
    unsigned   vcap = 32;
    tazR_TVal* vbuf = tazE_mallocRaw( eng, &vbufA, sizeof(tazR_TVal)*vcap );

    tazE_RawAnchor cbufA;
    unsigned ccap = 256;
    void*    cbuf = tazE_mallocRaw( eng, &cbufA, ccap );

    fib->vstack.buf = vbuf;
    fib->vstack.top = vbuf;
    fib->vstack.cap = vcap;
    fib->cstack.buf = cbuf;
    fib->cstack.cap = ccap;
    fib->cstack.top = 0;
    fib->parent = NULL;
    fib->entry  = fun;
    fib->errval = tazR_udf;
    fib->errnum = taz_ErrNum_NONE;
    fib->trace  = NULL;
    fib->state  = taz_FibState_STOPPED;

    tazE_commitRaw( eng, &vbufA );
    tazE_commitRaw( eng, &cbufA );
    tazE_commitRaw( eng, &fibA );

    return fib;
}

tazR_TVal  tazR_getFibErrVal( tazE_Engine* eng, tazR_Fib* fib ) {
    return fib->errval;
}

taz_ErrNum tazR_getFibErrNum( tazE_Engine* eng, tazR_Fib* fib ) {
    return fib->errnum;
}

taz_Trace* tazR_getFibTrace( tazE_Engine* eng, tazR_Fib* fib ) {
    if( fib->trace )
        return fib->trace;
    
    assert( false ); // TODO: generate trace.
}

taz_FibState tazR_getFibState( tazE_Engine* eng, tazR_Fib* fib ) {
    return fib->state;
}

static void doLoop( tazE_Engine* eng );
static void doCall( tazE_Engine* eng );

void tazR_cont( tazE_Engine* eng, tazR_Fib* fib, taz_Tup* args, taz_Tup* rets, taz_LocInfo const* loc ) {
    if( fib->state != taz_FibState_STOPPED )
        tazE_error( eng, taz_ErrNum_FIB_NOT_STOPPED );
}