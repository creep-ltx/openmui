/* grow N [page]: a window that grows past the screen after it opened. It opens with one
 * button; then N "Button" buttons are added to its column (MUIM_Group_InitChange,
 * OM_ADDMEMBER, MUIM_Group_ExitChange), or with "page" a page of N buttons is added to a
 * page group and shown. Prints the window, the screen, the root's minimum and the
 * buttons' font before and after, and where the last button ends. */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Library *MUIMasterBase;

static void report(const char *when, Object *win, Object *root, Object *probe)
{
    struct Window *w = NULL;
    IPTR open = 0;
    get(win, MUIA_Window_Open, &open);
    get(win, MUIA_Window_Window, &w);
    if (!open || !w) { printf("%s: window closed\n", when); return; }
    printf("%s: window %p at %d,%d size %d x %d on screen %d x %d; root min %d x %d now %d x %d",
        when, (void *)w, w->LeftEdge, w->TopEdge, w->Width, w->Height,
        w->WScreen->Width, w->WScreen->Height,
        _minwidth(root), _minheight(root), _width(root), _height(root));
    if (probe && _font(probe))
        printf("; last button font %s %d, bottom at window y %d (inner bottom %d)",
            _font(probe)->tf_Message.mn_Node.ln_Name ? _font(probe)->tf_Message.mn_Node.ln_Name : "?",
            _font(probe)->tf_YSize, _bottom(probe), w->Height - w->BorderBottom - 1);
    printf("\n");
    fflush(stdout);
}

int main(int argc, char **argv)
{
    Object *app, *win, *root, *col, *first, *last = NULL, *pages = NULL, *page = NULL;
    int i, n = argc > 1 ? atoi(argv[1]) : 30;
    int pagemode = argc > 2 && !strcmp(argv[2], "page");
    ULONG sigs = 0;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject, MUIA_Application_Title, (IPTR)"Grow", MUIA_Application_Base, (IPTR)"GROW",
        SubWindow, (IPTR)(win = WindowObject, MUIA_Window_Title, (IPTR)"Grow", MUIA_Window_ID, 0,
            WindowContents, (IPTR)(root = VGroup,
                Child, (IPTR)(first = SimpleButton("First")),
                Child, (IPTR)(col = VGroup, End),
                Child, (IPTR)(pages = PageGroup, Child, (IPTR)HVSpace, End),
                End), End), End;
    if (!app) return 20;
    set(win, MUIA_Window_Open, TRUE);
    Delay(50);
    report("before", win, root, first);

    if (pagemode)
    {
        page = VGroup, End;
        for (i = 0; i < n; i++)
            DoMethod(page, OM_ADDMEMBER, (IPTR)(last = SimpleButton("Button")));
        DoMethod(pages, MUIM_Group_InitChange);
        DoMethod(pages, OM_ADDMEMBER, (IPTR)page);
        DoMethod(pages, MUIM_Group_ExitChange);
        set(pages, MUIA_Group_ActivePage, 1);
    }
    else
    {
        DoMethod(col, MUIM_Group_InitChange);
        for (i = 0; i < n; i++)
            DoMethod(col, OM_ADDMEMBER, (IPTR)(last = SimpleButton("Button")));
        DoMethod(col, MUIM_Group_ExitChange);
    }
    /* let the application handle what the change caused; report every change of the
     * Intuition window for 4 s, then keep it on screen for a screenshot */
    {
        struct Window *prev = (struct Window *)-1, *w;
        IPTR open;
        for (i = 0; i < 80; i++)
        {
            DoMethod(app, MUIM_Application_NewInput, (IPTR)&sigs);
            w = NULL; open = 0;
            get(win, MUIA_Window_Open, &open);
            get(win, MUIA_Window_Window, &w);
            if (!open) w = NULL;
            if (w != prev || (w && i == 79))
            {
                char when[24];
                sprintf(when, "after %d ms", i * 40);
                report(when, win, root, last);
                prev = w;
            }
            Delay(2);
        }
        printf("ready\n");
        fflush(stdout);
        Delay(500);
    }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
