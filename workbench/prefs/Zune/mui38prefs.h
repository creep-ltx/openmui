#ifndef ZUNE_MUI38PREFS_H
#define ZUNE_MUI38PREFS_H

/*
 * Reading MUI 3.8's settings files (ENVARC:MUI/«Global».prefs and the
 * per-application <BASE>.<n>.prefs files) into Zune's form. The format and
 * every item translated here were measured black-box: one setting changed
 * at a time in MUI 3.8's settings program, the saved file compared (see
 * "Reading MUI 3.8's settings files" in docs/lean/README.md).
 *
 * mui38_convert() is plain C with no AmigaOS calls, so the host test
 * (tests/check_mui38_import.py) compiles and runs it on the PC.
 */

/* Says whether the file of a picture or image class spec ("3:", "4:" or
 * "5:", name without the prefix) can be found; NULL: keep them all */
typedef int (*mui38_have_file)(const char *name);

/* Convert the file image in[0..inlen) into a Zune settings file image in
 * out (at most outmax bytes). Returns the output length, or -1 when the
 * input is not a FORM PREF or the output does not fit. outmax = inlen + 64
 * is always enough. */
long mui38_convert(const unsigned char *in, long inlen,
                   unsigned char *out, long outmax, mui38_have_file have);

#endif
