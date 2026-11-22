/* mupen64plus's core and plugins linked into one program: dlopen's stand-in.
 * See tools/gen_static.py. */
#ifndef STATIC_DYNLIB_H
#define STATIC_DYNLIB_H

#include "m64p_types.h"

struct static_symbol {
    const char *name;
    void *fn;
};

struct static_library {
    const char *name;               /* what ui-console sees as the file name */
    m64p_plugin_type type;
    const struct static_symbol *symbols;
};

extern const struct static_symbol static_core_symbols[];
extern const struct static_library static_plugins[];

#endif
