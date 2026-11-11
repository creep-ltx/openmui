/* setleft: one window for the four settings MUI 3.8's settings program
 * offers that roadmap phase 7 left unhonoured: a cycle gadget with six
 * entries (popup speed), a sortable list to drag in (middle-button drag,
 * drag look: ghosted on the list, solid outside it), and buttons and a
 * string gadget to Tab through (colour of the active object). Appends
 * "cycle N" when the cycle changes and "list N" when the list's active
 * entry changes to T:setleft.log (opened for each line, so it can be read
 * while the program runs). Test tool only, used with tests/lean/mpress.c.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -I<build>/include \
 *        -Wl,-u,___stkinit setleft.c -o setleft -lmui -lamiga */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <string.h>

struct Library *MUIMasterBase;
ULONG __stack = 32768;

static CONST_STRPTR entries[] = { "First", "Second", "Third", "Fourth",
    "Fifth", "Sixth", NULL };

static void say(const char *what, long n)
{
    char buf[40];
    BPTR fh;

    sprintf(buf, "%s %ld\n", what, n);
    if ((fh = Open("T:setleft.log", MODE_READWRITE)))
    {
        Seek(fh, 0, OFFSET_END);
        Write(fh, buf, strlen(buf));
        Close(fh);
    }
}

int main(void)
{
    Object *app, *win, *cyc, *list;
    ULONG sigs = 0;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject,
        MUIA_Application_Title, "setleft",
        MUIA_Application_Base, "SETLEFT",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "setleft",
            MUIA_Window_ID, MAKE_ID('S','L','F','T'),
            WindowContents, VGroup,
                Child, cyc = CycleObject, MUIA_Cycle_Entries, entries,
                    MUIA_CycleChain, 1, End,
                Child, ListviewObject, MUIA_CycleChain, 1,
                    MUIA_Listview_DragType, MUIV_Listview_DragType_Immediate,
                    MUIA_Listview_List, list = ListObject, InputListFrame,
                        MUIA_List_SourceArray, entries,
                        MUIA_List_DragSortable, TRUE,
                        End,
                    End,
                Child, HGroup,
                    Child, SimpleButton("_One"),
                    Child, SimpleButton("_Two"),
                    End,
                Child, StringObject, StringFrame, MUIA_CycleChain, 1,
                    MUIA_String_Contents, "text", End,
                End,
            End,
        End;
    if (!app) { CloseLibrary(MUIMasterBase); return 20; }
    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, app, 2,
        MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    DoMethod(cyc, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime, app, 2,
        MUIM_Application_ReturnID, 1);
    DoMethod(list, MUIM_Notify, MUIA_List_Active, MUIV_EveryTime, app, 2,
        MUIM_Application_ReturnID, 2);
    set(win, MUIA_Window_Open, TRUE);
    for (;;)
    {
        LONG id = DoMethod(app, MUIM_Application_NewInput, &sigs);
        if (id == MUIV_Application_ReturnID_Quit) break;
        if (id == 1) say("cycle", (long)XGET(cyc, MUIA_Cycle_Active));
        if (id == 2) say("list", (long)XGET(list, MUIA_List_Active));
        if (sigs && !(sigs = Wait(sigs | SIGBREAKF_CTRL_C))) continue;
        if (sigs & SIGBREAKF_CTRL_C) break;
    }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
