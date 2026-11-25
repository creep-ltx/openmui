/*
    Copyright  2002-2026, The AROS Development Team. All rights reserved.
*/

#include <exec/memory.h>
#include <intuition/icclass.h>
#include <intuition/gadgetclass.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/muimaster.h>
#include <proto/cybergraphics.h>

#include <cybergraphx/cybergraphics.h>
#include <graphics/rpattr.h>

#include "../datatypescache.h"
#include "../imspec_intern.h"

#include "mui.h"
#include "muimaster_intern.h"
#include "support.h"
#include "prefs.h"

/*  #define MYDEBUG 1 */
#include "debug.h"

/****** Prop.mui/MUIC_Prop ************
*
*   NAME
*       MUIC_Prop
*
*   SUPERCLASS
*       Area.mui
*
*   DESCRIPTION
*       Represent a proportional range with Entries, Visible and First.
*       Construction defaults to zero for those values, vertical orientation and
*       no window border; DeltaFactor defaults to one. Maintain a coherent
*       nonnegative range with Visible no greater than Entries and First no
*       greater than Entries minus Visible. The implementation does not enforce
*       all these invariants.
*       Prop owns its in-content Intuition propgclass gadget, created by Show
*       and disposed by Hide. In window-border mode it instead borrows the
*       window gadget. Setup registers an IDCMPUPDATE handler and allocates a
*       gadget ID for nonborder mode; Cleanup removes the handler and releases
*       that ID. Pair Hide before Cleanup and complete both before disposal. No
*       local Dispose operation substitutes for removing the event handler.
*
*   METHODS
*       Show returns Area's result even if gadget creation fails, a requested
*       border gadget is unavailable, or virtual-group clipping prevents
*       creation. A nonborder gadget is created only when entirely inside every
*       virtual ancestor's inner rectangle; it is not partially clipped. Draw
*       skips border mode, otherwise draws Area and refreshes the gadget for
*       full drawing. Hide leaves a borrowed border gadget alive; Cleanup clears
*       only the local pointer, not the gadget UserData backlink.
*       HandleEvent accepts IDCMPUPDATE only with matching GA_ID and a PGA_Top
*       tag. It converts native Top to the logical range, suppresses unchanged
*       repeat events, and sends First plus private release/trigger tags through
*       the setter. It returns zero and does not call Area's event handler.
*       The active scaling algorithm leaves values unchanged below 65536
*       Entries. At larger totals it maps values to 0..65535 using integer
*       arithmetic and maps them back with truncation. A First query can
*       therefore update the stored value to a quantized native position.
*
*   NOTES
*       Setters modify writable caller tags to TAG_IGNORE for unchanged
*       Entries/First/Visible. Negative First is clamped in stored state, but
*       the original tag value still reaches superclass notification processing.
*       Native gadget updates leave ICA_TARGET unchanged and can render
*       immediately; they are not deferred exclusively to Draw.
*
*   BUGS
*       Getter success for First is reported even though the underlying PGA_Top
*       getter result is unchecked. Gadget allocation and handler-registration
*       success are not fully reflected in lifecycle return values. Range
*       arithmetic, conversions and custom image bounds are not comprehensively
*       validated. Slider and DoSmooth provide no working local feature; Horiz
*       has no local getter despite its header annotation.
*
*/
/****** Prop.mui/MUIA_Prop_Entries ************
*
*   NAME
*       MUIA_Prop_Entries
*
*   FUNCTION
*       Set the logical total range.
*
*   INPUTS
*       LONG declaration, stored as ULONG; initialize, set, get. Default zero.
*
*   NOTES
*       Values are not validated against Visible or First. Negative inputs
*       convert to unsigned values. At totals of at least 65536, native gadget
*       values are scaled to 65535; rounding can change a later First query. An
*       unchanged setter tag becomes TAG_IGNORE. The getter returns stored
*       Entries.
*
*/
/****** Prop.mui/MUIA_Prop_Visible ************
*
*   NAME
*       MUIA_Prop_Visible
*
*   FUNCTION
*       Set the logical visible portion of Entries.
*
*   INPUTS
*       LONG declaration, stored as ULONG; initialize, set, get. Default zero.
*
*   NOTES
*       Keep Visible between zero and Entries. No such validation is performed,
*       and changing Visible does not repair First. Native values use the
*       Entries-dependent scale. An unchanged setter tag becomes TAG_IGNORE. The
*       getter returns stored Visible.
*
*/
/****** Prop.mui/MUIA_Prop_First ************
*
*   NAME
*       MUIA_Prop_First
*
*   FUNCTION
*       Set or query the first visible logical position.
*
*   INPUTS
*       LONG; initialize, set, get. Default zero.
*
*   NOTES
*       Construction and setting clamp negative stored values to zero, but do
*       not impose an upper limit. When a native gadget exists, the getter reads
*       PGA_Top, converts it back and updates stored First. The native getter
*       result is unchecked. Unchanged setter tags become TAG_IGNORE; changed
*       negative tag values are not rewritten to the stored zero before
*       notifications. Maintain a coherent range and allow for native
*       quantization.
*
*/
/****** Prop.mui/MUIA_Prop_Horiz ************
*
*   NAME
*       MUIA_Prop_Horiz
*
*   FUNCTION
*       Select horizontal orientation when nonzero; otherwise vertical.
*
*   INPUTS
*       BOOL; initialize only in this implementation. Default FALSE.
*
*   NOTES
*       Stored orientation is normalized. There is no runtime setter or local
*       getter, despite the header declaring get support. Orientation affects
*       native freedom and default dimensions; it is not changed by Group_Horiz
*       on this Area subclass.
*
*/
/****** Prop.mui/MUIA_Prop_Slider ************
*
*   NAME
*       MUIA_Prop_Slider
*
*   FUNCTION
*       Declared slider flag with no effective local implementation.
*
*   INPUTS
*       BOOL; header declares initialize, set and get.
*
*   NOTES
*       Constructor and setter contain empty cases; no state or behavior is
*       changed and there is no local getter. Do not infer slider-specific
*       operation or a stored default.
*
*/
/****** Prop.mui/MUIA_Prop_DoSmooth ************
*
*   NAME
*       MUIA_Prop_DoSmooth
*
*   FUNCTION
*       Declared attribute without a consumer in Prop.
*
*   INPUTS
*       LONG; initialization declaration only.
*
*   NOTES
*       No local constructor branch, setter or getter implements this attribute.
*       It does not enable smoothing in the current implementation; no
*       functional default is defined here.
*
*/
/****** Prop.mui/MUIA_Prop_DeltaFactor ************
*
*   NAME
*       MUIA_Prop_DeltaFactor
*
*   FUNCTION
*       Multiply the amount used by Increase and Decrease.
*
*   INPUTS
*       LONG; initialize and set. Default one.
*
*   NOTES
*       There is no local getter. Zero, negative values and overflow-prone
*       factors are accepted. The factor affects the two methods, not every
*       native knob movement. Use nonnegative amounts/factors with representable
*       products for forward/decrease semantics.
*
*/
/****** Prop.mui/MUIA_Prop_UseWinBorder ************
*
*   NAME
*       MUIA_Prop_UseWinBorder
*
*   FUNCTION
*       Select an existing native window-border proportional gadget instead of
*       an owned in-content gadget.
*
*   INPUTS
*       LONG; initialize only. Default None.
*
*   NOTES
*       Right borrows mri_VertProp; Bottom borrows mri_HorizProp. Configure the
*       window with the matching UseRightBorderScroller or
*       UseBottomBorderScroller attribute. Left is declared but has no Show
*       branch; other nonzero values likewise mark border mode without finding a
*       gadget. No local setter/getter exists. Show writes the borrowed gadget
*       UserData; Hide does not dispose it, and Cleanup does not restore its
*       UserData backlink.
*
*/
/****** Prop.mui/MUIM_Prop_Increase ************
*
*   NAME
*       MUIM_Prop_Increase
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Prop_Increase, amount);
*
*   FUNCTION
*       Add amount times DeltaFactor to/from stored First.
*
*   INPUTS
*       amount - LONG. Use a nonnegative representable value and a coherent
*       range; multiplication/addition are not overflow-checked.
*
*   RESULT
*       Always one, including when no value changes. Setter and native gadget
*       results are not propagated.
*
*   NOTES
*       Increase limits the result against Entries minus Visible before setting
*       First. Mixed signed/unsigned range arithmetic makes invalid ranges
*       unsafe. The method uses stored First, not an explicit fresh native
*       getter, and calls the ordinary setter only when needed. Notifications
*       remain enabled.
*
*/
/****** Prop.mui/MUIM_Prop_Decrease ************
*
*   NAME
*       MUIM_Prop_Decrease
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Prop_Decrease, amount);
*
*   FUNCTION
*       Subtract amount times DeltaFactor to/from stored First.
*
*   INPUTS
*       amount - LONG. Use a nonnegative representable value and a coherent
*       range; multiplication/addition are not overflow-checked.
*
*   RESULT
*       Always one, including when no value changes. Setter and native gadget
*       results are not propagated.
*
*   NOTES
*       Decrease returns immediately when stored First is at or below zero.
*       Otherwise it clamps a negative calculated result to zero; it does not
*       impose an upper bound for negative amounts or factors. The method uses
*       stored First, not an explicit fresh native getter, and calls the
*       ordinary setter only when needed. Notifications remain enabled.
*
*/

