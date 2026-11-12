/*
    Copyright (C) 1995-2011, The AROS Development Team. All rights reserved.
*/

#define MUIMASTER_YES_INLINE_STDARG

#include <exec/types.h>
#include <utility/date.h>

#include <aros/debug.h>
#include <aros/asmcall.h>

#include <proto/alib.h>
#include <proto/muimaster.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/timer.h>

#include <string.h>
#include <stdio.h>

#include "Busy_mcc.h"
#include "busy_private.h"

/*** Settings ***************************************************************/
/* MUI 3.8's Busy settings (Busy.mcp) are in the configuration under 0x8002....:
   numbers as 4 bytes, the two colours as pen specs. Missing = MUI 3.8's defaults. */
static ULONG cfg_num(Object *obj, ULONG id, ULONG def, ULONG min, ULONG max)
{
    IPTR d;

    if (DoMethod(obj, MUIM_GetConfigItem, id, (IPTR)&d) && d)
    {
        def = *(ULONG *)d;
        if (def < min) def = min;
        if (def > max) def = max;
    }
    return def;
}

static void cfg_pen(Object *obj, struct Busy_DATA *data, int i, const char *def)
{
    IPTR d;
    ULONG id = i ? BUSYCFG_Color2 : BUSYCFG_Color1;

    if (data->spec[i].buf[0]) return;   /* given as an attribute */
    strncpy((char *)data->spec[i].buf,
        DoMethod(obj, MUIM_GetConfigItem, id, (IPTR)&d) && d ? (char *)d : def, 31);
}

static void load_settings(Object *obj, struct Busy_DATA *data)
{
    if (!(data->flags & FLG_STYLESET))
        data->style = cfg_num(obj, BUSYCFG_Style, 3, 0, 3);
    data->size = cfg_num(obj, BUSYCFG_Size, 8, 4, 20);
    data->number = cfg_num(obj, BUSYCFG_Number, 8, 4, 32);
    data->fade = cfg_num(obj, BUSYCFG_Fade, 6, 0, 16);
    data->frame = cfg_num(obj, BUSYCFG_Frame, 1, 0, 4);
}

static void get_pens(Object *obj, struct Busy_DATA *data)
{
    int i;

    for (i = 0; i < 2; i++)
        data->pen[i] = MUI_ObtainPen(muiRenderInfo(obj), &data->spec[i], 0);
    data->nopens = TRUE;
}

static void drop_pens(Object *obj, struct Busy_DATA *data)
{
    int i;

    if (!data->nopens) return;
    for (i = 0; i < 2; i++) MUI_ReleasePen(muiRenderInfo(obj), data->pen[i]);
    data->nopens = FALSE;
}

/* the speed of the bar: given, or (MUIV_Busy_Speed_User) MUI 3.8's setting, 251 = off */
static void apply_speed(struct Busy_DATA *data, LONG speed)
{
    if (speed == MUIV_Busy_Speed_User)
    {
        data->flags |= FLG_USERSPEED;
        speed = BUSY_SPEED_DEF;
    }
    else
        data->flags &= ~FLG_USERSPEED;
    data->speed = speed;
}

/*** Methods ****************************************************************/
IPTR Busy__OM_NEW(Class *cl, Object *obj, struct opSet *msg)
{
    struct Busy_DATA *data;
    struct TagItem *tags = msg->ops_AttrList, *tag;

    obj = (Object *) DoSuperMethodA(cl, obj, (Msg)msg);
    if (!obj) return 0;

    data = INST_DATA(cl, obj);
    data->speed = BUSY_SPEED_DEF;
    while ((tag = NextTagItem(&tags)) != NULL)
    {
        switch (tag->ti_Tag)
        {
        case MUIA_Busy_ShowHideIH:
            if (tag->ti_Data) data->flags |= FLG_SHOWHIDEIH;
            break;
        case MUIA_Busy_Speed:
            apply_speed(data, (LONG)tag->ti_Data);
            break;
        case MUIA_Busy_Style:
            data->style = tag->ti_Data & 3;
            data->flags |= FLG_STYLESET;
            break;
        case MUIA_Busy_Color1:
            strncpy((char *)data->spec[0].buf, (char *)tag->ti_Data, 31);
            break;
        case MUIA_Busy_Color2:
            strncpy((char *)data->spec[1].buf, (char *)tag->ti_Data, 31);
            break;
        }
    }

    data->ihn.ihn_Flags  = MUIIHNF_TIMER;
    data->ihn.ihn_Method = MUIM_Busy_Move;
    data->ihn.ihn_Object = obj;
    data->ihn.ihn_Millis = 0;

    return (IPTR)obj;
}

