/* The library's side of the modules (modules.h): the lookup, and the
 * functions the rest of the library calls, under their old names. Each
 * calls the module's function of the same name, or fails as it would
 * without memory. */
#include <exec/execbase.h>
#include <dos/dos.h>
#include <intuition/classusr.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/muimaster.h>
#include <proto/asl.h>

#include "muimaster_intern.h"
#include "support.h"
#include "support_classes.h"
#include "imspec_intern.h"
#include "datatypescache.h"
#include "dragndrop.h"
#include "menu.h"
#include "requesters.h"
#include "modules.h"

extern struct Library *MUIMasterBase;

static CONST_STRPTR const module_names[ZMOD_COUNT] = {
    MUIC_Zmenu, MUIC_Zpicture, MUIC_Zdragdrop, MUIC_Zrequest, MUIC_Zgradient
};

/* Modules a process could not load: not tried again while the library
 * is in memory (a missing file would otherwise be looked for on every
 * draw or mouse move). A task that is not a process cannot load files,
 * and memory may be free again later, so those failures are not
 * remembered. */
static UBYTE module_failed[ZMOD_COUNT];

APTR zune_module_get(ULONG which, Class **clp)
{
    struct Task *me;
    Class *cl;
    ULONG method = MUIM_ZuneModule_Get;
    APTR table;

    *clp = NULL;
    if (module_failed[which])
        return NULL;
    if (!(cl = ZUNE_GetBuiltinClass(module_names[which], MUIMasterBase)))
    {
        me = FindTask(NULL);
        if (me->tc_Node.ln_Type == NT_PROCESS
            && IoErr() != ERROR_NO_FREE_STORE)
            module_failed[which] = 1;
        return NULL;
    }
    if (!(table = (APTR)CallHookPkt(&cl->cl_Dispatcher, NULL, &method)))
    {
        MUI_FreeClass(cl);
        return NULL;
    }
    *clp = cl;
    return table;
}

void zune_module_put(Class *cl)
{
    if (cl)
        MUI_FreeClass(cl);
}

#define MODULE(type, which) \
    Class *cl; \
    const struct type *m = zune_module_get(which, &cl)

/* Zune-drawn menus: context menus and MUIM_Menustrip_Popup (menu.c) */

struct ZMenu *zune_open_menu_at(struct Window *wnd, struct NewMenu *newmenu,
    LONG x, LONG y)
{
    MODULE(ZuneMenuModule, ZMOD_MENU);
    struct ZMenu *menu = m ? m->open_menu_at(wnd, newmenu, x, y) : NULL;

    zune_module_put(cl);
    return menu;
}

struct ZMenu *zune_open_menu(struct Window *wnd, struct NewMenu *newmenu)
{
    return zune_open_menu_at(wnd, newmenu, -1, -1);
}

/* The others are only called with a menu the module made */
void zune_mouse_update(struct ZMenu *menu, int left_down)
{
    if (menu)
    {
        MODULE(ZuneMenuModule, ZMOD_MENU);
        if (m)
            m->mouse_update(menu, left_down);
        zune_module_put(cl);
    }
}

int zune_poll_menu(struct ZMenu *menu, struct MenuItem **selected)
{
    MODULE(ZuneMenuModule, ZMOD_MENU);
    int picked = m ? m->poll_menu(menu, selected) : -1;

    zune_module_put(cl);
    return picked;
}

struct MenuItem *zune_leave_menu(struct ZMenu *menu)
{
    MODULE(ZuneMenuModule, ZMOD_MENU);
    struct MenuItem *item = m ? m->leave_menu(menu) : NULL;

    zune_module_put(cl);
    return item;
}

void zune_close_menu(struct ZMenu *menu)
{
    if (menu)
    {
        MODULE(ZuneMenuModule, ZMOD_MENU);
        if (m)
            m->close_menu(menu);
        zune_module_put(cl);
    }
}

/* Datatype pictures (brush and bitmap image specs, menu images) and
 * custom frames (datatypescache.c) */

