/* mupen64plus's core and plugins linked into one program (RISC OS builds are static).
 *
 * Stands in for every osal_dynlib_* (the core's, ui-console's and each plugin's, which
 * have the same names and signatures) and for ui-console's osal_library_search. A
 * "library" is a table of its exported functions (src/gen/static_tables.c); a handle
 * points at the table. Opening any path whose last part starts with a plugin's name
 * opens that plugin; anything else opens the core.
 *
 * Copyright (C) 2026 David Sharp. GPL-2.0, as mupen64plus. */
#include <stdlib.h>
#include <string.h>

#include "m64p_types.h"
#include "static_dynlib.h"

/* the core's handle: a library of its own, not a plugin */
static const struct static_library core_library = { "mupen64plus", M64PLUGIN_CORE, static_core_symbols };

static const char *last_part(const char *path)
{
    const char *p = strrchr(path, '/');
    return p ? p + 1 : path;
}

m64p_error osal_dynlib_open(m64p_dynlib_handle *pLibHandle, const char *pccLibraryPath)
{
    const struct static_library *lib;
    const char *name;

    if (pLibHandle == NULL || pccLibraryPath == NULL)
        return M64ERR_INPUT_ASSERT;
    name = last_part(pccLibraryPath);
    for (lib = static_plugins; lib->name != NULL; lib++)
    {
        if (strncmp(name, lib->name, strlen(lib->name)) == 0)
        {
            *pLibHandle = (m64p_dynlib_handle) lib;
            return M64ERR_SUCCESS;
        }
    }
    if (strstr(name, "mupen64plus") != NULL && strstr(name, "mupen64plus-") == NULL)
    {
        *pLibHandle = (m64p_dynlib_handle) &core_library;
        return M64ERR_SUCCESS;
    }
    return M64ERR_INPUT_NOT_FOUND;
}

void *osal_dynlib_getproc(m64p_dynlib_handle LibHandle, const char *pccProcedureName)
{
    const struct static_library *lib = (const struct static_library *) LibHandle;
    const struct static_symbol *s;

    if (lib == NULL || pccProcedureName == NULL)
        return NULL;
    for (s = lib->symbols; s->name != NULL; s++)
        if (strcmp(s->name, pccProcedureName) == 0)
            return s->fn;
    return NULL;
}

m64p_error osal_dynlib_close(m64p_dynlib_handle LibHandle)
{
    (void) LibHandle;
    return M64ERR_SUCCESS;
}

/* ui-console's plugin search (osal_files.h): every plugin linked in, wherever it looks */
#include "osal_files.h"

const int osal_libsearchdirs = 1;
const char *osal_libsearchpath[1] = { "" };

osal_lib_search *osal_library_search(const char *searchpath)
{
    osal_lib_search *head = NULL, **tail = &head;
    const struct static_library *lib;

    (void) searchpath;
    for (lib = static_plugins; lib->name != NULL; lib++)
    {
        osal_lib_search *e = calloc(1, sizeof *e);
        if (e == NULL)
            break;
        strncpy(e->filepath, lib->name, sizeof e->filepath - 1);
        e->filename = e->filepath;
        e->plugin_type = lib->type;
        *tail = e;
        tail = &e->next;
    }
    return head;
}

void osal_free_lib_list(osal_lib_search *head)
{
    while (head != NULL)
    {
        osal_lib_search *next = head->next;
        free(head);
        head = next;
    }
}
