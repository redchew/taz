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

struct BaseAR {
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
    tazR_TVal    errval;
    taz_ErrNum   errnum;
    taz_Trace*   trace;
    taz_FibState state;
};

static void doCall( tazE_Engine* eng );
static void doLoop( tazE_Engine* eng );
static void pushArgTup( tazE_Engine* eng, tazR_Fib* fib, taz_Tup* tup );
static void popRetTup( tazE_Engine* eng, tazR_Fib* fib, taz_Tup* tup );
static void formatArgs( tazE_Engine* eng, tazR_Fib* fib );
static void ensureVStack( tazE_Engine* eng, tazR_Fib* fib, unsigned n );
static void ensureCStack( tazE_Engine* eng, tazR_Fib* fib, size_t sz );

static size_t cSize( tazR_Code* code ) {
    if( code->type == tazR_CodeType_HOST )
        return sizeof(HostAR) + ((tazR_HostCode*)code)->cSize;
    else
        return sizeof(ByteAR);
}

static BaseAR* pushAR( tazE_Engine* eng, tazR_Fib* fib, tazR_Fun* fun, tazR_TVal* sb ) {
    size_t sz = cSize( fun->code );
    ensureCStack( eng, fib, sz + sizeof(BaseAR) );

    BaseAR* ar = fib->cstack.top;
    ar->fun  = fun;
    ar->sb   = sb;
    if( fun->code->type == tazR_CodeType_HOST ) {
        HostAR* har = (HostAR*)ar;
        har->cp  = -1;
        har->loc = (taz_LocInfo){ 0 };
    }
    else {
        ByteAR* bar = (ByteAR*)ar;
        bar->wp = ((tazR_ByteCode*)fun->code)->wordBuf;
        bar->ws = 0;
    }

    BaseAR* top = (fib->cstack.top += sz);
    top->prev = ar;
}

static void popAR( tazE_Engine* eng, tazR_Fib* fib ) {
    assert( fib->cstack.top && fib->cstack.top->prev );

    fib->cstack.top = fib->cstack.top->prev;
}

static unsigned vSize( tazR_Code* code ) {
    return 1 + code->numLocals + code->numFixedParams + !!code->hasVarParams;
}

tazR_Fib* tazR_makeFib( tazE_Engine* eng, tazR_Fun* fun ) {
    tazE_ObjAnchor fibA;
    tazR_Fib* fib = tazE_malloc( eng, &fibA, sizeof(tazR_Fib), tazR_Type_FIB );

    tazE_RawAnchor vbufA;
    unsigned   vcap = 32;
    tazR_TVal* vbuf = tazE_mallocRaw( eng, &vbufA, sizeof(tazR_TVal)*vcap );
    
    tazE_RawAnchor cbufA;
    unsigned ccap = 256;
    void*    cbuf = tazE_mallocRaw( eng, &cbufA, ccap );
    BaseAR*  top = cbuf;
    top->prev = NULL;

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

    *(fib->vstack.top++) = tazR_funVal( fun );
    pushAR( eng, fib, fun, fib->vstack.buf );

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
    return NULL;
}

taz_FibState tazR_getFibState( tazE_Engine* eng, tazR_Fib* fib ) {
    return fib->state;
}

void tazR_cont( tazE_Engine* eng, tazR_Fib* fib, taz_Tup* args, taz_Tup* rets, taz_LocInfo const* loc ) {
    if( fib->state != taz_FibState_STOPPED )
        tazE_error( eng, taz_ErrNum_FIB_NOT_STOPPED );
    
    tazE_Barrier bar = { 0 };
    if( setjmp( bar.errorDst ) ) {
        if( fib->parent )
            fib->parent->state = taz_FibState_CURRENT;
        eng->fiber = fib->parent;

        fib->state  = taz_FibState_FAILED;
        fib->parent = NULL;
        fib->errnum = bar.errnum;
        fib->errval = bar.errval;

        if( bar.errnum >= taz_ErrNum_FATAL )
            tazE_error( eng, bar.errnum );
        return;
    }

    if( setjmp( bar.yieldDst ) ) {
        if( fib->parent )
            fib->parent->state = taz_FibState_CURRENT;
        eng->fiber = fib->parent;

        if( fib->state != taz_FibState_FINISHED )
            fib->state = taz_FibState_STOPPED;
        
        fib->parent = NULL;
        popRetTup( eng, fib, rets );
        return;
    }
    tazE_pushBarrier( eng, &bar );

    fib->parent        = eng->fiber;
    fib->parent->state = taz_FibState_PAUSED;
    fib->state         = taz_FibState_CURRENT;
    engine->fiber      = fib;

    // Push continuation arguments onto the value stack.
    pushArgTup( eng, fib, args );
    formatArgs( eng, fib );

    // Enter the interpret loop.
    doLoop( eng );

    fib->state = taz_FibState_FINISHED;
    tazE_yield( eng );
}

void tazR_call( tazE_Engine* eng, tazR_Fun* fun, taz_Tup* args, taz_Tup* rets, taz_LocInfo const* loc ) {
    tazR_Fib* fib = eng->fiber;
    assert( fib && fib->cstack.top && fib->cstack.top->fun->code->type == tazR_CodeType_HOST );

    HostAR* har = (HostAR*)fib->cstack.top;
    har->loc = *loc;

    pushArgTup( eng, fib, args );
    formatArgs( eng, fib );
    doCall( eng );

    popRetTup( eng, fib, rets );
}

static void pushArgTup( tazE_Engine* eng, tazR_Fib* fib, taz_Tup* tup ) {
    ensureVStack( eng, fib, tup->size + 1 );
    for( unsigned i = 0 ; i < tup->size ; i++ )
        *(fib->vstack.top++) = tazR_varGet( tup->vars[i] );
    if( tup->size != 1 )
        *(fib->vstack.top++) = tazR_tupVal( tup->size );
}

