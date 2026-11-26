/*
    Copyright (C) 2002-2026, The AROS Development Team. All rights reserved.

*/
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include <exec/types.h>
#include <prefs/prefhdr.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/iffparse.h>
#include <proto/dos.h>
#include <proto/commodities.h>
#include <proto/muimaster.h>

/*  #define MYDEBUG 1 */
#include "debug.h"

#include "muimaster_intern.h"
#include "mui.h"
#include "support.h"
#include "prefs.h"
#include "imspec.h"
#include "modules.h"
#include "classes/configdata_private.h"

extern struct Library *MUIMasterBase;

/* ENV:OpenMUI/global.prefs read and parsed once for every application
 * (an application's start with a full settings file was mostly this: 160
 * records read, 200 lookups, 15 frame specs; opbench prof). Kept by the
 * library while the file's date, size and key are the same (one Lock,
 * Examine and UnLock per application instead of the load); an
 * application holds the one it was made with until it is disposed of.
 * Never written: an application's own records are in its own dataspace
 * and looked at first (cfg_find); anything that changes or writes an
 * application's records gives it copies of these first (cfg_records),
 * and the settings follow cfg_own as the shared defaults do. */
struct cfg_global
{
    LONG users;                 /* its applications, and 1 while current */
    Object *ds;                 /* the file's records: a Dataspace */
    LONG stamp[5];              /* the file's date, size and key */
    struct ZunePrefsNew prefs;  /* built from the records alone */
};
static struct cfg_global *cfg_glob;     /* the current one, or NULL */

BOOL zune_dataspace_only(struct IClass *cl, Object *obj, ULONG id);
APTR zune_dataspace_get(struct IClass *cl, Object *obj, ULONG id);
void zune_dataspace_underlay(struct IClass *cl, Object *obj, Object *src);

#define CFG_LOADING 2

struct MUI_ConfigdataData
{
    Object                      *app;
    CONST_STRPTR                appbase;
    struct ZunePrefsNew        *prefs;     /* own, cfg_shared or glob's */
    struct cfg_global          *glob;      /* the shared global settings */
    UBYTE                       loaded;    /* a settings file read at creation */
    UBYTE                       direct;    /* OM_NEW's reads: the lock is held */
    UBYTE                       unshared;  /* glob's records are not looked at
                                            * (CFG_LOADING: being read) */
};

IPTR Dataspace__MUIM_Find(struct IClass *cl, Object *obj,
    struct MUIP_Dataspace_Find *msg);

/* A Configdata is built for every application and reads about 80
 * settings at creation; ask the dataspace, our superclass, for each
 * without passing through our own dispatcher first, and take the
 * defaults from the tables being walked (perfbench create+dispose) */
static __attribute__((noinline)) APTR cfg_find(struct IClass *cl, Object *obj,
    ULONG id)
{
    struct MUIP_Dataspace_Find msg = { MUIM_Dataspace_Find, id };

    struct MUI_ConfigdataData *data = INST_DATA(cl, obj);

    /* The constructor holds the dataspace lock across all initial reads. */
    if (data->direct)
        return (APTR)Dataspace__MUIM_Find(cl->cl_Super, obj, &msg);
    return (APTR) DoSuperMethodA(cl, obj, (Msg) &msg);
}

/* A record of the object's own, else the shared global settings' (never
 * written, so read without their lock) */
static APTR cfg_get(struct IClass *cl, Object *obj, ULONG id)
{
    struct MUI_ConfigdataData *data = INST_DATA(cl, obj);
    APTR res = cfg_find(cl, obj, id);

    if (!res && data->glob && !data->unshared)
        return zune_dataspace_get(cl->cl_Super, data->glob->ds, id);
    return res;
}

static ULONG cfg_ulong(struct IClass *cl, Object *obj, ULONG id);
static CONST_STRPTR cfg_string(struct IClass *cl, Object *obj, ULONG id);




/**************************************************************************
 Default ImageSpec values
**************************************************************************/


const static struct spec_cfg DefImspecValues[] =
{
    { MUII_WindowBack,    MUICFG_Background_Window,     "0:128" },
    { MUII_RequesterBack, MUICFG_Background_Requester,  "0:137" },
    { MUII_ButtonBack,    MUICFG_Background_Button,     "0:128" },
    { MUII_ListBack,      MUICFG_Background_List,       "0:128" },
    { MUII_TextBack,      MUICFG_Background_Text,       "0:128" },
    { MUII_PropBack,      MUICFG_Background_Prop,       "0:128" },
    { MUII_PopupBack,     MUICFG_Background_PopUp,      "0:130" },
    { MUII_SelectedBack,  MUICFG_Background_Selected,   "0:131" },
    { MUII_ListCursor,    MUICFG_Background_ListCursor, "0:131" },
    { MUII_ListSelect,    MUICFG_Background_ListSelect, "0:135" },
    { MUII_ListSelCur,    MUICFG_Background_ListSelCur, "0:138" },
    { MUII_ArrowUp,       MUICFG_Image_ArrowUp,         "1:0" },
    { MUII_ArrowDown,     MUICFG_Image_ArrowDown,       "1:1" },
    { MUII_ArrowLeft,     MUICFG_Image_ArrowLeft,       "1:2" },
    { MUII_ArrowRight,    MUICFG_Image_ArrowRight,      "1:3" },
    { MUII_CheckMark,     MUICFG_Image_CheckMark,       "1:4" },
    { MUII_RadioButton,   MUICFG_Image_RadioButton,     "1:5" },
    { MUII_Cycle,         MUICFG_Image_Cycle,           "1:6" },
    { MUII_PopUp,         MUICFG_Image_PopUp,           "1:7" },
    { MUII_PopFile,       MUICFG_Image_PopFile,         "1:8" },
    { MUII_PopDrawer,     MUICFG_Image_PopDrawer,       "1:9" },
    { MUII_PropKnob,      MUICFG_Image_PropKnob,        "0:129" },
    { MUII_Drawer,        MUICFG_Image_Drawer,          "1:10" },
    { MUII_HardDisk,      MUICFG_Image_HardDisk,        "1:11" },
    { MUII_Disk,          MUICFG_Image_Disk,            "1:12" },
    { MUII_Chip,          MUICFG_Image_Chip,            "1:13" },
    { MUII_Volume,        MUICFG_Image_Volume,          "1:14" },
    { MUII_RegisterBack,  MUICFG_Background_Register,   "0:128" },
    { MUII_Network,       MUICFG_Image_Network,         "1:15" },
    { MUII_Assign,        MUICFG_Image_Assign,          "1:16" },
    { MUII_TapePlay,      MUICFG_Image_TapePlay,        "1:17" },
    { MUII_TapePlayBack,  MUICFG_Image_TapePlayBack,    "1:18" },
    { MUII_TapePause,     MUICFG_Image_TapePause,       "1:19" },
    { MUII_TapeStop,      MUICFG_Image_TapeStop,        "1:20" },
    { MUII_TapeRecord,    MUICFG_Image_TapeRecord,      "1:21" },
    { MUII_GroupBack,     MUICFG_Background_Framed,     "0:128" },
    { MUII_SliderBack,    MUICFG_Background_Slider,     "0:128" },
    { MUII_SliderKnob,    MUICFG_Background_SliderKnob, "0:128" },
    { MUII_TapeUp,        MUICFG_Image_TapeUp,          "1:22" },
    { MUII_TapeDown,      MUICFG_Image_TapeDown,        "1:23" },
    { MUII_PageBack,      MUICFG_Background_Page,       "0:128" },
    { MUII_ReadListBack,  MUICFG_Background_ReadList,   "0:128" },
    /* MUI 3.9 draws the font popup like the popup arrow and has no
     * setting of its own for it (tests/native/imgspec.c) */
    { MUII_PopFont,       MUICFG_Image_PopUp,           "1:7" },
    { 0,                  0,                            NULL },
};

