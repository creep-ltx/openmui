/* fixed: a window whose root cannot grow (size gadget or not), dumped */
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
    Object *app, *win, *root;
    IPTR open = 0;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject, MUIA_Application_Title, (IPTR)"W0", MUIA_Application_Base, (IPTR)"W0",
        SubWindow, (IPTR)(win = WindowObject, MUIA_Window_Title, (IPTR)"W0",
            WindowContents, (IPTR)(root = VGroup,
                Child, (IPTR)(RectangleObject, MUIA_FixWidth, 100, MUIA_FixHeight, 50, End),
                End), End), End;
    if (!app) return 20;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (open) { struct Window *w = NULL; get(win, MUIA_Window_Window, &w); printf("window %dx%d\n", w->Width, w->Height); dumptree(root, 0); }
    Delay(400);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
