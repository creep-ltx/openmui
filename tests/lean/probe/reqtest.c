/* reqtest N REF: a window with a column of N buttons (N large: the window
 * is fitted in a smaller font), then MUI_Request with that window as the
 * reference (REF 1) or none (REF 0); prints the answer. */
#include <libraries/mui.h>
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
    int i, n = argc > 1 ? atoi(argv[1]) : 2, ref = argc > 2 ? atoi(argv[2]) : 1;
    LONG r;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject, MUIA_Application_Title, (IPTR)"ReqTest", MUIA_Application_Base, (IPTR)"REQTEST",
        SubWindow, (IPTR)(win = WindowObject, MUIA_Window_Title, (IPTR)"ReqTest",
            WindowContents, (IPTR)(grp = VGroup, End), End), End;
    if (!app) return 20;
    for (i = 0; i < n; i++)
        DoMethod(grp, OM_ADDMEMBER, (IPTR)SimpleButton("Button"));
    set(win, MUIA_Window_Open, TRUE);
    r = MUI_Request(app, ref ? win : NULL, 0, (char *)"Quit Program?", argc > 3 ? argv[3] : (char *)"_Yes|_No", argc > 4 ? argv[4] : (char *)"Quit Program?");
    printf("answer %ld\n", (long)r);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