/* (re)start the timer after the speed changed */
static void restart_timer(Object *obj, struct Busy_DATA *data)
{
    if ((data->flags & FLG_SHOWN) ||
        ((data->flags & FLG_SETUP) && !(data->flags & FLG_SHOWHIDEIH)))
    {
        ULONG ms = data->speed;

        if ((data->flags & FLG_USERSPEED) && ms >= BUSY_SPEED_OFF) ms = 0;
        if (data->ihn.ihn_Millis)
            DoMethod(_app(obj), MUIM_Application_RemInputHandler, (IPTR) &data->ihn);
        data->ihn.ihn_Millis = ms;
        if (ms)
            DoMethod(_app(obj), MUIM_Application_AddInputHandler, (IPTR) &data->ihn);
    }
}

IPTR Busy__OM_SET(Class *cl, Object *obj, struct opSet *msg)
{
    struct Busy_DATA *data = INST_DATA(cl, obj);
    struct TagItem       *tags  = msg->ops_AttrList;
    struct TagItem       *tag;
    BOOL                  redraw = FALSE;

    while ((tag = NextTagItem(&tags)) != NULL)
    {
        switch(tag->ti_Tag)
        {
            case MUIA_Busy_Speed:
                apply_speed(data, (LONG)tag->ti_Data);
                if (data->flags & FLG_USERSPEED)
                    data->speed = cfg_num(obj, BUSYCFG_Speed, BUSY_SPEED_DEF, 1, BUSY_SPEED_OFF);
                restart_timer(obj, data);
                break;
            case MUIA_Busy_Style: data->style = tag->ti_Data & 3; data->flags |= FLG_STYLESET; redraw = TRUE; break;
            case MUIA_Busy_Size: data->size = tag->ti_Data; redraw = TRUE; break;
            case MUIA_Busy_Number: data->number = tag->ti_Data; redraw = TRUE; break;
            case MUIA_Busy_Fade: data->fade = tag->ti_Data; redraw = TRUE; break;
            case MUIA_Busy_Frame: data->frame = tag->ti_Data; redraw = TRUE; break;
            case MUIA_Busy_Color1:
            case MUIA_Busy_Color2:
            {
                int i = tag->ti_Tag == MUIA_Busy_Color2;

                strncpy((char *)data->spec[i].buf, (char *)tag->ti_Data, 31);
                if (data->nopens)
                {
                    MUI_ReleasePen(muiRenderInfo(obj), data->pen[i]);
                    data->pen[i] = MUI_ObtainPen(muiRenderInfo(obj), &data->spec[i], 0);
                    redraw = TRUE;
                }
                break;
            }
        } /* switch(tag->ti_Tag) */

    } /* while ((tag = NextTagItem(&tags)) != NULL) */

    if (redraw && (data->flags & FLG_SHOWN))
    {
         MUI_Redraw(obj, MADF_DRAWOBJECT);
    }

    return DoSuperMethodA(cl, obj, (Msg)msg);
}


