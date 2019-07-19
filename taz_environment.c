#include "taz_environment.h"
#include "taz_engine.h"
#include "taz_record.h"
#include "taz_index.h"

typedef struct {
    tazR_State base;

    tazR_Idx*  globalsIdx;
    tazR_TVal* globalsBuf;
    unsigned   globalsCap;

    tazR_Rec* importLoaders;
    tazR_Rec* importTranslators;
    tazR_Rec* operatorFunctions;
} Env;

static void scanEnv( tazE_Engine* eng, tazR_State* self ) {
    Env* env = (Env*)self;
    tazE_markObj( eng, env->globalsIdx );
    tazE_markObj( eng, env->importLoaders );
    tazE_markObj( eng, env->importTranslators );
    tazE_markObj( eng, env->operatorFunctions );

    for( unsigned i = 0 ; i < env->globalsCap ; i++ )
        tazE_markVal( eng, env->globalsBuf[i] );
}

static size_t sizeofEnv( tazE_Engine* eng, tazR_State* self ) {
    return sizeof(Env);
}

void tazR_initEnv( tazE_Engine* eng ) {
    struct {
        tazE_Bucket base;
        tazR_TVal   globalsIdx;
        tazR_TVal   tmpIdx;
        tazR_TVal   importLoaders;
        tazR_TVal   importTranslators;
        tazR_TVal   operatorFunctions;
    } buc;
    tazE_addBucket( eng, &buc, 5 );

    tazR_Idx* globalsIdx = tazR_makeIdx( eng );
    buc.globalsIdx = tazR_idxVal( globalsIdx );

    tazR_Idx* importStrategyIdx = tazR_makeIdx( eng );
    buc.tmpIdx = tazR_idxVal( importStrategyIdx );

    tazR_Rec* importLoadersRec = tazR_makeRec( eng, importStrategyIdx );
    buc.importLoaders = tazR_recVal( importLoadersRec );

    tazR_Rec* importTranslatorsRec = tazR_makeRec( eng, importStrategyIdx );
    buc.importTranslators = tazR_recVal( importTranslatorsRec );

    tazR_Idx* operatorFunctionsIdx = tazR_makeIdx( eng );
    buc.operatorFunctions = tazR_idxVal( operatorFunctionsIdx );

    tazR_Rec* operatorFunctionsRec = tazR_makeRec( eng, operatorFunctionsIdx );
    buc.operatorFunctions = tazR_recVal( operatorFunctionsRec );

    tazE_ObjAnchor envA;
    Env* env = tazE_mallocObj( eng, &envA, sizeof(Env), tazR_Type_STATE );
    env->globalsIdx = globalsIdx;
    env->globalsBuf = NULL;
    env->globalsCap = 0;
    env->importLoaders     = importLoadersRec;
    env->importTranslators = importTranslatorsRec;
    env->operatorFunctions = operatorFunctionsRec;

    tazE_commitObj( eng, &envA );
    tazE_remBucket( eng, &buc );
    
    eng->envState = (tazR_State*)env;
}

void ensureGlobal( tazE_Engine* eng, unsigned loc ) {
    Env* env = (Env*)eng->envState;
    if( loc < env->globalsCap )
        return;

    tazE_RawAnchor bufA = { .sz = env->globalsCap*sizeof(tazR_TVal), .raw = env->globalsBuf };

    unsigned   ncap = env->globalsCap*2 + 1;
    tazR_TVal* nbuf = tazE_reallocRaw( eng, &bufA, sizeof(tazR_TVal)*ncap );
    for( unsigned i = env->globalsCap ; i < ncap ; i++ )
        nbuf[i] = tazR_udf;
    
    env->globalsCap = ncap;
    env->globalsBuf = nbuf;

    tazE_commitRaw( eng, &bufA );
}

unsigned tazR_getGlobalLoc( tazE_Engine* eng, tazR_Str name ) {
    Env* env = (Env*)eng->envState;

    unsigned loc = tazR_idxInsert( eng, env->globalsIdx, tazR_strVal( name ) );
    ensureGlobal( eng, loc );

    return loc;
}

taz_Var tazR_getGlobalVar( tazE_Engine* eng, tazR_Str name ) {
    Env* env = (Env*)eng->envState;

    unsigned loc = tazR_idxInsert( eng, env->globalsIdx, tazR_strVal( name ) );
    ensureGlobal( eng, loc );

    taz_Var var = { ._base = (void**)&env->globalsBuf, ._offset = loc };
    return var;
}

tazR_TVal* tazR_getGlobalVal( tazE_Engine* eng, tazR_Str name ) {
    Env* env = (Env*)eng->envState;

    unsigned loc = tazR_idxInsert( eng, env->globalsIdx, tazR_strVal( name ) );
    ensureGlobal( eng, loc );

    return &env->globalsBuf[loc];
}

taz_Var tazR_getGlobalVarByLoc( tazE_Engine* eng, unsigned loc ) {
    Env* env = (Env*)eng->envState;
    ensureGlobal( eng, loc );

    taz_Var var = { ._base = (void**)&env->globalsBuf, ._offset = loc };
    return var;
}

tazR_TVal* tazR_getGlobalValByLoc( tazE_Engine* eng, unsigned loc ) {
    Env* env = (Env*)eng->envState;
    ensureGlobal( eng, loc );

    return &env->globalsBuf[loc];
}

void tazR_setImportStrategy( tazE_Engine* eng, tazR_Str name, tazR_Fun* load, tazR_Fun* trans ) {
    Env* env = (Env*)eng->envState;
    tazR_recDef( eng, env->importLoaders, tazR_strVal( name ), tazR_funVal( load ) );
    tazR_recDef( eng, env->importTranslators, tazR_strVal( name ), tazR_funVal( trans ) );
}

void tazR_getImportStrategy( tazE_Engine* eng, tazR_Str name, tazR_Fun** load, tazR_Fun** trans ) {
    Env* env = (Env*)eng->envState;
    tazR_TVal loadVal  = tazR_recGet( eng, env->importLoaders, tazR_strVal( name ) );
    tazR_TVal transVal = tazR_recGet( eng, env->importTranslators, tazR_strVal( name ) );

    if( tazR_getValType( loadVal ) == tazR_Type_FUN )
        *load = tazR_getValFun( loadVal );
    else
        *load = NULL;
    
    if( tazR_getValType( transVal ) == tazR_Type_FUN )
        *trans = tazR_getValFun( transVal );
    else
        *trans = NULL;
}

void tazR_setOperatorFunction( tazE_Engine* eng, tazR_Str opr, tazR_Fun* fun ) {
    Env* env = (Env*)eng->envState;
    tazR_recDef( eng, env->operatorFunctions, tazR_strVal( opr ), tazR_funVal( fun ) );
}

tazR_Fun* tazR_getOperatorFunction( tazE_Engine* eng, tazR_Str opr ) {
    Env* env = (Env*)eng->envState;
    tazR_TVal funVal = tazR_recGet( eng, env->operatorFunctions, tazR_strVal( opr ) );
    if( tazR_getValType( funVal ) == tazR_Type_FUN )
        return tazR_getValFun( funVal );
    else
        return NULL;
}