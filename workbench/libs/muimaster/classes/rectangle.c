/*
    Copyright (C) 2002-2026, The AROS Development Team.
    Copyright (C) 1999, David Le Corfec.
    All rights reserved.

*/

#include <string.h>

#include <exec/types.h>

#include <clib/alib_protos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/utility.h>
#include <proto/exec.h>
#include <proto/muimaster.h>

#include "mui.h"
#include "muimaster_intern.h"
#include "support.h"
#include "textengine.h"
#include "prefs.h"

/*  #define MYDEBUG 1 */
#include "debug.h"

/****** Rectangle.mui/MUIC_Rectangle ************
*
*   NAME
*       MUIC_Rectangle
*
*   SUPERCLASS
*       Area.mui
*
*   DESCRIPTION
*       Provide flexible space, a separator line or a title. The default has no
*       title or separator. Construction prepends the title-font selection;
*       caller tags can override it. This class has no local setter for its
*       attributes. Version and Revision return the built-in class version,
*       which equals the library version (21.227) by default and is 1.1 in a
*       MUI5_TEST=0 build.
*       A title is copied with StrDup during construction and freed on disposal.
*       Setup creates a separate Zune text object; Cleanup destroys that object
*       before calling Area. Complete Cleanup before disposal. Cleanup does not
*       clear its text pointer, so repeating Cleanup without a matching Setup
*       can free stale storage.
*
*   METHODS
*       Setup returns FALSE on Area setup failure, otherwise TRUE even if text
*       allocation fails. With a stored title, AskMinMax and Draw dereference
*       the text object without checking that allocation succeeded. Constructor
*       title-copy failure instead leaves the object without a title and does
*       not fail construction.
*       Without a title, AskMinMax adds two pixels to both minimum width and
*       minimum height for HBar or VBar. With a title it uses measured text
*       dimensions, adding one pixel in each dimension for the 3D title
*       preference. Default sizes equal resulting minimum sizes; both maxima
*       become MUI_MAXMAX.
*       AskMinMax returns TRUE.
*       Draw first calls Area and returns zero unless MADF_DRAWOBJECT is set. It
*       returns TRUE for empty content geometry or after drawing. A title is
*       centered and colored using the group-title preference. HBar adds
*       separated line segments on either side when space permits; VBar has no
*       line when a title is present. Without a title, HBar or VBar paints a
*       centered two-pixel separator. A normal rectangle draws no additional
*       content in a nondebug build.
*
*   NOTES
*       Use a valid connected render context and normal
*       Setup/AskMinMax/Draw/Cleanup ordering. No native gadget is created. Bar
*       tags select a type by presence, regardless of their BOOL value; if both
*       are present, the last one wins. Repeating BarTitle at construction leaks
*       earlier copies because each tag overwrites the previous pointer without
*       freeing it.
*
*/
/****** Rectangle.mui/MUIA_Rectangle_BarTitle ************
*
*   NAME
*       MUIA_Rectangle_BarTitle
*
*   FUNCTION
*       Supply or retrieve the separator title.
*
*   INPUTS
*       Initialize and get, STRPTR. Default NULL. Supply a readable
*       NUL-terminated string; NULL leaves no title.
*
*   NOTES
*       The constructor copies the string. Get returns the owned copy as a
*       borrowed pointer; do not free or modify it, and do not retain it after
*       disposal. No local setter exists. Use at most one BarTitle tag: repeated
*       tags leak earlier copies. Copy failure leaves no title but does not fail
*       construction.
*       The Zune text engine parses the title at Setup. An empty string remains
*       a present title and takes the title layout/drawing path. With HBar, the
*       line is split around centered text; with VBar, only the title is drawn.
*       Title allocation at Setup is not checked before later sizing/drawing
*       dereferences the text object.
*
*/
/****** Rectangle.mui/MUIA_Rectangle_HBar ************
*
*   NAME
*       MUIA_Rectangle_HBar
*
*   FUNCTION
*       Select or query a horizontal separator.
*
*   INPUTS
*       Initialize and get, BOOL. Default FALSE when no bar tag is supplied.
*
*   NOTES
*       The presence of this constructor tag selects its separator even when its
*       value is FALSE. If HBar and VBar both occur, the last tag selects the
*       type; getters report only that selected type. No local setter exists.
*       Without a title, the separator is a centered two-pixel line and adds two
*       pixels to both minimum dimensions. With a title, HBar draws
*       side segments if space permits; VBar draws no separator line. Both
*       maximum size dimensions remain flexible.
*
*/
/****** Rectangle.mui/MUIA_Rectangle_VBar ************
*
*   NAME
*       MUIA_Rectangle_VBar
*
*   FUNCTION
*       Select or query a vertical separator.
*
*   INPUTS
*       Initialize and get, BOOL. Default FALSE when no bar tag is supplied.
*
*   NOTES
*       The presence of this constructor tag selects its separator even when its
*       value is FALSE. If HBar and VBar both occur, the last tag selects the
*       type; getters report only that selected type. No local setter exists.
*       Without a title, the separator is a centered two-pixel line and adds two
*       pixels to both minimum dimensions. With a title, HBar draws
*       side segments if space permits; VBar draws no separator line. Both
*       maximum size dimensions remain flexible.
*
*/

