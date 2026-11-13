/* Rarely used parts of the library that are not classes, as modules: each
 * is a private class (a name only Zune uses) in a file of its own,
 * LIBS:OpenMUI/<Name>.mui, loaded through the class-file loader (classfile.h)
 * the first time the library needs it. The class has no objects; its
 * dispatcher, called with no object, answers MUIM_ZuneModule_Get with a
 * table of the module's functions. The library's functions of the same
 * names (modules.c) look the table up on every call and hold a reference
 * on the class for the length of the call, so the module stays in memory
 * while it runs and leaves with the library's expunge, as class files do.
 * A module that cannot be loaded (no file, no memory, a caller that is not
 * a process the first time) makes its feature fail the way it does on low
 * memory: no menu, no drag image, no picture, a plain fill.
 * CLASSFILES= (no class files) builds the modules into the library, where
 * the same lookup finds them among the built-in classes.
 */
#ifndef ZUNE_MODULES_H
#define ZUNE_MODULES_H

#include <exec/types.h>
#include <intuition/classes.h>

/* The class names, and the files (gcc/classfiles.py) */
#define MUIC_Zmenu      "Zmenu.zmod"        /* Zmenu.mui: menu.c, menupopup.c */
#define MUIC_Zpicture   "Zpicture.zmod"     /* Zpicture.mui: datatypescache.c, frame_tile.c */
#define MUIC_Zdragdrop  "Zdragdrop.zmod"    /* Zdragdrop.mui: dragndrop.c, dragwin.c */
#define MUIC_Zrequest   "Zrequest.zmod"     /* Zrequest.mui: requesters.c, mui_request.c */
#define MUIC_Zgradient  "Zgradient.zmod"    /* Zgradient.mui: imspec_gradientdraw.c */
#define MUIC_Zwintitle  "Zwintitle.zmod"    /* Zwintitle.mui: wintitle.c */
#define MUIC_Zframes    "Zframes.zmod"      /* Zframes.mui: frame_round.c */
#define MUIC_Zlist      "Zlist.zmod"        /* Zlist.mui: listx.c */
/* Zmui45.mui: mui45.c, pointer45.c, textmark45.c, rtgtext45.c, makeobj45.c */
#define MUIC_Zmui45     "Zmui45.zmod"
#define MUIC_Zprefs     "Zprefs.zmod"       /* Zprefs.mui: configset.c */
#define MUIC_Zapp       "Zapp.zmod"         /* Zapp.mui: appx.c */

/* Numbers, not an enum: modules.c's call stubs give them to the
 * assembler */
#define ZMOD_MENU 0
#define ZMOD_PICTURE 1
#define ZMOD_DRAGDROP 2
#define ZMOD_REQUEST 3
#define ZMOD_GRADIENT 4
#define ZMOD_WINTITLE 5
#define ZMOD_FRAMES 6
#define ZMOD_LIST 7
#define ZMOD_MUI45 8
#define ZMOD_PREFS 9
#define ZMOD_APP 10
#define ZMOD_COUNT 11

/* The only method of a module class; answered with no object */
#define MUIM_ZuneModule_Get 0xDA4D0001

/* The module's table and a reference on its class, or NULL; give the
 * reference back with zune_module_put() once the call returns. */
APTR zune_module_get(ULONG which, Class **cl);
void zune_module_put(Class *cl);

struct Window;
struct NewMenu;
struct MenuItem;
struct ZMenu;
struct MUIP_Menustrip_Popup;
struct ZuneMenuModule
{
    struct ZMenu *(*open_menu_at)(struct Window *, struct NewMenu *, LONG,
        LONG);
    void (*mouse_update)(struct ZMenu *, int);
    int (*poll_menu)(struct ZMenu *, struct MenuItem **);
    struct MenuItem *(*leave_menu)(struct ZMenu *);
    void (*close_menu)(struct ZMenu *);
    IPTR (*menustrip_popup)(Object *, struct MUIP_Menustrip_Popup *);
    BOOL (*popup_rollout)(struct Window *, LONG, void (*)(Object *, APTR),
        Object *, APTR);
    IPTR (*area_menu)(Object *, struct IntuiMessage *);
};
/* area_menu: Area's context menu events (areamenu.c); 0 without the
 * module */
