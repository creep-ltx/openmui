/*
    Copyright (C) 2002-2026, The AROS Development Team.
    All rights reserved.
*/

/* Area's side of drag and drop: MUIM_DoDrag, the default drag image
 * (MUIM_CreateDragImage, MUIM_DeleteDragImage) and the drop target's
 * outline (MUIM_DragBegin, MUIM_DragFinish). Only a drag sends them, and
 * a drag needs the drag and drop module for its image and its events
 * anyway, so in the native build they are part of it (modules.h,
 * classes/zdragdrop.c): Area's dispatcher hands these methods to
 * zune_area_drag(). Other builds include this file in classes/area.c. */

#include <exec/types.h>
#include <graphics/gfxmacros.h>
#include <devices/inputevent.h>
#include <libraries/commodities.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include "mui.h"
#include "muimaster_intern.h"
#include "support.h"
#include "frame.h"
#include "modules.h"

extern struct Library *MUIMasterBase;

/* The drop target's dashed outline on its box, in the shine and shadow
 * pens; erased by redrawing the object under a clip of the four edges.
 * Nothing for an object that cannot draw; the window's root gets no
 * outline (its erasing redraws its edges, as it always did). */
static void area_drop_outline(Object *obj, BOOL draw)
{
    Object *parent;

    if (!(_flags(obj) & MADF_CANDRAW))
        return;
    if (draw)
    {
        if (!(parent = _parent(obj)) || parent == _win(obj))
            return;

        struct RastPort *rp = _rp(obj);
        UWORD oldDrPt = rp->LinePtrn;
        UBYTE oldDrMd = rp->DrawMode, oldBPen = rp->BgPen;

        SetABPenDrMd(rp, _pens(obj)[MPEN_SHINE], _pens(obj)[MPEN_SHADOW],
            JAM2);
        SetDrPt(rp, 0xF0F0);
        Move(rp, _left(obj), _top(obj));
        Draw(rp, _right(obj), _top(obj));
        Draw(rp, _right(obj), _bottom(obj));
        Draw(rp, _left(obj), _bottom(obj));
        Draw(rp, _left(obj), _top(obj));
        SetDrPt(rp, oldDrPt);
        SetBPen(rp, oldBPen);
        SetDrMd(rp, oldDrMd);
    }
    else
    {
        struct Region *region = NewRegion();
        APTR clip = NULL;

        if (region)
        {
            /* the four edges, one pixel wide: top, right, bottom, left;
             * a bit per corner: x1 (1), y1 (2), x2 (4), y2 (8) choose
             * right or bottom instead of left or top */
            static const UBYTE edge[4] = {4, 1 | 4 | 8, 2 | 4 | 8, 8};
            WORD xs[2] = {_left(obj), _right(obj)};
            WORD ys[2] = {_top(obj), _bottom(obj)};
            struct Rectangle rect;
            int i;

            for (i = 0; i < 4; i++)
            {
                UBYTE e = edge[i];

                rect.MinX = xs[e & 1];
                rect.MinY = ys[(e >> 1) & 1];
                rect.MaxX = xs[(e >> 2) & 1];
                rect.MaxY = ys[e >> 3];
                OrRectRegion(region, &rect);
            }
            clip = MUI_AddClipRegion(muiRenderInfo(obj), region);
        }
        MUI_Redraw(obj, MADF_DRAWOBJECT | MADF_DRAWFRAME);
        if (region)
            MUI_RemoveClipRegion(muiRenderInfo(obj), clip);
    }
}

/* The object's pixels copied into a bitmap with the settings' drag frame
 * around them (MUIV_Frame_Drag); NULL when the structure cannot be
 * allocated, bm NULL when the bitmap cannot */
static struct MUI_DragImage *area_drag_image(Object *obj,
    struct MUIP_CreateDragImage *msg)
{
    struct MUI_DragImage *img = AllocVec(sizeof(struct MUI_DragImage),
        MEMF_CLEAR);

    if (img)
    {
        const struct ZuneFrameGfx *zframe = zune_zframe_get(obj,
            &muiGlobalInfo(obj)->mgi_Prefs->frames[MUIV_Frame_Drag]);
        LONG depth = GetBitMapAttr(_screen(obj)->RastPort.BitMap, BMA_DEPTH);

        img->width = _width(obj) + zframe->ileft + zframe->iright;
        img->height = _height(obj) + zframe->itop + zframe->ibottom;
        if ((img->bm = AllocBitMap(img->width, img->height, depth,
            BMF_MINPLANES, _screen(obj)->RastPort.BitMap)))
        {
            struct RastPort *rp_save = muiRenderInfo(obj)->mri_RastPort;
            struct RastPort temprp;

            InitRastPort(&temprp);
            temprp.BitMap = img->bm;
            ClipBlit(_rp(obj), _left(obj), _top(obj), &temprp,
                zframe->ileft, zframe->itop, _width(obj), _height(obj), 0xc0);
            muiRenderInfo(obj)->mri_RastPort = &temprp;
            zframe->draw(zframe->customframe, muiRenderInfo(obj), 0, 0,
                img->width, img->height, 0, 0, img->width, img->height);
            muiRenderInfo(obj)->mri_RastPort = rp_save;
        }
        img->touchx = msg->touchx;
        img->touchy = msg->touchy;
        img->flags = 0;
    }
    return img;
}

