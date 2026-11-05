/* ftall N [W H MODEID]: tall.c on a custom screen of its own (default 640x256 hires):
 * a window with N SimpleButton("Button") in one column; prints the window's size */
#include <libraries/mui.h>
#include <graphics/modeid.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <stdlib.h>

struct Library *MUIMasterBase;

int main(int argc, char **argv)
{
    Object *app, *win, *grp;
    struct Screen *scr;
    struct Window *iw = NULL;
    IPTR w = 0, h = 0;
    int i, n = argc > 1 ? atoi(argv[1]) : 2;
    int sw = argc > 2 ? atoi(argv[2]) : 640, sh = argc > 3 ? atoi(argv[3]) : 256;
    ULONG mode = argc > 4 ? strtoul(argv[4], NULL, 16) : HIRES_KEY;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    scr = OpenScreenTags(NULL, SA_Width, sw, SA_Height, sh, SA_DisplayID, mode, SA_Depth, 2,
        SA_LikeWorkbench, TRUE, SA_SharePens, TRUE, SA_Title, (IPTR)"FTall", TAG_DONE);
    if (!scr) { CloseLibrary(MUIMasterBase); return 20; }
    app = ApplicationObject, MUIA_Application_Title, (IPTR)"Tall",
        SubWindow, (IPTR)(win = WindowObject, MUIA_Window_Title, (IPTR)"Tall", MUIA_Window_Screen, (IPTR)scr,
            WindowContents, (IPTR)(grp = VGroup, End), End), End;
    if (app) {
        for (i = 0; i < n; i++)
            DoMethod(grp, OM_ADDMEMBER, (IPTR)SimpleButton("Button"));
        set(win, MUIA_Window_Open, TRUE);
        get(win, MUIA_Window_Width, &w); get(win, MUIA_Window_Height, &h); get(win, MUIA_Window_Window, &iw);
        printf("screen %dx%d buttons %d get %ldx%ld intuition %ldx%ld\n", sw, sh, n, (long)w, (long)h,
            iw ? (long)iw->Width : 0L, iw ? (long)iw->Height : 0L);
        fflush(stdout);
        Delay(50);
        MUI_DisposeObject(app);
    }
    CloseScreen(scr);
    CloseLibrary(MUIMasterBase);
    return 0;
}