extern struct Library *MUIMasterBase;

#define SCALE16_METHOD 2        /* 1 or 2 */

#if SCALE16_METHOD != 1
#define calcscale16(x)
#endif

struct Prop_DATA
{
    ULONG entries;
    LONG first;
    ULONG visible;
    LONG deltafactor;
#if SCALE16_METHOD == 1
    ULONG scale16;
#endif
    LONG gadgetid;

    UBYTE horiz;
    UBYTE usewinborder;
    UWORD minwidth, minheight;
    UWORD maxwidth, maxheight;
    UWORD defwidth, defheight;

    Object *prop_object;
    struct MUI_EventHandlerNode ehn;
    /* (Zune's custom scrollbar buffers and display hook, 48 bytes, were
     * AROS-only: never set on AmigaOS 3, gone in trim1) */
};

#if SCALE16_METHOD == 1
/*
 * Inactive SCALE16_METHOD == 1 alternative: compute cached 16.16 scale from
 * Entries. Requires live instance, no allocation or result. Current file
 * selects method 2, so this definition is not compiled.
 */
static void calcscale16(struct Prop_DATA *data)
{
    if (data->entries < 65535)
    {
        data->scale16 = 0x10000;
    }
    else
    {
        unsigned long long v =
            ZuneDiv64(((unsigned long long)data->entries) * 0x10000, 65535,
                NULL);

        data->scale16 = (ULONG) v;
    }
}
#endif

