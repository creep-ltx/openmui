/* idprobe VERSION [LETTERS]: open muimaster.library with VERSION, make an Application (with,
 * by the letters: b a Busy.mcc object, w the window opened, d dumptree's 32 MUI_GetClass calls,
 * l a Floattext, r a titled bar, f a tiny-font text; "both" = bw) and print the
 * library it got and the identity byte the application's global info carries; run in a
 * sequence of versions it shows whether a program gets the identity it asked for after
 * other programs ran (the opener records of identity.c).
 * Build: m68k-amigaos-gcc -noixemul -Os -s -I<build>/include idprobe.c -o idprobe -lamiga */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "dumptree.h"

struct Library *MUIMasterBase;

int main(int argc, char **argv)
{
    Object *app, *w1, *root, *busy = NULL;
    ULONG ver = argc > 1 ? atoi(argv[1]) : 19;
    const char *mode = argc > 2 ? argv[2] : "";
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", ver))) return 20;
    if (!strcmp(mode, "both")) mode = "bw";
    if (strchr(mode, 'b'))
        busy = MUI_NewObject("Busy.mcc", TAG_DONE);
    app = ApplicationObject, MUIA_Application_Base, "IDPROBE",
        SubWindow, w1 = WindowObject, MUIA_Window_Title, "idprobe",
            WindowContents, root = VGroup,
                Child, TextObject, MUIA_Text_Contents, "idprobe", End,
                Child, busy ? busy : HVSpace,
                Child, strchr(mode, 'l') ? FloattextObject, MUIA_Frame, MUIV_Frame_InputList, MUIA_Floattext_Text, "a\nb", End : HVSpace,
                Child, strchr(mode, 'r') ? RectangleObject, MUIA_Rectangle_HBar, TRUE, MUIA_Rectangle_BarTitle, "Title", End : HVSpace,
                Child, strchr(mode, 'f') ? TextObject, MUIA_Font, MUIV_Font_Tiny, MUIA_Text_Contents, "Ti", End : HVSpace,
            End, End,
        End;
    if (!app) { printf("idprobe %ld %s: no application (library %d.%d)\n", ver, mode, MUIMasterBase->lib_Version, MUIMasterBase->lib_Revision); CloseLibrary(MUIMasterBase); return 20; }
    if (strchr(mode, 'w'))
    {
        set(w1, MUIA_Window_Open, TRUE);
        Delay(strchr(mode, 's') ? 250 : 25);   /* s: stay 5 s, so the next program overlaps */
    }
    if (strchr(mode, 'd'))
        dumptree(root, 0);   /* MUI_GetClass of 32 class names, never freed, as the dump probes do */
    printf("idprobe %ld %s: library %d.%d, identity byte %d\n", ver, mode,
        MUIMasterBase->lib_Version, MUIMasterBase->lib_Revision, ((UBYTE *)&muiGlobalInfo(root)->priv0)[0]);
    fflush(stdout);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
