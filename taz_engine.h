#ifndef taz_engine_h
#define taz_engine_h
#include "taz_common.h"

#include <setjmp.h>

/* Type: taz_Engine
The `taz_Engine` type makes up the central component of the language runtime,
it's responsible for memory management (including garbage collection), string
pooling, error propegation (through `setjmp.h` utils), and maintaining
references to the state objects of other runtime subcomponents.  The references
are maintained for two purposes: 1) to serve as GC roots, 2) to allow access
to all subsystems through a common component.  The second purpose keeps our
code tidy since we don't need to pass all necessery services separately when
invoking a routine.  It also decouples our code, since subroutines needn't make
their dependencies on the various subsystems explicit, these dependencies can
change without affecting the rest of the runtime.
*/

typedef struct tazE_ObjAnchor tazE_ObjAnchor;
typedef struct tazE_RawAnchor tazE_RawAnchor;
typedef struct tazE_Bucket    tazE_Bucket;
typedef struct tazE_Barrier   tazE_Barrier;
typedef struct tazE_Listener  tazE_Listener;
typedef enum   tazE_EventType tazE_EventType;

struct tazE_Engine {
    tazR_State* environment;
    tazR_State* interface;

    tazR_TVal errvalBadAlloc;
    tazR_TVal errvalBadKey;
    tazR_TVal errvalTooManyLocals;
    tazR_TVal errvalTooManyUpvals;
    tazR_TVal errvalTooManyConsts;
    tazR_TVal errvalBadParamName;
    tazR_TVal errvalBadUpvalName;
    tazR_TVal errvalMultipleEllipsis;
    tazR_TVal errvalSetFromUdf;
    tazR_TVal errvalSetToUdf;
    tazR_TVal errvalInvalidFormatSpec;
};

tazE_Engine* tazE_makeEngine( taz_Config const* cfg );
void         tazE_freeEngine( tazE_Engine* eng );
bool         tazE_testEngine( void );

/* Note: Memory Allocation
The `taz_Engine` uses long jumps for propegating errors efficiently, thus
avoiding the overhead of countless error checks and forwards.  Unfortunately
this presents some issues when dealing with memory management, since an error
may occur (and and cause destruction of a stack frame) while dynamic memory
is held on the stack; thus leaking such memory.

To avoid this potential issue the engine initially allocates memory only on
a tentative basis, meaning the allotted memory will be automatically released
by the engine if an interruption (long jump) is triggered while it's still
on the stack.  The engine keeps track of tentative allocations via a list of
anchor objects, which maintain the meta info associated with the allocation.

To avoid the overhead and fragmentation of an additional memory allocation
(for the anchor) for each memory request, such anchors are allocated on the
stack; but added to a linked list within the engine until committed or
cancelled, so it's *critical* that one of these actions is performed before
the stack frame in which the anchor was allocated is destroyed, otherwise
memory corruption will occur, and the bugs caused by this are very difficult
to pinpoint.

Since an allocation is expected to be incomplete or uninitialized while in a
tentative state, garbage collected objects in such a state will not be released
nor scanned by the GC.

Note that `tazE_reallocRaw()` doesn't accept a pointer to the old allocation
since this as well as the allocation's size is expected to be provided within
the fields of the given anchor, which will be adjusted for the new allocation.
*/

struct tazE_ObjAnchor {
    tazE_ObjAnchor*  next;
    tazE_ObjAnchor** link;
    
    tazR_Obj* obj;
    size_t    sz;
};

void* tazE_mallocObj( tazE_Engine* eng, tazE_ObjAnchor* anchor, size_t sz, tazR_Type type );
void* tazE_zallocObj( tazE_Engine* eng, tazE_ObjAnchor* anchor, size_t sz, tazR_Type type );
void  tazE_cancelObj( tazE_Engine* eng, tazE_ObjAnchor* anchor );
void  tazE_commitObj( tazE_Engine* eng, tazE_ObjAnchor* anchor );

struct tazE_RawAnchor {
    tazE_RawAnchor*  next;
    tazE_RawAnchor** link;
    
    void*  raw;
    size_t sz;
};

void* tazE_mallocRaw( tazE_Engine* eng, tazE_RawAnchor* anchor, size_t sz );
void* tazE_zallocRaw( tazE_Engine* eng, tazE_RawAnchor* anchor, size_t sz );
void* tazE_reallocRaw( tazE_Engine* eng, tazE_RawAnchor* anchor, size_t sz );
void  tazE_freeRaw( tazE_Engine* eng, void* raw, size_t sz );
void  tazE_cancelRaw( tazE_Engine* eng, tazE_RawAnchor* anchor );
void  tazE_commitRaw( tazE_Engine* eng, tazE_RawAnchor* anchor );

/* Note: Garbage Collection
Cleanup and scanning routines are defined elsewhere for different types of Taz
values so the engine provides a function to be called elsewhere to mark an
objects as being references.
*/

void tazE_markObj( tazE_Engine* eng, void* ptr );
void tazE_markStr( tazE_Engine* eng, tazR_Str str );


#define tazE_markVal( ENG, VAL ) do {                                      \
    tazR_Type type = tazR_getValType( (VAL) );                             \
    if( type > tazR_Type_LAST_ATOMIC ) {                                   \
        if( type == tazR_Type_STR )                                        \
            tazE_markStr( (ENG), tazR_getValStr( (VAL) ) );                \
        else                                                               \
            tazE_markObj( (ENG), tazR_getValObj( (VAL) ) );                \
    }                                                                      \
} while( 0 )