/*
 * Convert an unsigned logical value to native scale using instance Entries.
 * Active method 2 is identity below 65536, otherwise truncates value times
 * 65535 divided by Entries using a wide intermediate. Does not clamp
 * incoherent values; returns converted ULONG, no ownership change.
 */
static ULONG downscale(struct Prop_DATA *data, ULONG val)
{
#if SCALE16_METHOD == 1
    if (data->scale16 != 0x10000)
    {
        unsigned long long v =
            ((unsigned long long)val) * 0x10000 / data->scale16;
        val = (ULONG) v;
    }
#else
    if (data->entries >= 0x10000)
    {
        unsigned long long v =
            ZuneDiv64(((unsigned long long)val) * 65535, data->entries, NULL);
        val = (ULONG) v;
    }

#endif
    return val;
}

/*
 * Convert a native unsigned value back to logical units. Active method 2 is
 * identity below 65536 Entries, otherwise truncates value times Entries
 * divided by 65535. Requires live instance; no validation or ownership
 * change.
 */
static ULONG upscale(struct Prop_DATA *data, ULONG val)
{
#if SCALE16_METHOD == 1
    if (data->scale16 != 0x10000)
    {
        unsigned long long v =
            ((unsigned long long)val) * data->scale16 / 0x10000;
        val = (ULONG) v;
    }
#else
    if (data->entries >= 0x10000)
    {
        unsigned long long v =
            ZuneDiv64(((unsigned long long)val) * data->entries, 65535, NULL);
        val = (ULONG) v;
    }
#endif
    return val;
}

/*
 * Construct Area with default PropBack, parse
 * range/orientation/border/factor tags and initialize event descriptor and
 * dimensions. Clamp only negative First; leave other range invariants
 * unchecked. Return object or zero on Area failure. No native gadget
 * allocated here; Slider case does nothing.
 */
