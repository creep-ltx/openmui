/* cfgset: Configdata's setters (MUIM_Configdata_SetULong ... SetString,
 * Zune's private methods, the Zprefs module since branch smaller2) on a
 * Configdata object of its own: each value set to something else and then
 * back to its default, printing what GetULong/GetString answer and whether
 * the dataspace holds the item. The output must be the same on every
 * library build. Built by hand:
 *   m68k-amigaos-gcc -m68000 -noixemul -Os -s -o cfgset tests/lean/cfgset.c
 */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>

#ifndef IPTR
#define IPTR ULONG
#endif

#ifndef MUIB_Configdata /* Zune's mui.h has them */
#define MUIB_Configdata 0x90420900
#define MUIM_Configdata_GetString (MUIB_Configdata | 0)
#define MUIM_Configdata_GetULong (MUIB_Configdata | 1)
#define MUIM_Configdata_SetULong (MUIB_Configdata | 2)
#define MUIM_Configdata_SetImspec (MUIB_Configdata | 3)
#define MUIM_Configdata_SetFramespec (MUIB_Configdata | 4)
#define MUIM_Configdata_SetFont (MUIB_Configdata | 5)
#define MUIM_Configdata_SetPenspec (MUIB_Configdata | 8)
#define MUIM_Configdata_SetString (MUIB_Configdata | 9)
#endif
#ifndef MUIM_Dataspace_Find
#define MUIM_Dataspace_Find 0x8042832c
#endif

#define CFG_Window_Spacing_Left 0x01
#define CFG_Font_Normal 0x1e
#define CFG_Frame_Button 0x2b
#define CFG_Background_Window 0x37
#define CFG_Keyboard_Press 0x5f
#define CFG_String_Text 0x64

struct Library *MUIMasterBase;
static Object *cfg;

static void show(CONST_STRPTR what, ULONG id, BOOL ul)
{
    IPTR found = DoMethod(cfg, MUIM_Dataspace_Find, id);

    if (ul)
        printf("%-16s ulong %lu stored %s\n", what,
            (ULONG)DoMethod(cfg, MUIM_Configdata_GetULong, id),
            found ? "yes" : "no");
    else
    {
        CONST_STRPTR s = (CONST_STRPTR)DoMethod(cfg,
            MUIM_Configdata_GetString, id);
        printf("%-16s \"%s\" stored %s\n", what, s ? (char *)s : "(null)",
            found ? "yes" : "no");
    }
}

#define CSET(m, id, v) DoMethod(cfg, m, id, (IPTR)(v))

int main(void)
{
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 0)))
        return 20;
    cfg = MUI_NewObject("Configdata.mui", TAG_DONE);
    if (!cfg)
    {
        printf("no Configdata\n");
        CloseLibrary(MUIMasterBase);
        return 20;
    }
    show("ulong default", CFG_Window_Spacing_Left, TRUE);
    CSET(MUIM_Configdata_SetULong, CFG_Window_Spacing_Left, 9);
    show("ulong 9", CFG_Window_Spacing_Left, TRUE);
    CSET(MUIM_Configdata_SetULong, CFG_Window_Spacing_Left, 4);
    show("ulong 4 (def)", CFG_Window_Spacing_Left, TRUE);

    CSET(MUIM_Configdata_SetImspec, CFG_Background_Window, "2:00000000,00000000,00000000");
    show("imspec rgb", CFG_Background_Window, FALSE);
    CSET(MUIM_Configdata_SetImspec, CFG_Background_Window, "6:bad");
    show("imspec 6: (bad)", CFG_Background_Window, FALSE);
    CSET(MUIM_Configdata_SetImspec, CFG_Background_Window, "0:128");
    show("imspec def", CFG_Background_Window, FALSE);

    CSET(MUIM_Configdata_SetFramespec, CFG_Frame_Button, "302211");
    show("frame 3", CFG_Frame_Button, FALSE);
    CSET(MUIM_Configdata_SetFramespec, CFG_Frame_Button, "");
    show("frame empty", CFG_Frame_Button, FALSE);
    CSET(MUIM_Configdata_SetFramespec, CFG_Frame_Button, "202211");
    show("frame def", CFG_Frame_Button, FALSE);

    CSET(MUIM_Configdata_SetPenspec, CFG_String_Text, "r00000000,00000000,00000000");
    show("pen rgb", CFG_String_Text, FALSE);
    CSET(MUIM_Configdata_SetPenspec, CFG_String_Text, "m5");
    show("pen def", CFG_String_Text, FALSE);

    CSET(MUIM_Configdata_SetFont, CFG_Font_Normal, "topaz/9");
    show("font topaz/9", CFG_Font_Normal, FALSE);
    CSET(MUIM_Configdata_SetFont, CFG_Font_Normal, "");
    show("font empty", CFG_Font_Normal, FALSE);

    CSET(MUIM_Configdata_SetString, CFG_Keyboard_Press, "-upstroke space");
    show("key space", CFG_Keyboard_Press, FALSE);
    CSET(MUIM_Configdata_SetString, CFG_Keyboard_Press, "-upstroke return");
    show("key def", CFG_Keyboard_Press, FALSE);

    MUI_DisposeObject(cfg);
    CloseLibrary(MUIMasterBase);
    printf("done\n");
    return 0;
}
