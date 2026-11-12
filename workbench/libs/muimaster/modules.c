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
#include "dragwin.h"
#include "menu.h"
#include "requesters.h"
#include "modules.h"
#include "classes/list_private.h"
#include <stddef.h>

extern struct Library *MUIMasterBase;

static CONST_STRPTR const module_names[ZMOD_COUNT] = {
    MUIC_Zmenu, MUIC_Zpicture, MUIC_Zdragdrop, MUIC_Zrequest, MUIC_Zgradient,
    MUIC_Zwintitle, MUIC_Zframes, MUIC_Zlist, MUIC_Zmui45,
    MUIC_Zprefs
};

/* Modules a process could not load: not tried again while the library
 * is in memory (a missing file would otherwise be looked for on every
 * draw or mouse move). A task that is not a process cannot load files,
 * and memory may be free again later, so those failures are not
 * remembered. */
static UBYTE module_failed[ZMOD_COUNT];

/* Externally visible: zune_modcall below refers to these two by name from
 * assembly, which link-time optimization cannot see (support_classes.c's
 * builtinDraw for the same reason). */
APTR __attribute__((used, externally_visible))
zune_module_get(ULONG which, Class **clp)
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

void __attribute__((used, externally_visible))
zune_module_put(Class *cl)
{
    if (cl)
        MUI_FreeClass(cl);
}

#define MODULE(type, which) \
    Class *cl; \
    const struct type *m = zune_module_get(which, &cl)

/* Most of the library's names for the modules' functions are only a call
 * through the module's table with the caller's own arguments. Each is a
 * 10-byte stub that loads a code into D0 and jumps to zune_modcall:
 * bits 0-6 the number of arguments (longwords), bit 7 "return the
 * failure value when the first argument is NULL", bits 8-15 the
 * function's index in the module's table, bits 16-23 the module, bits
 * 24-31 the value returned when the module cannot be loaded (signed).
 * zune_modcall looks the module up, copies the arguments, calls the
 * function and gives the class reference back, as the C functions it
 * replaces did (modules.h). An argument shorter than a longword is
 * passed in a longword slot, so the count is the number of arguments. */
asm(
    "\t.text\n"
    "\t.even\n"
    "_zune_modcall:\n"
    "\tmovem.l %d2-%d3/%a2,-(%sp)\n"
    "\tmove.l %d0,%d2\n"
    "\ttst.b %d0\n"
    "\tbpl.s 1f\n"
    "\ttst.l 16(%sp)\n"                 /* the first argument */
    "\tbeq.s 8f\n"
    "1:\tclr.l -(%sp)\n"                /* Class *cl */
    "\tpea (%sp)\n"
    "\tswap %d0\n"
    "\tand.l #0xff,%d0\n"
    "\tmove.l %d0,-(%sp)\n"
    "\tjsr _zune_module_get\n"
    "\taddq.l #8,%sp\n"
    "\tmove.l %d0,%a0\n"
    "\ttst.l %d0\n"
    "\tbeq.s 7f\n"
    "\tmove.w %d2,%d0\n"
    "\tlsr.w #6,%d0\n"
    "\tand.w #0x3fc,%d0\n"
    "\tmove.l 0(%a0,%d0.w),%a2\n"       /* the function */
    "\tmoveq #0x7f,%d3\n"
    "\tand.l %d2,%d3\n"
    "\tadd.w %d3,%d3\n"
    "\tadd.w %d3,%d3\n"                 /* bytes of arguments */
    "\tlea 20(%sp,%d3.w),%a0\n"         /* past the caller's last one */
    "\tmove.w %d3,%d0\n"
    "\tbra.s 3f\n"
    "2:\tmove.l -(%a0),-(%sp)\n"
    "3:\tsubq.w #4,%d0\n"
    "\tbcc.s 2b\n"
    "\tjsr (%a2)\n"
    "\tadd.w %d3,%sp\n"
    "\tmove.l %d0,%d2\n"
    "\tbra.s 6f\n"
    "7:\tbsr.s 9f\n"
    "6:\tjsr _zune_module_put\n"        /* cl is on top of the stack */
    "\taddq.l #4,%sp\n"
    "5:\tmove.l %d2,%d0\n"
    "\tmovem.l (%sp)+,%d2-%d3/%a2\n"
    "\trts\n"
    "8:\tbsr.s 9f\n"
    "\tbra.s 5b\n"
    "9:\tswap %d2\n"                    /* the failure value */
    "\tasr.w #8,%d2\n"
    "\text.l %d2\n"
    "\trts\n"
);