static void popRetTup( tazE_Engine* eng, tazR_Fib* fib, taz_Tup* tup ) {
    unsigned   nvals = 1;
    tazR_TVal* vals  = fib->vstack.top - 1;
    if( tazR_getValType( vals[0] ) == tazR_Type_TUP ) {
        nvals = tazR_getValTup( vals[0] );
        vals -= nvals;
    }
    if( tup ) {
        if( tup->cap < nvals )
            tazE_error( eng, taz_ErrNum_TOO_MANY_RETURNS );
        if( tup->has == 0 ) {
            tup->has = nvals;
        }
        else {
            if( tup->has < nvals )
                tazE_error( eng, taz_ErrNum_TOO_MANY_RETURNS );
            else
            if( tup->has > nvals )
                razE_error( eng, taz_ErrNum_TOO_FEW_RETURNS );
        }
        for( unsigned i = 0 ; i < nvals ; i++ )
            tazR_varSet( tup->vars[i], vals[i] );
    }
    fib->vstack.top = vals;
}

static void formatArgs( tazE_Engine* eng, tazR_Fib* fib ) {
    unsigned   nargs = 1;
    tazR_TVal* args  = fib->top - 1;
    if( tazR_getValType( eng, args[0] ) ) {
        nargs = tazR_getValTup( args[0] );
        args -= nargs;
    }

    assert( fib->cstack.top );
    tazR_Code* code = fib->cstack.top->fun->code;
    if( nargs < code->numFixedParams )
        tazE_error( eng, taz_ErrNum_TOO_FEW_ARGS );
    
    for( unsigned i = 0 ; i < nargs ; i++ ) {
        if( tazR_getValType( args[i] ) == tazR_Type_UDF )
            tazE_error( eng, taz_ErrNum_UDF_AS_ARG );
    }


    if( nargs > code->numFixedParams ) {
        if( !code->varParamsIdx )
            tazE_error( eng, taz_ErrNum_TOO_MANY_ARGS );
        
        struct {
            tazE_Bucket base;
            tazR_TVal   vrec;
        } buc;
        tazE_addBucket( eng, &buc, 1 );

        tazR_Rec* vrec = tazR_makeRec( eng, code->varParamsIdx );
        buc.vrec = tazR_recVal( vrec );

        unsigned nparams = code->numFixedParams;
        for( unsigned i = nparams ; i < nargs ; i++ )
            tazR_recDef( eng, vrec, tazR_intVal( i - nparams ), args[i] );
        
        args[nparams] = buc.vrec;
        fib->vstack.top = args + nparams + 1;

        tazE_remBucket( eng, &buc );
    }
}

static void ensureVStack( tazE_Engine* eng, tazR_Fib* fib, unsigned n ) {
    tazR_TVal* obuf = fib->vstack.buf;
    tazR_TVal* otop = fib->vstack.top;
    unsigned   ocap = fib->vstack.cap;

    tazE_RawAnchor bufA = { .raw = obuf, .sz = sizeof(tazR_TVal)*ocap };
    unsigned   cap = ocap*2;
    tazR_TVal* buf = tazE_mallocRaw( eng, &bufA, sizeof(tazR_TVal)*cap );

    fib->vstack.cap = cap;
    fib->vstack.buf = buf;
    fib->vstack.top = buf + (otop - obuf);

    tazE_commitRaw( eng, &bufA );

    // We only need to update pointers if the buffer was moved.
    if( fib->vstack.buf == obuf )
        return;
    
    // Update stack pointers.
    BaseAR* arIt = fib->vstack.top;
    while( arIt ) {
        arIt->sb = buf + (arIt->sb - obuf);
        arIt = arIt->prev;
    }
}


static void ensureCStack( tazE_Engine* eng, tazR_Fib* fib, size_t sz ) {
    void*   obuf = fib->cstack.buf;
    BaseAR* otop = fib->cstack.top;
    size_t  ocap = fib->cstack.cap;

    tazE_RawAnchor bufA = { .raw = obuf, .sz = ocap };
    size_t     cap = ocap*2;
    tazR_TVal* buf = tazE_mallocRaw( eng, &bufA, cap );

    fib->cstack.cap = cap;
    fib->cstack.buf = buf;
    fib->cstack.top = buf + ((void*)otop - obuf);

    tazE_commitRaw( eng, &bufA );

    // Only need to update pointers if the buffer was moved.
    if( fib->cstack.buf == obuf )
        return;
    
    // Update linkage.
    BaseAR* arIt = fib->cstack.top;
    while( arIt ) {
        arIt->prev = buf + ((void*)arIt->prev - obuf);

        arIt = arIt->prev;
    }
}

static void doCall( tazE_Engine* eng ) {
    tazR_Fib* fib = eng->fiber;
    assert( fib && fib->cstack.top );

    tazR_Fun*  fun = NULL;
    tazR_TVal* sb  = NULL;
    if( tazR_getValType( fib->cstack.top[-1] ) == tazR_Type_TUP ) {
        sb = fib->cstack.top - tazR_getValTup( fib->cstack.top[-1] ) - 2;
    }
    else {
        sb = fib->cstack.top - 2;
    }
    assert( tazR_getValType( *sb ) == tazR_Type_FUN );
    fun = tazR_getValFun( *sb );

    ensureVStack( eng, fib, vSize( fun->code ) );
    BaseAR* ar = pushAR( eng, fib, fun, sb );
    
    if( fun->code->type == tazR_CodeType_HOST ) {

    }
    else {
        
    }
}