IPTR Prop__OM_NEW(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct Prop_DATA *data;
    struct TagItem *tags, *tag;

    obj = (Object *) DoSuperNewTags(cl, obj, NULL,
        MUIA_Background, MUII_PropBack, TAG_MORE, (IPTR) msg->ops_AttrList);

    if (!obj)
        return FALSE;

    data = INST_DATA(cl, obj);
    data->deltafactor = 1;

    /* parse initial taglist */
    for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));)
    {
        switch (tag->ti_Tag)
        {
        case MUIA_Prop_Entries:
            data->entries = tag->ti_Data;
            break;
        case MUIA_Prop_First:
            data->first = tag->ti_Data;
            break;
        case MUIA_Prop_Horiz:
            data->horiz = tag->ti_Data != 0;
            break;
        case MUIA_Prop_Slider:
            break;
        case MUIA_Prop_UseWinBorder:
            data->usewinborder = tag->ti_Data;
            break;
        case MUIA_Prop_Visible:
            data->visible = tag->ti_Data;
            break;

        case MUIA_Prop_DeltaFactor:
            data->deltafactor = tag->ti_Data;
            break;
        }
    }

    if (data->horiz)
    {
        data->minwidth = 12;
        data->minheight = 6;
        data->maxwidth = MUI_MAXMAX;
        data->maxheight = MUI_MAXMAX;
        data->defwidth = 50;
        data->defheight = 6;
    }
    else
    {
        data->minwidth = 6;
        data->minheight = 12;
        data->maxwidth = MUI_MAXMAX;
        data->maxheight = MUI_MAXMAX;
        data->defwidth = 6;
        data->defheight = 50;
    }

    if (data->first < 0)
        data->first = 0;

    data->ehn.ehn_Events = IDCMP_IDCMPUPDATE;
    data->ehn.ehn_Priority = 0;
    data->ehn.ehn_Flags = 0;
    data->ehn.ehn_Object = obj;
    data->ehn.ehn_Class = cl;

    if (data->usewinborder)
        _flags(obj) |= MADF_BORDERGADGET;

    calcscale16(data);

    return (IPTR) obj;
}

/*
 * Remove/dispose an owned nonborder gadget if still present and dispose
 * Area. Requires valid
 * window if removing a gadget and prior handler Cleanup. Borrowed border
 * gadget is not disposed. Return superclass result.
 */
IPTR Prop__OM_DISPOSE(struct IClass *cl, Object *obj, Msg msg)
{
    struct Prop_DATA *data = INST_DATA(cl, obj);

    if (data->prop_object && !data->usewinborder)
    {
        RemoveGadget(_window(obj), (struct Gadget *)data->prop_object);
        DisposeObject(data->prop_object);
    }

    return DoSuperMethodA(cl, obj, msg);
}

/*
 * Store range/factor tags and mutate unchanged range tags to TAG_IGNORE;
 * clamp stored negative First. Private OnlyTrigger suppresses native
 * refresh only, not storage or notifications. Refresh existing gadget
 * synchronously, leaving ICA_TARGET unchanged, then return Area setter
 * result. Caller tag list must be writable; range upper bounds and
 * arithmetic are unchecked.
 */
IPTR Prop__OM_SET(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct TagItem *tags, *tag;
    struct Prop_DATA *data = INST_DATA(cl, obj);
    int refresh = 0;
    int only_trigger = 0;

    for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));)
    {
        switch (tag->ti_Tag)
        {
        case MUIA_Prop_Entries:
            if ((IPTR) data->entries != tag->ti_Data)
            {
                data->entries = tag->ti_Data;
                refresh = 1;
            }
            else
            {
                tag->ti_Tag = TAG_IGNORE;
            }
            break;

        case MUIA_Prop_First:
            if ((IPTR) data->first != tag->ti_Data)
            {
                data->first = tag->ti_Data;
                refresh = 1;
            }
            else
            {
                tag->ti_Tag = TAG_IGNORE;
            }
            break;

        case MUIA_Prop_Slider:
            break;

        case MUIA_Prop_Visible:
            if ((IPTR) data->visible != tag->ti_Data)
            {
                data->visible = tag->ti_Data;
                refresh = 1;
            }
            else
            {
                tag->ti_Tag = TAG_IGNORE;
            }
            break;

        case MUIA_Prop_OnlyTrigger:
            only_trigger = tag->ti_Data;
            break;

        case MUIA_Prop_DeltaFactor:
            data->deltafactor = tag->ti_Data;
            break;
        }
    }

    if (data->first < 0)
        data->first = 0;

    if (data->prop_object && refresh && !only_trigger)
    {
        calcscale16(data);

        /* Rendering will happen here! This could make problems with
         * virtual groups, forward this to MUIM_Draw???
         * The gadget keeps its ICTARGET_IDCMP: propgclass sends no
         * notification for attributes a program sets (tests/native/
         * propnotify.c, listscrollmsgs.c), and MUI 3.8 updates it with
         * this one call too. Clearing and restoring the target cost two
         * more attribute passes per scroll. */
        if (SetGadgetAttrs((struct Gadget *)data->prop_object, _window(obj),
                NULL, PGA_Top, downscale(data, data->first), PGA_Visible,
                downscale(data, data->visible), PGA_Total, downscale(data,
                    data->entries), TAG_DONE))
            RefreshGList((struct Gadget *)data->prop_object, _window(obj),
                NULL, 1);
    }

    return DoSuperMethodA(cl, obj, (Msg) msg);
}

