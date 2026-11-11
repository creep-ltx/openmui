/* Configdata's setters (MUIM_Configdata_SetULong, SetImspec, SetFramespec,
 * SetPenspec, SetFont, SetString): only the prefs program and
 * MUIM_Application_SetConfigItem use them, so they are the module
 * Zprefs.mui (modules.h, classes/zprefs.c). A value equal to its default
 * is removed from the dataspace instead of stored (it would be redundant).
 * Configdata's dispatcher brings the prefs structure up to date after
 * SetString itself. */
#include <string.h>
#include <exec/types.h>
#include <intuition/classusr.h>
#include <clib/alib_protos.h>

#include "mui.h"
#include "muimaster_intern.h"
#include "modules.h"
#include "classes/configdata_private.h"

static BOOL is_default_spec(const struct spec_cfg *t, ULONG id,
    CONST_STRPTR val)
{
    for (; t->defspec; t++)
        if (t->cfgid == id && !strcmp(t->defspec, val))
            return TRUE;
    return FALSE;
}

static BOOL is_default_str(const struct def_strval *t, ULONG id,
    CONST_STRPTR val)
{
    for (; t->id; t++)
        if (t->id == id && !strcmp(t->val, val))
            return TRUE;
    return FALSE;
}

IPTR zune_configdata_set(Object *obj, Msg msg, const struct ZuneCfgTables *t)
{
    /* every setter's message: method, id, value */
    struct { STACKED ULONG MethodID; STACKED ULONG id; STACKED IPTR val; }
        *m = (APTR)msg;
    CONST_STRPTR val = (CONST_STRPTR)m->val;
    ULONG v;
    BOOL def = FALSE;

    switch (msg->MethodID)
    {
    case MUIM_Configdata_SetULong:
        {
            const struct def_ulval *u;

            for (u = t->ulvals; u->id; u++)
                if (u->id == m->id && u->val == m->val)
                    def = TRUE;
            if (!def)
            {
                v = AROS_LONG2BE(m->val);
                DoMethod(obj, MUIM_Dataspace_Add, (IPTR)&v, 4, m->id);
                return 0;
            }
        }
        break;

    case MUIM_Configdata_SetImspec:
        if (!val || !*val || *val == '6')
            return 0;
        def = is_default_spec(t->imspecs, m->id, val)
            || is_default_str(t->strvals, m->id, val);
        break;

    case MUIM_Configdata_SetFramespec:
        if (!val || !*val)
            return 0;
        def = is_default_spec(t->framespecs, m->id, val);
        break;

    case MUIM_Configdata_SetPenspec:
        if (!val || !*val)
            return 0;
        /* fall through */
    case MUIM_Configdata_SetString:
        def = is_default_str(t->strvals, m->id, val);
        break;

    case MUIM_Configdata_SetFont:
        def = !val || !*val;
        break;

    default:
        return 0;
    }
    if (def)
        DoMethod(obj, MUIM_Dataspace_Remove, m->id);
    else
        DoMethod(obj, MUIM_Dataspace_Add, (IPTR)val, strlen(val) + 1, m->id);
    return 0;
}