/* called by Configdata_New */
static void init_imspecs(struct IClass *cl, Object *obj,
    struct MUI_ConfigdataData *data)
{
    int i;

    for (i = 0; DefImspecValues[i].defspec; i++)
    {
        CONST_STRPTR imspec;
        const struct spec_cfg *img = DefImspecValues + i;

        imspec = cfg_get(cl, obj, img->cfgid);
        if (!imspec)
            imspec = img->defspec;
/*          D(bug("init_imspecs: %ld %lx %s ...\n", img->muiv, img->cfgid, imspec)); */
        data->prefs->imagespecs[img->muiv] = imspec;
        if (!data->prefs->imagespecs[img->muiv])
        {
/*              D(bug("*** init_imspecs: null imagespec %ld\n", img->muiv)); */
        }
    }
}

/**************************************************************************
 Default FrameSpec values
**************************************************************************/

/* spec format : type, recessed, left, right, up, down spacing */
const static struct spec_cfg DefFramespecValues[] =
{
    { MUIV_Frame_None,        MUICFG_Invalid,           "000000" }, /* invisible frame          */
    { MUIV_Frame_Button,      MUICFG_Frame_Button,      "202211" }, /* text button              */
    { MUIV_Frame_ImageButton, MUICFG_Frame_ImageButton, "202211" }, /* image button             */
    { MUIV_Frame_Text,        MUICFG_Frame_Text,        "212211" }, /* textfield without input  */
    { MUIV_Frame_String,      MUICFG_Frame_String,      "302211" }, /* string gadget            */
    { MUIV_Frame_ReadList,    MUICFG_Frame_ReadList,    "212211" }, /* list without input       */
    { MUIV_Frame_InputList,   MUICFG_Frame_InputList,   "202211" }, /* list with input          */
    { MUIV_Frame_Prop,        MUICFG_Frame_Prop,        "202211" }, /* scrollbar container      */
    { MUIV_Frame_Gauge,       MUICFG_Frame_Gauge,       "210000" }, /* gauge                    */
    { MUIV_Frame_Group,       MUICFG_Frame_Group,       "314444" }, /* normal group             */
    { MUIV_Frame_PopUp,       MUICFG_Frame_PopUp,       "112211" }, /* cycle menu, popup window */
    { MUIV_Frame_Virtual,     MUICFG_Frame_Virtual,     "212211" }, /* virt group               */
    { MUIV_Frame_Slider,      MUICFG_Frame_Slider,      "400000" }, /* slider container         */
    { MUIV_Frame_Knob,        MUICFG_Frame_Knob,        "202200" }, /* slider knob              */
    { MUIV_Frame_Drag,        MUICFG_Frame_Drag,        "300000" }, /* dnd frame                */
    { -1,                     -1,                       NULL     },
};

/* called by Configdata_New */
static void init_framespecs(struct IClass *cl, Object *obj,
    struct MUI_ConfigdataData *data)
{
    int i;

    for (i = 0; DefFramespecValues[i].defspec; i++)
    {
        CONST_STRPTR framespec;
        const struct spec_cfg *fcfg = DefFramespecValues + i;

        framespec = cfg_get(cl, obj, fcfg->cfgid);
        if (!framespec)
            framespec = fcfg->defspec;
        zune_frame_spec_to_intern(framespec,
            &data->prefs->frames[fcfg->muiv]);
    }
}

/**************************************************************************
 Default ULONG values
**************************************************************************/


#define PREF_DEFAULT(id, value, field) \
    {id, value, offsetof(struct ZunePrefsNew, field) \
     | (sizeof(((struct ZunePrefsNew *)0)->field) == sizeof(UWORD) \
        ? DEF_UWORD : 0)}

/* The defaults are MUI 3.8's, read from its settings program (frames:
 * top and bottom inner spacing 1; groups 6/3; window 4/3; titles above;
 * positions remembered; bubbles after 2.0 s, then 0.3 s). Redraw stays
 * "without clear": the same pixels as MUI 3.8's "with clear", drawn once
 * instead of cleared and drawn. */
const static struct def_ulval DefULValues[] = {
    PREF_DEFAULT(MUICFG_Window_Spacing_Left, 4, window_inner_left),
    PREF_DEFAULT(MUICFG_Window_Spacing_Right, 4, window_inner_right),
    PREF_DEFAULT(MUICFG_Window_Spacing_Top, 3, window_inner_top),
    /* MUI 3.8: no extra title bar gadgets until switched on */
    PREF_DEFAULT(MUICFG_Window_Buttons, 0, window_buttons),
    PREF_DEFAULT(MUICFG_Window_Spacing_Bottom, 3, window_inner_bottom),
    /* MUI 3.8: thick frames on screens at least twice as wide as tall */
    PREF_DEFAULT(MUICFG_Frame_Thickness, FRAME_THICKNESS_AUTO, frame_thickness),
    PREF_DEFAULT(MUICFG_Window_Positions, WINDOW_POSITION_REMEMBER_ON_EXIT, window_position),
    PREF_DEFAULT(MUICFG_Window_Redraw, WINDOW_REDRAW_WITHOUT_CLEAR, window_redraw),
    /* MUI 3.8's default; MUI 3.9 changed to simple refresh, which loses
     * covered contents of applications that draw outside MUIM_Draw
     * (AmiPG_MUI's keyboard) and repaints slower */
    PREF_DEFAULT(MUICFG_Window_Refresh, WINDOW_REFRESH_SMART, window_refresh),
    PREF_DEFAULT(MUICFG_Radio_HSpacing, 4, radiobutton_hspacing),
    PREF_DEFAULT(MUICFG_Radio_VSpacing, 1, radiobutton_vspacing),
    PREF_DEFAULT(MUICFG_Group_HSpacing, 6, group_hspacing),
    PREF_DEFAULT(MUICFG_Group_VSpacing, 3, group_vspacing),
    PREF_DEFAULT(MUICFG_Cycle_MenuCtrl_Position, CYCLE_MENU_POSITION_BELOW, cycle_menu_position),
    PREF_DEFAULT(MUICFG_Cycle_MenuCtrl_Level, 2, cycle_menu_min_entries),
    PREF_DEFAULT(MUICFG_Cycle_MenuCtrl_Speed, 0, cycle_menu_speed),
    PREF_DEFAULT(MUICFG_Cycle_Menu_Recessed, FALSE, cycle_menu_recessed_entries),
    PREF_DEFAULT(MUICFG_Listview_Font_Leading, 1, list_linespacing),
    PREF_DEFAULT(MUICFG_Listview_Smoothed, FALSE, list_smoothed),
    PREF_DEFAULT(MUICFG_Listview_SmoothVal, 0, list_smoothval),
    PREF_DEFAULT(MUICFG_Listview_Refresh, LISTVIEW_REFRESH_MIXED, list_refresh),
    PREF_DEFAULT(MUICFG_Listview_Multi, LISTVIEW_MULTI_SHIFTED, list_multi),
    PREF_DEFAULT(MUICFG_GroupTitle_Position, GROUP_TITLE_POSITION_ABOVE, group_title_position),
    PREF_DEFAULT(MUICFG_GroupTitle_Color, GROUP_TITLE_COLOR_HILITE, group_title_color),
    PREF_DEFAULT(MUICFG_Scrollbar_Type, SCROLLBAR_TYPE_STANDARD, scrollbar_type),
    PREF_DEFAULT(MUICFG_Scrollbar_Arrangement, SCROLLBAR_ARRANGEMENT_TOP, scrollbar_arrangement),
    PREF_DEFAULT(MUICFG_Balance_Look, BALANCING_SHOW_FRAMES, balancing_look),
    PREF_DEFAULT(MUICFG_Dragndrop_Look, DND_LOOK_GHOSTED_ON_BOX, drag_look),
    PREF_DEFAULT(MUICFG_Drag_Autostart, TRUE, drag_autostart),
    PREF_DEFAULT(MUICFG_Drag_Autostart_Length, 3, drag_autostart_length),
    PREF_DEFAULT(MUICFG_Drag_LeftButton, TRUE, drag_left_button),
    PREF_DEFAULT(MUICFG_Drag_MiddleButton, FALSE, drag_middle_button),
    /* MUI 3.8 has no such setting: its register titles never widen the
     * group, they are cut (MultiRen's preferences: "Open on sta") */
    PREF_DEFAULT(MUICFG_Register_TruncateTitles, TRUE, register_truncate_titles),
    PREF_DEFAULT(MUICFG_Screen_Mode, 0, screenmode),
    PREF_DEFAULT(MUICFG_Screen_Mode_ID, -1, screenmodeid),
    PREF_DEFAULT(MUICFG_Screen_Width, 0, screen_width),
    PREF_DEFAULT(MUICFG_Screen_Height, 0, screen_height),
    PREF_DEFAULT(MUICFG_PublicScreen_PopToFront, TRUE, publicscreen_pop_to_front),
    PREF_DEFAULT(MUICFG_Iconification_ShowIcon, TRUE, iconification_show_icon),
    PREF_DEFAULT(MUICFG_Iconification_ShowMenu, FALSE, iconification_show_menu),
    PREF_DEFAULT(MUICFG_Iconification_OnStartup, FALSE, iconification_on_startup),
    PREF_DEFAULT(MUICFG_Interfaces_EnableARexx, TRUE, interfaces_enable_arexx),
    PREF_DEFAULT(MUICFG_BubbleHelp_FirstDelay, 20, bubblehelp_first_delay),
    PREF_DEFAULT(MUICFG_BubbleHelp_NextDelay, 3, bubblehelp_next_delay),
    {0, 0, 0},
};