extern struct Library *MUIMasterBase;

#include "native_version.h"

/* A bar title and its parsed text, one block; made with the title */
struct RectTitle
{
    ZText *ztext;
    char text[1];
};

/* 4 bytes (was 12; MUI 3.8: 6): most rectangles are spaces without a
 * title or a bar, and with Area's 136 bytes and the object's header 12
 * more made a 160 byte block of what is now 152. */
struct Rectangle_DATA
{
    IPTR title;     /* a struct RectTitle * (NULL: no bar title) with
                     * the RECTANGLE_TYPE_* in its two low bits */
};
#define RECT_TYPE(d) ((d)->title & 3)
#define RECT_TITLE(d) ((struct RectTitle *)((d)->title & ~3))

#define RECTANGLE_TYPE_NORMAL 0
#define RECTANGLE_TYPE_HBAR 1
#define RECTANGLE_TYPE_VBAR 2

/*
 * A bar's two lines: the shadow pen's at y, the shine pen's at y + 1 (a
 * horizontal bar) or at x + 1 (vertical: x1 == x2), each from (x1, y1)
 * to (x2, y2). Leaves the shine pen set and the pen at the second line's
 * end, as the lines drawn one by one did.
 */
static __attribute__((noinline)) void bar_lines(Object *obj,
    LONG x1, LONG y1, LONG x2, LONG y2)
{
    struct RastPort *rp = _rp(obj);
    const UWORD *pens = _pens(obj);
    LONG dx = x1 == x2, dy = !dx;

    SetAPen(rp, pens[MPEN_SHADOW]);
    Move(rp, x1, y1);
    Draw(rp, x2, y2);
    SetAPen(rp, pens[MPEN_SHINE]);
    Move(rp, x1 + dx, y1 + dy);
    Draw(rp, x2 + dx, y2 + dy);
}


/* The bar title copied into its block (the last one given counts) */
static __attribute__((noinline)) void Rectangle_SetTitle(
    struct Rectangle_DATA *data, CONST_STRPTR title)
{
    struct RectTitle *t = NULL;

    FreeVec(RECT_TITLE(data));
    if (title && (t = AllocVec(sizeof(struct RectTitle) + strlen(title),
            MEMF_CLEAR)))
        strcpy(t->text, title);
    data->title = (data->title & 3) | (IPTR) t;
}

/* A bar title, centred, over a horizontal bar's two lines left and right
 * of it (a vertical bar with a title draws the title alone). Tail-called
 * from Draw: its frame is not added to Draw's. */
static __attribute__((noinline, noipa)) IPTR rect_title(Object *obj,
    struct RectTitle *title, int type)
{
    ZText *zt = title->ztext;
    int color = muiGlobalInfo(obj)->mgi_Prefs->group_title_color;
    LONG d3 = color == GROUP_TITLE_COLOR_3D;
    LONG x1 = _mleft(obj) + (_mwidth(obj) - (zt->width + d3)) / 2 - d3;
    LONG x2 = x1 + zt->width + d3;
    LONG yt = _mtop(obj) + (_mheight(obj) - (zt->height + d3)) / 2;

    SetAPen(_rp(obj), _pens(obj)[MPEN_SHADOW]);
    if (d3)
    {
        /* the shadow one pixel down and right, the shine over it */
        zune_text_draw(zt, obj, x1 + 1, x2, yt + 1);
        SetAPen(_rp(obj), _pens(obj)[MPEN_SHINE]);
    }
    else if (color == GROUP_TITLE_COLOR_HILITE)
        SetAPen(_rp(obj), _pens(obj)[MPEN_SHINE]);
    zune_text_draw(zt, obj, x1, x2 - 1, yt);

