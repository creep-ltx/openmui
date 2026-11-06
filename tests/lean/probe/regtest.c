/* regtest [W H MODE]: a Register with two pages (long titles), dumped */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdlib.h>
#include "dumptree.h"

struct Library *MUIMasterBase;
static const char *titles[] = { "Open on startup settings", "More settings here", NULL };

int main(int argc, char **argv)
{
    Object *app, *win, *root;
    struct Screen *scr = NULL;
    IPTR open = 0;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    if (argc > 2) scr = dt_screen(atoi(argv[1]), atoi(argv[2]), argc > 3 ? strtoul(argv[3], NULL, 16) : 0x8000, NULL, 0);
    app = ApplicationObject, MUIA_Application_Title, (IPTR)"RegTest", MUIA_Application_Base, (IPTR)"REGTEST",
        SubWindow, (IPTR)(win = WindowObject, MUIA_Window_Title, (IPTR)"RegTest",
            scr ? MUIA_Window_Screen : TAG_IGNORE, (IPTR)scr,
            WindowContents, (IPTR)(root = VGroup,
                Child, (IPTR)(RegisterGroup(titles),
                    Child, (IPTR)(VGroup, Child, (IPTR)SimpleButton("Page one"), End),
                    Child, (IPTR)(VGroup, Child, (IPTR)SimpleButton("Two"), Child, (IPTR)SimpleButton("Two b"), End),
                    End),
                End), End), End;
    if (!app) return 20;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (open) { struct Window *w = NULL; get(win, MUIA_Window_Window, &w); printf("window %dx%d\n", w->Width, w->Height); dumptree(root, 0); }
    Delay(argc > 4 ? atoi(argv[4]) : 50);
    MUI_DisposeObject(app);
    if (scr) CloseScreen(scr);
    CloseLibrary(MUIMasterBase);
    return 0;
}