#define STORE *(msg->opg_Storage)
/*
 * Return stored Entries/Visible, or refresh First from native PGA_Top
 * without checking that getter result. Private Release always returns zero
 * with success to support notification discovery; it is not stored release
 * state. Requires IPTR storage. Other attributes delegate to Area,
 * including unsupported Horiz getter.
 */
IPTR Prop__OM_GET(struct IClass *cl, Object *obj, struct opGet *msg)
{
    struct Prop_DATA *data = INST_DATA(cl, obj);
    switch (msg->opg_AttrID)
    {
    case MUIA_Prop_First:
        {
            if (data->prop_object)
            {
                IPTR v;
                /* So we can get a more current value */
                GetAttr(PGA_Top, data->prop_object, &v);
                data->first = upscale(data, v);
            }
            STORE = data->first;
            return 1;
        }
    case MUIA_Prop_Entries:
        STORE = data->entries;
        return 1;
    case MUIA_Prop_Visible:
        STORE = data->visible;
        return 1;

        /* Private notification-discovery getter; no release state stored. */
    case MUIA_Prop_Release:
        STORE = 0;
        return 1;
    default:
        return DoSuperMethodA(cl, obj, (Msg) msg);
    }

    return 1;
}
#undef STORE

/*
 * Ask Area, add instance minima/defaults and overwrite maxima with instance
 * maxima. Requires setup-derived size data and writable MinMaxInfo. Return
 * TRUE regardless of superclass result; no allocation or ownership change.
 */
IPTR Prop__MUIM_AskMinMax(struct IClass *cl, Object *obj,
    struct MUIP_AskMinMax *msg)
{
    struct MUI_MinMax *mm = msg->MinMaxInfo;
    struct Prop_DATA *data = INST_DATA(cl, obj);

    /*
     ** let our superclass first fill in what it thinks about sizes.
     ** this will e.g. add the size of frame and inner spacing.
     */
    DoSuperMethodA(cl, obj, (Msg) msg);

    mm->MinWidth += data->minwidth;
    mm->DefWidth += data->defwidth;
    mm->MaxWidth = data->maxwidth;

    mm->MinHeight += data->minheight;
    mm->DefHeight += data->defheight;
    mm->MaxHeight = data->maxheight;

    D(bug("Prop %p minheigh=%d\n", obj, mm->MinHeight));

    return TRUE;
}

/*
 * Set up Area, register IDCMPUPDATE handler and allocate nonborder gadget
 * ID; choose custom-background thickness when available. Return zero on
 * Area failure, otherwise one. Requires paired lifecycle and connected
 * window/preferences.
 * Handler/ID results are not validated.
 */
IPTR Prop__MUIM_Setup(struct IClass *cl, Object *obj,
    struct MUIP_Setup *msg)
{
    struct Prop_DATA *data = INST_DATA(cl, obj);
    ULONG rc = DoSuperMethodA(cl, obj, (Msg) msg);
    if (!rc)
        return 0;

    DoMethod(_win(obj), MUIM_Window_AddEventHandler, (IPTR) & data->ehn);

    if (!data->usewinborder)
    {
        data->gadgetid = DoMethod(_win(obj), MUIM_Window_AllocGadgetID);
        if (data->horiz)
        {
            data->minwidth = 12;
            data->minheight = 6;
            data->maxwidth = MUI_MAXMAX;
            data->maxheight = MUI_MAXMAX;
            data->defwidth = 50;
            data->defheight = 6;
        }
        else
        {
            data->minwidth = 6;
            data->minheight = 12;
            data->maxwidth = MUI_MAXMAX;
            data->maxheight = MUI_MAXMAX;
            data->defwidth = 6;
            data->defheight = 50;
        }

        struct MUI_AreaData *adata = muiAreaData(obj);
        struct MUI_ImageSpec_intern *spec = adata->mad_Background;

        if (spec)
        {
            if (spec->type == IST_BITMAP)
            {
                struct dt_node *node = spec->u.bitmap.dt;
                if (node)
                {
                    if (node->mode == MODE_PROP)
                    {
                        set(obj, MUIA_Frame, MUIV_Frame_None);

                        if (data->horiz)
                        {
                            data->minheight =
                                node->img_horizontalcontainer->h >> 1;
                            data->defheight = data->minheight;
                            data->maxheight = data->minheight;
                        }
                        else
                        {
                            data->minwidth =
                                node->img_verticalcontainer->w >> 1;
                            data->defwidth = data->minwidth;
                            data->maxwidth = data->minwidth;
                        }
                    }
                }
            }
        }
    }

