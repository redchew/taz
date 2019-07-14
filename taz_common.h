#ifndef taz_common_h
#define taz_common_h
#include "taz.h"
#include "taz_config.h"
#include "taz_math.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>

typedef long long          longest;
typedef unsigned char      uchar;
typedef unsigned short     ushort;
typedef unsigned int       uint;
typedef unsigned long      ulong;
typedef unsigned long long ulongest;
typedef uint8_t            uint8;
typedef uint16_t           uint16;
typedef uint32_t           uint32;
typedef uint64_t           uint64;
typedef int8_t             int8;
typedef int16_t            int16;
typedef int32_t            int32;
typedef int64_t            int64;

#ifndef static_assert
    #define static_assert( COND, MSG )
#endif

#ifndef NDEBUG
    #include <stdio.h>
    #include <stdlib.h>
    
    #define result_assert( COND, RESULT, MSG )                      \
    ((COND)                                                         \
        ? (RES)                                                     \
        : (                                                         \
            fprintf(                                                \
                stderr, "Assertion failed(%s:%u): %s\n",            \
                __FILE__, (uint)__LINE__                            \
            ),                                                      \
            abort(),                                                \
            (RES)                                                   \
        )                                                           \
    )
#else
    #define result_assert( COND, RESULT MSG )
#endif


typedef enum {
    tazR_Type_NONE,
    tazR_Type_FIRST,
    tazR_Type_FIRST_ATOMIC = tazR_Type_FIRST,
    tazR_Type_UDF = tazR_Type_FIRST_ATOMIC,
    tazR_Type_NIL,
    tazR_Type_LOG,
    tazR_Type_INT,
    tazR_Type_DEC,
    tazR_Type_TUP,
    tazR_Type_REF,
    tazR_Type_LAST_ATOMIC = tazR_Type_REF,
    
    tazR_Type_HYBRID_FIRST,
    tazR_Type_STR = tazR_Type_HYBRID_FIRST,
    tazR_Type_HYBRID_LAST = tazR_Type_STR,
    
    tazR_Type_FIRST_OBJECT,
    tazR_Type_IDX = tazR_Type_FIRST_OBJECT,
    tazR_Type_REC,
    tazR_Type_CODE,
    tazR_Type_FUN,
    tazR_Type_FIB,
    tazR_Type_BOX,
    tazR_Type_STATE,
    tazR_Type_LAST_OBJECT = tazR_Type_STATE,
    
    tazR_Type_LAST = tazR_Type_NIL
} tazR_Type;
static_assert( tazR_Type_LAST_OBJECT < 16, "Too many value types, NaN tag will overflow" );

typedef bool   tazR_Log;
typedef int32  tazR_Int;
typedef double tazR_Dec;
typedef uint64 tazR_Str;
typedef uint8  tazR_Tup;
typedef uint16 tazR_Ref;

typedef struct tazR_Obj   tazR_Obj;
typedef struct tazR_State tazR_State;
typedef struct tazR_Idx   tazR_Idx;
typedef struct tazR_Rec   tazR_Rec;
typedef struct tazR_Box   tazR_Box;
typedef struct tazR_Code  tazR_Code;
typedef struct tazR_Fun   tazR_Fun;
typedef struct tazR_Fib   tazR_Fib;

typedef struct tazE_Engine tazE_Engine;

#if taz_CONFIG_DISABLE_PTR_TAGGING
    typedef struct {
        uint16 tag;
        void*  addr;
    } tazR_TPtr;
    
    #define tazR_makeTPtr( TAG, ADDR )              (tazR_TPtr){.tag = (TAG), .addr = (ADDR)}
    #define tazR_getPtrTag( PTR )                   ((PTR).tag)
    #define tazR_getPtrAddr( PTR )                  ((PTR).addr)
#else
    typedef uint64 tazR_TPtr;

    #define tazR_makeTPtr( TAG, ADDR )              ((uint64)(ADDR) << 16 | (TAG))
    #define tazR_getPtrTag( PTR )                   ((PTR) & 0xFFFF )
    #define tazR_getPtrAddr( PTR )                  (void*)((PTR) >> 16)
#endif

#define tazR_getPtrTagBits( PTR, MASK, SHIFT ) ((tazR_getPtrTag( PTR ) & (MASK)) >> (SHIFT))