IPTR Busy__OM_GET(Class *cl, Object *obj, struct opGet *msg)
{
    struct Busy_DATA *data = INST_DATA(cl, obj);
    IPTR              retval = TRUE;

    switch(msg->opg_AttrID)
    {
        case MUIA_Busy_Speed:
            *msg->opg_Storage = data->speed;
            break;
        case MUIA_Busy_Style: *msg->opg_Storage = data->style; break;
        case MUIA_Busy_Size: *msg->opg_Storage = data->size; break;
        case MUIA_Busy_Number: *msg->opg_Storage = data->number; break;
        case MUIA_Busy_Fade: *msg->opg_Storage = data->fade; break;
        case MUIA_Busy_Frame: *msg->opg_Storage = data->frame; break;

        default:
            retval = DoSuperMethodA(cl, obj, (Msg)msg);
            break;
    }

    return retval;
}


IPTR Busy__MUIM_Setup(Class *cl, Object *obj, struct MUIP_Setup *msg)
{
    struct Busy_DATA *data = INST_DATA(cl, obj);

    if (!DoSuperMethodA(cl, obj, (Msg)msg)) return FALSE;

    load_settings(obj, data);
    cfg_pen(obj, data, 0, "m5");
    cfg_pen(obj, data, 1, "m0");
    get_pens(obj, data);
    if (data->flags & FLG_USERSPEED)
        data->speed = cfg_num(obj, BUSYCFG_Speed, BUSY_SPEED_DEF, 1, BUSY_SPEED_OFF);

    data->flags |= FLG_SETUP;
    if (!(data->flags & FLG_SHOWHIDEIH))
        restart_timer(obj, data);

    return TRUE;
}


IPTR Busy__MUIM_Cleanup(Class *cl, Object *obj, struct MUIP_Cleanup *msg)
{
    struct Busy_DATA *data = INST_DATA(cl, obj);

    if (data->ihn.ihn_Millis && !(data->flags & FLG_SHOWHIDEIH))
    {
        DoMethod(_app(obj), MUIM_Application_RemInputHandler, (IPTR) &data->ihn);
        data->ihn.ihn_Millis = 0;
    }

    data->flags &= ~FLG_SETUP;
    drop_pens(obj, data);

    return DoSuperMethodA(cl, obj, (Msg)msg);
}

IPTR Busy__MUIM_Show(Class *cl, Object *obj, struct MUIP_Show *msg)
{
    struct Busy_DATA *data = INST_DATA(cl, obj);
    IPTR              retval;
    
    retval = DoSuperMethodA(cl, obj, (Msg)msg);

    if (data->speed && (data->flags & FLG_SHOWHIDEIH))
    {
        data->ihn.ihn_Millis = data->speed;
        DoMethod(_app(obj), MUIM_Application_AddInputHandler, (IPTR) &data->ihn);
    }
    
    data->flags |= FLG_SHOWN;
    
    return retval;
}