    return 1;
}

/*
 * Release nonborder gadget ID or clear borrowed border pointer/ID, remove
 * event handler, then return Area Cleanup result. Requires prior Hide and
 * matching Setup. Does not restore borrowed gadget UserData.
 */
IPTR Prop__MUIM_Cleanup(struct IClass *cl, Object *obj,
    struct MUIP_Cleanup *msg)
{
    struct Prop_DATA *data = INST_DATA(cl, obj);
    if (!data->usewinborder)
    {
        DoMethod(_win(obj), MUIM_Window_FreeGadgetID, data->gadgetid);
    }
    else
    {
        data->prop_object = NULL;
        data->gadgetid = 0;
    }

    DoMethod(_win(obj), MUIM_Window_RemEventHandler, (IPTR) & data->ehn);
    return DoSuperMethodA(cl, obj, (Msg) msg);
}



/*
 * Call Area Show then create/replace owned nonborder gadget only if fully
 * visible through virtual ancestors, or borrow Right/Bottom border gadget
 * and set UserData. Return Area result regardless of gadget failure.
 * Requires valid setup/window.
 */
IPTR Prop__MUIM_Show(struct IClass *cl, Object *obj, struct MUIP_Show *msg)
{
    struct Prop_DATA *data = INST_DATA(cl, obj);

    ULONG rc = DoSuperMethodA(cl, obj, (Msg) msg);
    if (!data->usewinborder)
    {
        BOOL isnewlook, completely_visible = TRUE;;

        if (_flags(obj) & MADF_INVIRTUALGROUP)
        {
            Object *wnd = NULL, *parent;

            get(obj, MUIA_WindowObject, &wnd);
            parent = obj;
            while (get(parent, MUIA_Parent, &parent))
            {
                if (!parent)
                    break;
                if (parent == wnd)
                    break;

                if (_flags(parent) & MADF_ISVIRTUALGROUP)
                {
                    if ((_mleft(obj) < _mleft(parent)) ||
                        (_mright(obj) > _mright(parent)) ||
                        (_mtop(obj) < _mtop(parent)) ||
                        (_mbottom(obj) > _mbottom(parent)))
                    {
                        completely_visible = FALSE;
                        D(bug("=== prop object: completely visible FALSE "
                            "for obj %x at %d,%d - %d,%d\n",
                            obj, _mleft(obj), _mtop(obj), _mright(obj),
                            _mbottom(obj)));
                        break;
                    }
                }
            }
        }

        if (completely_visible)
        {
            if (muiGlobalInfo(obj)->mgi_Prefs->scrollbar_type ==
                SCROLLBAR_TYPE_NEWLOOK)
                isnewlook = TRUE;
            else
                isnewlook = FALSE;

            ULONG width = _mwidth(obj);
            ULONG height = _mheight(obj);

            if (data->prop_object)
            {
                RemoveGadget(_window(obj),
                    (struct Gadget *)data->prop_object);
                DisposeObject(data->prop_object);
            }

            if ((data->prop_object = NewObject(NULL, "propgclass",
                        GA_Left, _mleft(obj),
                        GA_Top, _mtop(obj),
                        GA_Width, width,
                        GA_Height, height, GA_ID, data->gadgetid,
                        PGA_Freedom, data->horiz ? FREEHORIZ : FREEVERT,
                        PGA_Total, downscale(data, data->entries),
                        PGA_Visible, downscale(data, data->visible),
                        PGA_Top, downscale(data, data->first),
                        PGA_NewLook, isnewlook,
                        PGA_Borderless, TRUE,
                        ICA_TARGET, ICTARGET_IDCMP,  /* for notification */
                        TAG_DONE)))
            {
                AddGadget(_window(obj), (struct Gadget *)data->prop_object,
                    ~0);
            }
        }
    }
    else if (!data->prop_object)
    {
        switch (data->usewinborder)
        {
        case MUIV_Prop_UseWinBorder_Right:
            data->prop_object = muiRenderInfo(obj)->mri_VertProp;
            if (data->prop_object)
            {
                /* Store pointer to this propclass object in
                   propgadget->UserData,
                   so that window class when receiving IDCMP_IDCMUPDATE from
                   arrow gadgets can notify propclass object */

                ((struct Gadget *)data->prop_object)->UserData = obj;
            }
            break;

        case MUIV_Prop_UseWinBorder_Bottom:
            data->prop_object = muiRenderInfo(obj)->mri_HorizProp;
            if (data->prop_object)
            {
                /* Store pointer to this propclass object in
                   propgadget->UserData,
                   so that window class when receiving IDCMP_IDCMUPDATE from
                   arrow gadgets can notify propclass object */

                ((struct Gadget *)data->prop_object)->UserData = obj;
            }
            break;
        }
        if (data->prop_object)
        {
            data->gadgetid = ((struct Gadget *)data->prop_object)->GadgetID;

            /* As in OM_SET: the gadget keeps its ICTARGET_IDCMP */
            if (SetGadgetAttrs((struct Gadget *)data->prop_object,
                    _window(obj), NULL, PGA_Top, downscale(data,
                        data->first), PGA_Visible, downscale(data,
                        data->visible), PGA_Total, downscale(data,
                        data->entries), TAG_DONE))
            {
                RefreshGList((struct Gadget *)data->prop_object,
                    _window(obj), NULL, 1);
            }
        }
    }

    return rc;
}

