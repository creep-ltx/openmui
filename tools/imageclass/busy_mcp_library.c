/* Busy.mcp: the settings page of Busy.mcc, with MUI 3.8's settings and item
 * ids (0x8002 0001..000b, see docs/lean/settings-mui38.md). A page of Zune
 * prefs, found in LIBS:MUI like any other .mcp. */
#include <libraries/mui.h>
#include <mcc_common.h>
#include <SDI_hook.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/muimaster.h>
#include <stdio.h>
#include <string.h>

#define DEBUG 0
#define D(...) ((void)0)
#define E(...) ((void)0)
#include "shellstart.c"

#include "Busy_mcc.h"
#include "busy_private.h"

struct BusyP_DATA
{
    Object *sample, *color1, *color2, *style, *speed, *size, *number, *fade, *frame;
};

#define CLASS "Busy.mcp"
#define SUPERCLASSP MUIC_Mccprefs
#define INSTDATAP BusyP_DATA
#define VERSION 1
#define REVISION 1
#define USERLIBID CLASS " 1.1 (11.11.2026) Zune68k"
#define MASTERVERSION 19
#define MIN_STACKSIZE 8192
#define CLASSEXPUNGE
#define SetupDebug() ((void)0)
#define CleanupDebug() ((void)0)
static VOID ClassExpunge(struct Library *base);
#include "mccinit.c"
#include "mcp_support.h"

static VOID ClassExpunge(struct Library *base)
{
    FreeNumSliderClass();
}

#define MUIM_BusyP_Style 0x80020100

static const struct NumInfo SpeedInfo = { BUSY_SPEED_OFF, "OFF", "%ld ms" };
static const struct NumInfo SizeInfo = { -1, "", "%ld" };
static const struct NumInfo NumberInfo = { -1, "", "%ld" };
static const struct NumInfo FadeInfo = { 0, "No fade", "%ld segments" };
static const struct NumInfo FrameInfo = { 0, "No frame", "%ld pixel" };

static const struct Item Items[] =
{
    { BUSYCFG_Color1, MUIA_Pendisplay_Spec, (LONG)"m5", TRUE },
    { BUSYCFG_Color2, MUIA_Pendisplay_Spec, (LONG)"m0", TRUE },
    { BUSYCFG_Style, MUIA_Cycle_Active, 3, FALSE },
    { BUSYCFG_Speed, MUIA_Numeric_Value, BUSY_SPEED_DEF, FALSE },
    { BUSYCFG_Size, MUIA_Numeric_Value, 8, FALSE },
    { BUSYCFG_Number, MUIA_Numeric_Value, 8, FALSE },
    { BUSYCFG_Fade, MUIA_Numeric_Value, 6, FALSE },
    { BUSYCFG_Frame, MUIA_Numeric_Value, 1, FALSE },
};

static Object **Gadgets(struct BusyP_DATA *d, Object **list)
{
    list[0] = d->color1; list[1] = d->color2; list[2] = d->style; list[3] = d->speed;
    list[4] = d->size; list[5] = d->number; list[6] = d->fade; list[7] = d->frame;
    return list;
}

/* the sample bar shows what the gadgets say */
static void ToSample(struct BusyP_DATA *d)
{
    if (!d->sample) return;
    SetAttrs(d->sample,
        MUIA_Busy_Style, xget(d->style, MUIA_Cycle_Active),
        MUIA_Busy_Size, xget(d->size, MUIA_Numeric_Value),
        MUIA_Busy_Number, xget(d->number, MUIA_Numeric_Value),
        MUIA_Busy_Fade, xget(d->fade, MUIA_Numeric_Value),
        MUIA_Busy_Frame, xget(d->frame, MUIA_Numeric_Value),
        MUIA_Busy_Color1, xget(d->color1, MUIA_Pendisplay_Spec),
        MUIA_Busy_Color2, xget(d->color2, MUIA_Pendisplay_Spec),
        MUIA_Busy_Speed, xget(d->speed, MUIA_Numeric_Value) >= BUSY_SPEED_OFF
            ? 0 : xget(d->speed, MUIA_Numeric_Value),
        TAG_DONE);
}

