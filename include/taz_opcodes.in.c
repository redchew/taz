/* Note: Instructions
Here we list out the virtual instructions understood by Taz's interpreter,
with a brief description and a some of meta info.  This file can be included
elsewhere in the system after defining the `OP()` macro wherever a listing
of these opcodes is needed.

While Taz's bytecode format can be serialized, it isn't guaranteed to be
consistent across major versions of the language, so external units should
avoid depending on the bytecode format at all if possible, otherwise the
language version should be taken into account.

Every instruction fits in either a single byte or a two byte word.  Single
byte instructions are generally reserved for the more common ones both to
preserve memory and reduce memory fetches.  Single byte instructions take
the form:

    A: xxxccccc

Where `c` represents an opcode bit and `x` an operand bit.  This gives up
to 8 operand values, suitable for encoding smaller variable numbers and
such.  Two byte instructions look like this:

    B: xvvccccc xxxxxxxx

Where `c` bits give the opcode, `v` bits give any variations, and `x` bits
give the main operand.  These forms are referred to as A-form and B-form
respectively.

The op-code listing that follows gives each opcode in the form:

    OP( NAME, FORM, SE )

Where SE gives the effect that the opcode will have on the stack, this is
used to estimate the number of slots that must be allocated before a function
is called, to avoid checking stack boundaries for every instruction that
pushes values to the stack.  The effect is given as a linear expression:

    SE( MUL, OFF )

Which expands to:

    X*MUL + OFF

Where `X` gives the instruction operand (all `x` bits).  We try to keep the
more common instructions clustered at the lower opcodes to improve the
performance of the instruction cache within the interpret loop.  The SE won't
give the exact stack effect of an instruction, but the worst case effect,
meaning the one which grows the stack most, or shrinks it least.

The actual opcode value for each listing is implicit, matching the entry's
location in the listing.
*/

/* Brief: Access lower variables and constants
*/
OP( GET_LOCAL_A, A, SE( 0, 1 ) )
OP( GET_UPVAL_A, A, SE( 0, 1 ) )
OP( GET_CLOSED_A, A, SE( 0, 1 ) )
OP( GET_CONST_A, A, SE( 0, 1 ) )

/* Brief: Access record fields by slot
*/
OP( GET_FIELD_A, A, SE( 0, 0 ) )

/* Brief: Jump to lower labels
*/
OP( AND_JUMP_A, A, SE( 0, 0 ) )
OP( OR_JUMP_A, A, SE( 0, 0 ) )
OP( ALT_JUMP_A, A, SE( 0, 0 ) )

/* Brief: Load somthing
Load one of the following values depending on the operand.

    0 - 0
    1 - 0.0
    2 - nil
    3 - udf
    4 - true
    5 - false
    6 - ''
    7 - ""
*/
OP( LOAD_THING, A, SE( 0, 1 ) )

/* Brief: Unary operations
*/
OP( NOT, A, SE( 0, -1 ) )
OP( NEG, A, SE( 0, -1 ) )
OP( FLIP, A, SE( 0, -1 ) )

/* Brief: Arithmetic operations
*/
OP( MUL, A, SE( 0, -1 ) )
OP( DIV, A, SE( 0, -1 ) )
OP( MOD, A, SE( 0, -1 ) )
OP( SUB, A, SE( 0, -1 ) )
OP( ADD, A, SE( 0, -1 ) )

/* Brief: Shift operations
*/
OP( LSL, A, SE( 0, -1 ) )
OP( LSR, A, SE( 0, -1 ) )

/* Brief: Comparison operations
*/
OP( LT, A, SE( 0, -1 ) )
OP( LE, A, SE( 0, -1 ) )
OP( GT, A, SE( 0, -1 ) )
OP( GE, A, SE( 0, -1 ) )
OP( IE, A, SE( 0, -1 ) )
OP( NE, A, SE( 0, -1 ) )
OP( UE, A, SE( 0, -1 ) )

/* Brief: Logical operations
*/
OP( AND, A, SE( 0, -1 ) )
OP( XOR, A, SE( 0, -1 ) )
OP( OR, A, SE( 0, -1 ) )

/* Brief: Function linkage
*/
OP( CALL, A, SE( 0, TUP_MAX ) )
OP( RET, A, SE( 0, 0 ) ) 

/* Brief: Stack manipulation
*/
OP( POP, A, SE( 0, -1 ) )
OP( DUP, A, SE( 0, TUP_MAX + 1 ) )
OP( SWAP, A, SE( 0, 0 ) )

/* Brief: Assignments
The variation bits here indicate the language level assignment pattern
used.

    00 - Simple tuple pattern
    01 - Variadic tuple pattern
    10 - Simple record pattern
    11 - Variadic record pattern
*/
OP( DEF_VARS, B, SE( -1, -1 ) )
OP( SET_VARS, B, SE( -1, -1 ) )
OP( DEF_FIELDS, B, SE( -1, -2 ) )
OP( SET_FIELDS, B, SE( -1, -2 ) )


/* Brief: Access variables and constants
*/
OP( GET_LOCAL_B, B, SE( 0, 1 ) )
OP( GET_UPVAL_B, B, SE( 0, 1 ) )
OP( GET_CLOSED_B, B, SE( 0, 1 ) )
OP( GET_CONST_B, B, SE( 0, 1 ) )

/* Brief: Access record field
*/
OP( GET_FIELD_B, B, SE( 0, -1 ) )

/* Brief: Jump to upper labels
*/
OP( AND_JUMP_B, B, SE( 0, 0 ) )
OP( OR_JUMP_B, B, SE( 0, 0 ) )
OP( ALT_JUMP_B, B, SE( 0, 0 ) )
