/* Rarely used parts of the library that are not classes, as modules: each
 * is a private class (a name only Zune uses) in a file of its own,
 * LIBS:Zune/<Name>.mui, loaded through the class-file loader (classfile.h)
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
#include "imspec_intern.h"   /* VECTOR_DRAW_FUNC */

/* The class names, and the files (gcc/classfiles.py) */
#define MUIC_Zmenu      "Zmenu.zmod"        /* Zmenu.mui: menu.c, menupopup.c */
#define MUIC_Zpicture   "Zpicture.zmod"     /* Zpicture.mui: datatypescache.c, frame_tile.c */
#define MUIC_Zdragdrop  "Zdragdrop.zmod"    /* Zdragdrop.mui: dragndrop.c, dragwin.c */
#define MUIC_Zrequest   "Zrequest.zmod"     /* Zrequest.mui: requesters.c, mui_request.c */
#define MUIC_Zgradient  "Zgradient.zmod"    /* Zgradient.mui: imspec_gradientdraw.c */
#define MUIC_Zwintitle  "Zwintitle.zmod"    /* Zwintitle.mui: wintitle.c */
#define MUIC_Zframes    "Zframes.zmod"      /* Zframes.mui: frame_round.c */
#define MUIC_Zimages    "Zimages.zmod"      /* Zimages.mui: imspec_vector_rare.c */
#define MUIC_Zlist      "Zlist.zmod"        /* Zlist.mui: listx.c */

enum
{
    ZMOD_MENU, ZMOD_PICTURE, ZMOD_DRAGDROP, ZMOD_REQUEST, ZMOD_GRADIENT,
    ZMOD_WINTITLE, ZMOD_FRAMES, ZMOD_IMAGES, ZMOD_LIST,
    ZMOD_COUNT
};

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
};
/* MUIM_Menustrip_Popup (menupopup.c) */
IPTR zune_menustrip_popup(Object *obj, struct MUIP_Menustrip_Popup *msg);

struct dt_node;
struct dt_frame_image;
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
};

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
};

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
};
void zune_wintitle_create(struct Window *win, ULONG buttons, LONG right,
    struct Gadget **gadgets, Class **clp);
int zune_wintitle_menu(struct Window *pw, struct DrawInfo *dri,
    struct Gadget *popup, BOOL has_id);
/* in classes/window.c: is the window object asleep (MUIA_Window_Sleep) */
BOOL zune_window_sleeping(Object *win);

/* The rounded frame types (frame_round.c): their table, up and down for
 * 5, 7, 8, A, B, or NULL. The module's code stays loaded until the
 * library's expunge, so the table is looked up once. */
struct ZuneFrameGfx;
struct ZuneFramesModule
{
    const struct ZuneFrameGfx *frames;
};
const struct ZuneFrameGfx *zune_round_frames_get(void);

/* The rare vector images 10-23 (imspec_vector_rare.c): their drawing
 * functions, or NULL. Looked up once, as the round frames. */
struct ZuneImagesModule
{
    const VECTOR_DRAW_FUNC *vectors;
};
const VECTOR_DRAW_FUNC *zune_rare_vectors_get(void);

/* List's in-place editing and drag and drop (listx.c); the library's
 * functions of the same names (zune_list_*, classes/list_private.h) call
 * them */
struct MUI_ListData;
struct MUIP_DragReport;
struct MUIP_DragDrop;
struct MUIP_CreateDragImage;
struct ZuneListModule
{
    IPTR (*edit)(struct IClass *, Object *, LONG, LONG);
    IPTR (*end_edit)(struct IClass *, Object *, ULONG, BOOL);
    void (*layout_editor)(Object *, struct MUI_ListData *);
    IPTR (*drag_report)(struct IClass *, Object *, struct MUIP_DragReport *);
    IPTR (*drag_drop)(struct IClass *, Object *, struct MUIP_DragDrop *);
    IPTR (*create_drag_image)(struct IClass *, Object *,
        struct MUIP_CreateDragImage *);
};

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
