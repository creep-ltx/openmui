/* Automatically generated header (sfdc 1.12)! Do not edit! */

#ifndef CLIB_UP_MCC_PROTOS_H
#define CLIB_UP_MCC_PROTOS_H

/*
**   $VER: up_mcc_protos.h 1.0 $Id: up-mcc_lib.sfd 1.0 (07.11.2026) $
**
**   C prototypes. For use with 32 bit integers only.
**
**   Copyright (c) 2001 Amiga, Inc.
**       All Rights Reserved
*/

#include <exec/types.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* UP- MUI class package support library (design 2026-11-06-mui-mcc-package.md section 8.4). Table order = the ADDTABL order in lib/support_libheader.c. */
LONG UP_ThemeLoad(STRPTR path);
LONG UP_ThemeReload(void);
LONG UP_ThemeSetTokens(const ULONG * rgb, ULONG count);
const ULONG * UP_ThemeTokens(void);
LONG UP_IconFind(STRPTR name);
LONG UP_SetFontFile(STRPTR path);
LONG UP_ThemeAttach(APTR key, APTR obj, APTR app, APTR hook);
LONG UP_ThemeDetach(APTR key);
ULONG UP_ThemeObjects(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CLIB_UP_MCC_PROTOS_H */