IPTR Busy__MUIM_Hide(Class *cl, Object *obj, struct MUIP_Hide *msg)
{
    struct Busy_DATA *data = INST_DATA(cl, obj);
 
    if (data->ihn.ihn_Millis && (data->flags & FLG_SHOWHIDEIH))
    {
        DoMethod(_app(obj), MUIM_Application_RemInputHandler, (IPTR) &data->ihn);
        data->ihn.ihn_Millis = 0;
    }
    
    data->flags &= ~FLG_SHOWN;
    
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

IPTR Busy__MUIM_AskMinMax(Class *cl, Object *obj, struct MUIP_AskMinMax *msg)
{
    DoSuperMethodA(cl, obj, (Msg)msg);
    
    msg->MinMaxInfo->MinWidth  += 8 + 4;   /* Busy 2.4 on MUI 3.8 (its demo dumped): a bar of 8x4 at least, 24x8 by default, in a thin recessed frame with a pixel beside it */
    msg->MinMaxInfo->MinHeight += 4 + 2;
    msg->MinMaxInfo->DefWidth  += 24 + 4;
    msg->MinMaxInfo->DefHeight += 8 + 2;
    msg->MinMaxInfo->MaxWidth   = MUI_MAXMAX;
    msg->MinMaxInfo->MaxHeight  = MUI_MAXMAX;

    return TRUE;
}


/* One segment: Color 1 behind, the bar in Color 2 (a faded one is lower). */
static void draw_cell(Object *obj, struct Busy_DATA *data, WORD x0, WORD x1, WORD level, WORD levels)
{
    struct RastPort *rp = _rp(obj);
    WORD top = _mtop(obj) + 1, bot = _mbottom(obj) - 1, h = bot - top + 1, bh, f = data->frame;

    SetAPen(rp, data->pen[0]);
    RectFill(rp, x0, top, x1, bot);
    if (level < 0) return;
    bh = (h - 2 * f) * (levels - level) / levels;
    if (bh < 1 || x1 - x0 + 1 <= 2 * f) return;
    top += (h - bh) / 2;
    SetAPen(rp, data->pen[1]);
    RectFill(rp, x0 + f, top, x1 - f, top + bh - 1);
}

IPTR Busy__MUIM_Draw(Class *cl, Object *obj, struct MUIP_Draw *msg)
{
    struct Busy_DATA *data = INST_DATA(cl, obj);
    WORD              x, w = _mwidth(obj) - 4, left = _mleft(obj) + 2;
    IPTR              retval;

    retval = DoSuperMethodA(cl, obj, (Msg)msg);

    if (!(msg->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE))) return 0;
    if (w < 1 || _mheight(obj) < 3) return retval;
    /* the thin recessed frame (Busy 2.4's default look) and the pixel beside the bar */
    {
        struct RastPort *rp = _rp(obj);
        WORD l = _mleft(obj), t = _mtop(obj), r = _mright(obj), b = _mbottom(obj);
        SetAPen(rp, _pens(obj)[MPEN_SHADOW]);
        RectFill(rp, l, t, r - 1, t); RectFill(rp, l, t, l, b - 1);
        SetAPen(rp, _pens(obj)[MPEN_SHINE]);
        RectFill(rp, l + 1, b, r, b); RectFill(rp, r, t + 1, r, b);
        SetAPen(rp, data->pen[0]);
        RectFill(rp, l + 1, t + 1, l + 1, b - 1); RectFill(rp, r - 1, t + 1, r - 1, b - 1);
    }

    if (data->style == 0)
    {
        WORD s = data->size;

        /* One RectFill per stripe rather than per column. */
        for(x = 0; x < w; )
        {
            WORD col = ((x + data->pos) / s) % 2;
            WORD run = s - (x + data->pos) % s;

            if (x + run > w) run = w - x;
            SetAPen(_rp(obj), data->pen[col ? 0 : 1]);
            RectFill(_rp(obj), left + x, _mtop(obj) + 1, left + x + run - 1, _mbottom(obj) - 1);
            x += run;
        }
    }
    else
    {
        LONG n = data->number, i, head, mirror = -1, period = n, levels = data->fade + 1;

        /* Queue: the head runs left to right, KnightRider: back and forth,
           StarTrek: two heads from the ends towards the middle and back. */
        if (data->style == 2) period = 2 * n - 2;
        head = data->pos % period;
        if (data->style == 2 && head >= n) head = period - head;
        if (data->style == 3) mirror = n - 1 - head;
        for (i = 0; i < n; i++)
        {
            LONG d = data->style == 1 ? (head - i + n) % n : i > head ? i - head : head - i;

            if (mirror >= 0)
            {
                LONG m = i > mirror ? i - mirror : mirror - i;

                if (m < d) d = m;
            }
            draw_cell(obj, data, left + w * i / n, left + w * (i + 1) / n - 1,
                d < levels ? d : -1, levels);
        }
    }

    return retval;
}


IPTR Busy__MUIM_Busy_Move(Class *cl, Object *obj, Msg msg)
{
    struct Busy_DATA *data;
    
    data = INST_DATA(cl, obj);
    data->pos++;

    MUI_Redraw(obj, MADF_DRAWUPDATE);
    
    return 0;
}