struct tazR_Obj {
    tazR_TPtr next_and_tag;
    #define tazR_OBJ_TAG_TYPE_SHIFT (0)
    #define tazR_OBJ_TAG_TYPE_MASK  (0xF << tazR_OBJ_TAG_TYPE_SHIFT)
    #define tazR_OBJ_TAG_MARK_SHIFT (4)
    #define tazR_OBJ_TAG_MARK_MASK  (0x1 << tazR_OBJ_TAG_MARK_SHIFT)
    #define tazR_OBJ_TAG_DEAD_SHIFT (5)
    #define tazR_OBJ_TAG_DEAD_MASK  (0x1 << tazR_OBJ_TAG_DEAD_SHIFT)
};
#define tazR_getObjType( OBJ ) \
    tazR_getPtrTagBits( (OBJ)->next_and_tag, tazR_OBJ_TAG_TYPE_MASK, tazR_OBJ_TAG_TYPE_SHIFT )
#define tazR_getObjNext( OBJ ) \
    (tazR_Obj*)tazR_getPtrAddr( (OBJ)->next_and_tag )
#define tazR_getObjData( OBJ ) \
    ((void*)(OBJ) + sizeof(tazR_Obj))
#define tazR_isObjMarked( OBJ ) \
    !!tazR_getPtrTagBits( (OBJ)->next_and_tag, tazR_OBJ_TAG_MARK_MASK, tazR_OBJ_TAG_MARK_SHIFT )
#define tazR_isObjDead( OBJ ) \
    !!tazR_getPtrTagBits( (OBJ)->next_and_tag, tazR_OBJ_TAG_DEAD_MASK, tazR_OBJ_TAG_DEAD_SHIFT )
#define tazR_toObj( PTR ) \
    (tazR_Obj*)((void*)(PTR) - sizeof(tazR_Obj))

/* Note: State Objects
State objects are general purpose objects for internal runtime use, they'll
never be exposed on the language end of things; and supply their own GC
callbacks.  The callback pointers are included in the state objects themselves
rather than an external vtable since these will generally just be used for
maintaining the state of certain runtime subsystems; so there usually won't
be enough of them for the memory overhead to be significant, and avoiding
the extra dereference will improve GC time by a bit.
*/
struct tazR_State {
    void   (*scan)( tazE_Engine* eng, tazR_State* self, bool full );
    void   (*finl)( tazE_Engine* eng, tazR_State* self );
    size_t (*size)( tazE_Engine* eng, tazR_State* self );
};

typedef enum {
    tazR_RefType_GLOBAL,
    tazR_RefType_LOCAL,
    tazR_RefType_BOXED
} tazR_RefType;

#if taz_CONFIG_DISABLE_NAN_TAGGING
    
    typedef struct {
        tazR_Type tag;
        union {
            uint64 u;
            double d;
        } u;
    } tazR_TVal;
    
    #define tazR_decVal( VAL )      (tazR_TVal){ .tag = tazR_Type_DEC, .u { .d = (double)(VAL) } }
    #define tazR_othVal( TAG, VAL ) (tazR_TVal){ .tag = (TAG), .u { .u = (uint64)(VAL) } }
    
    #ifndef NDEBUG
        static tazR_Type tazR_getValType_dbg( tazR_TVal val ) {
            return val.tag;
        }
    #endif
    
    #define tazR_getValType( VAL )    ((VAL).tag)
    #define tazR_getValByte( VAL )    (uchar)((VAL).tag | (((VAL).u.u & 0xF) << 4))
    #define tazR_getValHash( VAL )    ((VAL).u.u ^ (VAL).u.u >> 32 ^ (VAL).tag )
    #define tazR_getValRaw( VAL )     ((VAL).u.u)
    #define tazR_getValDec( VAL )     ((VAL).u.d)
    
    #define tazR_valEqual( VAL1, VAL2 ) ((VAL1).tag == (VAL2).tag && (VAL1).u.u == (VAL2).u.u)
    
    #define tazR_udf (tazR_TVal){ .tag = tazR_Type_UDF }
    #define tazR_nil (tazR_TVal){ .tag = tazR_Type_NIL }
