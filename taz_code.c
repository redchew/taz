#include "taz_code.h"
#include "taz_engine.h"
#include "taz_index.h"

#include <string.h>
#include <ctype.h>

typedef struct BCAssembler BCAssembler;
typedef struct BCInstr     BCInstr;
typedef struct BCLabel     BCLabel;

struct BCInstr {
    unsigned opcode;
    unsigned v;
    unsigned x;
    BCLabel* labels;
};

struct BCLabel {
    BCLabel* next;
    unsigned loc;
};


#define BUF_NAME InstrBuf
#define BUF_TYPE BCInstr
    #include "include/taz_buffer.in.c"
#undef BUF_NAME
#undef BUF_TYPE

#define BUF_NAME LabelBuf
#define BUF_TYPE BCLabel
    #include "include/taz_buffer.in.c"
#undef BUF_NAME
#undef BUF_TYPE

#define BUF_NAME ConstBuf
#define BUF_TYPE tazR_TVal
#define BUF_MARK tazE_markVal
    #include "include/taz_buffer.in.c"
#undef BUF_NAME
#undef BUF_TYPE
#undef BUF_MARK

#define BUF_NAME CodeBuf
#define BUF_TYPE ulongest
    #include "include/taz_buffer.in.c"
#undef BUF_NAME
#undef BUF_TYPE

struct BCAssembler {
    tazC_Assembler base;
    tazE_Engine*   eng;
    tazR_Str       name;
    taz_Scope      scope;

    unsigned numFixedParams;
    bool     hasVarParams;

    InstrBuf instrs;
    LabelBuf labels;
    ConstBuf consts;

    BCLabel* topLabels;

    tazR_Idx* upvalIdx;
    unsigned  numUpvals;

    tazR_Idx* localIdx;
    unsigned  numLocals;
};

static void finlAssembler( tazE_Engine* eng, tazR_State* self ) {
    BCAssembler* as = (BCAssembler*)self;

    InstrBuf_finl( eng, &as->instrs );
    LabelBuf_finl( eng, &as->labels );
    ConstBuf_finl( eng, &as->consts );
}

static void scanAssembler( tazE_Engine* eng, tazR_State* self, bool full ) {
    BCAssembler* as = (BCAssembler*)self;

    tazE_markStr( eng, as->name );
    tazE_markObj( eng, as->upvalIdx );
    tazE_markObj( eng, as->localIdx );

    ConstBuf_scan( eng, &as->consts, full );
}

static size_t sizeofAssembler( tazE_Engine* eng, tazR_State* self ) {
    return sizeof(BCAssembler);
}

static unsigned addLabel( tazC_Assembler* _as, size_t where ) {
    BCAssembler* as = (BCAssembler*)_as;
    assert( where <= as->instrs.top );

    BCLabel* label = LabelBuf_put( as->eng, &as->labels );
    label->loc = as->labels.top - 1;

    if( where == as->instrs.top ) {
        label->next = as->topLabels;
        as->topLabels = label;
    }
    else {
        label->next = as->instrs.buf[where].labels;
        as->instrs.buf[where].labels = label;
    }

    return label->loc;
}

static size_t addInstr( tazC_Assembler* _as, unsigned opcode, unsigned v, unsigned x ) {
    BCAssembler* as = (BCAssembler*)_as;
   
    BCInstr* instr = InstrBuf_put( as->eng, &as->instrs );
    instr->opcode = opcode;
    instr->v      = v;
    instr->x      = x;
    instr->labels = as->topLabels;
    as->topLabels = NULL;

    return as->instrs.top;
}

static tazR_Ref addConst( tazC_Assembler* _as, tazR_TVal val ) {
    BCAssembler* as = (BCAssembler*)_as;

    tazR_TVal* konst = ConstBuf_put( as->eng, &as->consts );
    *konst = val;

    return (tazR_Ref){ .bits = {
        .type = tazR_RefType_CONST,
        as->consts.top - 1
    }};
}

