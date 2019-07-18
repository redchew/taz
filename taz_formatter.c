#include "taz_formatter.h"
#include "taz_engine.h"
#include "taz_math.h"

#include <ctype.h>

void tazR_fmt( tazE_Engine* eng, taz_Writer* w, char const* pat, ... ) {
    va_list ap;
    va_start( ap, pat );

    tazR_vfmt( eng, w, pat, ap );

    va_end( ap );
}

typedef enum {
    tazR_FmtFlag_QUOTED = 1,
    tazR_FmtFlag_HEX    = 2,
    tazR_FmtFlag_DEC    = 4,
    tazR_FmtFlag_OCT    = 8,
    tazR_FmtFlag_BIN    = 16
} tazR_FmtFlags;

static void fmtVal( tazE_Engine* eng, taz_Writer* w, tazR_TVal val, tazR_FmtFlags flags );

static bool hasBase( tazR_FmtFlags flags ) {
    return (
        (flags & tazR_FmtFlag_HEX) ||
        (flags & tazR_FmtFlag_DEC) ||
        (flags & tazR_FmtFlag_OCT) ||
        (flags & tazR_FmtFlag_BIN)
    );
}

void tazR_vfmt( tazE_Engine* eng, taz_Writer* w, char const* pat, va_list ap ) {
    unsigned i = 0;
    while( pat[i] != '\0' ) {
        if( pat[i] == '{' ) {
            tazR_FmtFlags flags = 0;
            while( pat[++i] != '}' ) {
                switch( pat[i] ) {
                    case 'Q':
                        flags |= tazR_FmtFlag_QUOTED;
                    break;
                    case 'H':
                        if( hasBase( flags ) )
                            tazE_error( eng, taz_ErrNum_FORMAT, eng->errvalInvalidFormatSpec );
                        flags |= tazR_FmtFlag_HEX;
                    break;
                    case 'D':
                        if( hasBase( flags ) )
                            tazE_error( eng, taz_ErrNum_FORMAT, eng->errvalInvalidFormatSpec );
                        flags |= tazR_FmtFlag_DEC;
                    break;
                    case 'O':
                        if( hasBase( flags ) )
                            tazE_error( eng, taz_ErrNum_FORMAT, eng->errvalInvalidFormatSpec );
                        flags |= tazR_FmtFlag_OCT;
                    break;
                    case 'B':
                        if( hasBase( flags ) )
                            tazE_error( eng, taz_ErrNum_FORMAT, eng->errvalInvalidFormatSpec );
                        flags |= tazR_FmtFlag_BIN;
                    break;
                    default:
                        tazE_error( eng, taz_ErrNum_FORMAT, eng->errvalInvalidFormatSpec );
                    break;
                }
            }

            fmtVal( eng, w, va_arg( ap, tazR_TVal ), flags );
        }
        else {
            w->write( w, pat[i] );
        }
        i++;
    }
}

static void fmtRaw( tazE_Engine* eng, taz_Writer* w, char const* raw ) {
    for( unsigned i = 0 ; raw[i] != '\0' ; i++ )
        w->write( w, raw[i] );
}

static char  const digits[] = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

static void fmtUdf( tazE_Engine* eng, taz_Writer* w, tazR_FmtFlags flags ) {
    fmtRaw( eng, w, "udf" );
}

static void fmtNil( tazE_Engine* eng, taz_Writer* w, tazR_FmtFlags flags ) {
    fmtRaw( eng, w, "nil" );
}

static void fmtLog( tazE_Engine* eng, taz_Writer* w, tazR_Log log, tazR_FmtFlags flags ) {
    if( log ) {
        fmtRaw( eng, w, "true" );
    }
    else {
        fmtRaw( eng, w, "false" );
    }
}