struct IntuiMessage;
IPTR zune_area_menu(Object *obj, struct IntuiMessage *imsg);
/* MUIM_Menustrip_Popup (menupopup.c) */
IPTR zune_menustrip_popup(Object *obj, struct MUIP_Menustrip_Popup *msg);
/* A cycle popup rolled out from the top in about FRAMES video frames
 * (the settings' popup speed, MUI 3.8's look): draw(obj, data) is called
 * once per band of rows, clipped to it. FALSE without the module: the
 * caller draws it at once. */
BOOL zune_popup_rollout(struct Window *win, LONG frames,
    void (*draw)(Object *, APTR), Object *obj, APTR data);

struct dt_node;
struct dt_frame_image;
struct MUI_RenderInfo;
struct Library;
struct NewImage;
struct RastPort;
struct Screen;
struct ZunePictureModule
{
    struct dt_node *(*load_picture)(CONST_STRPTR, struct Screen *);
    void (*dispose_picture)(struct dt_node *);
    void (*put_on_rastport)(struct dt_node *, struct RastPort *, int, int);
    void (*put_mim_on_rastport)(struct dt_node *, struct RastPort *, int,
        int, int);
    void (*put_on_rastport_tiled)(struct dt_node *, struct RastPort *, int,
        int, int, int, int, int);
    struct dt_frame_image *(*load_custom_frame)(CONST_STRPTR,
        struct Screen *);
    void (*dispose_custom_frame)(struct dt_frame_image *);
    void (*do_frame_rects)(struct RastPort *, struct dt_frame_image *,
        struct NewImage *, int, int, int, int, int, int, int, int);
    /* "3:<name>.image" specs (imspec_boopsi.c) */
    BOOL (*boopsi_open)(CONST_STRPTR, struct MUI_RenderInfo *,
        struct Library **, Object **, UWORD *);
    void (*boopsi_close)(struct Library *, Object *);
};
/* The library's names for them (imspec.c) */
BOOL zune_boopsi_image_open(CONST_STRPTR name, struct MUI_RenderInfo *mri,
    struct Library **base, Object **obj, UWORD *sizes);
void zune_boopsi_image_close(struct Library *base, Object *obj);

struct DragNDrop;
struct BitMapNode;
struct ZuneDrag;
struct IntuiMessage;
struct TagItem;
struct ZuneDragDropModule
{
    struct DragNDrop *(*create)(struct TagItem *);
    VOID (*delete)(struct DragNDrop *);
    BOOL (*prepare)(struct DragNDrop *, struct Screen *);
    VOID (*finish)(struct DragNDrop *);
    VOID (*draw)(struct DragNDrop *, LONG, LONG);
    VOID (*undraw)(struct DragNDrop *);
    struct BitMapNode *(*create_node)(struct TagItem *);
    VOID (*delete_node)(struct BitMapNode *);
    VOID (*attach_node)(struct DragNDrop *, struct BitMapNode *);
    VOID (*detach_node)(struct BitMapNode *);
    IPTR (*drag_begin)(struct ZuneDrag *, Object *, LONG, LONG, ULONG,
        struct Screen *);
    BOOL (*drag_event)(Object *, struct ZuneDrag *, struct IntuiMessage *);
    IPTR (*area_drag)(struct IClass *, Object *, Msg);
    BOOL (*area_drag_motion)(Object *, struct IntuiMessage *);
};
/* area_drag_motion: whether a move over a draggable object starts its
 * drag, and the start (areadrag.c); FALSE without the module */
BOOL zune_area_drag_motion(Object *obj, struct IntuiMessage *imsg);
/* area_drag: Area's MUIM_DoDrag, MUIM_CreateDragImage,
 * MUIM_DeleteDragImage, MUIM_DragBegin and MUIM_DragFinish (areadrag.c);
 * 0 without the module */
IPTR zune_area_drag(struct IClass *cl, Object *obj, Msg msg);

struct ZuneRequestModule
{
    APTR (*alloc_asl)(ULONG, struct TagItem *);
    BOOL (*asl)(APTR, struct TagItem *);
    void (*free_asl)(APTR);
    LONG (*request)(APTR, APTR, LONGBITS, CONST_STRPTR, CONST_STRPTR,
        CONST_STRPTR, APTR);
};

