/* dsorder: a Dataspace read from a settings file, changed, written back.
 *
 * Reads the MUIC chunk of FILE (a global.prefs: FORM PREF, PRHD, MUIC)
 * into a Dataspace with MUIM_Dataspace_ReadIFF, then replaces, removes and
 * adds records (MUIM_Dataspace_Add/Remove), reads the file's chunk over it
 * a second time, and writes the result with MUIM_Dataspace_WriteIFF to
 * OUT. Prints every record found by MUIM_Dataspace_Find for ids 1..0x800
 * as "id len checksum" and the count. The same output and OUT on two
 * libraries means the same records in the same order (written order is
 * the list order). Public API only; runs on MUI 3.8 too.
 *
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -Wno-pointer-sign \
 *   -I$I/include -Iworkbench/libs/muimaster/include tests/lean/dsorder.c -o dsorder -lamiga
 * Run: dsorder FILE OUT
 */
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <libraries/iffparse.h>
#include <prefs/prefhdr.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/iffparse.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase, *IFFParseBase;

static Object *new_object(CONST_STRPTR name, ...)
{
    return MUI_NewObjectA((STRPTR)name, (struct TagItem *)(&name + 1));
}

#define ID_MUIC MAKE_ID('M','U','I','C')

static BOOL readfile(Object *ds, CONST_STRPTR name)
{
    struct IFFHandle *iff = AllocIFF();
    BOOL ok = FALSE;

    if (!iff)
        return FALSE;
    if ((iff->iff_Stream = (ULONG)Open((STRPTR)name, MODE_OLDFILE)))
    {
        InitIFFasDOS(iff);
        if (!OpenIFF(iff, IFFF_READ))
        {
            StopChunk(iff, ID_PREF, ID_MUIC);
            while (!ParseIFF(iff, IFFPARSE_SCAN))
                if (CurrentChunk(iff)->cn_ID == ID_MUIC)
                {
                    DoMethod(ds, MUIM_Dataspace_ReadIFF, (ULONG)iff);
                    ok = TRUE;
                }
            CloseIFF(iff);
        }
        Close((BPTR)iff->iff_Stream);
    }
    FreeIFF(iff);
    return ok;
}

static void writefile(Object *ds, CONST_STRPTR name)
{
    struct IFFHandle *iff = AllocIFF();

    if (!iff)
        return;
    if ((iff->iff_Stream = (ULONG)Open((STRPTR)name, MODE_NEWFILE)))
    {
        InitIFFasDOS(iff);
        if (!OpenIFF(iff, IFFF_WRITE))
        {
            if (!PushChunk(iff, ID_PREF, ID_FORM, IFFSIZE_UNKNOWN))
            {
                DoMethod(ds, MUIM_Dataspace_WriteIFF, (ULONG)iff, ID_PREF,
                    ID_MUIC);
                PopChunk(iff);
            }
            CloseIFF(iff);
        }
        Close((BPTR)iff->iff_Stream);
    }
    FreeIFF(iff);
}

int main(int argc, char **argv)
{
    Object *ds;
    ULONG id, n = 0;
    static const char val[] = "a replaced value";

    if (argc < 3)
        return 20;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    if (!(IFFParseBase = OpenLibrary("iffparse.library", 37)))
        return 20;
    ds = new_object(MUIC_Dataspace, TAG_DONE);
    if (!ds)
        return 20;
    if (!readfile(ds, argv[1]))
        printf("no MUIC chunk\n");
    for (id = 1; id <= 0x800; id += 7)
        DoMethod(ds, MUIM_Dataspace_Add, (ULONG)val, sizeof(val), id);
    for (id = 3; id <= 0x800; id += 11)
        DoMethod(ds, MUIM_Dataspace_Remove, id);
    readfile(ds, argv[1]);
    for (id = 2; id <= 0x800; id += 13)
        DoMethod(ds, MUIM_Dataspace_Remove, id);
    DoMethod(ds, MUIM_Dataspace_Add, (ULONG)val, sizeof(val), 0x7777);
    for (id = 1; id <= 0x800; id++)
    {
        UBYTE *p = (UBYTE *)DoMethod(ds, MUIM_Dataspace_Find, id);
        ULONG sum = 0, i;

        if (!p)
            continue;
        for (i = 0; i < 8 && p[i]; i++)
            sum = sum * 31 + p[i];
        printf("%lx %lx\n", (unsigned long)id, (unsigned long)sum);
        n++;
    }
    printf("found %lu\n", (unsigned long)n);
    writefile(ds, argv[2]);
    DoMethod(ds, MUIM_Dataspace_Clear);
    printf("after clear %s\n",
        DoMethod(ds, MUIM_Dataspace_Find, 0x7777) ? "found" : "none");
    readfile(ds, argv[1]);
    MUI_DisposeObject(ds);
    CloseLibrary(IFFParseBase);
    CloseLibrary(MUIMasterBase);
    return 0;
}
