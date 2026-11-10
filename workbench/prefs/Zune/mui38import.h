#ifndef ZUNE_MUI38IMPORT_H
#define ZUNE_MUI38IMPORT_H

#include <exec/types.h>

/* The name of MUI 3.8's global settings file in ENVARC:MUI, if it has one */
BOOL mui38_global_file(char *buf, LONG size);

/* Convert one MUI 3.8 settings file into a Zune settings file */
BOOL mui38_convert_file(CONST_STRPTR from, CONST_STRPTR to);

/* Every program's own MUI 3.8 file that Zune has none of yet, converted
 * into ENVARC:zune and ENV:zune. Returns how many. */
LONG mui38_import_programs(void);

/* No Zune settings yet, and MUI 3.8 settings to import */
BOOL mui38_first_run(void);

#endif
