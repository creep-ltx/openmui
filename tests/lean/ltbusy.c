/* Listtree.mcc and Busy.mcc with the settings of Listtree.mcp and Busy.mcp
 * (MUI 3.8's items 0x8002....): one Listtree with a few nodes and four Busy
 * bars, one of each style, speed from the settings (MUIV_Busy_Speed_User).
 * Run it, change the pages in Zune prefs (Test or Save and Use) and run it
 * again. Quit: close the window.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -I<build>/include
 *        -I<repo>/workbench/classes/zune/listtree -I<repo>/workbench/classes/zune/busy
 *        ltbusy.c -o ltbusy -lmui
 */
#include <stdio.h>
#include <dos/dos.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include "Listtree_mcc.h"
#include "Busy_mcc.h"

struct Library *MUIMasterBase;

int main(void)
{
    Object *app, *wnd, *tree;
    ULONG sigs = 0;
    int i;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 0)))
        return 20;
    tree = MUI_NewObject(MUIC_Listtree, MUIA_Frame, MUIV_Frame_InputList, TAG_DONE);
    if (!tree)
    {
        puts("no Listtree.mcc");
        CloseLibrary(MUIMasterBase);
        return 10;
    }
    app = ApplicationObject,
        MUIA_Application_Title, "ltbusy",
        MUIA_Application_Base, "LTBUSY",
        SubWindow, wnd = WindowObject,
            MUIA_Window_Title, "ltbusy",
            MUIA_Window_ID, 0x4c544231,
            WindowContents, VGroup,
                Child, HGroup,
                    Child, ListviewObject, MUIA_Listview_List, tree, End,
                    Child, VGroup, GroupFrame,
                        Child, MUI_NewObject(MUIC_Busy, MUIA_Busy_Speed, MUIV_Busy_Speed_User, TAG_DONE),
                        Child, MUI_NewObject(MUIC_Busy, MUIA_Busy_Speed, MUIV_Busy_Speed_User, MUIA_Busy_Style, 0, TAG_DONE),
                        Child, MUI_NewObject(MUIC_Busy, MUIA_Busy_Speed, MUIV_Busy_Speed_User, MUIA_Busy_Style, 1, TAG_DONE),
                        Child, MUI_NewObject(MUIC_Busy, MUIA_Busy_Speed, MUIV_Busy_Speed_User, MUIA_Busy_Style, 2, TAG_DONE),
                    End,
                End,
            End,
        End,
    End;
    if (!app)
    {
        puts("no application");
        MUI_DisposeObject(tree);
        CloseLibrary(MUIMasterBase);
        return 10;
    }
    {
        APTR a = (APTR)DoMethod(tree, MUIM_Listtree_Insert, "comp", NULL, MUIV_Listtree_Insert_ListNode_Root,
            MUIV_Listtree_Insert_PrevNode_Tail, TNF_LIST | TNF_OPEN);
        APTR b = (APTR)DoMethod(tree, MUIM_Listtree_Insert, "sys", NULL, a, MUIV_Listtree_Insert_PrevNode_Tail,
            TNF_LIST | TNF_OPEN);

        DoMethod(tree, MUIM_Listtree_Insert, "amiga", NULL, b, MUIV_Listtree_Insert_PrevNode_Tail, 0);
        DoMethod(tree, MUIM_Listtree_Insert, "mac", NULL, b, MUIV_Listtree_Insert_PrevNode_Tail, 0);
        a = (APTR)DoMethod(tree, MUIM_Listtree_Insert, "de", NULL, MUIV_Listtree_Insert_ListNode_Root,
            MUIV_Listtree_Insert_PrevNode_Tail, TNF_LIST);
        DoMethod(tree, MUIM_Listtree_Insert, "comm", NULL, a, MUIV_Listtree_Insert_PrevNode_Tail, 0);
    }
    DoMethod(wnd, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, app, 2, MUIM_Application_ReturnID,
        MUIV_Application_ReturnID_Quit);
    set(wnd, MUIA_Window_Open, TRUE);
    for (i = 0; ; )
    {
        ULONG id = DoMethod(app, MUIM_Application_NewInput, &sigs);

        if (id == MUIV_Application_ReturnID_Quit)
            break;
        if (sigs)
            sigs = Wait(sigs | SIGBREAKF_CTRL_C);
        if (sigs & SIGBREAKF_CTRL_C)
            break;
        i++;
    }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
