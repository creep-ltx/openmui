#ifndef ZUNE68_NATIVE_VERSION_H
#define ZUNE68_NATIVE_VERSION_H

/* Native distribution identity, shared by both native library names.
 * The library reports the version of MUI 5, because applications gate
 * their current code paths on it. That is not a claim of complete MUI 5
 * compatibility, and the number is not bumped for releases. A release is
 * identified by its edition (ZUNE68_EDITION) and its build date
 * (ZUNE68_LIBRARY_DATE, made by the build in $(BUILD)/build-date.h: the
 * day of the build, or ZUNE68_DATE=DD.MM.YYYY given to make), which
 * tools/release.py reads from the $VER string to name the archive and
 * fill the readmes.
 * MUI5_TEST=0 builds the AROS source baseline identity (muimaster.conf).
 * External classes retain their own MUIA_Version/MUIA_Revision handlers.
 */
#if ZUNE68_MUI5_TEST
#define ZUNE68_LIBRARY_VERSION 21
#define ZUNE68_LIBRARY_REVISION 227
#else
#define ZUNE68_LIBRARY_VERSION 19
#define ZUNE68_LIBRARY_REVISION 81
#endif
/* Lean edition: what a program behaving as MUI 3.8 or MUI 4 sees
 * (identity.c; the base in the library list reports MUI 5 above, so
 * that exec lets every program in). MUI 3.8's muimaster.library is
 * 19.35. For MUI 4, 20.6000 is above every MUI 4 revision the open
 * class sources test on AmigaOS 3/4 (NList 20.2326, TheBar 20.5341,
 * TextEditor and BetterString 20.5640); it is not a real MUI 4 number. */
#define ZUNE68_MUI38_VERSION 19
#define ZUNE68_MUI38_REVISION 35
#define ZUNE68_MUI4_VERSION 20
#define ZUNE68_MUI4_REVISION 6000
/* The edition after the date in the $VER strings: OpenMUI, the lean
 * edition of Zune68, with its beta number (X.YbN until it is daily driven).
 * It must contain "OpenMUI": OpenMUI prefs, OpenMUISwitch and the tests
 * look for it in the library's id string to tell it from a real MUI
 * (earlier editions said "Zune68k lean"; OpenMUISwitch knows those too).
 * The file name muimaster.library (the one library: zunemaster.library,
 * the same library under a second name, is gone), the version numbers
 * above and LIBS:Zune, ENV:Zune, ENVARC:Zune stay as they are. */
#define ZUNE68_EDITION_NUMBER "0.1b1"
#define ZUNE68_EDITION "OpenMUI " ZUNE68_EDITION_NUMBER
#define ZUNE68_STRINGIFY_(x) #x
#define ZUNE68_STRINGIFY(x) ZUNE68_STRINGIFY_(x)
#define ZUNE68_LIBRARY_VERSION_STRING \
    ZUNE68_STRINGIFY(ZUNE68_LIBRARY_VERSION) "." \
    ZUNE68_STRINGIFY(ZUNE68_LIBRARY_REVISION)

/* Do not raise these to bypass an application's unsupported-feature check. */
#if ZUNE68_MUI5_TEST
#define ZUNE68_BUILTIN_VERSION 21
#define ZUNE68_BUILTIN_REVISION 227
#else
#define ZUNE68_BUILTIN_VERSION 1
#define ZUNE68_BUILTIN_REVISION 1
#endif

#endif
