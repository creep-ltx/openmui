#ifndef ZUNE_MUI38IMPORT_H
#define ZUNE_MUI38IMPORT_H

#include <exec/types.h>

/* The name of MUI 3.8's global settings file in ENVARC:MUI, if it has one */
BOOL mui38_global_file(char *buf, LONG size);

/* Convert one MUI 3.8 settings file into a Zune settings file */
BOOL mui38_convert_file(CONST_STRPTR from, CONST_STRPTR to);

/* Every program's own MUI 3.8 file that OpenMUI has none of yet, converted
 * into ENVARC:OpenMUI and ENV:OpenMUI. Returns how many. */
LONG mui38_import_programs(void);

/* No OpenMUI settings yet, and MUI 3.8 settings to import */
BOOL mui38_first_run(void);

/* An earlier edition's settings (ENVARC:Zune, ENV:Zune), copied into
 * ENVARC:OpenMUI and ENV:OpenMUI when the new drawer is missing or empty
 * (or has no global.prefs while the old one has): once, the first time
 * OpenMUI prefs runs. Files already in the new drawer are not replaced;
 * the old files stay. Returns how many files were copied. */
LONG openmui_import_old(void);

#endif
