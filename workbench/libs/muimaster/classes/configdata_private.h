/* Configdata's default tables, as configdata.c and its setters
 * (configset.c, in the Zprefs module) both read them */
#ifndef _ZUNE_CONFIGDATA_PRIVATE_H
#define _ZUNE_CONFIGDATA_PRIVATE_H

#include <exec/types.h>

struct spec_cfg
{
    ULONG muiv;
    ULONG cfgid;
    CONST_STRPTR defspec;
};

struct def_ulval
{
    ULONG id;
    ULONG val;
    UWORD offset;
    UWORD size;
};

/* Every string setting: its id, where it goes in the prefs (Configdata_
 * StoreString: a string, or a key parsed into a ZuneKeySpec), and its
 * default. A table instead of a switch per id (2026-11-06: 0.8 KB less). */
struct def_strval
{
    UWORD id;
    UWORD field;            /* offsetof in struct ZunePrefsNew, flags */
    CONST_STRPTR val;
};

/* Each list ends with an entry of 0 / NULL */
struct ZuneCfgTables
{
    const struct spec_cfg *imspecs;
    const struct spec_cfg *framespecs;
    const struct def_ulval *ulvals;
    const struct def_strval *strvals;
};

#endif
