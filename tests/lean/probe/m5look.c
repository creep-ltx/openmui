/* m5look [VERSION [TICKS]]: what a program asking for muimaster VERSION (default 20) gets as a
 * window's inner spacing (the root's box against the window's inner box) and, for a
 * window taller than the screen, where the scrollbars go (window border gadgets,
 * GACT_RIGHTBORDER/BOTTOMBORDER props, and the root object's class): MUI 5's look
 * for MUI 5 programs (Compatibility round 5).
 * Build: m68k-amigaos-gcc -noixemul -Os -s -I<build>/include m5look.c -o m5look -lamiga */
#include <libraries/mui.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <stdlib.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

static void show(Object *win, const char *what)
{
    struct Window *w = NULL;
    struct Gadget *g;
    Object *root = NULL;
    int props = 0, rb = 0, bb = 0;

    get(win, MUIA_Window_Window, &w);
    get(win, MUIA_Window_RootObject, &root);
    if (!w || !root) { printf("%s: not open\n", what); return; }
    for (g = w->FirstGadget; g; g = g->NextGadget)
    {
        if ((g->GadgetType & GTYP_GTYPEMASK) == GTYP_PROPGADGET) props++;
        if (g->Activation & GACT_RIGHTBORDER) rb++;
        if (g->Activation & GACT_BOTTOMBORDER) bb++;
    }
    printf("%s: window %dx%d borders %d,%d,%d,%d root %d,%d %dx%d (inner %d,%d %d,%d) props %d rightborder %d bottomborder %d\n",
        what, w->Width, w->Height, w->BorderLeft, w->BorderTop, w->BorderRight, w->BorderBottom,
        _left(root), _top(root), _width(root), _height(root),
        _left(root) - w->BorderLeft, _top(root) - w->BorderTop,
        w->Width - w->BorderRight - _right(root) - 1, w->Height - w->BorderBottom - _bottom(root) - 1,
        props, rb, bb);
}

int main(int argc, char **argv)
{
    Object *app, *w1, *w2;
    int ver = argc > 1 ? atoi(argv[1]) : 20;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", ver))) return 20;
    app = ApplicationObject, MUIA_Application_Title, (IPTR)"M5L", MUIA_Application_Base, (IPTR)"M5L",
        SubWindow, (IPTR)(w1 = WindowObject, MUIA_Window_Title, (IPTR)"small", MUIA_Window_ID, 0,
            WindowContents, (IPTR)(VGroup, Child, (IPTR)(RectangleObject, MUIA_FixWidth, 100, MUIA_FixHeight, 50, End), End), End),
        SubWindow, (IPTR)(w2 = WindowObject, MUIA_Window_Title, (IPTR)"tall", MUIA_Window_ID, 0,
            WindowContents, (IPTR)(VGroup, Child, (IPTR)(RectangleObject, MUIA_FixWidth, 200, MUIA_FixHeight, 600, End),
                Child, (IPTR)(RectangleObject, MUIA_FixWidth, 760, MUIA_FixHeight, 20, End), End), End),
        End;
    if (!app) { printf("no app\n"); return 20; }
    set(w1, MUIA_Window_Open, TRUE);
    show(w1, "small");
    set(w1, MUIA_Window_Open, FALSE);
    set(w2, MUIA_Window_Open, TRUE);
    show(w2, "tall");
    fflush(stdout);
    Delay(argc > 2 ? atoi(argv[2]) : 20);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