#undef PREF_DEFAULT

/**************************************************************************
 Default string values
**************************************************************************/

/* Every string setting: its id, where it goes in the prefs (Configdata_
 * StoreString: a string, or a key parsed into a ZuneKeySpec), and its
 * default. A table instead of a switch per id (2026-11-06: 0.8 KB less). */
#define FIELD(f) ((UWORD)offsetof(struct ZunePrefsNew, f))
#define KEYFIELD 0x8000     /* a key (Configdata__SetKey) */
#define NOFIELD 0xffff      /* kept in the dataspace only */

const static struct def_strval DefStrValues[] = {
    {MUICFG_Font_Normal, FIELD(fonts[-MUIV_Font_Normal]), ""},
    {MUICFG_Font_List, FIELD(fonts[-MUIV_Font_List]), ""},
    {MUICFG_Font_Tiny, FIELD(fonts[-MUIV_Font_Tiny]), ""},
    {MUICFG_Font_Fixed, FIELD(fonts[-MUIV_Font_Fixed]), ""},
    {MUICFG_Font_Title, FIELD(fonts[-MUIV_Font_Title]), ""},
    {MUICFG_Font_Big, FIELD(fonts[-MUIV_Font_Big]), ""},
    {MUICFG_Font_Button, FIELD(fonts[-MUIV_Font_Button]), ""},
    {MUICFG_Font_Knob, FIELD(fonts[-MUIV_Font_Knob]), ""},
    {MUICFG_String_Background, FIELD(string_bg_inactive), "2:m2"},
    {MUICFG_String_Text, FIELD(string_text_inactive), "m5"},
    {MUICFG_String_ActiveBackground, FIELD(string_bg_active), "2:m2"},
    {MUICFG_String_ActiveText, FIELD(string_text_active), "m5"},
    {MUICFG_String_Cursor, FIELD(string_cursor), "m7"},
    {MUICFG_String_MarkedBackground, FIELD(string_bg_marked), "m6"},
    {MUICFG_String_MarkedText, FIELD(string_text_marked), "m0"},
    {MUICFG_ActiveObject_Color, FIELD(active_object_color), "m0"},
    {MUICFG_Pen_Shine, FIELD(mui_pens[MPEN_SHINE]), ""},
    {MUICFG_Pen_Halfshine, FIELD(mui_pens[MPEN_HALFSHINE]), ""},
    {MUICFG_Pen_Background, FIELD(mui_pens[MPEN_BACKGROUND]), ""},
    {MUICFG_Pen_Halfshadow, FIELD(mui_pens[MPEN_HALFSHADOW]), ""},
    {MUICFG_Pen_Shadow, FIELD(mui_pens[MPEN_SHADOW]), ""},
    {MUICFG_Pen_Text, FIELD(mui_pens[MPEN_TEXT]), ""},
    {MUICFG_Pen_Fill, FIELD(mui_pens[MPEN_FILL]), ""},
    {MUICFG_Pen_Mark, FIELD(mui_pens[MPEN_MARK]), ""},
    {MUICFG_Keyboard_Press, FIELD(muikeys[MUIKEY_PRESS]) | KEYFIELD, "-upstroke return"},
    {MUICFG_Keyboard_Toggle, FIELD(muikeys[MUIKEY_TOGGLE]) | KEYFIELD, "-repeat space"},
    {MUICFG_Keyboard_Up, FIELD(muikeys[MUIKEY_UP]) | KEYFIELD, "-repeat up"},
    {MUICFG_Keyboard_Down, FIELD(muikeys[MUIKEY_DOWN]) | KEYFIELD, "-repeat down"},
    {MUICFG_Keyboard_PageUp, FIELD(muikeys[MUIKEY_PAGEUP]) | KEYFIELD, "-repeat shift up"},
    {MUICFG_Keyboard_PageDown, FIELD(muikeys[MUIKEY_PAGEDOWN]) | KEYFIELD, "-repeat shift down"},
    {MUICFG_Keyboard_Top, FIELD(muikeys[MUIKEY_TOP]) | KEYFIELD, "control up"},
    {MUICFG_Keyboard_Bottom, FIELD(muikeys[MUIKEY_BOTTOM]) | KEYFIELD, "control down"},
    {MUICFG_Keyboard_Left, FIELD(muikeys[MUIKEY_LEFT]) | KEYFIELD, "-repeat left"},
    {MUICFG_Keyboard_Right, FIELD(muikeys[MUIKEY_RIGHT]) | KEYFIELD, "-repeat right"},
    {MUICFG_Keyboard_WordLeft, FIELD(muikeys[MUIKEY_WORDLEFT]) | KEYFIELD, "-repeat control left"},
    {MUICFG_Keyboard_WordRight, FIELD(muikeys[MUIKEY_WORDRIGHT]) | KEYFIELD, "-repeat control right"},
    {MUICFG_Keyboard_LineStart, FIELD(muikeys[MUIKEY_LINESTART]) | KEYFIELD, "shift left"},
    {MUICFG_Keyboard_LineEnd, FIELD(muikeys[MUIKEY_LINEEND]) | KEYFIELD, "shift right"},
    {MUICFG_Keyboard_NextGadget, FIELD(muikeys[MUIKEY_GADGET_NEXT]) | KEYFIELD, "-repeat tab"},
    {MUICFG_Keyboard_PrevGadget, FIELD(muikeys[MUIKEY_GADGET_PREV]) | KEYFIELD, "-repeat shift tab"},
    {MUICFG_Keyboard_GadgetOff, FIELD(muikeys[MUIKEY_GADGET_OFF]) | KEYFIELD, "control tab"},
    {MUICFG_Keyboard_CloseWindow, FIELD(muikeys[MUIKEY_WINDOW_CLOSE]) | KEYFIELD, "esc"},
    {MUICFG_Keyboard_NextWindow, FIELD(muikeys[MUIKEY_WINDOW_NEXT]) | KEYFIELD, "-repeat alt tab"},
    {MUICFG_Keyboard_PrevWindow, FIELD(muikeys[MUIKEY_WINDOW_PREV]) | KEYFIELD, "-repeat alt shift tab"},
    {MUICFG_Keyboard_Help, FIELD(muikeys[MUIKEY_HELP]) | KEYFIELD, "help"},
    {MUICFG_Keyboard_Popup, FIELD(muikeys[MUIKEY_POPUP]) | KEYFIELD, "control p"},
    {MUICFG_Drag_LMBModifier, FIELD(drag_left_modifier) | KEYFIELD, "control"},
    {MUICFG_Drag_MMBModifier, FIELD(drag_middle_modifier) | KEYFIELD, ""},
    {MUICFG_PublicScreen, FIELD(publicscreen_name), ""},
    {MUICFG_Iconification_Hotkey, FIELD(iconification_hotkey), ""},
    {0, 0, 0},
};

