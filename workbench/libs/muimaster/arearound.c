/*
    Copyright (C) 2002-2026, The AROS Development Team.
    All rights reserved.
*/

/* Area's background under a rounded frame (types 5, 7, 8, A and B): the
 * parent's background over the whole box first (it shows in the corners
 * outside the frame), then the object's own background clipped to the
 * rounded inside. The frame's drawing is in the round frame module, so in
 * the native build this is too (modules.h, classes/zframes.c): Area's
 * MUIM_Draw calls zune_area_round_background() for such a frame. Other
 * builds include it in classes/area.c. */

#include <exec/types.h>
#include <graphics/regions.h>
#include <clib/alib_protos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include "mui.h"
#include "muimaster_intern.h"
#include "support.h"
#include "frame.h"
#include "imspec.h"
#include "modules.h"

extern struct Library *MUIMasterBase;

/* flags: the draw's flags; returns no corner hints (a rounded frame has no
 * bevel corners to leave) */
UBYTE zune_area_round_background(Object *obj, ULONG flags,
    const struct ZuneFrameGfx *zframe, BOOL drawframe)
{
    struct MUI_AreaData *data = muiAreaData(obj);
    struct MUI_ImageSpec_intern *background;
    struct MUI_FrameSpec_intern tempframe;
    const struct MUI_FrameSpec_intern *frame;
    struct Rectangle r, r2, rects[4];
    struct Region *region;
    APTR clip = (APTR)-1;
    int i, bgtop, bgleft, bgw, bgh, numrects;

    /* Like MUI 3.9, only an object with its own background shows the
     * selected background; others keep their parent's. */
    background = data->mad_Background;
    if ((data->mad_Flags & (MADF_SELECTED | MADF_SHOWSELSTATE))
        == (MADF_SELECTED | MADF_SHOWSELSTATE) && background)
        background = AREA_SELBACK(data);

    bgtop = data->mad_TitleHeightAbove;
    bgleft = _left(obj);
    bgw = _width(obj);
    bgh = _height(obj) - bgtop;
    bgtop += _top(obj);
    /* the parent's background for the whole box: it fills the corners
     * outside the rounded frame */
    DoMethod(obj, MUIM_DrawParentBackground, bgleft, bgtop, bgw, bgh, bgleft,
        bgtop, flags);
    r.MinX = bgleft;
    r.MinY = bgtop;
    r.MaxX = bgleft + bgw - 1;
    r.MaxY = bgtop + bgh - 1;

    /* the object's background only inside the rounded frame: the current
     * frame's radius, not the frame type's default */
    frame = &muiGlobalInfo(obj)->mgi_Prefs->frames[MUIV_Frame_None];
    if (data->mad_Frame <= 100)
        frame = &muiGlobalInfo(obj)->mgi_Prefs->frames[data->mad_Frame];
    else if (zune_frame_spec_to_intern((CONST_STRPTR)data->mad_Frame,
        &tempframe))
        frame = &tempframe;
    if ((region = zune_frame_create_clip_region(bgleft, bgtop, bgw, bgh,
        frame->border_radius)))
        /* on failure MUI_AddClipRegion has disposed of it */
        clip = MUI_AddClipRegion(data->mad_RenderInfo, region);

    if (data->mad_Flags & MADF_FILLAREA) {
        /* MUI 3.9 passes a custom backfill the whole box (below a frame
         * title) when the frame is drawn too, and only the inner box when
         * the frame keeps its pixels (tests/native/backfillrect.c) */
        if (data->mad_Flags2 & MADF2_CUSTOMBACKFILL) {
            if (drawframe) {
                r.MinX = _left(obj);
                r.MinY = _top(obj) + data->mad_TitleHeightAbove;
                r.MaxX = _right(obj);
                r.MaxY = _bottom(obj);
            } else {
                r.MinX = _mleft(obj);
                r.MinY = _mtop(obj);
                r.MaxX = _mright(obj);
                r.MaxY = _mbottom(obj);
            }
        }
        rects[0] = r;
        numrects = 1;
    } else {
        /* only the frame's part outside the inner box */
        r2.MinX = _mleft(obj);
        r2.MinY = _mtop(obj);
        r2.MaxX = _mright(obj);
        r2.MaxY = _mbottom(obj);
        numrects = SubtractRectFromRect(&r, &r2, rects);
    }

    for (i = 0; i < numrects; i++) {
        if ((data->mad_Flags2 & (MADF2_CUSTOMBACKFILL |
                MADF2_BACKFILL_ACTIVE)) == MADF2_CUSTOMBACKFILL)
            zune_area_custom_backfill(obj, rects[i].MinX, rects[i].MinY,
                rects[i].MaxX - rects[i].MinX + 1,
                rects[i].MaxY - rects[i].MinY + 1);
        else if (background)
            /* Patterns start at the object's corner, as in MUI 3.9 */
            zune_imspec_draw_at(background, data->mad_RenderInfo,
                rects[i].MinX, rects[i].MinY,
                rects[i].MaxX - rects[i].MinX + 1,
                rects[i].MaxY - rects[i].MinY + 1, rects[i].MinX,
                rects[i].MinY, 0, ZUNE_APP_MUI38(obj) ? 0 : _left(obj),
                ZUNE_APP_MUI38(obj) ? 0 : _top(obj));
        /* without one, the parent's background drawn above stays */
    }
    if (clip != (APTR)-1)
        MUI_RemoveClipRegion(data->mad_RenderInfo, clip);

    /* the title's gap above the frame line, when the parent above has not
     * drawn it (MADF_DRAWALL) */
    if (drawframe && muiGlobalInfo(obj)->mgi_Prefs->window_redraw ==
            WINDOW_REDRAW_WITHOUT_CLEAR && bgtop > _top(obj)
        && !(flags & MADF_DRAWALL))
        DoMethod(obj, MUIM_DrawParentBackground, bgleft, _top(obj), bgw,
            bgtop - _top(obj), bgleft, _top(obj), data->mad_Flags);
    return 0;
}
