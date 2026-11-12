/* Automatically generated header (sfdc 1.12)! Do not edit! */

#ifndef PROTO_UP_MCC_H
#define PROTO_UP_MCC_H

#include <clib/up_mcc_protos.h>

#if defined(_CONST_BASES)
# ifndef __CONSTLIBBASEDECL__
# define __CONSTLIBBASEDECL__ const
# endif /* __CONSTLIBBASEDECL__ */
# ifndef __SEGMENTLIBBASEDECL__
# define __SEGMENTLIBBASEDECL__  __attribute__((__section__(".data")))
# endif /* __SEGMENTLIBBASEDECL__ */
#endif /* _CONST_BASES */
#ifdef __amigaos4__
# include <interfaces/up_mcc.h>
# ifndef __NOGLOBALIFACE__
   extern struct UPMCCIFace *IUPMCC;
# endif /* __NOGLOBALIFACE__*/
#endif /* !__amigaos4__ */
#ifndef __NOLIBBASE__
  extern struct Library *
# ifdef __CONSTLIBBASEDECL__
   __CONSTLIBBASEDECL__
# endif /* __CONSTLIBBASEDECL__ */
  UPMCCBase
# ifdef __SEGMENTLIBBASEDECL__
 __SEGMENTLIBBASEDECL__
# endif /* __SEGMENTLIBBASEDECL__ */
;
#endif /* !__NOLIBBASE__ */

#ifndef _NO_INLINE
# if defined(__GNUC__)
#  ifdef __AROS__
#   include <defines/up_mcc.h>
#  else
#   include <inline/up_mcc.h>
#  endif
# else
#  include <pragmas/up_mcc_pragmas.h>
# endif
#endif /* _NO_INLINE */

#endif /* !PROTO_UP_MCC_H */