struct MUI_ImageSpec_intern;
struct MUI_RenderInfo;
struct ZuneGradientModule
{
    VOID (*drawfast)(struct MUI_ImageSpec_intern *, struct RastPort *,
        struct MUI_RenderInfo *, WORD, WORD, WORD, WORD, WORD, WORD, WORD,
        WORD, WORD, WORD, WORD);
};

/* The extra title bar gadgets (wintitle.c); classes/window.c calls the
 * library's functions of these names */
struct Window;
struct Gadget;
struct DrawInfo;
#define ZUNE_TITLE_BOX 23   /* width of a title gadget's box */
/* the window menu's actions, in its order */
enum { ZUNE_TA_ICONIFY, ZUNE_TA_SNAPSHOT, ZUNE_TA_UNSNAPSHOT,
    ZUNE_TA_SETTINGS, ZUNE_TA_ABOUT };
struct ZuneWintitleModule
{
    void (*create)(struct Window *, ULONG, LONG, struct Gadget **,
        Class **);
    int (*menu)(struct Window *, struct DrawInfo *, struct Gadget *, BOOL);
    Object *(*scrollwrap)(Object *, ULONG);
    void (*save_snapshot)(Object *, ULONG, BOOL);
};
void zune_wintitle_create(struct Window *win, ULONG buttons, LONG right,
    struct Gadget **gadgets, Class **clp);
int zune_wintitle_menu(struct Window *pw, struct DrawInfo *dri,
    struct Gadget *popup, BOOL has_id);
/* MUI 5's scrollable window contents (MUI 5 ChangeLog, 2018-04-03): a
 * window larger than the screen gets its root object inside a Scrollgroup
 * with a Virtgroup. ZUNE_SW_WRAP: for the given window (its root not set
 * up), the new root (the Scrollgroup, connected to the window), or NULL
 * (the root is then untouched); ZUNE_SW_UNWRAP: take
 * the original root out of the given Scrollgroup, dispose the Scrollgroup,
 * return the root; ZUNE_SW_PEEK: the original root. In the title gadget
 * module (loaded only for a MUI 5 program's too large window). */
#define ZUNE_SW_WRAP 0
#define ZUNE_SW_UNWRAP 1
#define ZUNE_SW_PEEK 2
Object *zune_window_scrollwrap(Object *obj, ULONG op);
/* MUIM_Window_Snapshot: edit window id's record in the application's
 * saved settings (ENV: and ENVARC:), remember = its position from the
 * application's table, else forget it (in the title gadget module) */
void zune_window_save_snapshot(Object *obj, ULONG id, BOOL remember);
/* in classes/window.c: is the window object asleep (MUIA_Window_Sleep) */
BOOL zune_window_sleeping(Object *win);
Object *ZuneWindowActiveObject(Object *win);

/* The rounded frame types (frame_round.c): their table, up and down for
 * 5, 7, 8, A, B, or NULL. The module's code stays loaded until the
 * library's expunge, so the table is looked up once. */
struct ZuneFrameGfx;
struct ZuneFramesModule
{
    const struct ZuneFrameGfx *frames;
    UBYTE (*area_round_background)(Object *, ULONG,
        const struct ZuneFrameGfx *, BOOL);
};
/* area_round_background: Area's background under a rounded frame
 * (arearound.c); nothing drawn without the module (nor the frame) */
UBYTE zune_area_round_background(Object *obj, ULONG flags,
    const struct ZuneFrameGfx *zframe, BOOL drawframe);
/* Area's custom backfill of a rectangle (classes/area.c) */
IPTR zune_area_custom_backfill(Object *obj, LONG left, LONG top, LONG width,
    LONG height);
const struct ZuneFrameGfx *zune_round_frames_get(void);

/* List's in-place editing and drag and drop (listx.c); the library's
 * functions of the same names (zune_list_*, classes/list_private.h) call
 * them */
