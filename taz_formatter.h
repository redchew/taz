#ifndef taz_formatter_h
#define taz_formatter_h
#include "taz_common.h"

#include <stdarg.h>

void tazR_fmt( tazE_Engine* eng, taz_Writer* w, char const* pat, ... );
void tazR_vfmt( tazE_Engine* eng, taz_Writer* w, char const* pat, va_list ap );

#endif