static tazR_Ref addUpval( tazC_Assembler* _as, tazR_Str name ) {
    BCAssembler* as = (BCAssembler*)_as;

    tazR_Ref ref;
    unsigned loc = tazR_idxInsert( as->eng, as->upvalIdx, tazR_strVal( name ) );
    ref.bits.type  = tazR_RefType_UPVAL;
    ref.bits.which = loc;
    if( loc > ref.bits.which )
        tazE_error( as->eng, taz_ErrNum_COMPILE, tazR_nil ); // TODO: add error message

    as->numUpvals++;
    return ref;
}

static tazR_Ref addLocal( tazC_Assembler* _as, tazR_Str name ) {
    BCAssembler* as = (BCAssembler*)_as;

    tazR_Ref ref;
    if( as->scope == taz_Scope_GLOBAL ) {
        // TODO
        assert( false );
        ref.flat = 0;
    }
    else {
        unsigned loc = tazR_idxInsert( as->eng, as->localIdx, tazR_strVal( name ) );
        ref.bits.type  = tazR_RefType_LOCAL;
        ref.bits.which = loc;
        if( loc > ref.bits.which )
            tazE_error( as->eng, taz_ErrNum_COMPILE, tazR_nil );
        
        as->numLocals++;
    }

    return ref;
}

static tazR_Ref addParam( tazC_Assembler* _as, tazR_Str name, bool var ) {
    BCAssembler* as = (BCAssembler*)_as;
    assert( as->numLocals == 0 && !as->hasVarParams );

    tazR_Ref ref;
    if( as->scope == taz_Scope_GLOBAL ) {
        // TODO
        assert( false );
        ref.flat = 0;
    }
    else {
        unsigned loc = tazR_idxInsert( as->eng, as->localIdx, tazR_strVal( name ) );
        ref.bits.type  = tazR_RefType_LOCAL;
        ref.bits.which = loc;
        if( loc > ref.bits.which )
            tazE_error( as->eng, taz_ErrNum_COMPILE, tazR_nil );
        
        if( var )
            as->hasVarParams = true;
        else
            as->numFixedParams++;
    }

    return ref;
}

static tazC_Assembler* makeNestedAssembler( tazC_Assembler* _as, tazR_Str name ) {
    BCAssembler* as = (BCAssembler*)_as;

    return tazR_makeAssembler( as->eng, name, as->scope );
}

#define A true
#define B false
#define OP( NAME, FORM, SE ) FORM,

bool isAForm[] = {
    #include "include/taz_opcodes.in.c"
};

#undef A
#undef B
#undef OP

static tazR_Code* makeByteCode( tazC_Assembler* _as ) {
    BCAssembler* as = (BCAssembler*)_as;

    tazE_RawAnchor labelsA;
    unsigned numLabels = as->labels.top;
    tazR_Label* labels = tazE_mallocRaw( as->eng, &labelsA, sizeof(tazR_Label)*numLabels );

    CodeBuf codeBuf;
    CodeBuf_init( as->eng, &codeBuf );

    unsigned i = 0;
    while( i < as->instrs.top ) {
        BCInstr*  instr = &as->instrs.buf[i];
        ulongest* word  = CodeBuf_put( as->eng, &codeBuf );
        *word = 0;

        unsigned j = 0;
        while( j < sizeof(ulongest) && i < as->instrs.top ) {
            unsigned shift = j*8;

            if( isAForm[instr->opcode] ) {
                assert( instr->x <= 0x3 );
                *word |= (instr->x << 5 | instr->opcode ) << shift;
                j++;
            }
            else {
                // No room in the next word, leave the rest as 0 = NOP
                // and move on to the next instruction with a new word
                if( j + 1 >= sizeof(ulongest) )
                    break;
                
                assert( instr->x <= 0x1FFFF );
                assert( instr->v <= 0x3 );
                *word |= (instr->x << 7 | instr->v << 5 | instr->opcode) << shift;
                j += 2;
            }

            BCLabel* labelIter = instr->labels;
            while( labelIter ) {
                // The addr is just an offest for now, need to add
                // the base address after finalizing the code buffer.
                labels[labelIter->loc].addr  = (ulongest*)(uintptr_t)i;
                labels[labelIter->loc].shift = shift;
                labelIter = labelIter->next;
            }

            i++;
            instr = &as->instrs.buf[i];
        }
    }

    tazE_ObjAnchor codeA;
    tazR_ByteCode* code = tazE_mallocObj( as->eng, &codeA, sizeof(tazR_ByteCode), tazR_Type_CODE );
    code->base.type           = tazR_CodeType_BYTE;
    code->base.name           = as->name;
    code->base.numFixedParams = as->numFixedParams;
    code->base.hasVarParams   = as->hasVarParams;
    code->base.upvalIdx       = as->upvalIdx;
    code->base.numUpvals      = as->numUpvals;
    code->base.localIdx       = as->localIdx;
    code->base.numLocals      = as->numLocals;

    tazR_TVal* consts; unsigned numConsts;
    ConstBuf_pack( as->eng, &as->consts, &consts, &numConsts );
    code->constBuf  = (tazR_TVal const*)consts;
    code->numConsts = numConsts;

    tazE_commitRaw( as->eng, &labelsA );
    code->labelBuf  = (tazR_Label const*)labels;
    code->numLabels = numLabels;

    ulongest* words; unsigned numWords; 
    CodeBuf_pack( as->eng, &codeBuf, &words, &numWords );
    code->wordBuf  = (ulongest const*)words;
    code->numWords = numWords;

    tazE_commitObj( as->eng, &codeA );

    return (tazR_Code*)code;
}