/* Size is for the old style only, the other three settings for the others */
static void EnableByStyle(struct BusyP_DATA *d)
{
    BOOL old = xget(d->style, MUIA_Cycle_Active) == 0;

    nnset(d->size, MUIA_Disabled, !old);
    nnset(d->number, MUIA_Disabled, old);
    nnset(d->fade, MUIA_Disabled, old);
    nnset(d->frame, MUIA_Disabled, old);
}

static Object *BuildPage(struct BusyP_DATA *d)
{
    static const char *styles[] = { "Old", "Queue", "KnightRider", "StarTrek", NULL };
    Object *g;

    if (!MakeNumSliderClass()) return NULL;
    d->sample = MUI_NewObject(MUIC_Busy, MUIA_FixHeight, 14, TAG_DONE);
    g = VGroup,
        Child, VGroup, GroupFrameT("Sample Busy-Bar"),
            Child, d->sample ? d->sample : MUI_NewObject(MUIC_Rectangle, TAG_DONE),
        End,
        Child, ColGroup(4),
            Child, PrefLabel("Color 1:"),
            Child, d->color1 = PoppenObject, MUIA_CycleChain, TRUE, End,
            Child, PrefLabel("Size:"),
            Child, d->size = NumSlider(&SizeInfo, 4, 20),
            Child, PrefLabel("Color 2:"),
            Child, d->color2 = PoppenObject, MUIA_CycleChain, TRUE, End,
            Child, PrefLabel("Number:"),
            Child, d->number = NumSlider(&NumberInfo, 4, 32),
            Child, PrefLabel("Style:"),
            Child, d->style = CycleObject, MUIA_Cycle_Entries, styles, MUIA_CycleChain, TRUE, End,
            Child, PrefLabel("Fade:"),
            Child, d->fade = NumSlider(&FadeInfo, 0, 16),
            Child, PrefLabel("Speed:"),
            Child, d->speed = NumSlider(&SpeedInfo, 1, BUSY_SPEED_OFF),
            Child, PrefLabel("Frame:"),
            Child, d->frame = NumSlider(&FrameInfo, 0, 4),
        End,
        Child, TextObject, MUIA_Font, MUIV_Font_Tiny, MUIA_Text_PreParse, "\33c",
            MUIA_Text_Contents, "Busy.mcp 1.1 (Zune68k)", End,
    End;
    return g;
}

DISPATCHER(_DispatcherP)
{
    struct BusyP_DATA *d = INST_DATA(cl, obj);
    Object *list[8];
    int i;

    switch (msg->MethodID)
    {
    case OM_NEW:
        obj = (Object *)DoSuperMethodA(cl, obj, msg);
        if (obj)
        {
            Object *page;

            d = INST_DATA(cl, obj);
            if ((page = BuildPage(d)))
            {
                DoMethod(obj, OM_ADDMEMBER, page);
                /* the sample follows every gadget */
                for (i = 0; i < 8; i++)
                {
                    Object **g = Gadgets(d, list);
                    ULONG attr = i < 2 ? MUIA_Pendisplay_Spec : i == 2 ? MUIA_Cycle_Active : MUIA_Numeric_Value;

                    DoMethod(g[i], MUIM_Notify, attr, MUIV_EveryTime, obj, 1, MUIM_BusyP_Style);
                }
                return (IPTR)obj;
            }
            CoerceMethod(cl, obj, OM_DISPOSE);
        }
        return 0;

    case MUIM_BusyP_Style:
        EnableByStyle(d);
        ToSample(d);
        return 0;

    case MUIM_Settingsgroup_ConfigToGadgets:
    {
        Object *configdata = ((struct MUIP_Settingsgroup_ConfigToGadgets *)msg)->configdata;

        Gadgets(d, list);
        for (i = 0; i < 8; i++) ItemToGadget(configdata, list[i], &Items[i]);
        EnableByStyle(d);
        ToSample(d);
        return 0;
    }

    case MUIM_Settingsgroup_GadgetsToConfig:
    {
        Object *configdata = ((struct MUIP_Settingsgroup_ConfigToGadgets *)msg)->configdata;

        Gadgets(d, list);
        for (i = 0; i < 8; i++) ItemFromGadget(configdata, list[i], &Items[i]);
        return 0;
    }
    }
    return DoSuperMethodA(cl, obj, msg);
}