void tazE_collect( tazE_Engine* eng, bool full );


/* Note: Reference Buckets
In some subroutines we need to keep references to garbage collected objects
for the duration of its invocation.  For this we can allocate a `tazE_Bucket`
where references can be found and scanned by the engine.  The bucket is
expected to be stack allocated, and will be removed automatically when an
interruption occurs.  Buckets of particular capacities should be defined as
structs with `tazE_Bucket` used as a header.  All members after the header
should be of type `tazR_TVal` and will be initialized to `udf` when the bucket
is installed.  Here's an example:

    struct {
        tazE_Bucket header;
        tazR_TVal   someStr;
        tazR_TVal   someRec;
    } myBucket;
    tazR_addBucket( eng, &myBucket, 2 );
    myBucket.someStr = makeTVal( tazR_Type_STR, someStr );
    myBucket.someRec = makeTVal( tazR_Type_REC, someRec );
    ...
    tazR_remBucket( eng, &myBucket );
    
As with anchors, since buckets are stack allocated it's critical that it be
removed from the bucket list before the stack frame in which it was allocated
expires.
*/

struct tazE_Bucket {
    tazE_Bucket*  next;
    tazE_Bucket** link;
    unsigned      size;
};

void tazE_addBucket( tazE_Engine* eng, void* buc, unsigned size );
void tazE_remBucket( tazE_Engine* eng, void* buc );


/* Note: Barriers
The Taz runtime triggers and handles two types of interruption: errors and
fiber yields.  These are both implemented as long jumps and cause the engine
to drop references to stack allocations above the point where the interrupt
will be handled (above the respective setjmp() on the stack).  Thus a pair
of long jump destinations need to be installed before execution of interruption
prone code.  These jump destinations, as well as the heads linked lists for
stack allocated entities, are wrapped up in a `tazE_Barrier` struct.  The
caller is expected to initialize the jump destination fields: errorDst and
yieldDst; as well as the pre-interrupt callbacks: errorFun and yieldFun.
The callbacks can be set to NULL of nothing needs to be done before the
interrupt, and destruction of stack frames, is invoked.
*/

struct tazE_Barrier {
    tazE_Barrier*  prev;
    
    tazE_ObjAnchor* objAnchors;
    tazE_RawAnchor* rawAnchors;
    tazE_Bucket*    buckets;
    
    taz_ErrNum  errnum;
    tazR_TVal   errval;
    
    void (*errorFun)( tazE_Engine* eng, tazE_Barrier* bar );
    void (*yieldFun)( tazE_Engine* eng, tazE_Barrier* bar );
    
    jmp_buf errorDst;
    jmp_buf yieldDst;
};

void tazE_pushBarrier( tazE_Engine* eng, tazE_Barrier* barrier );
void tazE_popBarrier( tazE_Engine* eng, tazE_Barrier* barrier );


/* Note: Interrupts
These next few functions trigger interrupts which are expected to be handled by
the topmost barrier.  A call to `tazE_error()` will long jump to `errorDst`
after setting the barrier's `errnum` and `errval` fields from the respective
arguments.  A call to `tazE_yield()` jumps to `yieldDst`.  In either case the
engine will drop any references to stack allocated entities linked onto the
topmost barrier, and will release uncommitted allocations.
*/
void tazE_error( tazE_Engine* eng, taz_ErrNum errnum, tazR_TVal errval );
void tazE_yield( tazE_Engine* eng );


/* Note: Strings
The engine is responsible for string pooling, which it can do at its own
discretion as the rest of the runtime doesn't depend on any particular
system.  But this implementation breaks strings into three sizes: short,
medium, and long.  Short strings are those which are 0-5 bytes long, they
can be encoded within a value payload itself without any additional memory.
Medium strings are 6-16 bytes long, and are interned to make for more
efficient comparison.  Long strings are anything larger than 16 bytes,
these are allocated in independent buffers.

This division of strings into different lengths allows the language to deal
with the practical use cases of strings efficiently:
    - Using strings as characters (short strings)
    - Using strings as enumerations and keys(medium strings)
    - Using strings as generic data or text buffers (long strings)

*/

tazR_Str tazE_makeStr( tazE_Engine* eng, char const* str, size_t len );
void     tazE_borrowStr( tazE_Engine* eng, tazR_Str str, taz_StrLoan* loan );
void     tazE_returnStr( tazE_Engine* eng, taz_StrLoan* loan );
void     tazE_stealStr( tazE_Engine* eng, taz_StrLoan* loan );
unsigned tazE_strHash( tazE_Engine* eng, tazR_Str str );
bool     tazE_strEqual( tazE_Engine* eng, tazR_Str str1, tazR_Str str2 );
bool     tazE_strLess( tazE_Engine* eng, tazR_Str str1, tazR_Str str2 );
bool     tazE_strMore( tazE_Engine* eng, tazR_Str str1, tazR_Str str2 );

#define tazE_strIsLong( ENG, STR ) (((STR) >> 46) == 2)
#define tazE_strIsGCed( ENG, STR ) (((STR) >> 46) != 0)

#endif