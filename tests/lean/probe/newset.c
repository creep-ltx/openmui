/* newset: which attributes a subclass's OM_SET sees while the superclass's OM_NEW
 * still runs (the superclass applying its own tags with set(obj, ...) dispatches
 * to the subclass, whose instance data is still zero then). One subclass each of
 * Group, List, Scale, Popobject, Popasl, Poplist and Popscreen; a line per class
 * with the attribute ids its OM_SET got during OM_NEW ("none" when MUI applied
 * them without the subclass). Black-box: works on MUI 3.8 and Zune.
 * Build: m68k-amigaos-gcc -noixemul -Os -s -I<build>/include newset.c -o newset -lamiga */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>
#include <clib/alib_protos.h>
#include <stdio.h>

struct Library *MUIMasterBase;

static int in_new;
static ULONG seen[64]; static int nseen;

static ULONG dispatch_set(struct IClass *cl __asm("a0"), Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    if (msg->MethodID == OM_NEW)
    {
        ULONG r;
        in_new++;
        r = DoSuperMethodA(cl, obj, msg);
        in_new--;
        return r;
    }
    if (msg->MethodID == OM_SET && in_new)
    {
        struct TagItem *tags = ((struct opSet *)msg)->ops_AttrList, *tag;
        while ((tag = NextTagItem(&tags)))
            if (nseen < 64) seen[nseen++] = tag->ti_Tag;
    }
    return DoSuperMethodA(cl, obj, msg);
}

static void report(const char *name)
{
    int i;
    printf("%-10s OM_SET during OM_NEW:", name);
    if (!nseen) printf(" none");
    for (i = 0; i < nseen; i++) printf(" %08lx", seen[i]);
    printf("\n");
    nseen = 0;
}

static Object *make(struct MUI_CustomClass **mcc, const char *super, struct TagItem *tags)
{
    *mcc = MUI_CreateCustomClass(NULL, (char *)super, NULL, 4, (APTR)dispatch_set);
    if (!*mcc) { printf("%s: no class\n", super); return NULL; }
    return NewObjectA((*mcc)->mcc_Class, NULL, tags);
}

int main(void)
{
    struct MUI_CustomClass *mcc; Object *o, *str, *but, *lv, *app;
    static const char *array[] = { "one", "two", NULL };
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 11))) return 20;
    {
        struct TagItem t[] = { { MUIA_Disabled, TRUE }, { MUIA_Group_Child, 0 }, { TAG_DONE, 0 } };
        t[1].ti_Data = (IPTR)TextObject, MUIA_Text_Contents, "x", End;
        o = make(&mcc, MUIC_Group, t); report("Group"); if (o) MUI_DisposeObject(o); if (mcc) MUI_DeleteCustomClass(mcc);
    }
    {
        struct TagItem t[] = { { MUIA_List_DragSortable, TRUE }, { TAG_DONE, 0 } };
        o = make(&mcc, MUIC_List, t); report("List"); if (o) MUI_DisposeObject(o); if (mcc) MUI_DeleteCustomClass(mcc);
    }
    {
        struct TagItem t[] = { { TAG_DONE, 0 } };
        o = make(&mcc, MUIC_Scale, t); report("Scale"); if (o) MUI_DisposeObject(o); if (mcc) MUI_DeleteCustomClass(mcc);
    }
    {
        struct TagItem t[] = { { MUIA_Popstring_String, 0 }, { MUIA_Popstring_Button, 0 }, { MUIA_Popobject_Object, 0 }, { TAG_DONE, 0 } };
        t[0].ti_Data = (IPTR)(str = StringObject, End);
        t[1].ti_Data = (IPTR)(but = ImageObject, MUIA_Image_Spec, MUII_PopUp, End);
        t[2].ti_Data = (IPTR)(lv = ListviewObject, MUIA_Listview_List, ListObject, End, End);
        o = make(&mcc, MUIC_Popobject, t); report("Popobject"); if (o) MUI_DisposeObject(o); if (mcc) MUI_DeleteCustomClass(mcc);
    }
    {
        struct TagItem t[] = { { MUIA_Popstring_String, 0 }, { MUIA_Popstring_Button, 0 }, { TAG_DONE, 0 } };
        t[0].ti_Data = (IPTR)(str = StringObject, End);
        t[1].ti_Data = (IPTR)(but = ImageObject, MUIA_Image_Spec, MUII_PopFile, End);
        o = make(&mcc, MUIC_Popasl, t); report("Popasl"); if (o) MUI_DisposeObject(o); if (mcc) MUI_DeleteCustomClass(mcc);
    }
    {
        struct TagItem t[] = { { MUIA_Popstring_String, 0 }, { MUIA_Popstring_Button, 0 }, { MUIA_Poplist_Array, (IPTR)array }, { TAG_DONE, 0 } };
        t[0].ti_Data = (IPTR)(str = StringObject, End);
        t[1].ti_Data = (IPTR)(but = ImageObject, MUIA_Image_Spec, MUII_PopUp, End);
        o = make(&mcc, MUIC_Poplist, t); report("Poplist"); if (o) MUI_DisposeObject(o); if (mcc) MUI_DeleteCustomClass(mcc);
    }
    {
        struct TagItem t[] = { { MUIA_Popstring_String, 0 }, { MUIA_Popstring_Button, 0 }, { TAG_DONE, 0 } };
        t[0].ti_Data = (IPTR)(str = StringObject, End);
        t[1].ti_Data = (IPTR)(but = ImageObject, MUIA_Image_Spec, MUII_PopUp, End);
        o = make(&mcc, MUIC_Popscreen, t); report("Popscreen"); if (o) MUI_DisposeObject(o); if (mcc) MUI_DeleteCustomClass(mcc);
    }
    {
        struct TagItem t[] = { { MUIA_Application_Base, (IPTR)"NEWSET" }, { TAG_DONE, 0 } };
        app = make(&mcc, MUIC_Application, t); report("Application"); if (app) MUI_DisposeObject(app); if (mcc) MUI_DeleteCustomClass(mcc);
    }
    CloseLibrary(MUIMasterBase);
    return 0;
}