/* ParseIX inverts the keymap and compares qualifier names: about 0.3 ms
 * per binding, and every Configdata parses 24 of them. The parsed
 * expressions are kept here for every application: the default strings
 * (constants, found by address) and the saved ones (a copy of the
 * string, found by its text: a global.prefs gives every application the
 * same 24). Freed at expunge (zune_keycache_free). */
#define KEYCACHE 48
static struct { CONST_STRPTR str; IX ix; UWORD field; UBYTE well, owned; }
    keycache[KEYCACHE];
static int keycache_count;

/* The settings of an application that read no settings file: the
 * defaults, the same for every such application, built by the first one
 * and shared by the others (an empty application's start was mostly
 * this build: about 200 lookups in an empty dataspace, 15 frame specs
 * and 24 key bindings). Never written: Configdata_StoreString gives an
 * application its own copy first. Kept until expunge. */
static struct ZunePrefsNew *cfg_shared;

/* The settings are not the object's own to write or free */
static BOOL cfg_borrowed(struct MUI_ConfigdataData *data)
{
    return data->prefs == cfg_shared
        || (data->glob && data->prefs == &data->glob->prefs);
}

/* The shared settings are never written: an own copy first, which the
 * application's GlobalInfo then points to. FALSE without memory. */
static __attribute__((noinline)) BOOL cfg_own(struct MUI_ConfigdataData *data)
{
    struct ZunePrefsNew *own;
    struct MUI_GlobalInfo *mgi;

    if (!cfg_borrowed(data))
        return TRUE;
    if (!(own = AllocMem(sizeof(*own), MEMF_ANY)))
        return FALSE;
    CopyMem(data->prefs, own, sizeof(*own));
    if (data->app && (mgi = muiNotifyData(data->app)->mnd_GlobalInfo)
        && mgi->mgi_Prefs == data->prefs)
        mgi->mgi_Prefs = own;
    data->prefs = own;
    return TRUE;
}

/* One user less; the last one frees it */
static __attribute__((noinline)) void cfg_release(
    register struct cfg_global *g __asm("a0"))
{
    LONG left;

    if (!g)
        return;
    Forbid();
    left = --g->users;
    Permit();
    if (left)
        return;
    if (g->ds)
        MUI_DisposeObject(g->ds);
    FreeMem(g, sizeof(*g));
}

/* Before the built-in classes are freed (expunge) */
void zune_cfg_global_free(void)
{
    struct cfg_global *g = cfg_glob;

    cfg_glob = NULL;
    cfg_release(g);
}

/* At OM_DISPOSE: the own settings */
static __attribute__((noinline)) void cfg_free(struct MUI_ConfigdataData *data)
{
    if (data->prefs && !cfg_borrowed(data))
        FreeMem(data->prefs, sizeof(*data->prefs));
    cfg_release(data->glob);
}

void zune_keycache_free(void)
{
    int i;

    if (cfg_shared)
        FreeMem(cfg_shared, sizeof(*cfg_shared));
    cfg_shared = NULL;

    for (i = 0; i < keycache_count; i++)
        if (keycache[i].owned)
            FreeVec((APTR)keycache[i].str);
    keycache_count = 0;
}

static BOOL is_default_string(CONST_STRPTR val)
{
    int i;

    for (i = 0; DefStrValues[i].id; i++)
        if (DefStrValues[i].val == val)
            return TRUE;
    return FALSE;
}

/* A parsed binding into the cache; a saved one's text copied */
static __attribute__((noinline)) void keycache_add(CONST_STRPTR val,
    ZuneKeySpec *key, UWORD field)
{
    UBYTE owned = !is_default_string(val);
    CONST_STRPTR str = owned ? (CONST_STRPTR)StrDup(val) : val;

    if (!str)
        return;
    Forbid();
    if (keycache_count < KEYCACHE)
    {
        keycache[keycache_count].str = str;
        keycache[keycache_count].ix = key->ix;
        keycache[keycache_count].well = key->ix_well;
        keycache[keycache_count].owned = owned;
        keycache[keycache_count].field = field;
        keycache_count++;
        str = NULL;
    }
    Permit();
    if (str && owned)
        FreeVec((APTR)str);
}

/* The cache's entry of binding field with the text val, or NULL: a
 * cached text is compared only for its own binding (one strcmp, not one
 * per cached binding) */
static __attribute__((noinline)) APTR keycache_find(
    register CONST_STRPTR val __asm("a0"), register UWORD field __asm("d0"))
{
    int i, count = keycache_count;

    for (i = 0; i < count; i++)
        if (keycache[i].field == field && (keycache[i].str == val
            || (keycache[i].owned && !strcmp(keycache[i].str, val))))
            return &keycache[i];
    return NULL;
}

static __attribute__((noinline)) void Configdata__SetKey(ZuneKeySpec *key,
    CONST_STRPTR val,
    UWORD field)
{
    typeof(&keycache[0]) hit;

    if ((key->readable_hotkey = val) == NULL)
    {
        key->ix_well = 0;
        return;
    }
    if ((hit = keycache_find(val, field)))
    {
        key->ix = hit->ix;
        key->ix_well = hit->well;
        return;
    }
    key->ix_well = ZUNE_CX() && !ParseIX(key->readable_hotkey, &key->ix);
    if (keycache_count < KEYCACHE)
        keycache_add(val, key, field);
}

/* Assign a !persistent! string stored in the dataspace (or a default) to
 * its ZunePrefsNew field */
static void Configdata_StoreField(struct MUI_ConfigdataData *data,
    UWORD field, CONST_STRPTR val)
{
    APTR at = (UBYTE *)data->prefs + (field & ~KEYFIELD);

    if (field == NOFIELD)
        return;
    if (field & KEYFIELD)
        Configdata__SetKey(at, val, field);
    else
        *(CONST_STRPTR *)at = val;
}

static void Configdata_StoreString(struct MUI_ConfigdataData *data,
    ULONG id, CONST_STRPTR val)
{
    int i;

    if (!cfg_own(data))
        return;

    /* the 16 custom frames: consecutive ids and fields */
    if (id >= MUICFG_CustomFrame_1 && id <= MUICFG_CustomFrame_16)
    {
        (&data->prefs->customframe_config_1)[id - MUICFG_CustomFrame_1] = val;
        return;
    }
    for (i = 0; DefStrValues[i].id; i++)
        if (DefStrValues[i].id == id)
        {
            Configdata_StoreField(data, DefStrValues[i].field, val);
            return;
        }
}