struct MUI_ListData;
struct MUIP_DragReport;
struct MUIP_DragDrop;
struct MUIP_CreateDragImage;
struct ListEntry;
struct ColumnInfo;
struct TagItem;
struct ZuneListModule
{
    IPTR (*edit)(struct IClass *, Object *, LONG, LONG);
    IPTR (*end_edit)(struct IClass *, Object *, ULONG, BOOL);
    void (*layout_editor)(Object *, struct MUI_ListData *);
    IPTR (*drag_report)(struct IClass *, Object *, struct MUIP_DragReport *);
    IPTR (*drag_drop)(struct IClass *, Object *, struct MUIP_DragDrop *);
    IPTR (*create_drag_image)(struct IClass *, Object *,
        struct MUIP_CreateDragImage *);
    void (*draw_sort_mark)(Object *, struct MUI_ListData *, LONG, LONG, LONG);
    BOOL (*column_order)(struct MUI_ListData *, const UBYTE *);
    LONG (*cell_dims)(Object *, struct MUI_ListData *, struct ListEntry *,
        LONG);
    void (*draw_cell)(Object *, Object *, LONG, LONG, LONG, LONG);
    void (*layout_horiz)(Object *, struct MUI_ListData *, LONG, LONG, LONG,
        LONG);
    void (*check_horiz)(Object *, struct MUI_ListData *);
    APTR (*draw_title_strip)(Object *, struct MUI_ListData *);
    IPTR (*sort_entries)(struct IClass *, Object *, APTR *);
    void (*set_columns)(Object *, struct MUI_ListData *, struct TagItem *,
        BOOL);
    void (*format_mui45)(struct ColumnInfo *, IPTR *);
};

/* MUI 4/5 attributes of the core classes that MUI 3 programs never use
 * (mui45.c). area: Area's MUIA_FrameVisible and MUIA_FrameDynamic; with
 * store set, OM_GET (TRUE when answered), else OM_NEW/OM_SET of value. */
struct MUI_StringData;
struct ZuneStringOps;
struct TagItem;
struct ZunePointer;
struct ZText;
struct TextFont;
struct ZuneMui45Module
{
    IPTR (*area)(Object *, ULONG, IPTR, IPTR *);
    IPTR (*string)(Object *, struct MUI_StringData *, ULONG,
        struct TagItem *, IPTR *);
    void (*string_draw)(Object *, struct MUI_StringData *, ULONG, ULONG,
        LONG, LONG);
    LONG (*string_pos_at)(Object *, struct MUI_StringData *, LONG, LONG);
    IPTR (*string_textinput)(struct IClass *, Object *, Msg,
        const struct ZuneStringOps *);
    Object *(*pointer_image)(struct Window *, ULONG, struct ZunePointer **);
    void (*text_shortened)(struct ZText *, Object *, WORD, WORD, WORD, LONG);
    LONG (*text_mark)(struct ZText *, Object *, LONG, LONG, LONG, LONG, LONG,
        BOOL);
    STRPTR (*text_copy)(struct ZText *, LONG, LONG);
    BOOL (*text_argb)(struct RastPort *, CONST_STRPTR, ULONG, ULONG, ULONG);
    Object *(*image_button)(CONST_STRPTR, CONST_STRPTR, struct TextFont *);
    BOOL (*case_shortcut)(Object *, struct Menu *, ULONG);
    BOOL (*menu_verify)(Object *, Object *, struct IntuiMessage *);
    IPTR (*area_aros)(struct IClass *, Object *, Msg);
};
/* area_aros: Area's MUIM_DrawBackgroundBuffered,
 * MUIM_QueryFrameCharacteristics and MUIM_CreateFrameClippingRegion
 * (mui45.c), which only AROS programs send; 0 without the module */
IPTR zune_area_aros45(struct IClass *cl, Object *obj, Msg msg);
/* menu_verify: Window's IDCMP_MENUVERIFY (mui45.c); FALSE without the
 * module: the message is not answered then */
BOOL zune_window_menu_verify(Object *win, Object *strip,
    struct IntuiMessage *imsg);
/* case_shortcut: a case-sensitive menu strip's shortcut triggered
 * (mui45.c); FALSE without the module or a matching item */
BOOL zune_window_case_shortcut(Object *win, struct Menu *menu, ULONG key);
IPTR zune_area_mui45(Object *obj, ULONG attr, IPTR value, IPTR *store);
/* string: String's kept Textinput attributes and MUIA_String_Integer64
 * (mui45.c says how) */
IPTR zune_string_mui45(Object *obj, struct MUI_StringData *data, ULONG attr,
    struct TagItem *tag, IPTR *store);
/* string_draw: String's MUIM_Draw of MUI 5's placeholder (what
 * ZUNE_SD_PLACEHOLDER) or of a Multiline String (ZUNE_SD_MULTILINE, with
 * ZUNE_SD_UPDATE for MADF_DRAWUPDATE and ZUNE_SD_MARKED when the range is
 * marked); string_pos_at: a Multiline String's buffer position at a point.
 * Without the module nothing is drawn and the position is 0. */
