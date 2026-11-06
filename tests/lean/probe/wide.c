/* wide N: a window with N buttons in one row; prints whether it opened */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <stdlib.h>
#include "dumptree.h"

struct Library *MUIMasterBase;

int main(int argc, char **argv)
{
    Object *app, *win, *grp;
    int i, n = argc > 1 ? atoi(argv[1]) : 30;
    IPTR open = 0, h = 0, w = 0; struct Window *iw = NULL; LONG ih = argc > 2 ? atoi(argv[2]) : 0;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject, MUIA_Application_Title, (IPTR)"Tall",
        SubWindow, (IPTR)(win = WindowObject, MUIA_Window_Title, (IPTR)"Tall", ih ? MUIA_Window_Height : TAG_IGNORE, ih, ih ? MUIA_Window_Width : TAG_IGNORE, ih,
            WindowContents, (IPTR)(grp = HGroup, End), End), End;
    if (!app) { CloseLibrary(MUIMasterBase); return 20; }
    for (i = 0; i < n; i++)
        DoMethod(grp, OM_ADDMEMBER, (IPTR)SimpleButton("Button"));
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    get(win, MUIA_Window_Height, &h); get(win, MUIA_Window_Width, &w); get(win, MUIA_Window_Window, &iw);
    printf("buttons %d open %ld get %ldx%ld intuition %ldx%ld inner %ldx%ld\n", n, (long)open, (long)w, (long)h, iw ? (long)iw->Width : 0L, iw ? (long)iw->Height : 0L, iw ? (long)iw->GZZWidth : 0L, iw ? (long)iw->GZZHeight : 0L);
    if (open) { Object *r = NULL; get(win, MUIA_Window_RootObject, &r); dumptree(r, 0); }
    fflush(stdout);
    Delay(50);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
