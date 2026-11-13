/* The title gadget module (modules.h): the extra title bar gadgets
 * (Popup, Prefs, Snapshot, Iconify) and the Popup gadget's window menu
 * (wintitle.c). Off by default; the library calls it through modules.c
 * under the names modules.h gives them. */

#define zune_wintitle_create zmod_wintitle_create
#define zune_wintitle_menu zmod_wintitle_menu
#include "../wintitle.c"
#include <stdio.h>
#include <proto/dos.h>
#include "classes/application.h"
#include "classes/configdata.h"
#include "classes/dataspace.h"

/* MUIM_Window_Snapshot's change of the saved files (modules.h,
 * zune_window_save_snapshot): only explicit snapshots save, so the code
 * is loaded when a program or the user takes one (2026-11-13: it was in
 * the library) */
/* Edit only this ID in each saved position table. In particular, Save must
 * not copy session-only appearance preferences from ENV into ENVARC. */
static void zmod_window_save_snapshot(Object *obj, ULONG id, BOOL remember)
{
    Object *app = _app(obj), *config;
    CONST_STRPTR base = NULL;
    struct windowpos *current = NULL;
    struct {
        ULONG size;
        struct windowpos entries[MAXWINS];
    } *positions;
    int file;
    get(app, MUIA_Application_Base, &base);
    if (!base || !*base) return;
    set(app, MUIA_Application_SearchWinId, id);
    get(app, MUIA_Application_GetWinPos, &current);
    if (remember && !current) return;
    positions = AllocVec(sizeof(*positions), MEMF_PUBLIC | MEMF_CLEAR);
    config = MUI_NewObject(MUIC_Configdata, TAG_DONE);
    if (!positions || !config) goto done;
    for (file = 0; file < 2; file++)
    {
        char path[256];
        ULONG *saved, bytes = 0, count = 0, i;
        if (snprintf(path, sizeof(path), "%s:OpenMUI/%s.prefs",
                file ? "ENVARC" : "ENV", base) >= sizeof(path)) continue;
        BPTR existing = Lock(path, SHARED_LOCK);
        if (existing) UnLock(existing);
        DoMethod(config, MUIM_Dataspace_Clear);
        if (!DoMethod(config, MUIM_Configdata_Load, path) && existing)
            continue; /* Never replace an existing file that could not be read. */
        saved = (ULONG *)DoMethod(config, MUIM_Dataspace_Get,
            MUICFG_WindowPos, &bytes);
        if (saved)
        {
            if (bytes < sizeof(ULONG) || bytes > sizeof(*positions) ||
                *saved != bytes ||
                (bytes - sizeof(ULONG)) % sizeof(struct windowpos))
                continue; /* Preserve malformed data rather than reading past it. */
            count = (bytes - sizeof(ULONG)) / sizeof(struct windowpos);
            CopyMem(saved + 1, positions->entries,
                count * sizeof(struct windowpos));
        }
        for (i = 0; i < count; i++)
            if (positions->entries[i].id == id) break;
        if (remember)
        {
            if (i == count && count >= MAXWINS - 1) continue;
            positions->entries[i] = *current;
            if (i == count) count++;
        }
        else if (i < count)
        {
            memmove(&positions->entries[i], &positions->entries[i + 1],
                (count - i - 1) * sizeof(struct windowpos));
            count--;
        }
        positions->size = sizeof(ULONG) + count * sizeof(struct windowpos);
        if (count)
            DoMethod(config, MUIM_Dataspace_Add, positions, positions->size,
                MUICFG_WindowPos);
        else
            DoMethod(config, MUIM_Dataspace_Remove, MUICFG_WindowPos);
        DoMethod(config, MUIM_Configdata_Save, path);
    }
done:
    if (config) MUI_DisposeObject(config);
    FreeVec(positions);
}

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
    zmod_wintitle_create, zmod_wintitle_menu, zmod_window_scrollwrap,
    zmod_window_save_snapshot
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
