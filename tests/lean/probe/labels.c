/* labels: the label objects of MUI_MakeObject (Label, Label1, Label2, FreeLabel1), plain Text
 * objects with the ImageButton and String frames (phantom and real), and a Coloradjust, dumped
 * with their minimum sizes (MUI 3.8's Coloradjust labels are 9 pixels high in topaz 8, OpenMUI's
 * were 11). The window stays open 60 s (a screenshot, a click on Coloradjust's popup).
 * Build: m68k-amigaos-gcc -noixemul -Os -s -I<build>/include labels.c -o labels -lamiga */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include "dumptree.h"

struct Library *MUIMasterBase;

int main(void)
{
    Object *app, *w1;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 11))) return 20;
    app = ApplicationObject, MUIA_Application_Base, "LABELS",
        SubWindow, w1 = WindowObject, MUIA_Window_Title, "labels", MUIA_Window_LeftEdge, 20, MUIA_Window_TopEdge, 30,
            WindowContents, VGroup,
                Child, ColGroup(2),
                    Child, Label("Label"), Child, TextObject, MUIA_Text_Contents, "plain Text", End,
                    Child, Label1("Label1"), Child, TextObject, MUIA_Frame, MUIV_Frame_ImageButton, MUIA_Text_Contents, "ImageButton frame", End,
                    Child, Label2("Label2"), Child, TextObject, MUIA_Frame, MUIV_Frame_String, MUIA_Text_Contents, "String frame", End,
                    Child, FreeLabel1("FreeLabel1"), Child, TextObject, MUIA_Frame, MUIV_Frame_ImageButton, MUIA_FramePhantomHoriz, TRUE, MUIA_Text_Contents, "ImageButton phantom", End,
                    Child, Label("x"), Child, TextObject, MUIA_Frame, MUIV_Frame_String, MUIA_FramePhantomHoriz, TRUE, MUIA_Text_Contents, "String phantom", End,
                    Child, Label("y"), Child, TextObject, MUIA_Frame, MUIV_Frame_Button, MUIA_FramePhantomHoriz, TRUE, MUIA_Text_Contents, "Button phantom", End,
                End,
                Child, ColoradjustObject, End,
            End, End,
        End;
    if (!app) return 20;
    set(w1, MUIA_Window_Open, TRUE);
    { Object *r; struct Window *w;
      get(w1, MUIA_Window_RootObject, &r); get(w1, MUIA_Window_Window, &w); printf("labels: window %dx%d\n", w ? w->Width : 0, w ? w->Height : 0); dumptree(r, 0); }
    fflush(stdout);
    Delay(3000);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
