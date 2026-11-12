/* nullchild: does a Group with a NULL child fail (MUI's "a failed child fails the whole call")?
   Also a Window with a NULL root object and an Application with a NULL window. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/muimaster.h>
#include <libraries/mui.h>
struct Library *MUIMasterBase;
int main(void)
{
    Object *o, *t;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 11))) return 20;
    Printf("lib %s\n", (ULONG)MUIMasterBase->lib_IdString);
    t = MUI_NewObject(MUIC_Text, MUIA_Text_Contents, "a", TAG_DONE);
    o = MUI_NewObject(MUIC_Group, MUIA_Group_Child, t, MUIA_Group_Child, NULL, TAG_DONE);
    Printf("group with NULL child: %s\n", (ULONG)(o ? "made" : "failed"));
    if (o) MUI_DisposeObject(o);
    t = MUI_NewObject(MUIC_Text, MUIA_Text_Contents, "b", TAG_DONE);
    o = MUI_NewObject(MUIC_Group, MUIA_Group_Child, NULL, MUIA_Group_Child, t, TAG_DONE);
    Printf("group with NULL first child: %s\n", (ULONG)(o ? "made" : "failed"));
    if (o) MUI_DisposeObject(o);
    o = MUI_NewObject(MUIC_Window, MUIA_Window_RootObject, NULL, TAG_DONE);
    Printf("window with NULL root: %s\n", (ULONG)(o ? "made" : "failed"));
    if (o) MUI_DisposeObject(o);
    o = MUI_NewObject(MUIC_Application, MUIA_Application_Base, "NULLCHILD", MUIA_Application_Window, NULL, TAG_DONE);
    Printf("application with NULL window: %s\n", (ULONG)(o ? "made" : "failed"));
    if (o) MUI_DisposeObject(o);
    o = MUI_NewObject(MUIC_Listview, MUIA_Listview_List, NULL, TAG_DONE);
    Printf("listview with NULL list: %s\n", (ULONG)(o ? "made" : "failed"));
    if (o) MUI_DisposeObject(o);
    o = MUI_NewObject(MUIC_Menustrip, MUIA_Family_Child, NULL, TAG_DONE);
    Printf("menustrip with NULL child: %s\n", (ULONG)(o ? "made" : "failed"));
    if (o) MUI_DisposeObject(o);
    CloseLibrary(MUIMasterBase);
    return 0;
}
