/* Host driver for workbench/prefs/Zune/mui38prefs.c (tests/check_mui38_import.py):
 * mui38conv IN OUT [NOFILES] converts a MUI 3.8 settings file into Zune's form;
 * with a third argument no picture file is found (they are left out). */
#include <stdio.h>
#include <stdlib.h>
#include "mui38prefs.h"

static int no_file(const char *name)
{
    (void)name;
    return 0;
}

int main(int argc, char **argv)
{
    FILE *f;
    unsigned char *in, *out;
    long n, m;

    if (argc < 3 || !(f = fopen(argv[1], "rb")))
        return 2;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    in = malloc(n + 1);
    out = malloc(n + 64);
    if (!in || !out || fread(in, 1, n, f) != (size_t)n)
        return 2;
    fclose(f);
    m = mui38_convert(in, n, out, n + 64, argc > 3 ? no_file : 0);
    if (m < 0)
        return 1;
    if (!(f = fopen(argv[2], "wb")) || fwrite(out, 1, m, f) != (size_t)m)
        return 2;
    fclose(f);
    return 0;
}