#else
    
    typedef union {
        uint64 u;
        double d;
    } tazR_TVal;
    
    #define tazR_decVal( VAL )          (tazR_TVal){ .d = (double)(VAL) }
    #define tazR_othVal( TAG, VAL )     (tazR_TVal){ .u = 0x7FFLLU << 52 | (uint64)(VAL) << 4 | (TAG) }
    
    #ifndef NDEBUG
        static tazR_Type tazR_getValType_dbg( tazR_TVal val ) {
            return isnan( val.d )? val.u & 0xF : tazR_Type_DEC;
        }
    #endif
    
    #define tazR_getValType( VAL )    (isnan( (VAL).d )? (VAL).u & 0xF : tazR_Type_DEC)
    #define tazR_getValByte( VAL )    (uchar)((VAL).u & 0xFF)
    #define tazR_getValHash( VAL )    ((VAL).u ^ (VAL).u >> 32)
    #define tazR_getValRaw( VAL )     (((VAL).u >> 4) & 0xFFFFFFFFFFFF)
    #define tazR_getValDec( VAL )     ((VAL).d)
    
    #define tazR_valEqual( VAL1, VAL2 ) ((VAL1).u == (VAL2).u)
    
    #define tazR_udf (tazR_TVal){ .u = 0x7FFLLU << 52 | tazR_Type_UDF }
    #define tazR_nil (tazR_TVal){ .u = 0x7FFLLU << 52 | tazR_Type_NIL }
#endif

#define tazR_getValObj( VAL )   ((void*)tazR_getValRaw( VAL ))
#define tazR_getValLog( VAL )   ((tazR_Log)tazR_getValRaw( VAL ))
#define tazR_getValInt( VAL )   ((tazR_Int)tazR_getValRaw( VAL ))
#define tazR_getValStr( VAL )   ((tazR_Str)tazR_getValRaw( VAL ))
#define tazR_getValTup( VAL )   ((tazR_Tup)tazR_getValRaw( VAL ))
#define tazR_getValRef( VAL )   ((tazR_Ref)tazR_getValRaw( VAL ))
#define tazR_getValIdx( VAL )   ((tazR_Idx*)tazR_getValObj( VAL ))
#define tazR_getValRec( VAL )   ((tazR_Idx*)tazR_getValObj( VAL ))
#define tazR_getValCode( VAL )  ((tazR_Code*)tazR_getValObj( VAL ))
#define tazR_getValFun( VAL )   ((tazR_Fun*)tazR_getValObj( VAL ))
#define tazR_getValFib( VAL )   ((tazR_Fib*)tazR_getValObj( VAL ))
#define tazR_getValBox( VAL )   ((tazR_Box*)tazR_getValObj( VAL ))
#define tazR_getValState( VAL ) ((tazR_State*)tazR_getValObj( VAL ))

#define tazR_objVal( VAL )      tazR_othVal( tazR_getObjType( tazR_toObj( (VAL) ) ), (VAL) )
#define tazR_logVal( VAL )      tazR_othVal( tazR_Type_LOG, (VAL) )
#define tazR_intVal( VAL )      tazR_othVal( tazR_Type_INT, (VAL) )
#define tazR_strVal( VAL )      tazR_othVal( tazR_Type_INT, (VAL) )
#define tazR_tupVal( VAL )      tazR_othVal( tazR_Type_TUP, (VAL) )
#define tazR_refVal( VAL )      tazR_othVal( tazR_Type_REF, (VAL) )
#define tazR_idxVal( VAL )      tazR_othVal( tazR_Type_IDX, (VAL) )
#define tazR_recVal( VAL )      tazR_othVal( tazR_Type_REC, (VAL) )
#define tazR_codeVal( VAL )     tazR_othVal( tazR_Type_CODE, (VAL) )
#define tazR_funVal( VAL )      tazR_othVal( tazR_Type_FUN, (VAL) )
#define tazR_fibVal( VAL )      tazR_othVal( tazR_Type_FIB, (VAL) )
#define tazR_boxVal( VAL )      tazR_othVal( tazR_Type_BOX, (VAL) )
#define tazR_stateVal( VAL )    tazR_othVal( tazR_Type_STATE, (VAL) )


#define tazR_linkWithNextAndLink( LIST, NODE ) do {                         \
    (NODE)->link = (LIST);                                                  \
    (NODE)->next = *(LIST);                                                 \
    *(LIST) = (NODE);                                                       \
    if( (NODE)->next )                                                      \
        (NODE)->next->link = &(NODE)->next;                                 \
} while( 0 )
    
#define tazR_unlinkWithNextAndLink( NODE ) do {                             \
    *(NODE)->link = (NODE)->next;                                           \
    if( (NODE)->next )                                                      \
        (NODE)->next->link = (NODE)->link;                                  \
} while( 0 )


#define elemsof( BUF ) (sizeof(BUF)/sizeof(BUF[0]))
#endif