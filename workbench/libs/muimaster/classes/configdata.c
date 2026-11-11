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

#include <proto/muiscreen.h>

#define PUBSCREEN_FILE "env:Zune/PublicScreens.iff"

#define DFSN(x)
#define DPSD(x)

extern struct Library *MUIMasterBase;

struct MUI_ConfigdataData
{
    Object                      *app;
    CONST_STRPTR                appbase;
    struct ZunePrefsNew         prefs;
    struct SignalSemaphore      psLock;
    struct MsgPort             *fsNotifyPort;
    struct DateStamp            created;   /* public screens read before this */
    struct MsgPort             *signalPort;
    struct MUI_InputHandlerNode fsNotifyIHN;
    struct NotifyRequest        fsNotifyRequest;
    struct List                 pubscreens;
    struct IClass             *init_dataspace;
};

IPTR Dataspace__MUIM_Find(struct IClass *cl, Object *obj,
    struct MUIP_Dataspace_Find *msg);

/* A Configdata is built for every application and reads about 80
 * settings at creation; ask the dataspace, our superclass, for each
 * without passing through our own dispatcher first, and take the
 * defaults from the tables being walked (perfbench create+dispose) */
static APTR cfg_find(struct IClass *cl, Object *obj, ULONG id)
{
    struct MUIP_Dataspace_Find msg = { MUIM_Dataspace_Find, id };

    struct MUI_ConfigdataData *data = INST_DATA(cl, obj);

    /* The constructor holds the dataspace lock across all initial reads. */
    if (data->init_dataspace)
        return (APTR)Dataspace__MUIM_Find(data->init_dataspace, obj, &msg);
    return (APTR) DoSuperMethodA(cl, obj, (Msg) &msg);
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

        imspec = cfg_find(cl, obj, img->cfgid);
        if (!imspec)
            imspec = img->defspec;
/*          D(bug("init_imspecs: %ld %lx %s ...\n", img->muiv, img->cfgid, imspec)); */
        data->prefs.imagespecs[img->muiv] = imspec;
        if (!data->prefs.imagespecs[img->muiv])
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

        framespec = cfg_find(cl, obj, fcfg->cfgid);
        if (!framespec)
            framespec = fcfg->defspec;
        zune_frame_spec_to_intern(framespec,
            &data->prefs.frames[fcfg->muiv]);
    }
}

/**************************************************************************
 Default ULONG values
**************************************************************************/


#define PREF_DEFAULT(id, value, field) \
    {id, value, offsetof(struct ZunePrefsNew, field), \
     sizeof(((struct ZunePrefsNew *)0)->field)}

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
    {0, 0, 0, 0},
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
 * per binding, and every Configdata parses 24 of them. The default
 * strings are constants shared by all applications, so their parsed
 * expressions are kept here; saved bindings are parsed per object. */
#define KEYCACHE 32
static struct { CONST_STRPTR str; IX ix; LONG well; } keycache[KEYCACHE];
static int keycache_count;

static BOOL is_default_string(CONST_STRPTR val)
{
    int i;

    for (i = 0; DefStrValues[i].id; i++)
        if (DefStrValues[i].val == val)
            return TRUE;
    return FALSE;
}

static void Configdata__SetKey(ZuneKeySpec *key, CONST_STRPTR val)
{
    int i, count;

    if ((key->readable_hotkey = val) == NULL)
    {
        key->ix_well = 0;
        return;
    }
    count = keycache_count;
    for (i = 0; i < count; i++)
        if (keycache[i].str == val)
        {
            key->ix = keycache[i].ix;
            key->ix_well = keycache[i].well;
            return;
        }
    key->ix_well = ZUNE_CX() && !ParseIX(key->readable_hotkey, &key->ix);
    if (is_default_string(val))
    {
        Forbid();
        if (keycache_count < KEYCACHE)
        {
            keycache[keycache_count].str = val;
            keycache[keycache_count].ix = key->ix;
            keycache[keycache_count].well = key->ix_well;
            keycache_count++;
        }
        Permit();
    }
}

/* Assign a !persistent! string stored in the dataspace (or a default) to
 * its ZunePrefsNew field */
