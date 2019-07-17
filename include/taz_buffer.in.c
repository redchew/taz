#ifndef BUF_TYPE
    #error "Define BUF_TYPE before including taz_buffer.in.c"
#endif

#ifndef BUF_NAME
    #error "Define BUF_NAME before including taz_buffer.in.c"
#endif

#define NCAT( A, B )               A ## _ ## B
#define NAME( BUF_NAME, FUN_NAME ) NCAT( BUF_NAME, FUN_NAME )

typedef struct {
    unsigned  cap;
    unsigned  top;
    BUF_TYPE* buf;
} BUF_NAME;

static inline void NAME( BUF_NAME, init ) ( tazE_Engine* eng, BUF_NAME* buf ) {
    tazE_RawAnchor bufA;
    buf->cap = 7;
    buf->top = 0;
    buf->buf = tazE_mallocRaw( eng, &bufA, sizeof(BUF_TYPE)*buf->cap );
    
    tazE_commitRaw( eng, &bufA );
}

static inline void NAME( BUF_NAME, finl ) ( tazE_Engine* eng, BUF_NAME* buf ) {
    #ifdef BUF_CLEAN
        for( unsigned i = 0 ; i < buf->top ; i++ ) {
            BUF_CLEAN( eng, buf->buf[i] );
        }
    #endif

    tazE_freeRaw( eng, buf->buf, sizeof(BUF_TYPE)*buf->cap );
    buf->cap = 0;
    buf->top = 0;
    buf->buf = NULL;
}

static inline BUF_TYPE* NAME( BUF_NAME, put ) ( tazE_Engine* eng, BUF_NAME* buf ) {
    if( buf->top >= buf->cap ) {
        tazE_RawAnchor bufA = { .raw = buf->buf, .sz = sizeof(BUF_TYPE)*buf->cap };
        
        unsigned ncap = buf->cap*2;
        buf->buf = tazE_reallocRaw( eng, &bufA, sizeof(BUF_TYPE)*ncap );
        buf->cap = ncap;
        
        tazE_commitRaw( eng, &bufA );
    }

    BUF_TYPE* top = &buf->buf[buf->top++];
    #ifdef BUF_INIT
        BUF_INIT( eng, *top );
    #endif
    
    return top;
}

static inline void NAME( BUF_NAME, pack ) ( tazE_Engine* eng, BUF_NAME* buf, BUF_TYPE** ptr, unsigned* size ) {
    tazE_RawAnchor bufA = { .raw = buf->buf, .sz = sizeof(BUF_TYPE)*buf->cap };
    
    *ptr  = tazE_reallocRaw( eng, &bufA, sizeof(BUF_TYPE)*buf->top );
    *size = buf->top;
    
    tazE_commitRaw( eng, &bufA );

    buf->cap = 0;
    buf->top = 0;
    buf->buf = NULL;
}

#ifdef BUF_MARK

static inline void NAME( BUF_NAME, scan ) ( tazE_Engine* eng, BUF_NAME* buf, bool full ) {
    for( unsigned i = 0 ; i < buf->top ; i++ ) {
        BUF_MARK( eng, buf->buf[i] );
    }
}

#endif

#undef NAME
#undef NCAT