tazC_Assembler* tazR_makeAssembler( tazE_Engine* eng, tazR_Str name, taz_Scope scope ) {

    struct {
        tazE_Bucket base;
        tazR_TVal   upvalIdx;
        tazR_TVal   localIdx; 
    } buc;
    tazE_addBucket( eng, &buc, 2 );

    tazR_Idx* upvalIdx = tazR_makeIdx( eng );
    buc.upvalIdx = tazR_idxVal( upvalIdx );

    tazR_Idx* localIdx = tazR_makeIdx( eng );
    buc.localIdx = tazR_idxVal( localIdx );

    tazE_ObjAnchor asA;
    BCAssembler* as = tazE_mallocObj( eng, &asA, sizeof(BCAssembler), tazR_Type_STATE );

    as->base.base.finl      = finlAssembler;
    as->base.base.scan      = scanAssembler;
    as->base.base.size      = sizeofAssembler;
    as->base.addLabel       = addLabel;
    as->base.addInstr       = addInstr;
    as->base.addConst       = addConst;
    as->base.addUpval       = addUpval;
    as->base.addLocal       = addLocal;
    as->base.addParam       = addParam;
    as->base.makeAssembler  = makeNestedAssembler;
    as->base.makeCode       = makeByteCode;

    as->eng   = eng;
    as->name  = name;
    as->scope = scope;
    
    as->numFixedParams = 0;
    as->hasVarParams   = false;

    as->topLabels = NULL;

    as->localIdx  = localIdx;
    as->numLocals = 0;

    as->upvalIdx  = upvalIdx;
    as->numUpvals = 0;
    
    InstrBuf_init( eng, &as->instrs );
    LabelBuf_init( eng, &as->labels );
    ConstBuf_init( eng, &as->consts );

    tazE_commitObj( eng, &asA );
    tazE_remBucket( eng, &buc );
    return (tazC_Assembler*)as;
}

static bool checkParamName( char const* name, bool* isVar ) {
    if( !isalpha( name[0] ) && name[0] != '_' )
        return false;
    
    unsigned i;
    for( i = 1 ; name[i] != '\0' && name[i] != '.' ; i++ ) {
        if( !isalnum( name[i] ) && name[i] != '_' )
            return false;
    }

    if( name[i] == '\0' ) {
        *isVar = false;
        return true;
    }
    if( name[i] == '.' && name[i+1] == '.' && name[i+2] == '.' && name[i+3] == '\0' ) {
        *isVar = true;
        return true;
    }
    return false;
}

static bool checkUpvalName( char const* name ) {
    if( !isalpha( name[0] ) && name[0] != '_' )
        return false;
    
    unsigned i;
    for( i = 1 ; name[i] != '\0' ; i++ ) {
        if( !isalnum( name[i] ) && name[i] != '_' )
            return false;
    }

    return true;
}

