/* Listtree.mcp: the settings page of Listtree.mcc, with MUI 3.8's settings and
 * item ids (0x8002 0010..0020, see docs/lean/settings-mui38.md). A page of
 * Zune prefs, found in LIBS:MUI like any other .mcp. MUI 3.8's page has a
 * sample tree and a "Display opened" switch: no sample here (the tree reads
 * the settings when it is set up), and "Display opened" is left out because
 * what it does is not known (an item already in the settings is kept). */
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

struct ListtreeP_DATA
{
    Object *right, *down, *lines, *shadow, *style, *space, *remember;
};

#define CLASS "Listtree.mcp"
#define SUPERCLASSP MUIC_Mccprefs
#define INSTDATAP ListtreeP_DATA
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

#define LTCFG_Right     0x80020010
#define LTCFG_Down      0x80020011
#define LTCFG_Remember  0x80020012
#define LTCFG_Style     0x80020013
#define LTCFG_Space     0x80020014
#define LTCFG_Lines     0x80020016
#define LTCFG_Shadow    0x80020020

static const struct NumInfo SpaceInfo = { -1, "", "%ld" };

static const struct Item Items[] =
{
    { LTCFG_Right, MUIA_Imagedisplay_Spec, (LONG)"1:17", TRUE },
    { LTCFG_Down, MUIA_Imagedisplay_Spec, (LONG)"1:23", TRUE },
    { LTCFG_Lines, MUIA_Pendisplay_Spec, (LONG)"m5", TRUE },
    { LTCFG_Shadow, MUIA_Pendisplay_Spec, (LONG)"m4", TRUE },
    { LTCFG_Style, MUIA_Cycle_Active, 3, FALSE },
    { LTCFG_Space, MUIA_Numeric_Value, 6, FALSE },
    { LTCFG_Remember, MUIA_Selected, 1, FALSE },
};

static void Gadgets(struct ListtreeP_DATA *d, Object **list)
{
    list[0] = d->right; list[1] = d->down; list[2] = d->lines; list[3] = d->shadow;
    list[4] = d->style; list[5] = d->space; list[6] = d->remember;
}

static Object *BuildPage(struct ListtreeP_DATA *d)
{
    static const char *styles[] = { "Normal", "Inserted", "Lines", "Win95", "Mac", "Lines3D", NULL };

    if (!MakeNumSliderClass()) return NULL;
    return VGroup,
        Child, ColGroup(4),
            Child, PrefLabel("Right:"),
            Child, d->right = PopimageObject, MUIA_Imageadjust_Type, MUIV_Imageadjust_Type_Image,
                MUIA_CycleChain, TRUE, End,
            Child, PrefLabel("Down:"),
            Child, d->down = PopimageObject, MUIA_Imageadjust_Type, MUIV_Imageadjust_Type_Image,
                MUIA_CycleChain, TRUE, End,
            Child, PrefLabel("Lines:"),
            Child, d->lines = PoppenObject, MUIA_CycleChain, TRUE, End,
            Child, PrefLabel("Shadow:"),
            Child, d->shadow = PoppenObject, MUIA_CycleChain, TRUE, End,
            Child, PrefLabel("Style:"),
            Child, d->style = CycleObject, MUIA_Cycle_Entries, styles, MUIA_CycleChain, TRUE, End,
            Child, PrefLabel("Space:"),
            Child, d->space = NumSlider(&SpaceInfo, 0, 16),
        End,
        Child, HGroup,
            Child, HSpace(0),
            Child, PrefLabel("Remember status:"),
            Child, d->remember = ImageObject, ImageButtonFrame, MUIA_Image_Spec, MUII_CheckMark,
                MUIA_Image_FreeVert, TRUE, MUIA_InputMode, MUIV_InputMode_Toggle, MUIA_Selected, TRUE,
                MUIA_ShowSelState, FALSE, MUIA_CycleChain, TRUE, End,
            Child, HSpace(0),
        End,
        Child, TextObject, MUIA_Font, MUIV_Font_Tiny, MUIA_Text_PreParse, "\33c",
            MUIA_Text_Contents, "Listtree.mcp 1.1 (Zune68k)", End,
    End;
}

DISPATCHER(_DispatcherP)
{
    struct ListtreeP_DATA *d = INST_DATA(cl, obj);
    Object *list[7];
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
                return (IPTR)obj;
            }
            CoerceMethod(cl, obj, OM_DISPOSE);
        }
        return 0;

    case MUIM_Settingsgroup_ConfigToGadgets:
    {
        Object *configdata = ((struct MUIP_Settingsgroup_ConfigToGadgets *)msg)->configdata;

        Gadgets(d, list);
        for (i = 0; i < 7; i++) ItemToGadget(configdata, list[i], &Items[i]);
        return 0;
    }

    case MUIM_Settingsgroup_GadgetsToConfig:
    {
        Object *configdata = ((struct MUIP_Settingsgroup_ConfigToGadgets *)msg)->configdata;

        Gadgets(d, list);
        for (i = 0; i < 7; i++) ItemFromGadget(configdata, list[i], &Items[i]);
        return 0;
    }
    }
    return DoSuperMethodA(cl, obj, msg);
}