    if (type == RECTANGLE_TYPE_HBAR && (x1 - 2) > _mleft(obj)
        && (x2 + 2) < _mright(obj))
    {
        yt += zt->height / 2;
        bar_lines(obj, _mleft(obj), yt, x1 - 3 + d3, yt);
        bar_lines(obj, x2 + 2 + d3, yt, _mright(obj), yt);
    }
    return TRUE;
}

/* A bar without a title: two lines across the middle. Tail-called from
 * Draw. */
static __attribute__((noinline, noipa)) IPTR rect_bar(Object *obj, int type)
{
    struct RastPort *rp = _rp(obj);
    LONG t = _mtop(obj);

    if (type == RECTANGLE_TYPE_HBAR)
    {
        LONG y = t + _mheight(obj) / 2 - 1;

        bar_lines(obj, _mleft(obj), y, _mright(obj), y);
    }
    else if (type == RECTANGLE_TYPE_VBAR)
    {
        /* with thick frames MUI 3.8 doubles both lines, like the
         * sides of a frame, also on a bar narrower than the four
         * pixels (demopix: MUIO_VBar 1 and 2 in the Menus and Subtask
         * demos) */
        LONG x = _mleft(obj) + _mwidth(obj) / 2 - 1, b = _mbottom(obj);

        if (!(muiRenderInfo(obj)->mri_Flags & MUIMRI_THINFRAMES)
            && ZUNE_APP_MUI38(obj))
        {
            SetAPen(rp, _pens(obj)[MPEN_SHADOW]);
            Move(rp, x - 1, t);
            Draw(rp, x - 1, b);
            bar_lines(obj, x, t, x, b);
            Move(rp, x + 2, t);
            Draw(rp, x + 2, b);
        }
        else
            bar_lines(obj, x, t, x, b);
    }
    return TRUE;
}

/*
 * Construct Area with overridable title-font default; parse bar tags by
 * presence and duplicate each title tag. Return object or zero on
 * superclass failure. Copy failure does not fail construction; repeated
 * title tags leak earlier copies. Title storage is owned until disposal.
 */
IPTR Rectangle__OM_NEW(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct Rectangle_DATA *data;
    struct TagItem *tags, *tag;

    obj = (Object *) DoSuperNewTags(cl, obj, NULL,
        MUIA_Font, MUIV_Font_Title, TAG_MORE, (IPTR) msg->ops_AttrList);

    if (!obj)
        return 0;

    /* Initial local instance data */
    data = INST_DATA(cl, obj);

    /* parse initial taglist */

    for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));)
    {
        switch (tag->ti_Tag)
        {
        case MUIA_Rectangle_BarTitle:
            Rectangle_SetTitle(data, (STRPTR) tag->ti_Data);
            break;
        case MUIA_Rectangle_HBar:
            data->title = (data->title & ~3) | RECTANGLE_TYPE_HBAR;
            break;
        case MUIA_Rectangle_VBar:
            data->title = (data->title & ~3) | RECTANGLE_TYPE_VBAR;
            break;
        }
    }

    D(bug("muimaster.library/rectangle.c: New Rectangle Object at 0x%p\n",
            obj));

    return (IPTR) obj;
}

/*
 * Free the owned title via mui_free and return Area disposal result.
 * Requires prior matched Cleanup to release parsed text; this method does
 * not destroy ztext itself. No borrowed caller string is freed.
 */
IPTR Rectangle__OM_DISPOSE(struct IClass *cl, Object *obj, Msg msg)
{
    struct Rectangle_DATA *data = INST_DATA(cl, obj);

    FreeVec(RECT_TITLE(data));

    return DoSuperMethodA(cl, obj, msg);
}


/*
 * Write built-in version/revision, borrowed title pointer, or selected-bar
 * BOOL to valid caller-owned IPTR storage and return TRUE. Delegate other
 * attributes to Area. No ownership transfer or allocation.
 */
IPTR Rectangle__OM_GET(struct IClass *cl, Object *obj, struct opGet *msg)
{
/*---------------------------------------------------------------------------*/
/* small macro to simplify return value storage */
/*---------------------------------------------------------------------------*/
#define STORE *(msg->opg_Storage)

    struct Rectangle_DATA *data = INST_DATA(cl, obj);

    switch (msg->opg_AttrID)
    {
    case MUIA_Version:
        STORE = ZUNE68_BUILTIN_VERSION;
        return TRUE;

    case MUIA_Revision:
        STORE = ZUNE68_BUILTIN_REVISION;
        return TRUE;

    case MUIA_Rectangle_BarTitle:
        STORE = RECT_TITLE(data) ? (IPTR) RECT_TITLE(data)->text : 0;
        return TRUE;

    case MUIA_Rectangle_HBar:
        STORE = (RECT_TYPE(data) == RECTANGLE_TYPE_HBAR);
        return TRUE;

    case MUIA_Rectangle_VBar:
        STORE = (RECT_TYPE(data) == RECTANGLE_TYPE_VBAR);
        return TRUE;
    }

/* our handler didn't understand the attribute, we simply pass
** it to our superclass now
*/
    return DoSuperMethodA(cl, obj, (Msg) msg);
#undef STORE
}