struct dt_node *dt_load_picture(CONST_STRPTR filename, struct Screen *scr)
{
    MODULE(ZunePictureModule, ZMOD_PICTURE);
    struct dt_node *node = m ? m->load_picture(filename, scr) : NULL;

    zune_module_put(cl);
    return node;
}

void dt_dispose_picture(struct dt_node *node)
{
    if (node)
    {
        MODULE(ZunePictureModule, ZMOD_PICTURE);
        if (m)
            m->dispose_picture(node);
        zune_module_put(cl);
    }
}

void dt_put_on_rastport(struct dt_node *node, struct RastPort *rp, int x,
    int y)
{
    MODULE(ZunePictureModule, ZMOD_PICTURE);
    if (m)
        m->put_on_rastport(node, rp, x, y);
    zune_module_put(cl);
}

void dt_put_mim_on_rastport(struct dt_node *node, struct RastPort *rp,
    int x, int y, int state)
{
    MODULE(ZunePictureModule, ZMOD_PICTURE);
    if (m)
        m->put_mim_on_rastport(node, rp, x, y, state);
    zune_module_put(cl);
}

void dt_put_on_rastport_tiled(struct dt_node *node, struct RastPort *rp,
    int x1, int y1, int x2, int y2, int xoffset, int yoffset)
{
    MODULE(ZunePictureModule, ZMOD_PICTURE);
    if (m)
        m->put_on_rastport_tiled(node, rp, x1, y1, x2, y2, xoffset, yoffset);
    zune_module_put(cl);
}

/* Every window setup asks for all 16 custom frames; without a
 * frame.config in the prefs nothing is loaded. */
struct dt_frame_image *load_custom_frame(CONST_STRPTR filename,
    struct Screen *scr)
{
    struct dt_frame_image *fi;

    if (!filename || !filename[0])
        return NULL;
    {
        MODULE(ZunePictureModule, ZMOD_PICTURE);
        fi = m ? m->load_custom_frame(filename, scr) : NULL;
        zune_module_put(cl);
    }
    return fi;
}

void dispose_custom_frame(struct dt_frame_image *fi)
{
    if (fi)
    {
        MODULE(ZunePictureModule, ZMOD_PICTURE);
        if (m)
            m->dispose_custom_frame(fi);
        zune_module_put(cl);
    }
}

/* Drag and drop images (dragndrop.c) */

struct DragNDrop *CreateDragNDropA(struct TagItem *tlist)
{
    MODULE(ZuneDragDropModule, ZMOD_DRAGDROP);
    struct DragNDrop *dnd = m ? m->create(tlist) : NULL;

    zune_module_put(cl);
    return dnd;
}

/* The others are only called with a drag the module made */
#define DND_CALL(call) \
    MODULE(ZuneDragDropModule, ZMOD_DRAGDROP); \
    if (m) \
        m->call; \
    zune_module_put(cl)

VOID DeleteDragNDrop(struct DragNDrop *dnd)
{
    DND_CALL(delete(dnd));
}

BOOL PrepareDragNDrop(struct DragNDrop *dnd, struct Screen *scr)
{
    MODULE(ZuneDragDropModule, ZMOD_DRAGDROP);
    BOOL ok = m ? m->prepare(dnd, scr) : FALSE;

    zune_module_put(cl);
    return ok;
}

VOID FinishDragNDrop(struct DragNDrop *dnd)
{
    DND_CALL(finish(dnd));
}

VOID DrawDragNDrop(struct DragNDrop *dnd, LONG x, LONG y)
{
    DND_CALL(draw(dnd, x, y));
}

VOID UndrawDragNDrop(struct DragNDrop *dnd)
{
    DND_CALL(undraw(dnd));
}

struct BitMapNode *CreateBitMapNodeA(struct TagItem *tags)
{
    MODULE(ZuneDragDropModule, ZMOD_DRAGDROP);
    struct BitMapNode *bmn = m ? m->create_node(tags) : NULL;

    zune_module_put(cl);
    return bmn;
}