/* The defaults the setters compare with (configset.c, in the module) */
static const struct ZuneCfgTables cfg_tables = {
    DefImspecValues, DefFramespecValues, DefULValues, DefStrValues
};

static void Configdata_SynchronizeString(struct IClass *cl, Object *obj,
    ULONG id)
{
    Configdata_StoreString(INST_DATA(cl, obj), id,
        cfg_string(cl, obj, id));
}

/**************************************************************************
 OM_NEW
 Load global (and maybe application-specific) prefs files into the dataspace,
 then fill the prefs struct with dataspace or default values
**************************************************************************/
static BOOL prefs_read(Object *ds, CONST_STRPTR filename, BOOL clear);
static void prefs_write(Object *ds, CONST_STRPTR filename);

/* A settings file over what the object has: <ENV or ENVARC>:OpenMUI/
 * <name>.prefs, or with CFG_OLD the earlier edition's ENV(ARC):Zune/.
 * Called from OM_NEW (whose frame is small without the name), its
 * frame and OM_NEW's together are no deeper than OM_NEW's alone was
 * (316 bytes): room for a base name of 200 characters. */
#define CFG_ARC 1
#define CFG_OLD 2
static __attribute__((noinline)) BOOL cfg_read(Object *ds, LONG which,
    CONST_STRPTR name)
{
    char filename[224];

    snprintf(filename, sizeof(filename), "%s:%s/%s.prefs",
        which & CFG_ARC ? "ENVARC" : "ENV",
        which & CFG_OLD ? "Zune" : "OpenMUI", name);
    return prefs_read(ds, filename, FALSE);
}

static void cfg_build(struct IClass *cl, Object *obj,
    struct MUI_ConfigdataData *data);

/* The library's class itself over the library's Dataspace: what OM_NEW's
 * direct reads and the shared global settings need */
static inline __attribute__((always_inline)) BOOL cfg_plain(
    struct IClass *cl, Object *obj)
{
    return OCLASS(obj) == cl && cl->cl_Super->cl_ID
        && !strcmp(cl->cl_Super->cl_ID, MUIC_Dataspace);
}

/* The date, size and key of ENV:OpenMUI/global.prefs; FALSE without it */
#define CFG_GLOBAL "ENV:OpenMUI/global.prefs"
static __attribute__((noinline)) BOOL cfg_stamp(LONG *stamp,
    struct IFFHandle *iff)
{
    BPTR fh = iff ? (BPTR)iff->iff_Stream : 0;  /* open, or by name */
    UBYTE room[sizeof(struct FileInfoBlock) + 2];
    struct FileInfoBlock *fib = (APTR)(((IPTR)room + 2) & ~3);
    BPTR lock = 0;
    BOOL ok;

    if (!fh && !(lock = Lock(CFG_GLOBAL, SHARED_LOCK)))
        return FALSE;
    if ((ok = fh ? ExamineFH(fh, fib) : Examine(lock, fib)))
    {
        stamp[0] = fib->fib_Date.ds_Days;
        stamp[1] = fib->fib_Date.ds_Minute;
        stamp[2] = fib->fib_Date.ds_Tick;
        stamp[3] = fib->fib_Size;
        stamp[4] = fib->fib_DiskKey;
    }
    if (lock)
        UnLock(lock);
    return ok;
}

/* The shared global settings for this application (data->glob, and
 * data->prefs theirs): the current ones if the file is still the one
 * they were read from (asked only when there are current ones), else
 * read now. FALSE without the file (or without memory): the caller
 * reads as before. Out of line: OM_NEW's frame. */
static __attribute__((noinline)) BOOL cfg_attach(struct IClass *cl,
    Object *obj, struct MUI_ConfigdataData *data)
{
    LONG stamp[5];
    struct cfg_global *g = NULL, *old = NULL;
    BOOL have;

    if (cfg_glob)
    {
        have = cfg_stamp(stamp, NULL);
        Forbid();
        if ((g = cfg_glob) && have
            && !memcmp(g->stamp, stamp, sizeof(stamp)))
            g->users++;
        else
        {
            old = g;
            cfg_glob = g = NULL;
        }
        Permit();
        cfg_release(old);
        if (!have)
            return FALSE;
    }
    if (!g)
    {
        if (!(g = AllocMem(sizeof(*g), MEMF_CLEAR)))
            return FALSE;
        g->users = 1;
        /* (read through the object: its ReadIFF, cfg_records, takes the
         * file's stamp from the open handle and hands the records on) */
        data->glob = g;
        data->unshared = CFG_LOADING;
        if ((g->ds = MUI_NewObject(MUIC_Dataspace, TAG_DONE)))
            have = prefs_read(obj, CFG_GLOBAL, FALSE);
        data->unshared = FALSE;
        if (!g->ds || !have)
        {
            data->glob = NULL;
            cfg_release(g);
            return FALSE;
        }
        data->glob = g;
        data->prefs = &g->prefs;
        data->loaded = TRUE;
        cfg_build(cl, obj, data);
        Forbid();
        if (!cfg_glob)
        {
            cfg_glob = g;
            g->users++;
        }
        Permit();
    }
    data->glob = g;
    data->prefs = NULL;
    data->unshared = TRUE;      /* reading its file changes nothing */
    data->loaded = data->appbase && Stricmp(data->appbase, "global")
        && (cfg_read(obj, 0, data->appbase)
            || cfg_read(obj, CFG_ARC, data->appbase));
    data->unshared = FALSE;
    if (data->loaded && !zune_dataspace_only(cl->cl_Super, obj,
            MUICFG_WindowPos)
        && (data->prefs = AllocMem(sizeof(*data->prefs), MEMF_CLEAR)))
        cfg_build(cl, obj, data);
    if (!data->prefs)
        data->prefs = &g->prefs;
    data->loaded = TRUE;
    return TRUE;
}

/* The settings files over the object (the drawer checked first); TRUE
 * if one was read. Out of line: OM_NEW's frame. */
static __attribute__((noinline)) BOOL cfg_load(struct IClass *cl,
    Object *obj, struct MUI_ConfigdataData *data)
{
    BOOL loaded = FALSE;
    LONG res = 0;

    /* An application's: the global settings as the library has them,
     * and its own file over them. With nothing but window positions of
     * its own (what an application saves at its end) the settings are
     * the shared ones too. */
    /* (while the library has them: no look at the drawer either) */
    if (data->app && cfg_glob && cfg_plain(cl, obj)
        && cfg_attach(cl, obj, data))
        return TRUE;

    /* Without a preference drawer there is nothing to load: one Lock
     * replaces up to four failing Open calls (each a handler round
     * trip) per application. ENV: is restored from ENVARC: at boot and
     * the preferences program creates both drawers when it first saves.
     * The settings are in ENV:OpenMUI. An earlier edition kept them in
     * ENV:Zune: while an installation has not moved them (the installer
     * and OpenMUI prefs do), they are read from there, never written
     * (res CFG_OLD: the old drawer). */
    {
        BPTR drawer = Lock("ENV:OpenMUI", SHARED_LOCK);

        if (!drawer && (drawer = Lock("ENV:Zune", SHARED_LOCK)))
            res = CFG_OLD;
        if (!drawer)
            return FALSE;
        UnLock(drawer);
    }

    /* As MUI does: the global settings first, then the application's
     * own file on top of them. That file may hold only the window
     * positions remembered when the application quit; it must not hide
     * the global settings (it did: an application that once remembered
     * a window kept the settings of that moment for ever). */
    if (!res && data->app && !cfg_glob && cfg_plain(cl, obj))
    {
        if (cfg_attach(cl, obj, data))
            return TRUE;
        res = CFG_ARC;      /* no such file in ENV: */
    }
    loaded = !(res & CFG_OLD) && ((!res && cfg_read(obj, 0, "global"))
        || cfg_read(obj, CFG_ARC, "global"));
    res &= CFG_OLD;

    /* No global settings of OpenMUI's own: an earlier edition's */
    if (!loaded && (cfg_read(obj, CFG_OLD, "global")
        || cfg_read(obj, CFG_OLD | CFG_ARC, "global")))
        loaded = res = CFG_OLD;

    /* The application's own file, from the same drawer as the
     * global settings (window positions saved while the settings are
     * still the earlier edition's go to OpenMUI's drawer: they are
     * read once the settings have been moved there) */
    if (data->appbase && Stricmp(data->appbase, "global")
        && (cfg_read(obj, res, data->appbase)
            || cfg_read(obj, res | CFG_ARC, data->appbase)))
        loaded = TRUE;
    return data->loaded = loaded;
}

