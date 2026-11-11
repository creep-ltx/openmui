/* rootframe: a window whose root group has a group frame (TinyMeterPrefs' root), and
 * one whose root has none; dumped (the root's "add" is the frame plus the window's
 * inner spacing on MUI 3.8). Build: m68k-amigaos-gcc -noixemul -Os -s -I<build>/include
 * rootframe.c -o rootframe -lamiga */
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
    Object *app, *w1, *w2;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 11))) return 20;
    app = ApplicationObject, MUIA_Application_Base, "ROOTFRAME",
        SubWindow, w1 = WindowObject, MUIA_Window_Title, "framed root", MUIA_Window_LeftEdge, 20, MUIA_Window_TopEdge, 30,
            WindowContents, VGroup, MUIA_Frame, MUIV_Frame_Group,
                Child, TextObject, MUIA_Text_Contents, "framed root group", End,
                Child, SimpleButton("Button"),
            End, End,
        SubWindow, w2 = WindowObject, MUIA_Window_Title, "plain root", MUIA_Window_LeftEdge, 320, MUIA_Window_TopEdge, 30,
            WindowContents, VGroup,
                Child, TextObject, MUIA_Text_Contents, "plain root group", End,
                Child, SimpleButton("Button"),
            End, End,
        End;
    if (!app) return 20;
    set(w1, MUIA_Window_Open, TRUE);
    set(w2, MUIA_Window_Open, TRUE);
    { Object *r; struct Window *w;
      get(w1, MUIA_Window_RootObject, &r); get(w1, MUIA_Window_Window, &w); printf("framed root: window %dx%d\n", w ? w->Width : 0, w ? w->Height : 0); dumptree(r, 0);
      get(w2, MUIA_Window_RootObject, &r); get(w2, MUIA_Window_Window, &w); printf("plain root: window %dx%d\n", w ? w->Width : 0, w ? w->Height : 0); dumptree(r, 0); }
    Delay(100);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