VOID DeleteBitMapNode(struct BitMapNode *bmn)
{
    DND_CALL(delete_node(bmn));
}

VOID AttachBitMapNode(struct DragNDrop *dnd, struct BitMapNode *bmn)
{
    DND_CALL(attach_node(dnd, bmn));
}

VOID DetachBitMapNode(struct BitMapNode *bmn)
{
    DND_CALL(detach_node(bmn));
}

/* MUI_AllocAslRequest(), MUI_AslRequest(), MUI_FreeAslRequest()
 * (requesters.c) and MUI_RequestA()'s requester window (mui_request.c).
 * Behaving as MUI 3.8, the three are asl.library's own calls with the
 * program's tags, as MUI 3.8 makes them (measured: same tags, same
 * requester pointer, asl's requester); Zune's file and font panels only
 * when behaving as MUI 5. */

APTR ZuneAllocAslRequest(ULONG type, struct TagItem *tags)
{
    if (ZUNE_MUI38())
        return ZUNE_ASL() ? AllocAslRequest(type, tags) : NULL;
    {
    MODULE(ZuneRequestModule, ZMOD_REQUEST);
    APTR req = m ? m->alloc_asl(type, tags) : NULL;

    zune_module_put(cl);
    return req;
    }
}

BOOL ZuneAslRequest(APTR request, struct TagItem *tags)
{
    /* the allocation opened asl.library */
    if (ZUNE_MUI38())
        return request && AslBase ? AslRequest(request, tags) : FALSE;
    {
    MODULE(ZuneRequestModule, ZMOD_REQUEST);
    BOOL ok = m && request ? m->asl(request, tags) : FALSE;

    zune_module_put(cl);
    return ok;
    }
}

void ZuneFreeAslRequest(APTR request)
{
    if (request && ZUNE_MUI38())
    {
        if (AslBase)
            FreeAslRequest(request);
    }
    else if (request)
    {
        MODULE(ZuneRequestModule, ZMOD_REQUEST);
        if (m)
            m->free_asl(request);
        zune_module_put(cl);
    }
}

LONG ZuneRequestWindow(APTR app, APTR win, LONGBITS flags,
    CONST_STRPTR title, CONST_STRPTR gadgets, CONST_STRPTR format,
    APTR params)
{
    MODULE(ZuneRequestModule, ZMOD_REQUEST);
    LONG result = m ? m->request(app, win, flags, title, gadgets, format,
        params) : 0;

    zune_module_put(cl);
    return result;
}

/* Gradients (imspec_gradientdraw.c): only drawn as such on a true colour
 * screen with cybergraphics.library; a planar screen gets the start pen,
 * without the module. */

VOID zune_gradient_drawfast(struct MUI_ImageSpec_intern *spec,
    struct RastPort *rp, struct MUI_RenderInfo *mri, WORD mode, WORD abs_l,
    WORD abs_t, WORD abs_r, WORD abs_b, WORD x1, WORD y1, WORD x2, WORD y2,
    WORD xoff, WORD yoff)
{
    if (CyberGfxBase && (mri->mri_Flags & MUIMRI_TRUECOLOR)
        && spec->u.gradient.obj)
    {
        MODULE(ZuneGradientModule, ZMOD_GRADIENT);
        if (m)
            m->drawfast(spec, rp, mri, mode, abs_l, abs_t, abs_r, abs_b,
                x1, y1, x2, y2, xoff, yoff);
        zune_module_put(cl);
        if (m)
            return;
    }
    SetAPen(rp, spec->u.gradient.start_pen);
    RectFill(rp, x1, y1, x2, y2);
}

VOID zune_gradient_draw(struct MUI_ImageSpec_intern *spec,
    struct MUI_RenderInfo *mri, WORD x1, WORD y1, WORD x2, WORD y2,
    WORD xoff, WORD yoff)
{
    zune_gradient_drawfast(spec, mri->mri_RastPort,
        mri, 0, x1, y1, x2, y2, x1, y1, x2, y2, xoff, yoff);
}