/* data->prefs from the dataspace and the defaults; without a file read,
 * the shared defaults from then on. Out of line: OM_NEW's frame. */
static __attribute__((noinline)) void cfg_build(struct IClass *cl,
    Object *obj, struct MUI_ConfigdataData *data)
{
    int i;

    /* Only bypass dispatch for the builtin object during construction.
     * Subclasses retain their inherited lookup and semaphore methods. */
    if (cfg_plain(cl, obj))
    {
        DoMethod(obj, MUIM_Semaphore_ObtainShared);
        data->direct = TRUE;
    }

    /*---------- init strings ----------*/
    for (i = 0; DefStrValues[i].id; i++)
    {
        CONST_STRPTR value = cfg_get(cl, obj, DefStrValues[i].id);
        Configdata_StoreField(data, DefStrValues[i].field,
            value ? value : DefStrValues[i].val);
    }

    /*---------- images stuff ----------*/

    init_imspecs(cl, obj, data);

    /*---------- frame stuff ----------*/

    init_framespecs(cl, obj, data);

    /* Walk defaults once, storing each value in its native field width. */
    for (i = 0; DefULValues[i].id; i++)
    {
        const struct def_ulval *def = &DefULValues[i];
        ULONG *stored = cfg_get(cl, obj, def->id);
        ULONG value = stored ? AROS_BE2LONG(*stored) : def->val;
        APTR target = (UBYTE *)data->prefs + (def->offset & ~DEF_UWORD);
        if (def->offset & DEF_UWORD)
            *(UWORD *)target = value;
        else
            *(ULONG *)target = value;
    }
    data->prefs->register_look = REGISTER_LOOK_TRADITIONAL;

    /*---------- CustomFrames ----------*/
    /* consecutive ids and fields */
    for (i = 0; i < 16; i++)
        (&data->prefs->customframe_config_1)[i] =
            cfg_get(cl, obj, MUICFG_CustomFrame_1 + i);

    /*---------- Special ----------*/
    /* all taken care of in frames and images */

    if (data->direct)
    {
        data->direct = FALSE;
        DoMethod(obj, MUIM_Semaphore_Release);
    }

    /* the defaults, from now on for every application without a file */
    if (!data->loaded)
    {
        Forbid();
        if (!cfg_shared)
            cfg_shared = data->prefs;
        Permit();
    }
}

IPTR Configdata__OM_NEW(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct MUI_ConfigdataData *data;
    struct TagItem *tags;
    struct TagItem *tag;

    obj = (Object *) DoSuperMethodA(cl, obj, (Msg) msg);
    if (!obj)
        return (IPTR) NULL;

    data = INST_DATA(cl, obj);


    for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));)
    {
        switch (tag->ti_Tag)
        {
        case MUIA_Configdata_Application:
            data->app = (Object *) tag->ti_Data;
            break;
        case MUIA_Configdata_ApplicationBase:
            data->appbase = (CONST_STRPTR) tag->ti_Data;
            break;
        }
    }

    if (data->app && !data->appbase)
    {
        get(data->app, MUIA_Application_Base, &data->appbase);
    }

    /* No file read: the shared defaults, if built (cfg_shared) */
    if (!cfg_load(cl, obj, data) && (data->prefs = cfg_shared))
        return (IPTR) obj;
    if (data->prefs)    /* the shared global settings', or over them */
        return (IPTR) obj;
    if (!(data->prefs = AllocMem(sizeof(*data->prefs), MEMF_CLEAR)))
    {
        CoerceMethod(cl, obj, OM_DISPOSE);
        return (IPTR) NULL;
    }
    cfg_build(cl, obj, data);
    return (IPTR) obj;
}

/**************************************************************************
 OM_DISPOSE
**************************************************************************/
IPTR Configdata__OM_DISPOSE(struct IClass *cl, Object *obj, Msg msg)
{
    cfg_free(INST_DATA(cl, obj));

    return DoSuperMethodA(cl, obj, msg);
}

/**************************************************************************
 OM_GET
**************************************************************************/
IPTR Configdata__OM_GET(struct IClass *cl, Object *obj,
    struct opGet *msg)
{
    struct MUI_ConfigdataData *data = INST_DATA(cl, obj);
    IPTR *store = msg->opg_Storage;
    ULONG tag = msg->opg_AttrID;

    switch (tag)
    {
    case MUIA_Configdata_ZunePrefs:
        *store = (IPTR) data->prefs;
        DoMethod(obj, MUIM_Configdata_GetWindowPos, 0);
        return 1;
    }

    return DoSuperMethodA(cl, obj, (Msg) msg);
}

#ifndef AROS_BIG_ENDIAN
#define AROS_BIG_ENDIAN 0
#endif

#if (!AROS_BIG_ENDIAN)
static LONG windowpos_swapbytes(IPTR data, LONG size)
{
    LONG items;
    WORD cnt, i, j;
    void *p = (void *)data;

    *((LONG *) p) = AROS_SWAP_BYTES_LONG(*((LONG *) p));

    cnt = sizeof(LONG);
    items = (size - sizeof(LONG)) / sizeof(struct windowpos);
    D(bug("[MUI:Cfg] %s: size = %d items = %d\n", __func__, size, items));
    for (i = 0; i < items; i++) {
        *((LONG *) ((IPTR)p + cnt)) =
            AROS_SWAP_BYTES_LONG(*((LONG *) ((IPTR)p + cnt)));
        cnt += sizeof(LONG);
        for (j = 0; j < 8; j++) {
            *((WORD *) ((IPTR)p + cnt)) =
                AROS_SWAP_BYTES_WORD(*((WORD *) ((IPTR)p + cnt)));
            cnt += sizeof(WORD);
        }
    }
    D(bug("[MUI:Cfg] %s: actual = %d\n", __func__, cnt));
    return cnt;
}
#endif

static IPTR Configdata_GetWindowPos(struct IClass *cl, Object *obj,
    struct MUIP_Configdata_GetString *msg)
{
    struct MUI_ConfigdataData *data = INST_DATA(cl, obj);
    IPTR s;
    s = (IPTR) DoMethod(obj, MUIM_Dataspace_Find, MUICFG_WindowPos);
    if (s && data->app) {
#if (!AROS_BIG_ENDIAN)
        LONG size = AROS_BE2LONG(*((LONG *)s));
        D(bug("[MUI:Cfg] %s: %ld bytes\n", __func__, size));
        windowpos_swapbytes(s, size);
#endif
        set(data->app, MUIA_Application_CopyWinPosToApp, s);
#if (!AROS_BIG_ENDIAN)
        windowpos_swapbytes(s, size);
#endif
    }
    return s;
}

