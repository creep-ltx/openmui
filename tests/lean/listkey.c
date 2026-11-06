/* listkey: click a list row, keep the pointer there, press L: the button's control char must fire */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
struct Library *MUIMasterBase;
static const char *entries[] = {"one", "two", "three", NULL};
int main(void)
{
    Object *app, *win, *btn;
    ULONG sigs = 0;
    LONG id, presses = 0;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject, MUIA_Application_Title, "listkey",
        SubWindow, win = WindowObject, MUIA_Window_Title, "listkey",
            WindowContents, VGroup,
                Child, ListviewObject, MUIA_Listview_List, ListObject, InputListFrame,
                    MUIA_List_SourceArray, entries, End, End,
                Child, btn = SimpleButton("_Log"),
            End, End, End;
    if (!app) return 20;
    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    DoMethod(btn, MUIM_Notify, MUIA_Pressed, FALSE, app, 2, MUIM_Application_ReturnID, 1);
    set(win, MUIA_Window_Open, TRUE);
    while ((id = DoMethod(app, MUIM_Application_NewInput, &sigs)) != MUIV_Application_ReturnID_Quit)
    {
        if (id == 1) { presses++; { BPTR f = Open("RAM:presses", MODE_READWRITE); if (f) { Seek(f, 0, OFFSET_END); FPrintf(f, "pressed %ld\n", presses); Close(f); } } }
        if (sigs && !(sigs = Wait(sigs | SIGBREAKF_CTRL_C))) ;
        if (sigs & SIGBREAKF_CTRL_C) break;
    }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
