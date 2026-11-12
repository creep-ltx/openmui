/* Automatically generated header (sfdc 1.12)! Do not edit! */

#ifndef _INLINE_UP_MCC_H
#define _INLINE_UP_MCC_H

#ifndef _SFDC_VARARG_DEFINED
#define _SFDC_VARARG_DEFINED
#ifdef __HAVE_IPTR_ATTR__
typedef APTR _sfdc_vararg __attribute__((iptr));
#else
typedef ULONG _sfdc_vararg;
#endif /* __HAVE_IPTR_ATTR__ */
#endif /* _SFDC_VARARG_DEFINED */

#ifndef __INLINE_MACROS_H
#include <inline/macros.h>
#endif /* !__INLINE_MACROS_H */

#ifndef UP_MCC_BASE_NAME
#define UP_MCC_BASE_NAME UPMCCBase
#endif /* !UP_MCC_BASE_NAME */

#define UP_ThemeLoad(___path) \
      LP1(0x1e, LONG, UP_ThemeLoad , STRPTR, ___path, a0,\
      , UP_MCC_BASE_NAME)

#define UP_ThemeReload() \
      LP0(0x24, LONG, UP_ThemeReload ,\
      , UP_MCC_BASE_NAME)

#define UP_ThemeSetTokens(___rgb, ___count) \
      LP2(0x2a, LONG, UP_ThemeSetTokens , const ULONG *, ___rgb, a0, ULONG, ___count, d0,\
      , UP_MCC_BASE_NAME)

#define UP_ThemeTokens() \
      LP0(0x30, const ULONG *, UP_ThemeTokens ,\
      , UP_MCC_BASE_NAME)

#define UP_IconFind(___name) \
      LP1(0x36, LONG, UP_IconFind , STRPTR, ___name, a0,\
      , UP_MCC_BASE_NAME)

#define UP_SetFontFile(___path) \
      LP1(0x3c, LONG, UP_SetFontFile , STRPTR, ___path, a0,\
      , UP_MCC_BASE_NAME)

#define UP_ThemeAttach(___key, ___obj, ___app, ___hook) \
      LP4(0x42, LONG, UP_ThemeAttach , APTR, ___key, a0, APTR, ___obj, a1, APTR, ___app, a2, APTR, ___hook, a3,\
      , UP_MCC_BASE_NAME)

#define UP_ThemeDetach(___key) \
      LP1(0x48, LONG, UP_ThemeDetach , APTR, ___key, a0,\
      , UP_MCC_BASE_NAME)

#define UP_ThemeObjects() \
      LP0(0x4e, ULONG, UP_ThemeObjects ,\
      , UP_MCC_BASE_NAME)

#endif /* !_INLINE_UP_MCC_H */