/*
 * Call Area setup; if successful, parse a present title into owned ZText.
 * Return FALSE on superclass failure, otherwise TRUE even when parsing
 * allocation fails. Requires live render context and paired Cleanup; later
 * title sizing assumes success.
 */
IPTR Rectangle__MUIM_Setup(struct IClass *cl, Object *obj,
    struct MUIP_Setup *msg)
{
    struct Rectangle_DATA *data = INST_DATA(cl, obj);

    if (!(DoSuperMethodA(cl, obj, (Msg) msg)))
        return FALSE;

    if (RECT_TITLE(data))
    {
        RECT_TITLE(data)->ztext = zune_text_new(NULL,
            RECT_TITLE(data)->text, ZTEXT_ARG_NONE, 0);
    }

    return TRUE;
}

/*
 * Destroy non-NULL parsed text then return Area Cleanup result. Requires
 * matching successful Setup and no later drawing. Does not clear the freed
 * pointer, so duplicate Cleanup is unsafe. Title copy remains owned until
 * disposal.
 */
IPTR Rectangle__MUIM_Cleanup(struct IClass *cl, Object *obj,
    struct MUIP_Cleanup *msg)
{
    struct Rectangle_DATA *data = INST_DATA(cl, obj);

    if (RECT_TITLE(data) && RECT_TITLE(data)->ztext)
        zune_text_destroy(RECT_TITLE(data)->ztext);

    return DoSuperMethodA(cl, obj, (Msg) msg);
}

/**************************************************************************
 MUIM_AskMinMax

 AskMinMax method will be called before the window is opened
 and before layout takes place. We need to tell MUI the
 minimum, maximum and default size of our object.
**************************************************************************/
/*
 * Call Area then add two pixels in both dimensions for a bar, or parsed
 * title dimensions and 3D preference padding. Set defaults to minima and
 * maxima to MUI_MAXMAX; return TRUE. Requires writable size structure, live
 * preferences and valid parsed title when present; allocation failure is
 * not checked.
 */
IPTR Rectangle__MUIM_AskMinMax(struct IClass *cl, Object *obj,
    struct MUIP_AskMinMax *msg)
{
    struct Rectangle_DATA *data = INST_DATA(cl, obj);

    /*
     ** let our superclass first fill in what it thinks about sizes.
     ** this will e.g. add the size of frame and inner spacing.
     */
    DoSuperMethodA(cl, obj, (Msg) msg);

    /*
     ** now add the values specific to our object. note that we
     ** indeed need to *add* these values, not just set them!
     */

    if (!RECT_TITLE(data))
    {
/*          msg->MinMaxInfo->MinWidth += 1; */
/*          msg->MinMaxInfo->MinHeight += 1;             */
        /* MUI 3.9 makes both kinds of bar at least 2x2; MUI 3.8 with
         * thick frames 4x2, also MUIO_VBar 1 or 2 (demopix: the Menus and
         * Subtask demos, measured with tests/lean/probe/objdump.c) */
        if (RECT_TYPE(data) == RECTANGLE_TYPE_HBAR
            || RECT_TYPE(data) == RECTANGLE_TYPE_VBAR)
        {
            msg->MinMaxInfo->MinWidth +=
                !(muiRenderInfo(obj)->mri_Flags & MUIMRI_THINFRAMES)
                && ZUNE_APP_MUI38(obj) ? 4 : 2;
            msg->MinMaxInfo->MinHeight += 2;
        }
    }
    else
    {
        zune_text_get_bounds(RECT_TITLE(data)->ztext, obj);
        /* MUI 3.8: a bar title is 12 pixels wider and 1 taller than its text
         * (Busy 2.4's demo dumped: 84x9 for "Speed: 20" in topaz 8, Lamp's
         * demo 40x10 with 3D titles; OpenMUI had 72x8 and 28x9). MUI 5: 4
         * wider and no taller, in its title font (58x6 for the same title,
         * tests/lean/probe/bars.c on MUI 5 and MUI 3.8 side by side) */
        if (ZUNE_APP_MUI45(obj))
        {
            msg->MinMaxInfo->MinWidth += RECT_TITLE(data)->ztext->width + 4;
            msg->MinMaxInfo->MinHeight += RECT_TITLE(data)->ztext->height;
        }
        else
        {
            msg->MinMaxInfo->MinWidth += RECT_TITLE(data)->ztext->width + 12;
            msg->MinMaxInfo->MinHeight += RECT_TITLE(data)->ztext->height + 1;
        }
        D(bug("rect: minheight %ld\n", RECT_TITLE(data)->ztext->height));
        if (muiGlobalInfo(obj)->mgi_Prefs->group_title_color ==
            GROUP_TITLE_COLOR_3D)
        {
            msg->MinMaxInfo->MinWidth += 1;
            msg->MinMaxInfo->MinHeight += 1;
        }
    }

