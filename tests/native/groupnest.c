/* Group InitChange nesting (MUI 3.8: only the outermost ExitChange lays out): InitChange twice, add a child,
 * ExitChange once, twice; print the child's and the group's width after the input loop ran.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -Wl,-u,___stkinit groupnest.c -o groupnest -lamiga
 * MUI 3.8: "exit1 w=0" (the inner ExitChange does not lay out), "exit2 w=64" (OS 3.1, topaz 8) */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
struct Library *MUIMasterBase;
ULONG __stack = 65536;
static Object *gapp;
static void pump(void){int i; for(i=0;i<5;i++){DoMethod(gapp,MUIM_Application_InputBuffered); Delay(2);}}
int main(void)
{
    Object *app, *win, *grp, *c1, *c2;
    ULONG open = 0;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject, MUIA_Application_Title, "gnest",
        SubWindow, win = WindowObject, MUIA_Window_Title, "gnest",
            WindowContents, grp = HGroup, Child, TextObject, MUIA_Text_Contents, "A", End, End,
        End, End;
    if (!app) return 20;
    gapp = app;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (!open) return 20;
    c1 = TextObject, MUIA_Text_Contents, "BBBBBBBB", End;
    DoMethod(grp, MUIM_Group_InitChange);
    DoMethod(grp, MUIM_Group_InitChange);
    DoMethod(grp, OM_ADDMEMBER, c1);
    pump(); printf("added w=%ld gw=%ld\n", (long)_width(c1), (long)_width(grp));
    DoMethod(grp, MUIM_Group_ExitChange);
    pump(); printf("exit1 w=%ld gw=%ld\n", (long)_width(c1), (long)_width(grp));
    DoMethod(grp, MUIM_Group_ExitChange);
    pump(); printf("exit2 w=%ld gw=%ld\n", (long)_width(c1), (long)_width(grp));
    /* nested: inner group change inside outer */
    c2 = TextObject, MUIA_Text_Contents, "CCCC", End;
    DoMethod(grp, MUIM_Group_InitChange);
    DoMethod(grp, MUIM_Group_InitChange);
    DoMethod(grp, MUIM_Group_ExitChange);
    DoMethod(grp, OM_ADDMEMBER, c2);
    pump(); printf("after exit-then-add w=%ld gw=%ld\n", (long)_width(c2), (long)_width(grp));
    DoMethod(grp, MUIM_Group_ExitChange);
    pump(); printf("exit w=%ld gw=%ld\n", (long)_width(c2), (long)_width(grp));
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