static void fmtInt( tazE_Engine* eng, taz_Writer* w, tazR_Int in, tazR_FmtFlags flags ) {
    unsigned base = 10;
    if( (flags & tazR_FmtFlag_HEX) != 0 )
        base = 16;
    else
    if( (flags & tazR_FmtFlag_OCT) != 0 )
        base = 8;
    else
    if( (flags & tazR_FmtFlag_BIN) != 0 )
        base = 2;

    if( in == 0 ) {
        w->write( w, '0' );
    }
    else
    if( in < 0 ) {
        w->write( w, '-' );
        in = -in;
    }
    
    char buf[32];
    int  loc = sizeof(buf);
    while( in != 0 ) {
        assert( loc >= 0 );
        buf[--loc] = digits[ in % base ];
        in /= base;
    }
    while( loc < sizeof(buf) ) {
        w->write( w, buf[loc++] );
    }
}

static void fmtDec( tazE_Engine* eng, taz_Writer* w, tazR_Dec dec, tazR_FmtFlags flags ) {
    assert( !isnan( dec ) );

    unsigned base = 10;
    if( (flags & tazR_FmtFlag_HEX) != 0 )
        base = 16;
    else
    if( (flags & tazR_FmtFlag_OCT) != 0 )
        base = 8;
    else
    if( (flags & tazR_FmtFlag_BIN) != 0 )
        base = 2;

    
    if( dec < 0.0 ) {
        w->write( w, '-' );
        dec = -dec;
    }
    if( dec < 0.00000000000001 ) {
        fmtRaw( eng, w, "0.0" );
        return;
    }
    if( !isfinite( dec ) ) {
        fmtRaw( eng, w, "inf" );
        return;
    }

    tazR_Dec whole;
    tazR_Dec fract = modf( dec, &whole );
    unsigned count = 0;

    char buf[18];
    int  loc = sizeof(buf);
    while( whole >= 1.0 && count < 18 ) {
        buf[--loc] = digits[ (unsigned)fmod( whole, base ) ];
        whole = floor( whole / base );
        count++;
    }
    bool overflow = (count == 18);

    while( loc < sizeof(buf) ) {
        w->write( w, buf[loc++] );
    }

    if( overflow ) {
        w->write( w, '.' );
        w->write( w, '.' );
        w->write( w, '.' );
        return;
    }

    w->write( w, '.' );
    if( fract > 0.00000000000001 && count < 18 ) do {
        fract *= base;
        fract  = modf( fract, &whole );
        w->write( w, digits[ (unsigned)whole ] );
        count++;
    } while( fract > 0.00000000000001 && count < 18 );
    else {
        w->write( w, '0' );
    }

}

static void fmtStr( tazE_Engine* eng, taz_Writer* w, tazR_Str str, tazR_FmtFlags flags ) {
    taz_StrLoan sl;
    tazE_borrowStr( eng, str, &sl );

    bool multiline = false;
    if( flags & tazR_FmtFlag_QUOTED ) {
        for( unsigned i = 0 ; i < sl.len ; i++ ) {
            if( sl.str[i] == '\n' )
                multiline = true;
        }
        w->write( w, '\'' );
        if( multiline )
            w->write( w, '(' );
    }
    for( unsigned i = 0 ; i < sl.len ; i++ )
        w->write( w, sl.str[i] );
    
    if( flags & tazR_FmtFlag_QUOTED ) {
        if( multiline )
            w->write( w, ')' );
        w->write( w, '\'' );
    }

    tazE_returnStr( eng, &sl );
}

static bool isIdentKey( tazE_Engine* eng, tazR_TVal key ) {
    if( tazR_getValType( key ) != tazR_Type_STR )
        return false;
    
    taz_StrLoan sl;
    tazE_borrowStr( eng, tazR_getValStr( key ), &sl );

    bool isIdent = ( isalpha( sl.str[0] ) || sl.str[0] == '_' );
    for( unsigned i = 0 ; i < sl.len ; i++ )
        isIdent = (isIdent && isalnum( sl.str[i] ));
    
    return isIdent;
}

