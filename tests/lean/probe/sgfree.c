/* sgfree [autobars]: a Scrollgroup with MUIA_Scrollgroup_FreeHoriz FALSE around a
 * virtual group holding a 300-pixel-wide, 600-pixel-tall column (TheBar.mcp's
 * settings page does this for MUI 3); prints the scrollgroup's and the window's
 * sizes. "autobars" also passes MUIA_Scrollgroup_AutoBars TRUE (a MUI 4 tag). */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <string.h>

#ifndef MUIA_Scrollgroup_AutoBars
#define MUIA_Scrollgroup_AutoBars 0x8042f50e
#endif

struct Library *MUIMasterBase;

int main(int argc, char **argv)
{
    Object *app, *win, *sg, *col;
    IPTR open = 0;
    int i, autobars = argc > 1 && !strcmp(argv[1], "autobars");

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    col = VGroup, End;
    for (i = 0; i < 40; i++)
        DoMethod(col, OM_ADDMEMBER, (IPTR)(RectangleObject, MUIA_FixWidth, 300, MUIA_FixHeight, 13, End));
    app = ApplicationObject, MUIA_Application_Title, (IPTR)"SGFree", MUIA_Application_Base, (IPTR)"SGFREE",
        SubWindow, (IPTR)(win = WindowObject, MUIA_Window_Title, (IPTR)"SGFree",
            WindowContents, (IPTR)(VGroup,
                Child, (IPTR)(sg = ScrollgroupObject,
                    autobars ? MUIA_Scrollgroup_AutoBars : TAG_IGNORE, TRUE,
                    MUIA_Scrollgroup_FreeHoriz, FALSE,
                    MUIA_Scrollgroup_Contents, (IPTR)(VGroupV, Child, (IPTR)col, End),
                    End),
                End), End), End;
    if (!app) return 20;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (open)
    {
        struct Window *w = NULL;
        get(win, MUIA_Window_Window, &w);
        printf("autobars %d: scrollgroup min %d x %d def %d x %d max %d x %d, now %d x %d; column min %d x %d; window %d x %d\n",
            autobars, _minwidth(sg), _minheight(sg), _defwidth(sg), _defheight(sg),
            _maxwidth(sg), _maxheight(sg), _width(sg), _height(sg),
            _minwidth(col), _minheight(col), w->Width, w->Height);
    }
    Delay(100);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