#define ZMC_STR2(x) #x
#define ZMC_STR(x) ZMC_STR2(x)
#define ZMC_GUARD 128   /* fail when the first argument is NULL */

/* name: the library's function; which: ZMOD_*; type, field: the module
 * table and its entry; nargs: arguments (+ ZMC_GUARD); fail: returned
 * without the module */
#define ZMC(name, which, type, field, nargs, fail) \
    _Static_assert(offsetof(struct type, field) % 4 == 0 \
        && offsetof(struct type, field) / 4 < 256, #name); \
    asm("\t.text\n\t.even\n\t.globl _" #name "\n_" #name ":\n" \
        "\tmove.l #((" ZMC_STR(fail) ")<<24)+(" ZMC_STR(which) "<<16)+(" \
        ZMC_STR(ZMC_OFS(type, field)) "<<6)+(" ZMC_STR(nargs) "),%d0\n" \
        "\tbra.w _zune_modcall\n")

/* The table offsets the stubs use, as numbers the assembler can read;
 * each is checked against the structure below. */
#define ZMC_OFS(type, field) ZMC_OFS_##type##_##field
#define ZMC_CHECK(type, field, ofs) \
    _Static_assert(offsetof(struct type, field) == (ofs), #type "." #field)

#define ZMC_OFS_ZuneMenuModule_open_menu_at 0
#define ZMC_OFS_ZuneMenuModule_mouse_update 4
#define ZMC_OFS_ZuneMenuModule_poll_menu 8
#define ZMC_OFS_ZuneMenuModule_leave_menu 12
#define ZMC_OFS_ZuneMenuModule_close_menu 16
#define ZMC_OFS_ZuneMenuModule_menustrip_popup 20
#define ZMC_OFS_ZuneMenuModule_popup_rollout 24
ZMC_CHECK(ZuneMenuModule, open_menu_at, 0);
ZMC_CHECK(ZuneMenuModule, mouse_update, 4);
ZMC_CHECK(ZuneMenuModule, poll_menu, 8);
ZMC_CHECK(ZuneMenuModule, leave_menu, 12);
ZMC_CHECK(ZuneMenuModule, close_menu, 16);
ZMC_CHECK(ZuneMenuModule, menustrip_popup, 20);
ZMC_CHECK(ZuneMenuModule, popup_rollout, 24);

#define ZMC_OFS_ZunePictureModule_load_picture 0
#define ZMC_OFS_ZunePictureModule_dispose_picture 4
#define ZMC_OFS_ZunePictureModule_put_on_rastport 8
#define ZMC_OFS_ZunePictureModule_put_mim_on_rastport 12
#define ZMC_OFS_ZunePictureModule_put_on_rastport_tiled 16
#define ZMC_OFS_ZunePictureModule_load_custom_frame 20
#define ZMC_OFS_ZunePictureModule_dispose_custom_frame 24
#define ZMC_OFS_ZunePictureModule_do_frame_rects 28
#define ZMC_OFS_ZunePictureModule_boopsi_open 32
#define ZMC_OFS_ZunePictureModule_boopsi_close 36
ZMC_CHECK(ZunePictureModule, load_picture, 0);
ZMC_CHECK(ZunePictureModule, dispose_picture, 4);
ZMC_CHECK(ZunePictureModule, put_on_rastport, 8);
ZMC_CHECK(ZunePictureModule, put_mim_on_rastport, 12);
ZMC_CHECK(ZunePictureModule, put_on_rastport_tiled, 16);
ZMC_CHECK(ZunePictureModule, load_custom_frame, 20);
ZMC_CHECK(ZunePictureModule, dispose_custom_frame, 24);
ZMC_CHECK(ZunePictureModule, do_frame_rects, 28);
ZMC_CHECK(ZunePictureModule, boopsi_open, 32);
ZMC_CHECK(ZunePictureModule, boopsi_close, 36);

#define ZMC_OFS_ZuneDragDropModule_create 0
#define ZMC_OFS_ZuneDragDropModule_delete 4
#define ZMC_OFS_ZuneDragDropModule_prepare 8
#define ZMC_OFS_ZuneDragDropModule_finish 12
#define ZMC_OFS_ZuneDragDropModule_draw 16
#define ZMC_OFS_ZuneDragDropModule_undraw 20
#define ZMC_OFS_ZuneDragDropModule_create_node 24
#define ZMC_OFS_ZuneDragDropModule_delete_node 28
#define ZMC_OFS_ZuneDragDropModule_attach_node 32
#define ZMC_OFS_ZuneDragDropModule_detach_node 36
#define ZMC_OFS_ZuneDragDropModule_drag_begin 40
#define ZMC_OFS_ZuneDragDropModule_drag_event 44
ZMC_CHECK(ZuneDragDropModule, create, 0);
ZMC_CHECK(ZuneDragDropModule, delete, 4);
ZMC_CHECK(ZuneDragDropModule, prepare, 8);
ZMC_CHECK(ZuneDragDropModule, finish, 12);
ZMC_CHECK(ZuneDragDropModule, draw, 16);
ZMC_CHECK(ZuneDragDropModule, undraw, 20);
ZMC_CHECK(ZuneDragDropModule, create_node, 24);
ZMC_CHECK(ZuneDragDropModule, delete_node, 28);
ZMC_CHECK(ZuneDragDropModule, attach_node, 32);
ZMC_CHECK(ZuneDragDropModule, detach_node, 36);
ZMC_CHECK(ZuneDragDropModule, drag_begin, 40);
ZMC_CHECK(ZuneDragDropModule, drag_event, 44);

#define ZMC_OFS_ZuneRequestModule_alloc_asl 0
#define ZMC_OFS_ZuneRequestModule_asl 4
#define ZMC_OFS_ZuneRequestModule_free_asl 8
#define ZMC_OFS_ZuneRequestModule_request 12
ZMC_CHECK(ZuneRequestModule, alloc_asl, 0);
ZMC_CHECK(ZuneRequestModule, asl, 4);
ZMC_CHECK(ZuneRequestModule, free_asl, 8);
ZMC_CHECK(ZuneRequestModule, request, 12);

#define ZMC_OFS_ZuneWintitleModule_create 0
#define ZMC_OFS_ZuneWintitleModule_menu 4
#define ZMC_OFS_ZuneWintitleModule_scrollwrap 8
ZMC_CHECK(ZuneWintitleModule, create, 0);
ZMC_CHECK(ZuneWintitleModule, menu, 4);
ZMC_CHECK(ZuneWintitleModule, scrollwrap, 8);

#define ZMC_OFS_ZuneListModule_edit 0
#define ZMC_OFS_ZuneListModule_end_edit 4
#define ZMC_OFS_ZuneListModule_layout_editor 8
#define ZMC_OFS_ZuneListModule_drag_report 12
#define ZMC_OFS_ZuneListModule_drag_drop 16
#define ZMC_OFS_ZuneListModule_create_drag_image 20
#define ZMC_OFS_ZuneListModule_draw_sort_mark 24
#define ZMC_OFS_ZuneListModule_column_order 28
#define ZMC_OFS_ZuneListModule_cell_dims 32
#define ZMC_OFS_ZuneListModule_draw_cell 36
#define ZMC_OFS_ZuneListModule_layout_horiz 40
#define ZMC_OFS_ZuneListModule_check_horiz 44
#define ZMC_OFS_ZuneListModule_draw_title_strip 48
#define ZMC_OFS_ZuneListModule_sort_entries 52
ZMC_CHECK(ZuneListModule, sort_entries, 52);
#define ZMC_OFS_ZuneListModule_set_columns 56
#define ZMC_OFS_ZuneListModule_format_mui45 60
ZMC_CHECK(ZuneListModule, set_columns, 56);
ZMC_CHECK(ZuneListModule, format_mui45, 60);
ZMC_CHECK(ZuneListModule, column_order, 28);
ZMC_CHECK(ZuneListModule, cell_dims, 32);
ZMC_CHECK(ZuneListModule, draw_cell, 36);
ZMC_CHECK(ZuneListModule, layout_horiz, 40);
ZMC_CHECK(ZuneListModule, check_horiz, 44);
ZMC_CHECK(ZuneListModule, draw_title_strip, 48);
ZMC_CHECK(ZuneListModule, edit, 0);
ZMC_CHECK(ZuneListModule, end_edit, 4);
ZMC_CHECK(ZuneListModule, layout_editor, 8);
ZMC_CHECK(ZuneListModule, drag_report, 12);
ZMC_CHECK(ZuneListModule, drag_drop, 16);
ZMC_CHECK(ZuneListModule, create_drag_image, 20);
ZMC_CHECK(ZuneListModule, draw_sort_mark, 24);

#define ZMC_OFS_ZuneMui45Module_area 0
#define ZMC_OFS_ZuneMui45Module_string 4
#define ZMC_OFS_ZuneMui45Module_string_draw 8
#define ZMC_OFS_ZuneMui45Module_string_pos_at 12
#define ZMC_OFS_ZuneMui45Module_string_textinput 16
#define ZMC_OFS_ZuneMui45Module_pointer_image 20
ZMC_CHECK(ZuneMui45Module, area, 0);
ZMC_CHECK(ZuneMui45Module, string, 4);
ZMC_CHECK(ZuneMui45Module, string_draw, 8);
ZMC_CHECK(ZuneMui45Module, string_pos_at, 12);
ZMC_CHECK(ZuneMui45Module, string_textinput, 16);
ZMC_CHECK(ZuneMui45Module, pointer_image, 20);

#define ZMC_OFS_ZunePrefsModule_configdata_set 0
ZMC_CHECK(ZunePrefsModule, configdata_set, 0);

/* Zune-drawn menus: context menus and MUIM_Menustrip_Popup (menu.c); the
 * others are only called with a menu the module made */
ZMC(zune_open_menu_at, ZMOD_MENU, ZuneMenuModule, open_menu_at, 4, 0);
ZMC(zune_mouse_update, ZMOD_MENU, ZuneMenuModule, mouse_update,
    ZMC_GUARD + 2, 0);
ZMC(zune_poll_menu, ZMOD_MENU, ZuneMenuModule, poll_menu, 2, -1);
ZMC(zune_leave_menu, ZMOD_MENU, ZuneMenuModule, leave_menu, 1, 0);
ZMC(zune_close_menu, ZMOD_MENU, ZuneMenuModule, close_menu,
    ZMC_GUARD + 1, 0);
/* MUIM_Menustrip_Popup (menupopup.c): no pop-up menu without the
 * module, as when it cannot make the menu */
ZMC(zune_menustrip_popup, ZMOD_MENU, ZuneMenuModule, menustrip_popup, 2, 0);
/* A cycle popup rolled out (the settings' popup speed above 0) */
ZMC(zune_popup_rollout, ZMOD_MENU, ZuneMenuModule, popup_rollout, 5, 0);

struct ZMenu *zune_open_menu(struct Window *wnd, struct NewMenu *newmenu)
{
    return zune_open_menu_at(wnd, newmenu, -1, -1);
}

/* Datatype pictures (brush and bitmap image specs, menu images) and
 * custom frames (datatypescache.c); a custom frame is only drawn with
 * pictures the module loaded */
ZMC(dt_load_picture, ZMOD_PICTURE, ZunePictureModule, load_picture, 2, 0);
ZMC(dt_dispose_picture, ZMOD_PICTURE, ZunePictureModule, dispose_picture,
    ZMC_GUARD + 1, 0);
ZMC(dt_put_on_rastport, ZMOD_PICTURE, ZunePictureModule, put_on_rastport,
    4, 0);
ZMC(dt_put_mim_on_rastport, ZMOD_PICTURE, ZunePictureModule,
    put_mim_on_rastport, 5, 0);
ZMC(dt_put_on_rastport_tiled, ZMOD_PICTURE, ZunePictureModule,
    put_on_rastport_tiled, 8, 0);
ZMC(zmc_load_custom_frame, ZMOD_PICTURE, ZunePictureModule,
    load_custom_frame, 2, 0);
ZMC(dispose_custom_frame, ZMOD_PICTURE, ZunePictureModule,
    dispose_custom_frame, ZMC_GUARD + 1, 0);
ZMC(dt_do_frame_rects, ZMOD_PICTURE, ZunePictureModule, do_frame_rects,
    11, 0);
/* External BOOPSI image classes ("3:<name>.image", imspec_boopsi.c): a
 * spec that cannot be opened draws as the background, as a missing brush */
ZMC(zune_boopsi_image_open, ZMOD_PICTURE, ZunePictureModule, boopsi_open,
    5, 0);
ZMC(zmc_boopsi_image_close, ZMOD_PICTURE, ZunePictureModule, boopsi_close,
    2, 0);

struct dt_frame_image *zmc_load_custom_frame(CONST_STRPTR filename,
    struct Screen *scr);
void zmc_boopsi_image_close(struct Library *base, Object *obj);

/* Every window setup asks for all 16 custom frames; without a
 * frame.config in the prefs nothing is loaded. */
struct dt_frame_image *load_custom_frame(CONST_STRPTR filename,
    struct Screen *scr)
{
    if (!filename || !filename[0])
        return NULL;
    return zmc_load_custom_frame(filename, scr);
}

void zune_boopsi_image_close(struct Library *base, Object *obj)
{
    if (base || obj)
        zmc_boopsi_image_close(base, obj);
}

/* Drag and drop images (dragndrop.c); a window's MUIM_Window_DragObject
 * (dragwin.c): no drag without the module. The others are only called
 * with a drag the module made; drag_event only while one is on. */
ZMC(CreateDragNDropA, ZMOD_DRAGDROP, ZuneDragDropModule, create, 1, 0);
ZMC(zune_drag_begin, ZMOD_DRAGDROP, ZuneDragDropModule, drag_begin, 6, 0);
ZMC(zune_drag_event, ZMOD_DRAGDROP, ZuneDragDropModule, drag_event, 3, 1);
ZMC(DeleteDragNDrop, ZMOD_DRAGDROP, ZuneDragDropModule, delete, 1, 0);
ZMC(PrepareDragNDrop, ZMOD_DRAGDROP, ZuneDragDropModule, prepare, 2, 0);
ZMC(FinishDragNDrop, ZMOD_DRAGDROP, ZuneDragDropModule, finish, 1, 0);
ZMC(DrawDragNDrop, ZMOD_DRAGDROP, ZuneDragDropModule, draw, 3, 0);
ZMC(UndrawDragNDrop, ZMOD_DRAGDROP, ZuneDragDropModule, undraw, 1, 0);
ZMC(CreateBitMapNodeA, ZMOD_DRAGDROP, ZuneDragDropModule, create_node, 1,
    0);
ZMC(DeleteBitMapNode, ZMOD_DRAGDROP, ZuneDragDropModule, delete_node, 1,
    0);
ZMC(AttachBitMapNode, ZMOD_DRAGDROP, ZuneDragDropModule, attach_node, 2,
    0);
ZMC(DetachBitMapNode, ZMOD_DRAGDROP, ZuneDragDropModule, detach_node, 1,
    0);

/* MUI_AllocAslRequest(), MUI_AslRequest(), MUI_FreeAslRequest()
 * (requesters.c) and MUI_RequestA()'s requester window (mui_request.c).
 * Behaving as MUI 3.8, the three are asl.library's own calls with the
 * program's tags, as MUI 3.8 makes them (measured: same tags, same
 * requester pointer, asl's requester); Zune's file and font panels only
 * when behaving as MUI 5. */
ZMC(zmc_alloc_asl, ZMOD_REQUEST, ZuneRequestModule, alloc_asl, 2, 0);
ZMC(zmc_asl, ZMOD_REQUEST, ZuneRequestModule, asl, 2, 0);
ZMC(zmc_free_asl, ZMOD_REQUEST, ZuneRequestModule, free_asl, 1, 0);
ZMC(ZuneRequestWindow, ZMOD_REQUEST, ZuneRequestModule, request, 7, 0);

APTR zmc_alloc_asl(ULONG type, struct TagItem *tags);
BOOL zmc_asl(APTR request, struct TagItem *tags);
void zmc_free_asl(APTR request);

/* The identity comes from the calling task (identity.c). A request
 * made by Zune's panels for one program may reach AslRequest or Free
 * from a task behaving as MUI 3.8 (a subtask): once a panel request has
 * been made, those go through the module, which tells its own requests
 * from asl.library's. */
static BOOL panel_requests;

APTR ZuneAllocAslRequest(ULONG type, struct TagItem *tags)
{
    APTR req;

    if (ZUNE_MUI38())
        return ZUNE_ASL() ? AllocAslRequest(type, tags) : NULL;
    if ((req = zmc_alloc_asl(type, tags)))
        panel_requests = TRUE;
    return req;
}

BOOL ZuneAslRequest(APTR request, struct TagItem *tags)
{
    /* the allocation opened asl.library */
    if (!panel_requests && ZUNE_MUI38())
        return request && AslBase ? AslRequest(request, tags) : FALSE;
    return request ? zmc_asl(request, tags) : FALSE;
}

void ZuneFreeAslRequest(APTR request)
{
    if (request && !panel_requests && ZUNE_MUI38())
    {
        if (AslBase)
            FreeAslRequest(request);
    }
    else if (request)
        zmc_free_asl(request);
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

/* The extra title bar gadgets (wintitle.c): only made when the prefs
 * switch one on; without the module a window opens without them. */
ZMC(zune_wintitle_create, ZMOD_WINTITLE, ZuneWintitleModule, create, 5, 0);
ZMC(zune_window_scrollwrap, ZMOD_WINTITLE, ZuneWintitleModule, scrollwrap,
    2, 0);
ZMC(zune_wintitle_menu, ZMOD_WINTITLE, ZuneWintitleModule, menu, 4, -1);

/* The rounded frames (frame_round.c): asked for on every draw and layout
 * of such a frame, so the table is kept. Class files and modules are
 * only unloaded at the library's expunge (ZUNE_CloseClassFiles), so it
 * stays valid without a reference held. */
const struct ZuneFrameGfx *zune_round_frames_get(void)
{
    static const struct ZuneFrameGfx *round;

    if (!round)
    {
        MODULE(ZuneFramesModule, ZMOD_FRAMES);
        if (m)
            round = m->frames;
        zune_module_put(cl);
    }
    return round;
}

/* List's in-place editing (MUI 5) and drag and drop (listx.c). Without
 * the module a list cannot be edited in place (MUIM_List_Edit fails, an
 * editor that was never opened needs no ending) and takes no drop. The
 * sort mark on a SORTABLE column's title (MUI 4/5 lists; MUI 3.8's
 * format has no SORTABLE, so MUI 3 programs never get there). */
ZMC(zune_list_edit, ZMOD_LIST, ZuneListModule, edit, 4, 0);
ZMC(zune_list_end_edit, ZMOD_LIST, ZuneListModule, end_edit, 4, 1);
ZMC(zune_list_layout_editor, ZMOD_LIST, ZuneListModule, layout_editor, 2,
    0);
ZMC(zune_list_drag_report, ZMOD_LIST, ZuneListModule, drag_report, 3, 0);
ZMC(zune_list_drag_drop, ZMOD_LIST, ZuneListModule, drag_drop, 3, 0);
ZMC(zune_list_create_drag_image, ZMOD_LIST, ZuneListModule,
    create_drag_image, 3, 0);
ZMC(zune_list_draw_sort_mark, ZMOD_LIST, ZuneListModule, draw_sort_mark, 5,
    0);
/* What only lists of programs that get MUI 4/5 use (classes/list_private.h).
 * Without the module: no column order, ISOBJECT cells take no room and
 * are not drawn, no horizontal scroller is shown, the title strip is not
 * drawn (its row is, without a clip). */
ZMC(zune_list_column_order, ZMOD_LIST, ZuneListModule, column_order, 2, 0);
ZMC(zune_list_cell_dims, ZMOD_LIST, ZuneListModule, cell_dims, 4, 0);
ZMC(zune_list_draw_cell, ZMOD_LIST, ZuneListModule, draw_cell, 6, 0);
ZMC(zune_list_layout_horiz, ZMOD_LIST, ZuneListModule, layout_horiz, 6, 0);
ZMC(zune_list_check_horiz, ZMOD_LIST, ZuneListModule, check_horiz, 2, 0);
ZMC(zune_list_draw_title_strip, ZMOD_LIST, ZuneListModule,
    draw_title_strip, 2, -1);
ZMC(zune_list_sort_entries, ZMOD_LIST, ZuneListModule, sort_entries, 3, 0);
ZMC(zune_list_set_columns, ZMOD_LIST, ZuneListModule, set_columns, 4, 0);
ZMC(zune_list_format_mui45, ZMOD_LIST, ZuneListModule, format_mui45, 2, 0);

/* MUI 4/5 attributes of the core classes (mui45.c): only programs that
 * use them call these */
ZMC(zune_area_mui45, ZMOD_MUI45, ZuneMui45Module, area, 4, 0);
ZMC(zune_string_mui45, ZMOD_MUI45, ZuneMui45Module, string, 5, 0);
ZMC(zune_string_draw45, ZMOD_MUI45, ZuneMui45Module, string_draw, 6, 0);
ZMC(zune_string_pos_at, ZMOD_MUI45, ZuneMui45Module, string_pos_at, 4, 0);
ZMC(zune_string_textinput, ZMOD_MUI45, ZuneMui45Module, string_textinput,
    4, 0);
/* MUIA_PointerType's drawn shapes (pointer45.c, V20) */
ZMC(zune_pointer_image, ZMOD_MUI45, ZuneMui45Module, pointer_image, 3, 0);

/* Configdata's setters (configset.c): the prefs program's; without the
 * module a setting is not changed */
ZMC(zune_configdata_set, ZMOD_PREFS, ZunePrefsModule, configdata_set, 3, 0);