static IPTR Configdata_SetWindowPos(struct IClass *cl, Object *obj,
    struct MUIP_Configdata_GetString *msg)
{
    struct MUI_ConfigdataData *data;
    //kprintf ("setwindowpos\n");
    data = INST_DATA(cl, obj);

    if (data->app)
    {
        IPTR addr = 0;
        IPTR size = 0;

        get(data->app, MUIA_Application_GetWinPosAddr, &addr);
        get(data->app, MUIA_Application_GetWinPosSize, &size);

        D(bug("[MUI:Cfg] %s: 0x%p (%d bytes)\n", __func__, addr, size));

#if (!AROS_BIG_ENDIAN)
        windowpos_swapbytes(addr, size);
#endif
        DoMethod(obj, MUIM_Dataspace_Add, addr, size, MUICFG_WindowPos);
#if (!AROS_BIG_ENDIAN)
        windowpos_swapbytes(addr, size);
#endif
    }
    return 0;
}

/**************************************************************************
 MUIM_Configdata_GetString
 Check if string is found in dataspace, then if not found, search each
 builtin array
**************************************************************************/
static CONST_STRPTR cfg_string(struct IClass *cl, Object *obj, ULONG id)
{
    CONST_STRPTR s = cfg_get(cl, obj, id);
    int i;

    if (s)
        return s;
    for (i = 0; DefStrValues[i].id; i++)
    {
        if (DefStrValues[i].id == id)
            return DefStrValues[i].val;
    }
    for (i = 0; DefImspecValues[i].defspec; i++)
    {
        if (DefImspecValues[i].cfgid == id)
            return DefImspecValues[i].defspec;
    }
    for (i = 0; DefFramespecValues[i].defspec; i++)
    {
        if (DefFramespecValues[i].cfgid == id)
            return DefFramespecValues[i].defspec;
    }
    return NULL;
}

IPTR Configdata__MUIM_GetString(struct IClass *cl, Object *obj,
    struct MUIP_Configdata_GetString *msg)
{
    return (IPTR) cfg_string(cl, obj, msg->id);
}

/**************************************************************************
 MUIM_Configdata_GetULong
**************************************************************************/
static ULONG cfg_ulong(struct IClass *cl, Object *obj, ULONG id)
{
    ULONG *vp = cfg_get(cl, obj, id);
    int i;

    if (vp)
        return AROS_BE2LONG(*vp);
    for (i = 0; DefULValues[i].id != 0; i++)
    {
        if (DefULValues[i].id == id)
            return DefULValues[i].val;
    }
    return 0;
}

IPTR Configdata__MUIM_GetULong(struct IClass *cl, Object *obj,
    struct MUIP_Configdata_GetULong *msg)
{
    return cfg_ulong(cl, obj, msg->id);
}

/**************************************************************************
 MUIM_GetConfigItem
 Builtin numeric settings return values; custom settings retain dataspace
 pointers. Resolve builtin defaults even when no preferences file exists.
**************************************************************************/
IPTR Configdata__MUIM_GetConfigItem(struct IClass *cl, Object *obj,
    struct MUIP_GetConfigItem *msg)
{
    int i;
    IPTR value;

    if (!msg->storage)
        return FALSE;

    for (i = 0; DefULValues[i].id; i++)
    {
        if (DefULValues[i].id == msg->id)
        {
            struct MUIP_Configdata_GetULong getmsg =
                { MUIM_Configdata_GetULong, msg->id };

            *msg->storage = Configdata__MUIM_GetULong(cl, obj, &getmsg);
            return TRUE;
        }
    }

    {
        struct MUIP_Configdata_GetString getmsg =
            { MUIM_Configdata_GetString, msg->id };

        value = Configdata__MUIM_GetString(cl, obj, &getmsg);
    }
    if (!value)
        return FALSE;

    /* Zune keeps the string backgrounds as image specs, MUI returns pen
     * specs for them; Textinput.mcc obtains a pen from the answer. */
    if ((msg->id == MUICFG_String_Background
        || msg->id == MUICFG_String_ActiveBackground)
        && ((STRPTR)value)[0] == '2' && ((STRPTR)value)[1] == ':')
        value += 2;

    *msg->storage = value;
    return TRUE;
}

/**************************************************************************
 SavePrefsHeader: Write a PRHD chunk
**************************************************************************/
static int SavePrefsHeader(struct IFFHandle *iff)
{
    if (!PushChunk(iff, 0, MAKE_ID('P', 'R', 'H', 'D'), IFFSIZE_UNKNOWN))
    {
        struct PrefHeader ph;
        ph.ph_Version = 0;
        ph.ph_Type = 0;
        ph.ph_Flags = 0;

        if (WriteChunkBytes(iff, &ph, sizeof(struct PrefHeader)))
            if (!PopChunk(iff))
                return 1;
        PopChunk(iff);
    }
    return 0;
}

/* Read the MUIC chunk of a preferences file into a Dataspace object (a
 * Configdata or a plain Dataspace). clear: forget what it held first,
 * once the file is open (MUIM_Configdata_Load); else the file's items are
 * laid over the ones it has. FALSE if the file cannot be read. */
static BOOL prefs_read(Object *ds, CONST_STRPTR filename, BOOL clear)
{
    struct IFFHandle *iff;
    BOOL res = FALSE;

    if (!ZUNE_IFFPARSE() || !(iff = AllocIFF()))
        return FALSE;
    D(bug("loading prefs from %s\n", filename));
    if ((iff->iff_Stream = (IPTR) Open(filename, MODE_OLDFILE)))
    {
        InitIFFasDOS(iff);
        if (!OpenIFF(iff, IFFF_READ))
        {
            StopChunk(iff, MAKE_ID('P', 'R', 'E', 'F'), MAKE_ID('M',
                    'U', 'I', 'C'));
            if (clear)
                DoMethod(ds, MUIM_Dataspace_Clear);
            while (!ParseIFF(iff, IFFPARSE_SCAN))
            {
                struct ContextNode *cn;
                if (!(cn = CurrentChunk(iff)))
                    continue;
                if (cn->cn_ID == MAKE_ID('M', 'U', 'I', 'C'))
                    DoMethod(ds, MUIM_Dataspace_ReadIFF, (IPTR) iff);
            }
            CloseIFF(iff);
            res = TRUE;
        }
        Close((BPTR) iff->iff_Stream);
    }
    FreeIFF(iff);
    return res;
}

/* Write a Dataspace object as a preferences file (FORM PREF, PRHD, MUIC),
 * making the file's drawer if it is missing. */
static void prefs_write(Object *ds, CONST_STRPTR filename)
{
    struct IFFHandle *iff;

    if (!ZUNE_IFFPARSE() || !(iff = AllocIFF()))
        return;
    if (!(iff->iff_Stream = (IPTR) Open(filename, MODE_NEWFILE)))
    {
        /* Try to Create the directory where the file is located */
        char *path = StrDup(filename);
        if (path)
        {
            char *path_end = PathPart(path);
            if (path_end != path)
            {
                BPTR lock;
                *path_end = 0;
                if ((lock = CreateDir(path)))
                {
                    UnLock(lock);
                    iff->iff_Stream = (IPTR) Open(filename, MODE_NEWFILE);
                }
            }
            FreeVec(path);
        }
    }
    if (iff->iff_Stream)
    {
        InitIFFasDOS(iff);
        if (!OpenIFF(iff, IFFF_WRITE))
        {
            if (!PushChunk(iff, MAKE_ID('P', 'R', 'E', 'F'), ID_FORM,
                    IFFSIZE_UNKNOWN))
            {
                if (SavePrefsHeader(iff))
                {
                    DoMethod(ds, MUIM_Dataspace_WriteIFF, (IPTR) iff,
                        0, MAKE_ID('M', 'U', 'I', 'C'));
                }
                PopChunk(iff);
            }
            CloseIFF(iff);
        }
        Close((BPTR) iff->iff_Stream);
    }
    FreeIFF(iff);
}

