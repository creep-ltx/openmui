/* m45probe: black-box answers of a MUI to MUI 4/5 calls programs use
 * (MUIM_FindObject, MUIA_FrameVisible, MUIA_FrameDynamic).
 *
 *   m45probe [VERSION]
 *
 * Opens muimaster.library (default version 0), builds an application with
 * a window (not opened) holding a group with a button, plus a button in
 * no tree, and prints MUIM_FindObject's result from each level, then
 * whether the buttons know MUIA_FrameVisible / MUIA_FrameDynamic (GetAttr
 * result and value, at init and after set). Built by hand:
 *   m68k-amigaos-gcc -m68000 -noixemul -Os -s -o m45probe tests/lean/m45probe.c
 */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef IPTR
#define IPTR ULONG
#endif

#ifndef MUIM_FindObject
#define MUIM_FindObject 0x8042038f
#endif
#ifndef MUIA_FrameVisible
#define MUIA_FrameVisible 0x80426498
#endif
#ifndef MUIA_FrameDynamic
#define MUIA_FrameDynamic 0x804223c9
#endif

struct Library *MUIMasterBase;

static void attr(const char *what, Object *o, ULONG a)
{
    ULONG v = 0xdeadbeef, r = GetAttr(a, o, &v);
    printf("%s: get %lu value %lx\n", what, r, v);
}

int main(int argc, char **argv)
{
    Object *app, *win, *grp, *btn, *btn2, *lone;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", argc > 1 ? atoi(argv[1]) : 0)))
        return 20;
    btn = MUI_MakeObject(MUIO_Button, (IPTR)"Button");
    btn2 = MUI_NewObject(MUIC_Text, MUIA_Frame, MUIV_Frame_Button,
        MUIA_FrameVisible, FALSE, MUIA_FrameDynamic, TRUE,
        MUIA_Text_Contents, (IPTR)"Two", TAG_DONE);
    lone = MUI_MakeObject(MUIO_Button, (IPTR)"Lone");
    grp = MUI_NewObject(MUIC_Group, MUIA_Group_Child, (IPTR)btn,
        MUIA_Group_Child, (IPTR)btn2, TAG_DONE);
    win = MUI_NewObject(MUIC_Window, MUIA_Window_Title, (IPTR)"m45probe",
        MUIA_Window_RootObject, (IPTR)grp, TAG_DONE);
    app = MUI_NewObject(MUIC_Application, MUIA_Application_Title, (IPTR)"m45probe",
        MUIA_Application_Window, (IPTR)win, TAG_DONE);
    printf("muimaster %d.%d app %s\n", MUIMasterBase->lib_Version,
        MUIMasterBase->lib_Revision, app ? "made" : "failed");
    if (app)
    {
        printf("find btn: app %lu win %lu grp %lu btn %lu btn2 %lu lone %lu\n",
            DoMethod(app, MUIM_FindObject, (IPTR)btn), DoMethod(win, MUIM_FindObject, (IPTR)btn),
            DoMethod(grp, MUIM_FindObject, (IPTR)btn), DoMethod(btn, MUIM_FindObject, (IPTR)btn),
            DoMethod(btn2, MUIM_FindObject, (IPTR)btn), DoMethod(lone, MUIM_FindObject, (IPTR)lone));
        printf("find others: app/win %lu app/grp %lu win/grp %lu win/win %lu app/app %lu app/lone %lu grp/win %lu\n",
            DoMethod(app, MUIM_FindObject, (IPTR)win), DoMethod(app, MUIM_FindObject, (IPTR)grp),
            DoMethod(win, MUIM_FindObject, (IPTR)grp), DoMethod(win, MUIM_FindObject, (IPTR)win),
            DoMethod(app, MUIM_FindObject, (IPTR)app), DoMethod(app, MUIM_FindObject, (IPTR)lone),
            DoMethod(grp, MUIM_FindObject, (IPTR)win));
        attr("btn FrameVisible", btn, MUIA_FrameVisible);
        attr("btn FrameDynamic", btn, MUIA_FrameDynamic);
        attr("btn2 FrameVisible", btn2, MUIA_FrameVisible);
        attr("btn2 FrameDynamic", btn2, MUIA_FrameDynamic);
        SetAttrs(btn, MUIA_FrameVisible, FALSE, MUIA_FrameDynamic, TRUE, TAG_DONE);
        attr("btn FrameVisible set", btn, MUIA_FrameVisible);
        attr("btn FrameDynamic set", btn, MUIA_FrameDynamic);
        MUI_DisposeObject(app);
    }
    MUI_DisposeObject(lone);
    CloseLibrary(MUIMasterBase);
    return 0;
}
