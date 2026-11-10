/* The title gadget module (modules.h): the extra title bar gadgets
 * (Popup, Prefs, Snapshot, Iconify) and the Popup gadget's window menu
 * (wintitle.c). Off by default; the library calls it through modules.c
 * under the names modules.h gives them. */

#define zune_wintitle_create zmod_wintitle_create
#define zune_wintitle_menu zmod_wintitle_menu
#include "../wintitle.c"

/* MUI 5's scrollable contents for a window larger than the screen
 * (modules.h, zune_window_scrollwrap) */
static Object *zmod_window_scrollwrap(Object *obj, ULONG op)
{
    Object *virt = NULL, *root = NULL, *sg;
    struct List *children = NULL;
    APTR cstate;

    if (op == ZUNE_SW_WRAP)
    {
        /* obj is the window, its root set up no more: both groups made
         * empty first, so a failure leaves the root as it was */
        get(obj, MUIA_Window_RootObject, &root);
        virt = MUI_NewObject(MUIC_Virtgroup, TAG_DONE);
        sg = virt ? MUI_NewObject(MUIC_Scrollgroup,
            MUIA_Scrollgroup_Contents, (IPTR)virt, TAG_DONE) : NULL;
        if (!sg)
        {
            if (virt)
                MUI_DisposeObject(virt);
            return NULL;
        }
        DoMethod(root, MUIM_DisconnectParent);
        DoMethod(virt, OM_ADDMEMBER, (IPTR)root);
        DoMethod(sg, MUIM_ConnectParent, (IPTR)obj);
        return sg;
    }
    get(obj, MUIA_Scrollgroup_Contents, &virt);
    if (virt)
        get(virt, MUIA_Group_ChildList, &children);
    if (children)
    {
        cstate = children->lh_Head;
        root = NextObject(&cstate);
    }
    if (op == ZUNE_SW_UNWRAP && root)
    {
        DoMethod(virt, OM_REMMEMBER, (IPTR)root);
        MUI_DisposeObject(obj);
    }
    return root;
}

static const struct ZuneWintitleModule wintitle_module = {
    zmod_wintitle_create, zmod_wintitle_menu, zmod_window_scrollwrap
};

BOOPSI_DISPATCHER(IPTR, Zwintitle_Dispatcher, cl, obj, msg)
{
    if (msg->MethodID == MUIM_ZuneModule_Get)
        return (IPTR)&wintitle_module;
    return DoSuperMethodA(cl, obj, msg);
}
BOOPSI_DISPATCHER_END

const struct __MUIBuiltinClass _MUI_Zwintitle_desc =
{
    MUIC_Zwintitle,
    ROOTCLASS,
    0,
    (void *) Zwintitle_Dispatcher
};