#define ZUNE_SD_PLACEHOLDER 0
#define ZUNE_SD_MULTILINE   1
#define ZUNE_SD_UPDATE      2
#define ZUNE_SD_MARKED      4
/* ZUNE_SD_GADGET: the field of a MUI 3.8 program's string gadget
 * (string_private.h) filled with the background pen the program changed
 * there, or with ZUNE_SD_UPDATE its background when it changed none */
#define ZUNE_SD_GADGET      8
void zune_string_draw45(Object *obj, struct MUI_StringData *data,
    ULONG what, ULONG textpen, LONG markstart, LONG markstop);
LONG zune_string_pos_at(Object *obj, struct MUI_StringData *data, LONG x,
    LONG y);
/* string_textinput: String's MUIM_Textinput_* methods (a MUI 4/5 program
 * sends them; a program that gets MUI 4/5 has its keys sent as them), on
 * the library's editing through ops (classes/string_private.h). Without
 * the module a method does nothing (0). */
IPTR zune_string_textinput(struct IClass *cl, Object *obj, Msg msg,
    const struct ZuneStringOps *ops);
/* pointer_image: MUIA_PointerType's drawn shapes (pointer45.c): the
 * pointerclass image of a shape for the window, kept in *storage
 * (pointer.h), or NULL (no module, no memory: the normal pointer) */
Object *zune_pointer_image(struct Window *window, ULONG type,
    struct ZunePointer **storage);
/* image_button: MUI_MakeObject(MUIO_ImageButton) (AROS's, makeobj45.c) */
Object *zune_image_button(CONST_STRPTR label, CONST_STRPTR imagePath,
    struct TextFont *textFont);

/* Configdata's setters (configset.c): MUIM_Configdata_SetULong,
 * SetImspec, SetFramespec, SetPenspec, SetFont, with the defaults to
 * compare with */
struct ZuneCfgTables;
struct ZunePrefsModule
{
    IPTR (*configdata_set)(Object *, Msg, const struct ZuneCfgTables *);
};

/* Application's rarely used parts (appx.c, classes/application_private.h):
 * the library's functions of these names call them */
struct MUI_ApplicationData;
struct RexxMsg;
struct AppMessage;
struct ZuneAppModule
{
    IPTR (*method)(struct IClass *, Object *, Msg);
    void (*rexx)(struct MUI_ApplicationData *, Object *, struct RexxMsg *);
    void (*iconify)(struct MUI_ApplicationData *, Object *, BOOL);
    void (*cxmsg)(struct MUI_ApplicationData *, Object *, APTR);
    BOOL (*appmsg)(struct MUI_ApplicationData *, Object *,
        struct AppMessage *);
    void (*version)(struct MUI_ApplicationData *);
    void (*usedclasses)(STRPTR *);
};
/* the methods: 0 without the module */
IPTR zune_app_method(struct IClass *cl, Object *obj, Msg msg);
/* an ARexx command: the message's result fields stay as the library set
 * them (RETURN_FAIL) without the module */
void zune_app_rexx(struct MUI_ApplicationData *data, Object *obj,
    struct RexxMsg *rmsg);
/* Workbench's icon and menu item while iconified: none without it */
void zune_app_iconify(struct MUI_ApplicationData *data, Object *obj, BOOL on);
/* a commodities message, an AppMessage (TRUE: replied already) */
void zune_app_cxmsg(struct MUI_ApplicationData *data, Object *obj, APTR msg);
BOOL zune_app_appmsg(struct MUI_ApplicationData *data, Object *obj,
    struct AppMessage *appmsg);
void zune_app_version(struct MUI_ApplicationData *data);
void zune_app_usedclasses(STRPTR *list);
IPTR zune_configdata_set(Object *obj, Msg msg, const struct ZuneCfgTables *t);

/* In a module built as a file: take the library bases over again before
 * code that reads CyberGfxBase, which the library opens at the first
 * window setup on a deep screen, possibly after the file was loaded. */
#ifdef ZUNE_CLASSFILE
void zune_cf_refresh(void);
#define ZUNE_MODULE_REFRESH() zune_cf_refresh()
#else
#define ZUNE_MODULE_REFRESH() ((void)0)
#endif

#endif /* ZUNE_MODULES_H */