static void fmtRec( tazE_Engine* eng, taz_Writer* w, tazR_Rec* rec, tazR_FmtFlags flags ) {
    w->write( w, '{' );

    struct {
        tazE_Bucket base;
        tazR_TVal   iIter;
        tazR_TVal   eIter;
    } buc;
    tazE_addBucket( eng, &buc, 2 );

    // Print the sequenced values first, these are the
    // values of fields keyed with 0...N where N is the
    // first undefined integral key.  We print these
    // with implicit keys for better readability and
    // since that's likely the way they were entered.
    tazR_RecIter* iIter = tazR_makeRecIter( eng, rec );
    buc.iIter = tazR_stateVal( (tazR_State*)iIter );

    unsigned index = 0;
    tazR_TVal iKey = tazR_intVal( index );
    tazR_TVal iVal = tazR_recGet( eng, rec, iKey );
    while( tazR_getValType( iVal ) != tazR_Type_UDF ) {

        if( index++ > 0 )
            fmtRaw( eng, w, ", " );
        else
            fmtRaw( eng, w, " " );
        
        fmtVal( eng, w, iVal, flags | tazR_FmtFlag_QUOTED );

        iKey = tazR_intVal( index );
        iVal = tazR_recGet( eng, rec, iKey );
    }

    // Now add the explicitly keyed fields, the keys themselves
    // will be formatted as dot-keys if their keys are valid
    // identifiers, otherwise as at-keys.
    tazR_RecIter* eIter = tazR_makeRecIter( eng, rec );
    buc.eIter = tazR_stateVal( (tazR_State*)eIter );

    bool empty = (index == 0);

    tazR_TVal eKey, eVal;
    while( tazR_recIterNext( eng, eIter, &eKey, &eVal ) ) {
        if( tazR_getValType( eKey ) == tazR_Type_INT && tazR_getValInt( eKey ) < index )
            continue;
        
        if( !empty ) {
            fmtRaw( eng, w, ", " );
        }
        else {
            empty = false;
            fmtRaw( eng, w, " " );
        }
        
        if( isIdentKey( eng, eKey ) ) {
            w->write( w, '.' );
            fmtVal( eng, w, eKey, flags & ~tazR_FmtFlag_QUOTED );
        }
        else {
            w->write( w, '@' );
            fmtVal( eng, w, eKey, flags | tazR_FmtFlag_QUOTED );
        }
        fmtRaw( eng, w, ": " );
        fmtVal( eng, w, eVal, flags | tazR_FmtFlag_QUOTED );
    }

    if( !empty )
        w->write( w, ' ' );
    w->write( w, '}' );
}

static void fmtFun( tazE_Engine* eng, taz_Writer* w, tazR_Fun* fun, tazR_FmtFlags flags ) {
    assert( false );
}

static void fmtVal( tazE_Engine* eng, taz_Writer* w, tazR_TVal val, tazR_FmtFlags flags ) {
    switch( tazR_getValType( val ) ) {
        case tazR_Type_UDF:
            fmtUdf( eng, w, flags );
        break;
        case tazR_Type_NIL:
            fmtNil( eng, w, flags );
        break;
        case tazR_Type_LOG:
            fmtLog( eng, w, tazR_getValLog( val ), flags );
        break;
        case tazR_Type_INT:
            fmtInt( eng, w, tazR_getValInt( val ), flags );
        break;
        case tazR_Type_DEC:
            fmtDec( eng, w, tazR_getValDec( val ), flags );
        break;
        case tazR_Type_STR:
            fmtStr( eng, w, tazR_getValStr( val ), flags );
        break;
        case tazR_Type_REC:
            fmtRec( eng, w, tazR_getValRec( val ), flags );
        break;
        case tazR_Type_FUN:
            fmtFun( eng, w, tazR_getValFun( val ), flags );
        break;
        default:
            w->write( w, '<' );
            w->write( w, '?' );
            w->write( w, '>' );
        break;
    }
}