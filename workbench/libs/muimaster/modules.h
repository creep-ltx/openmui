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

/* The class names, and the files (gcc/classfiles.py) */
#define MUIC_Zmenu      "Zmenu.zmod"        /* Zmenu.mui: menu.c */
#define MUIC_Zpicture   "Zpicture.zmod"     /* Zpicture.mui: datatypescache.c, frame_tile.c */
#define MUIC_Zdragdrop  "Zdragdrop.zmod"    /* Zdragdrop.mui: dragndrop.c */
#define MUIC_Zrequest   "Zrequest.zmod"     /* Zrequest.mui: requesters.c, mui_request.c */
#define MUIC_Zgradient  "Zgradient.zmod"    /* Zgradient.mui: imspec_gradientdraw.c */

enum
{
    ZMOD_MENU, ZMOD_PICTURE, ZMOD_DRAGDROP, ZMOD_REQUEST, ZMOD_GRADIENT,
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
struct ZuneMenuModule
{
    struct ZMenu *(*open_menu_at)(struct Window *, struct NewMenu *, LONG,
        LONG);
    void (*mouse_update)(struct ZMenu *, int);
    int (*poll_menu)(struct ZMenu *, struct MenuItem **);
    struct MenuItem *(*leave_menu)(struct ZMenu *);
    void (*close_menu)(struct ZMenu *);
};

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
