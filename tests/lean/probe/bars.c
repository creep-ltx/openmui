/* bars: Rectangle bar titles (HBar/VBar with and without a title), a bare
 * Floattext (List's own scroller), a Coloradjust and a Busy.mcc, dumped
 * with their minimum and default sizes. Run on MUI 3.8, MUI 5 and OpenMUI
 * side by side: round 3 gave every program MUI 3.8's bar-title geometry
 * (12 wider, 1 taller than the text); this probe says what MUI 5 does.
 * Opens the library with the version in argv[1] (default 19 = MUI 3.8).
 * Build: m68k-amigaos-gcc -noixemul -Os -s -I<build>/include bars.c -o bars -lamiga */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdlib.h>
#include "dumptree.h"

struct Library *MUIMasterBase;

int main(int argc, char **argv)
{
    Object *app, *w1, *busy;
    ULONG ver = argc > 1 ? atoi(argv[1]) : 19;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", ver))) return 20;
    busy = MUI_NewObject("Busy.mcc", TAG_DONE);
    app = ApplicationObject, MUIA_Application_Base, "BARS",
        SubWindow, w1 = WindowObject, MUIA_Window_Title, "bars", MUIA_Window_LeftEdge, 20, MUIA_Window_TopEdge, 30,
            WindowContents, VGroup,
                Child, HBar(TRUE),
                Child, RectangleObject, MUIA_Rectangle_HBar, TRUE, MUIA_Rectangle_BarTitle, "Speed: 20", End,
                Child, RectangleObject, MUIA_Rectangle_HBar, TRUE, MUIA_Rectangle_BarTitle, "Title", MUIA_Weight, 0, End,
                Child, HGroup,
                    Child, VBar(TRUE),
                    Child, TextObject, MUIA_Text_Contents, "plain Text", End,
                    Child, VBar(TRUE),
                End,
                Child, FloattextObject, MUIA_Frame, MUIV_Frame_InputList, MUIA_Floattext_Text, "a\nb\nc", End,
                Child, busy ? busy : HVSpace,
                Child, HGroup, MUIA_Group_Spacing, 0,
                    Child, TextObject, MUIA_Font, MUIV_Font_Normal, MUIA_Text_Contents, "N", End,
                    Child, TextObject, MUIA_Font, MUIV_Font_Tiny, MUIA_Text_Contents, "Ti", End,
                    Child, TextObject, MUIA_Font, MUIV_Font_Title, MUIA_Text_Contents, "Tt", End,
                    Child, TextObject, MUIA_Font, MUIV_Font_Big, MUIA_Text_Contents, "B", End,
                    Child, TextObject, MUIA_Font, MUIV_Font_Fixed, MUIA_Text_Contents, "F", End,
                    Child, TextObject, MUIA_Font, MUIV_Font_List, MUIA_Text_Contents, "L", End,
                    Child, TextObject, MUIA_Font, MUIV_Font_Button, MUIA_Text_Contents, "Bu", End,
                    Child, TextObject, MUIA_Font, MUIV_Font_Knob, MUIA_Text_Contents, "K", End,
                End,
            End, End,
        End;
    if (!app) return 20;
    set(w1, MUIA_Window_Open, TRUE);
    { Object *r; struct Window *w;
      get(w1, MUIA_Window_RootObject, &r); get(w1, MUIA_Window_Window, &w);
      printf("bars (version %ld, library %d.%d, identity byte %d): window %dx%d\n", ver, MUIMasterBase->lib_Version, MUIMasterBase->lib_Revision, ((UBYTE *)&muiGlobalInfo(r)->priv0)[0], w ? w->Width : 0, w ? w->Height : 0);
      dumptree(r, 0); }
    fflush(stdout);
    Delay(argc > 2 ? atoi(argv[2]) : 25);   /* argv[2]: ticks to stay open */
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