/*
 * Return zero immediately in border mode. Otherwise draw Area and, for full
 * draw, capture custom background and refresh native gadget when present.
 * Return one, ignoring delegated results. Requires live drawing resources;
 * does not create a missing gadget.
 */
IPTR Prop__MUIM_Draw(struct IClass *cl, Object *obj,
    struct MUIP_Draw *msg)
{
    struct Prop_DATA *data = INST_DATA(cl, obj);

    /* No drawings if own border */
    if (data->usewinborder)
        return 0;

    DoSuperMethodA(cl, obj, (Msg) msg);

    if (!(msg->flags & MADF_DRAWOBJECT))
        return 1;

    if (data->prop_object)
        RefreshGList((struct Gadget *)data->prop_object, _window(obj), NULL,
            1);
    return 1;
}

/*
 * Remove and dispose only owned nonborder gadget, clearing its pointer,
 * then return Area Hide result. Requires live window and shown state.
 * Borrowed border gadget remains; Cleanup is still required.
 */
IPTR Prop__MUIM_Hide(struct IClass *cl, Object *obj,
    struct MUIP_Hide *msg)
{
    struct Prop_DATA *data = INST_DATA(cl, obj);

    if (data->prop_object)
    {
        if (!data->usewinborder)
        {
            RemoveGadget(_window(obj), (struct Gadget *)data->prop_object);
            DisposeObject(data->prop_object);
        }
        /* the border gadget is the window's: it goes when the window
         * closes, and the next Show links to the new one */
        data->prop_object = NULL;
    }

    return DoSuperMethodA(cl, obj, (Msg) msg);
}

/*
 * Process matching IDCMPUPDATE GA_ID/PGA_Top tags; upscale, suppress
 * unchanged repeat updates and invoke setter with OnlyTrigger and release
 * flag. Return zero for all events without superclass delegation. Requires
 * valid borrowed IntuiMessage/tag list. Release notifications carry repeat-
 * dependent state although the Release getter always returns zero.
 */
IPTR Prop__MUIM_HandleEvent(struct IClass *cl, Object *obj,
    struct MUIP_HandleEvent *msg)
{
    struct Prop_DATA *data = INST_DATA(cl, obj);

    if (msg->imsg)
    {
        if (msg->imsg->Class == IDCMP_IDCMPUPDATE)
        {
            struct TagItem *tag;
            ULONG v;

            /* Check if we are meant */
            tag = FindTagItem(GA_ID, (struct TagItem *)msg->imsg->IAddress);
            if (!tag)
                return 0;
            if (tag->ti_Data != data->gadgetid)
                return 0;

            /* Check if we PGA_Top has really changed */
            tag =
                FindTagItem(PGA_Top, (struct TagItem *)msg->imsg->IAddress);
            if (!tag)
                return 0;
            v = upscale(data, tag->ti_Data);

            //kprintf("PROP_HandleEvent: PGA_Top %d upscaled %d entries %d\n",
            //    tag->ti_Data, v, data->entries);

            if ((v == data->first)
                && (msg->imsg->Qualifier & IEQUALIFIER_REPEAT))
                return 0;

            if ((LONG) v < 0)
                v = 0;

            SetAttrs(obj, MUIA_Prop_First, v, MUIA_Prop_OnlyTrigger, TRUE,
                MUIA_Prop_Release,
                ((msg->imsg->
                        Qualifier & IEQUALIFIER_REPEAT) ? FALSE : TRUE),
                TAG_DONE);
        }
    }

