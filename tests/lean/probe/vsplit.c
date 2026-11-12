/* vsplit HEIGHT CHILD...: a vertical group (spacing 0) of Rectangles in a window of the
 * given inner HEIGHT; each CHILD is min[:max[:weight]] (max 0 = none, weight 100 by
 * default). Prints the height each child got: MUI 3.8's way of sharing the space among
 * weighted children when some are clamped, against OpenMUI's (tests/lean/probe/bars.c
 * found 31/32/32 against 32/32/31 with a child clamped at its minimum).
 * Build: m68k-amigaos-gcc -noixemul -Os -s -I<build>/include vsplit.c -o vsplit -lamiga */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct Library *MUIMasterBase;

int main(int argc, char **argv)
{
    Object *app, *w1, *grp, *kids[20];
    int n = 0, i;
    LONG height = argc > 1 ? atoi(argv[1]) : 100;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    grp = VGroup, MUIA_Group_Spacing, 0, End;
    if (!grp) return 20;
    for (i = 2; i < argc && n < 20; i++)
    {
        LONG min = 0, max = 0, weight = 100;
        char *s = argv[i], *p;
        min = atoi(s);
        if ((p = strchr(s, ':'))) { max = atoi(p + 1); if ((p = strchr(p + 1, ':'))) weight = atoi(p + 1); }
        /* a fixed child (max = min): one Rectangle of that height; else a group with a
         * fixed spacer of the minimum and a free spacer (minimum min, no maximum) */
        if (max && max == min)
            kids[n] = RectangleObject, MUIA_FixHeight, min, MUIA_Weight, weight, End;
        else
            kids[n] = VGroup, MUIA_Group_Spacing, 0, MUIA_Weight, weight,
                Child, RectangleObject, MUIA_FixHeight, min, End,
                Child, RectangleObject, End,
                End;
        if (!kids[n]) return 20;
        DoMethod(grp, OM_ADDMEMBER, (IPTR)kids[n]);
        n++;
    }
    app = ApplicationObject, MUIA_Application_Base, "VSPLIT",
        SubWindow, w1 = WindowObject, MUIA_Window_Title, "vsplit", MUIA_Window_Width, 60,
            MUIA_Window_Height, height, MUIA_Window_LeftEdge, 20, MUIA_Window_TopEdge, 20,
            WindowContents, grp, End,
        End;
    if (!app) return 20;
    set(w1, MUIA_Window_Open, TRUE);
    printf("vsplit %ld (library %d.%d): group %d high:", height, MUIMasterBase->lib_Version, MUIMasterBase->lib_Revision, _height(grp));
    for (i = 0; i < n; i++) printf(" %d", _height(kids[i]));
    printf("\n");
    fflush(stdout);
    Delay(10);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