/**************************************************************************
 MUIM_Configdata_Save
**************************************************************************/
IPTR Configdata__MUIM_Save(struct IClass *cl, Object *obj,
    struct MUIP_Configdata_Save *msg)
{
    Configdata_SetWindowPos(cl, obj, (APTR) msg);
    prefs_write(obj, msg->filename);
    return 0;
}

/**************************************************************************
 MUIM_Configdata_SaveWindowPositions
 What an application writes when it quits and remembers its windows: its
 own file as it was, with only the window positions replaced, as MUI does.
 The settings in use (global and its own merged) are not written: they
 would hide later changes of the global settings from this application.
**************************************************************************/
static IPTR Configdata_SaveWindowPositions(struct IClass *cl, Object *obj,
    struct MUIP_Configdata_Save *msg)
{
    struct MUI_ConfigdataData *data = INST_DATA(cl, obj);
    Object *own;
    IPTR addr = 0, size = 0;

    if (!data->app)
        return 0;
    if (!(own = MUI_NewObject(MUIC_Dataspace, TAG_DONE)))
        return 0;
    prefs_read(own, msg->filename, FALSE);
    get(data->app, MUIA_Application_GetWinPosAddr, &addr);
    get(data->app, MUIA_Application_GetWinPosSize, &size);
    if (addr && size)
    {
#if (!AROS_BIG_ENDIAN)
        windowpos_swapbytes(addr, size);
#endif
        DoMethod(own, MUIM_Dataspace_Add, addr, size, MUICFG_WindowPos);
#if (!AROS_BIG_ENDIAN)
        windowpos_swapbytes(addr, size);
#endif
    }
    else
        DoMethod(own, MUIM_Dataspace_Remove, MUICFG_WindowPos);
    prefs_write(own, msg->filename);
    MUI_DisposeObject(own);
    return 0;
}


/**************************************************************************
 MUIM_Configdata_Load
 Get the content of the file into the object.
**************************************************************************/
IPTR Configdata__MUIM_Load(struct IClass *cl, Object *obj,
    struct MUIP_Configdata_Load *msg)
{
    return prefs_read(obj, msg->filename, TRUE);
}

/*
 * The class dispatcher
 */
/**************************************************************************
 MUIM_Configdata_SetString: stays in the library, unlike the other setters:
 MUIM_Application_SetConfigItem (MUI 3.8 has it) calls it, and the suite's
 LoadSeg trace showed AmTelnet, a MUI 3 program, doing that at start.
 A value equal to its default is removed from the dataspace.
**************************************************************************/
static __attribute__((noinline)) IPTR Configdata_SetString(struct IClass *cl,
    Object *obj, struct MUIP_Configdata_SetString *msg)
{
    const struct def_strval *t;

    for (t = DefStrValues; t->id; t++)
        if (t->id == msg->id && !strcmp(t->val, msg->string))
            break;
    if (t->id)
        DoMethod(obj, MUIM_Dataspace_Remove, msg->id);
    else
        DoMethod(obj, MUIM_Dataspace_Add, (IPTR)msg->string,
            strlen(msg->string) + 1, msg->id);
    Configdata_SynchronizeString(cl, obj, msg->id);
    return 0;
}

/* The Dataspace methods that change, clear or write the object's
 * records. With shared global settings their records become the
 * object's own first (where it has none of that id): from then on it is
 * as if it had read the file itself. The settings in use still point
 * into the shared records, held until OM_DISPOSE. While the shared
 * settings are being read (cfg_attach) ReadIFF is theirs: the file's
 * stamp from its open handle, the records to their Dataspace. */
static __attribute__((noinline)) IPTR cfg_records(struct IClass *cl,
    Object *obj, Msg msg)
{
    struct MUI_ConfigdataData *data = INST_DATA(cl, obj);

    if (data->glob)
    {
        if (data->unshared == CFG_LOADING)
        {
            cfg_stamp(data->glob->stamp,
                ((struct MUIP_Dataspace_ReadIFF *)msg)->handle);
            return DoMethodA(data->glob->ds, msg);
        }
        if (!data->unshared)
        {
            data->unshared = TRUE;
            zune_dataspace_underlay(cl->cl_Super, obj, data->glob->ds);
        }
    }
    return DoSuperMethodA(cl, obj, msg);
}

BOOPSI_DISPATCHER(IPTR, Configdata_Dispatcher, cl, obj, msg)
{
    switch (msg->MethodID)
    {
    case MUIM_Dataspace_Find:
        return (IPTR)cfg_get(cl, obj,
            ((struct MUIP_Dataspace_Find *)msg)->id);
    case MUIM_Dataspace_Add:
    case MUIM_Dataspace_Remove:
    case MUIM_Dataspace_Clear:
    case MUIM_Dataspace_ReadIFF:
    case MUIM_Dataspace_WriteIFF:
        return cfg_records(cl, obj, msg);
    case OM_NEW:
        return Configdata__OM_NEW(cl, obj, (struct opSet *)msg);
    case OM_DISPOSE:
        return Configdata__OM_DISPOSE(cl, obj, (APTR) msg);
    case OM_GET:
        return Configdata__OM_GET(cl, obj, (APTR) msg);
    case MUIM_GetConfigItem:
        return Configdata__MUIM_GetConfigItem(cl, obj, (APTR) msg);
    case MUIM_Configdata_GetString:
        return Configdata__MUIM_GetString(cl, obj, (APTR) msg);
    case MUIM_Configdata_GetULong:
        return Configdata__MUIM_GetULong(cl, obj, (APTR) msg);
    /* the other setters: only the prefs program uses them, in the
     * module */
    case MUIM_Configdata_SetULong:
    case MUIM_Configdata_SetImspec:
    case MUIM_Configdata_SetFramespec:
    case MUIM_Configdata_SetPenspec:
    case MUIM_Configdata_SetFont:
        return zune_configdata_set(obj, msg, &cfg_tables);
    case MUIM_Configdata_SetString:
        return Configdata_SetString(cl, obj, (APTR)msg);
    case MUIM_Configdata_SaveWindowPositions:
        return Configdata_SaveWindowPositions(cl, obj, (APTR) msg);
    case MUIM_Configdata_Save:
        return Configdata__MUIM_Save(cl, obj, (APTR) msg);
    case MUIM_Configdata_Load:
        return Configdata__MUIM_Load(cl, obj, (APTR) msg);
    case MUIM_Configdata_SetWindowPos:
        return Configdata_SetWindowPos(cl, obj, (APTR) msg);
    case MUIM_Configdata_GetWindowPos:
        return Configdata_GetWindowPos(cl, obj, (APTR) msg);
    }

    return DoSuperMethodA(cl, obj, msg);
}
BOOPSI_DISPATCHER_END

/*
 * Class descriptor.
 */
const struct __MUIBuiltinClass _MUI_Configdata_desc =
{
    MUIC_Configdata,            /* Class name */
    MUIC_Dataspace,             /* super class name */
    sizeof(struct MUI_ConfigdataData),  /* size of class own datas */
    (void *) Configdata_Dispatcher       /* class dispatcher */
};
