/* radio: a horizontal and a vertical Radio (entries "0".."6", then "a", "bbb",
 * "ccccc"), each object's sizes and boxes dumped (dumptree.h); compares MUI 3.8's
 * Radio layout with Zune's. */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include "dumptree.h"

struct Library *MUIMasterBase;
static const char *e1[] = { "0", "1", "2", "3", "4", "5", "6", NULL };
static const char *e2[] = { "a", "bbb", "ccccc", NULL };

int main(void)
{
    Object *app, *win, *root;
    IPTR open = 0;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject, MUIA_Application_Title, (IPTR)"RadioP", MUIA_Application_Base, (IPTR)"RADIOP",
        SubWindow, (IPTR)(win = WindowObject, MUIA_Window_Title, (IPTR)"RadioP",
            WindowContents, (IPTR)(root = VGroup,
                Child, (IPTR)(RadioObject, MUIA_Radio_Entries, (IPTR)e1, MUIA_Group_Horiz, TRUE, End),
                Child, (IPTR)(RadioObject, MUIA_Radio_Entries, (IPTR)e2, MUIA_Group_Horiz, TRUE, End),
                Child, (IPTR)(RadioObject, MUIA_Radio_Entries, (IPTR)e2, End),
                End), End), End;
    if (!app) return 20;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (open)
    {
        struct Window *w = NULL;
        get(win, MUIA_Window_Window, &w);
        printf("window %d x %d\n", w->Width, w->Height);
        dumptree(root, 0);
    }
    Delay(50);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