    return 0;
}

/* Implement MUIM_Prop_Increase using stored First and amount times
 * DeltaFactor. Requires coherent range and representable arithmetic. Return
 * one unconditionally; ordinary setter notifications remain enabled. See
 * the public method contract above for exact clamping and limitations.
 */
IPTR Prop__MUIM_Prop_Increase(struct IClass *cl, Object *obj,
    struct MUIP_Prop_Increase *msg)
{
    struct Prop_DATA *data = INST_DATA(cl, obj);
    LONG newfirst;

    newfirst = data->first + msg->amount * data->deltafactor;

    if (newfirst + data->visible > data->entries)
        newfirst = data->entries - data->visible;
    if (newfirst != data->first)
        set(obj, MUIA_Prop_First, newfirst);
    return 1;
}

/* Implement MUIM_Prop_Decrease using stored First and amount times
 * DeltaFactor. Requires coherent range and representable arithmetic. Return
 * one unconditionally; ordinary setter notifications remain enabled. See
 * the public method contract above for exact clamping and limitations.
 */
IPTR Prop__MUIM_Prop_Decrease(struct IClass *cl, Object *obj,
    struct MUIP_Prop_Decrease *msg)
{
    struct Prop_DATA *data = INST_DATA(cl, obj);
    LONG newfirst;

    /* We cannot decrease if if are on the top */
    if (data->first <= 0)
        return 1;

    newfirst = data->first - msg->amount * data->deltafactor;

    if (newfirst < 0)
        set(obj, MUIA_Prop_First, 0);
    else if (newfirst != data->first)
        set(obj, MUIA_Prop_First, newfirst);
    return 1;
}


/*
 * Route Prop overrides and Increase/Decrease; delegate remaining methods to
 * Area. Return selected handler result. Requires live instance and ABI-
 * correct message except construction; native resources follow
 * Setup/Show/Hide/Cleanup pairing.
 */
BOOPSI_DISPATCHER(IPTR, Prop_Dispatcher, cl, obj, msg)
{
    switch (msg->MethodID)
    {
    case OM_NEW:
        return Prop__OM_NEW(cl, obj, (struct opSet *)msg);
    case OM_DISPOSE:
        return Prop__OM_DISPOSE(cl, obj, msg);
    case OM_GET:
        return Prop__OM_GET(cl, obj, (struct opGet *)msg);
    case OM_SET:
        return Prop__OM_SET(cl, obj, (struct opSet *)msg);
    case MUIM_Setup:
        return Prop__MUIM_Setup(cl, obj, (APTR) msg);
    case MUIM_Cleanup:
        return Prop__MUIM_Cleanup(cl, obj, (APTR) msg);
    case MUIM_Show:
        return Prop__MUIM_Show(cl, obj, (APTR) msg);
    case MUIM_Hide:
        return Prop__MUIM_Hide(cl, obj, (APTR) msg);
    case MUIM_AskMinMax:
        return Prop__MUIM_AskMinMax(cl, obj, (APTR) msg);
    case MUIM_Draw:
        return Prop__MUIM_Draw(cl, obj, (APTR) msg);
    case MUIM_HandleEvent:
        return Prop__MUIM_HandleEvent(cl, obj, (APTR) msg);
    case MUIM_Prop_Decrease:
        return Prop__MUIM_Prop_Decrease(cl, obj, (APTR) msg);
    case MUIM_Prop_Increase:
        return Prop__MUIM_Prop_Increase(cl, obj, (APTR) msg);
    default:
        return DoSuperMethodA(cl, obj, msg);

    }
}
BOOPSI_DISPATCHER_END

const struct __MUIBuiltinClass _MUI_Prop_desc =
{
    MUIC_Prop,
    MUIC_Area,
    sizeof(struct Prop_DATA),
    (void *) Prop_Dispatcher
};