/* A move with the left button held over a draggable object: the drag
 * starts when the pointer has gone the settings' distance from the click
 * (Navigation page: the autostart distance, or one pixel with the drag
 * qualifier held; MUI 3.8's defaults autostart, 3 pixels, control).
 * TRUE when it started. */
#define DRAG_QUALIFIERS (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT | \
    IEQUALIFIER_CAPSLOCK | IEQUALIFIER_CONTROL | IEQUALIFIER_LALT | \
    IEQUALIFIER_RALT | IEQUALIFIER_LCOMMAND | IEQUALIFIER_RCOMMAND)

BOOL zune_area_drag_motion(Object *obj, struct IntuiMessage *imsg)
{
    struct MUI_AreaData *data = muiAreaData(obj);
    struct MUI_AreaLive *live = data->mad_Live;
    struct ZunePrefsNew *prefs = muiGlobalInfo(obj)->mgi_Prefs;
    const ZuneKeySpec *k = &prefs->drag_left_modifier;
    LONG dist = 0, dx, dy;

    if (prefs->drag_left_button && k->ix_well)
    {
        /* the qualifier held, as commodities' MatchIX compares them,
         * mouse buttons left aside */
        UWORD want = k->ix.ix_Qualifier & DRAG_QUALIFIERS;
        UWORD qual = imsg->Qualifier & DRAG_QUALIFIERS;

        if (k->ix.ix_QualSame & IXSYM_SHIFT)
        {
            if (qual & IXSYM_SHIFTMASK) qual |= IXSYM_SHIFTMASK;
            if (want & IXSYM_SHIFTMASK) want |= IXSYM_SHIFTMASK;
        }
        if (k->ix.ix_QualSame & IXSYM_ALT)
        {
            if (qual & IXSYM_ALTMASK) qual |= IXSYM_ALTMASK;
            if (want & IXSYM_ALTMASK) want |= IXSYM_ALTMASK;
        }
        if (want && (qual & want) == want)
            dist = 1;
    }
    if (!dist && prefs->drag_autostart)
        dist = MAX(prefs->drag_autostart_length, 1);
    dx = live->ClickX - imsg->MouseX;
    dy = live->ClickY - imsg->MouseY;
    if (!dist || (dx < dist && -dx < dist && dy < dist && -dy < dist))
        return FALSE;
    if (data->mad_InputMode == MUIV_InputMode_RelVerify)
        set(obj, MUIA_Selected, FALSE);
    nnset(obj, MUIA_Pressed, FALSE);
    if (live->ehn.ehn_Events)
        DoMethod(_win(obj), MUIM_Window_RemEventHandler, (IPTR)&live->ehn);
    live->ehn.ehn_Events = IDCMP_MOUSEBUTTONS;
    DoMethod(_win(obj), MUIM_Window_AddEventHandler, (IPTR)&live->ehn);
    if (live->x && live->x->Timer.ihn_Millis)
    {
        DoMethod(_app(obj), MUIM_Application_RemInputHandler,
            (IPTR)&live->x->Timer);
        live->x->Timer.ihn_Millis = 0;
    }
    DoMethod(obj, MUIM_DoDrag, live->ClickX - _left(obj),
        live->ClickY - _top(obj), 0);
    return TRUE;
}

IPTR zune_area_drag(struct IClass *cl, Object *obj, Msg msg)
{
    switch (msg->MethodID)
    {
    case MUIM_DoDrag:
        {
            struct MUIP_DoDrag *m = (struct MUIP_DoDrag *)msg;
            LONG touchx = m->touchx, touchy = m->touchy;

            if (touchx == 0x80000000)
                touchx = _window(obj)->MouseX - _left(obj);
            if (touchy == 0x80000000)
                touchy = _window(obj)->MouseY - _top(obj);
            DoMethod(_win(obj), MUIM_Window_DragObject, (IPTR)obj, touchx,
                touchy, m->flags);
        }
        break;
    case MUIM_CreateDragImage:
        return (IPTR)area_drag_image(obj, (struct MUIP_CreateDragImage *)msg);
    case MUIM_DeleteDragImage:
        {
            struct MUI_DragImage *di =
                ((struct MUIP_DeleteDragImage *)msg)->di;

            if (di)
            {
                if (di->bm)
                    FreeBitMap(di->bm);
                FreeVec(di);
            }
        }
        break;
    case MUIM_DragBegin:
    case MUIM_DragFinish:
        area_drop_outline(obj, msg->MethodID == MUIM_DragBegin);
        break;
    }
    return 0;
}