static void Configdata_StoreField(struct MUI_ConfigdataData *data,
    UWORD field, CONST_STRPTR val)
{
    APTR at = (UBYTE *)&data->prefs + (field & ~KEYFIELD);

    if (field == NOFIELD)
        return;
    if (field & KEYFIELD)
        Configdata__SetKey(at, val);
    else
        *(CONST_STRPTR *)at = val;
}

static void Configdata_StoreString(struct MUI_ConfigdataData *data,
    ULONG id, CONST_STRPTR val)
{
    int i;

    /* the 16 custom frames: consecutive ids and fields */
    if (id >= MUICFG_CustomFrame_1 && id <= MUICFG_CustomFrame_16)
    {
        (&data->prefs.customframe_config_1)[id - MUICFG_CustomFrame_1] = val;
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
/**************************************************************************
 MUIM_Configdata_SetupNotify: watch the public screen file. Called by the
 application when it first waits for input.
**************************************************************************/
static IPTR Configdata_LoadPubScreens(struct IClass *cl, Object *obj,
    Msg msg);

static IPTR Configdata__MUIM_SetupNotify(struct IClass *cl, Object *obj,
    Msg msg)
{
    struct MUI_ConfigdataData *data = INST_DATA(cl, obj);

    if (data->fsNotifyPort || !data->app)
        return 0;
    /* Shares the application's signal when it gave us one */
    data->fsNotifyPort = ZuneCreateSharedPort(data->signalPort);
    if (data->fsNotifyPort) {
        /* Setup filesystem notification handler ---------------------------*/
        data->fsNotifyIHN.ihn_Signals = 1UL << data->fsNotifyPort->mp_SigBit;
        data->fsNotifyIHN.ihn_Object  = obj;
        data->fsNotifyIHN.ihn_Method  = MUIM_Configdata_PubScreensChanged;

        DoMethod(data->app, MUIM_Application_AddInputHandler, (IPTR)&data->fsNotifyIHN);

        data->fsNotifyRequest.nr_Name                 = PUBSCREEN_FILE;
        data->fsNotifyRequest.nr_Flags                = NRF_SEND_MESSAGE;
        data->fsNotifyRequest.nr_stuff.nr_Msg.nr_Port = data->fsNotifyPort;
        if (StartNotify(&data->fsNotifyRequest)) {
            DFSN(bug("[MUI:Cfg] %s: FileSystem-Notification setup for '%s'\n", __func__, data->fsNotifyRequest.nr_Name));
        } else {
            DFSN(bug("[MUI:Cfg] %s: FAILED to setup FileSystem-Notification for '%s'\n", __func__, data->fsNotifyRequest.nr_Name));
            DoMethod(data->app, MUIM_Application_RemInputHandler, (IPTR)&data->fsNotifyIHN);
            if (data->signalPort)
                ZuneDeleteSharedPort(data->fsNotifyPort);
            else
                DeleteMsgPort(data->fsNotifyPort);
            data->fsNotifyPort = NULL;
        }
    }

    /* Catch a public screen file written before the notification */
    if (data->fsNotifyPort
        && zune_file_written_since(PUBSCREEN_FILE, &data->created))
        Configdata_LoadPubScreens(cl, obj, msg);
    return data->fsNotifyPort != NULL;
}

static BOOL prefs_read(Object *ds, CONST_STRPTR filename, BOOL clear);
static void prefs_write(Object *ds, CONST_STRPTR filename);

IPTR Configdata__OM_NEW(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct MUI_ConfigdataData *data;
    struct TagItem *tags;
    struct TagItem *tag;
    //APTR cdata;
    int i, res = 0;

    obj = (Object *) DoSuperMethodA(cl, obj, (Msg) msg);
    if (!obj)
        return (IPTR) NULL;

/*      D(bug("Configdata_New(%p)\n", obj)); */

    data = INST_DATA(cl, obj);

    NEWLIST(&data->pubscreens);
    InitSemaphore(&data->psLock);
    DateStamp(&data->created);

    for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));)
    {
        switch (tag->ti_Tag)
        {
        case MUIA_Configdata_SignalPort:
            data->signalPort = (struct MsgPort *) tag->ti_Data;
            break;

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

    /* Without a preference drawer there is nothing to load: one Lock
     * replaces up to four failing Open calls (each a handler round
     * trip) per application. ENV: is restored from ENVARC: at boot and
     * the preferences program creates both drawers when it first saves. */
    {
        BPTR drawer = Lock("ENV:zune", SHARED_LOCK);

        if (drawer)
            UnLock(drawer);
        else
            res = -1;
    }

    /* As MUI does: the global settings first, then the application's
     * own file on top of them. That file may hold only the window
     * positions remembered when the application quit; it must not hide
     * the global settings (it did: an application that once remembered
     * a window kept the settings of that moment for ever). */
    if (res == 0)
    {
        BOOL loaded = prefs_read(obj, "ENV:zune/global.prefs", FALSE)
            || prefs_read(obj, "ENVARC:zune/global.prefs", FALSE);

        if (data->appbase && Stricmp(data->appbase, "global"))
        {
            char filename[255];

            snprintf(filename, 255, "ENV:zune/%s.prefs", data->appbase);
            if (prefs_read(obj, filename, FALSE))
                loaded = TRUE;
            else
            {
                snprintf(filename, 255, "ENVARC:zune/%s.prefs",
                    data->appbase);
                if (prefs_read(obj, filename, FALSE))
                    loaded = TRUE;
            }
        }
        if (loaded)
            DoMethod(obj, MUIM_Configdata_LoadPubScreens);
    }

    /* The public screen file notification starts when the application
     * first waits for input (MUIM_Configdata_SetupNotify): a DOS
     * notification costs a handler round trip, which create/dispose
     * cycles and non-interactive uses never need. */

    /* Only bypass dispatch for the builtin object during construction.
     * Subclasses retain their inherited lookup and semaphore methods. */
    if (OCLASS(obj) == cl && cl->cl_Super->cl_ID &&
        !strcmp(cl->cl_Super->cl_ID, MUIC_Dataspace))
    {
        DoMethod(obj, MUIM_Semaphore_ObtainShared);
        data->init_dataspace = cl->cl_Super;
    }

    /*---------- init strings ----------*/
    for (i = 0; DefStrValues[i].id; i++)
    {
        CONST_STRPTR value = cfg_find(cl, obj, DefStrValues[i].id);
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
        ULONG *stored = cfg_find(cl, obj, def->id);
        ULONG value = stored ? AROS_BE2LONG(*stored) : def->val;
        APTR target = (UBYTE *)&data->prefs + def->offset;
        if (def->size == sizeof(UWORD))
            *(UWORD *)target = value;
        else
            *(ULONG *)target = value;
    }
    data->prefs.register_look = REGISTER_LOOK_TRADITIONAL;

    /*---------- CustomFrames ----------*/
    /* consecutive ids and fields */
    for (i = 0; i < 16; i++)
        (&data->prefs.customframe_config_1)[i] =
            cfg_find(cl, obj, MUICFG_CustomFrame_1 + i);

    /*---------- Special ----------*/
    /* all taken care of in frames and images */

    if (data->init_dataspace)
    {
        data->init_dataspace = NULL;
        DoMethod(obj, MUIM_Semaphore_Release);
    }

    return (IPTR) obj;
}

static void Configdata__DisposeDescriptors(struct MUI_ConfigdataData *data)
{
    struct Node *psNode, *tmp;

    DPSD(bug("[MUI:Cfg] %s()\n", __func__);)

    ObtainSemaphore(&data->psLock);
    ForeachNodeSafe(&data->pubscreens, psNode, tmp) {
        Remove(psNode);
        MUIS_FreePubScreenDesc((struct MUI_PubScreenDesc *)psNode->ln_Name);
        FreeVec(psNode);
    }
    ReleaseSemaphore(&data->psLock);
}

/**************************************************************************
 OM_DISPOSE
**************************************************************************/
IPTR Configdata__OM_DISPOSE(struct IClass *cl, Object *obj, Msg msg)
{
    struct MUI_ConfigdataData *data = INST_DATA(cl, obj);

    if (data->fsNotifyPort) {
        DoMethod(data->app, MUIM_Application_RemInputHandler, (IPTR) &data->fsNotifyIHN);
        EndNotify(&data->fsNotifyRequest);
        if (data->signalPort)
            ZuneDeleteSharedPort(data->fsNotifyPort);
        else
            DeleteMsgPort(data->fsNotifyPort);
    }

    if (MUIScreenBase)
        Configdata__DisposeDescriptors(data);

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
        *store = (IPTR) & data->prefs;
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
    CONST_STRPTR s = cfg_find(cl, obj, id);
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
    ULONG *vp = cfg_find(cl, obj, id);
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
    IPTR res = prefs_read(obj, msg->filename, TRUE);

    if (res) {
        DoMethod(obj, MUIM_Configdata_LoadPubScreens);
    }

    return res;
}

static IPTR Configdata_GetPubScrnDesc(struct IClass *cl, Object *obj,
    struct MUIP_Configdata_GetPubScrnDesc *msg)
{
    struct MUI_ConfigdataData *data = INST_DATA(cl, obj);
    struct MUI_PubScreenDesc *desc = NULL;

    DPSD(bug("[MUI:Cfg] %s()\n", __func__);)

    if (MUIScreenBase) {
        struct Node *psNode;
        ObtainSemaphoreShared(&data->psLock);
        ForeachNode(&data->pubscreens, psNode) {
            if (!strcmp(((struct MUI_PubScreenDesc *)(psNode->ln_Name))->Name, msg->name)) {
                desc = (struct MUI_PubScreenDesc *)psNode->ln_Name;
                break;
            }
        }
        ReleaseSemaphore(&data->psLock);
    }
    return (IPTR)desc;
}

static IPTR Configdata_LoadPubScreens(struct IClass *cl, Object *obj,
    Msg msg)
{
    DPSD(bug("[MUI:Cfg] %s()\n", __func__);)

    if (MUIScreenBase) {
        struct MUI_ConfigdataData *data = INST_DATA(cl, obj);
        struct Node *psNode;
        APTR pfh;

        Configdata__DisposeDescriptors(data);

        DPSD(bug("[MUI:Cfg] %s: loading public screen details from '%s'\n", __func__, PUBSCREEN_FILE);)

        ObtainSemaphore(&data->psLock);
        if ((pfh = MUIS_OpenPubFile(PUBSCREEN_FILE, MODE_OLDFILE))) {
            struct MUI_PubScreenDesc *desc;
            while ((desc = MUIS_ReadPubFile(pfh))) {
                DPSD(bug("[MUI:Cfg] %s: descriptor @ 0x%p\n", __func__, desc);)
                psNode = AllocVec(sizeof(struct Node), MEMF_CLEAR);
                psNode->ln_Name = (char *)desc;
                AddTail(&data->pubscreens, psNode);
            }
            MUIS_ClosePubFile(pfh);
        }
        ReleaseSemaphore(&data->psLock);
    }
    return (IPTR)TRUE;
}

/* The notification port may share its signal with other ports: reload
 * only when a notification arrived, and reply to every message. */
static IPTR Configdata_PubScreensChanged(struct IClass *cl, Object *obj,
    Msg msg)
{
    struct MUI_ConfigdataData *data = INST_DATA(cl, obj);
    struct Message *nmsg;
    BOOL changed = FALSE;

    while ((nmsg = GetMsg(data->fsNotifyPort)))
    {
        ReplyMsg(nmsg);
        changed = TRUE;
    }
    return changed ? Configdata_LoadPubScreens(cl, obj, msg) : TRUE;
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

BOOPSI_DISPATCHER(IPTR, Configdata_Dispatcher, cl, obj, msg)
{
    switch (msg->MethodID)
    {
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
    case MUIM_Configdata_GetPubScrnDesc:
        return Configdata_GetPubScrnDesc(cl, obj, (APTR) msg);
    case MUIM_Configdata_LoadPubScreens:
        return Configdata_LoadPubScreens(cl, obj, (APTR) msg);
    case MUIM_Configdata_PubScreensChanged:
        return Configdata_PubScreensChanged(cl, obj, (APTR) msg);
    case MUIM_Configdata_SetupNotify:
        return Configdata__MUIM_SetupNotify(cl, obj, (APTR) msg);
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