    msg->MinMaxInfo->DefWidth = msg->MinMaxInfo->MinWidth;
    msg->MinMaxInfo->DefHeight = msg->MinMaxInfo->MinHeight;

    msg->MinMaxInfo->MaxWidth = MUI_MAXMAX;
    msg->MinMaxInfo->MaxHeight = MUI_MAXMAX;

    return TRUE;
}


/**************************************************************************
 MUIM_Draw

 Draw method is called whenever MUI feels we should render
 our object. This usually happens after layout is finished
 or when we need to refresh in a simplerefresh window.
 Note: You may only render within the rectangle
       _mleft(obj), _mtop(obj), _mwidth(obj), _mheight(obj).
**************************************************************************/
/*
 * Call Area then draw only with MADF_DRAWOBJECT and nonempty geometry.
 * Return zero without that flag, otherwise TRUE. Requires live
 * pens/preferences and measured parsed title when present. Title suppresses
 * vertical bar; horizontal bar may flank text. No allocation or result
 * propagation.
 */
IPTR Rectangle__MUIM_Draw(struct IClass *cl, Object *obj,
    struct MUIP_Draw *msg)
{
    struct Rectangle_DATA *data = INST_DATA(cl, obj);

    /*
     ** let our superclass draw itself first, area class would
     ** e.g. draw the frame and clear the whole region. What
     ** it does exactly depends on msg->flags.
     */

    DoSuperMethodA(cl, obj, (Msg) msg);

    /*
     ** if MADF_DRAWOBJECT isn't set, we shouldn't draw anything.
     ** MUI just wanted to update the frame or something like that.
     */

    if (!(msg->flags & MADF_DRAWOBJECT))
        return 0;


    D(bug("Draw Rectangle(0x%p) %ldx%ldx%ldx%ld mw=%ld mh=%ld\n", obj,
            _left(obj), _top(obj), _right(obj), _bottom(obj), _mwidth(obj),
            _mheight(obj)));

    if (_mwidth(obj) < 1 || _mheight(obj) < 1)
        return TRUE;

    if (RECT_TITLE(data))
        return rect_title(obj, RECT_TITLE(data), RECT_TYPE(data));
    if (RECT_TYPE(data))
        return rect_bar(obj, RECT_TYPE(data));
    return TRUE;
}


/*
 * Dispatch local overrides and return their results; delegate other
 * messages to Area. Requires ABI-correct message and live object except
 * construction. Private resource ownership follows the class lifecycle
 * contract.
 */
BOOPSI_DISPATCHER(IPTR, Rectangle_Dispatcher, cl, obj, msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return Rectangle__OM_NEW(cl, obj, (struct opSet *)msg);
    case OM_DISPOSE:
        return Rectangle__OM_DISPOSE(cl, obj, msg);
    case OM_GET:
        return Rectangle__OM_GET(cl, obj, (struct opGet *)msg);
    case MUIM_Setup:
        return Rectangle__MUIM_Setup(cl, obj, (APTR) msg);
    case MUIM_Cleanup:
        return Rectangle__MUIM_Cleanup(cl, obj, (APTR) msg);
    case MUIM_AskMinMax:
        return Rectangle__MUIM_AskMinMax(cl, obj, (APTR) msg);
    case MUIM_Draw:
        return Rectangle__MUIM_Draw(cl, obj, (APTR) msg);
    default:
        return DoSuperMethodA(cl, obj, msg);
    }
}
BOOPSI_DISPATCHER_END

const struct __MUIBuiltinClass _MUI_Rectangle_desc =
{
    MUIC_Rectangle,
    MUIC_Area,
    sizeof(struct Rectangle_DATA),
    (void *) Rectangle_Dispatcher
};