tazR_Code* tazR_makeHostCode( tazE_Engine* eng, taz_FunCb cb, size_t sz, char const* name, char const** params, char const** upvals ) {
    struct {
        tazE_Bucket base;
        tazR_TVal   nameStr;
        tazR_TVal   upvalIdx;
        tazR_TVal   localIdx; 
    } buc;
    tazE_addBucket( eng, &buc, 3 );
    
    tazR_Str nameStr  = tazE_makeStr( eng, name, strlen( name ) );
    buc.nameStr = tazR_strVal( nameStr );

    tazR_Idx* upvalIdx = tazR_makeIdx( eng );
    buc.upvalIdx = tazR_idxVal( upvalIdx );

    tazR_Idx* localIdx = tazR_makeIdx( eng );
    buc.localIdx = tazR_idxVal( localIdx );

    tazE_ObjAnchor codeA;
    tazR_HostCode* code = tazE_mallocObj( eng, &codeA, sizeof(tazR_HostCode), tazR_Type_CODE );
    code->base.type             = tazR_CodeType_HOST;
    code->base.name             = nameStr;
    code->base.numFixedParams   = 0;
    code->base.hasVarParams     = false;
    code->base.upvalIdx         = upvalIdx;
    code->base.numUpvals        = 0;
    code->base.localIdx         = localIdx;
    code->base.numLocals        = 0;
    code->callback              = cb;
    code->stateSize             = sz;

    for( unsigned i = 0 ; params[i] != NULL ; i++ ) {
        bool isVar = false;
        if( !checkParamName( params[i], &isVar ) )
            tazE_error( eng, taz_ErrNum_OTHER, tazR_nil ); // TODO: add error message
        if( code->base.hasVarParams && isVar )
            tazE_error( eng, taz_ErrNum_OTHER, tazR_nil ); // TODO: add error message
        
        if( isVar ) {
            code->base.hasVarParams = true;
            tazR_idxInsert( eng, localIdx, tazR_strVal( tazE_makeStr( eng, params[i], strlen( params[i] ) - 3 ) ) );
        }
        else {
            code->base.numFixedParams++;
            tazR_idxInsert( eng, localIdx, tazR_strVal( tazE_makeStr( eng, params[i], strlen( params[i] ) ) ) );
        }
    }

    for( unsigned i = 0 ; upvals[i] != NULL ; i++ ) {
        if( !checkUpvalName( upvals[i] ) )
            tazE_error( eng, taz_ErrNum_OTHER, tazR_nil ); // TODO: add error message
        code->base.numUpvals++;
        tazR_idxInsert( eng, upvalIdx, tazR_strVal( tazE_makeStr( eng, params[i], strlen( params[i] ) ) ) );
    }

    tazE_commitObj( eng, &codeA );
    tazE_remBucket( eng, &buc );

    return (tazR_Code*)code;
}

void tazR_dumpCode( tazE_Engine* eng, tazR_Code* code, taz_Writer* w ) {
    assert( false ); // TODO
}

void _tazR_scanCode( tazE_Engine* eng, tazR_Code* code, bool full ) {
    if( full )
        tazE_markStr( eng, code->name );
    
    tazE_markObj( eng, code->localIdx );
    tazE_markObj( eng, code->upvalIdx );

    if( code->type == tazR_CodeType_BYTE ) {
        tazR_ByteCode* bcode = (tazR_ByteCode*)code;
        for( unsigned i = 0 ; i < bcode->numConsts ; i++ )
            tazE_markVal( eng, bcode->constBuf[i] );
    }
}

void _tazR_finlCode( tazE_Engine* eng, tazR_Code* code ) {
    if( code->type != tazR_CodeType_BYTE )
        return;
    
    tazR_ByteCode* bcode = (tazR_ByteCode*)code;
    tazE_freeRaw( eng, (void*)bcode->constBuf, sizeof(tazR_TVal)*bcode->numConsts );
    tazE_freeRaw( eng, (void*)bcode->labelBuf, sizeof(tazR_Label)*bcode->numLabels );
    tazE_freeRaw( eng, (void*)bcode->wordBuf, sizeof(ulongest)*bcode->numWords );
}