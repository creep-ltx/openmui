/*
    Copyright  2009, The AROS Development Team. All rights reserved.
*/

#include <graphics/gfx.h>
#include <graphics/view.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <clib/alib_protos.h>
#include <libraries/asl.h>
#include <libraries/mui.h>
#include <libraries/muiscreen.h>
#include <mui/Rawimage_mcc.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/utility.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <dos/var.h>
#include <proto/dos.h>
#include <string.h>
#include <stdio.h>

#ifndef SYS_Asynch
#define SYS_Asynch   (TAG_USER + 4)
#endif
#ifndef SYS_Input
#define SYS_Input    (TAG_USER + 1)
#endif
#ifndef SYS_Output
#define SYS_Output   (TAG_USER + 2)
#endif
#ifndef NP_StackSize
#define NP_StackSize (TAG_USER + 50)
#endif

#ifdef __AROS__
#include <proto/alib.h>
#include <proto/muiscreen.h>
#else
#include <proto/muiscreen.h>
#endif

#include "zunestuff.h"

extern struct Library *MUIMasterBase;
extern struct Library *MUIScreenBase;

/* Utility class for the bubble help time slider */

struct BubbleSlider_DATA
{
    char buf[20];
};

static struct MUI_CustomClass *BubbleSlider_CLASS;

IPTR BubbleSlider__MUIM_Numeric_Stringify(struct IClass *cl, Object * obj, struct MUIP_Numeric_Stringify *msg)
{
    struct BubbleSlider_DATA *data = INST_DATA(cl, obj);

    if(msg->value != 0)
        sprintf(data->buf, "%ld.%ld s",
            (long)(msg->value / 10), (long)(msg->value % 10));
    else
        strcpy(data->buf, "off");

    data->buf[sizeof(data->buf) - 1] = 0;
    
    return (IPTR)data->buf;
}

BOOPSI_DISPATCHER(IPTR, BubbleSlider_Dispatcher, cl, obj, msg)
{
    switch (msg->MethodID)
    {
        case MUIM_Numeric_Stringify:
            return BubbleSlider__MUIM_Numeric_Stringify(cl, obj,
                (struct MUIP_Numeric_Stringify *)msg);
        default:
            return DoSuperMethodA(cl, obj, msg);
    }
}
BOOPSI_DISPATCHER_END

static int ensure_BubbleSlider_CLASS(void)
{
    if (BubbleSlider_CLASS == NULL)
    {
        BubbleSlider_CLASS = MUI_CreateCustomClass(NULL, MUIC_Slider, NULL,
            sizeof(struct BubbleSlider_DATA), BubbleSlider_Dispatcher);
    }
    return BubbleSlider_CLASS != NULL;
}

/* Utility class for choosing public screen */

#define PSD_NAME_DEFAULT "Default"

struct PopPublicScreen_DATA
{
    struct Hook strobj_hook;
    struct Hook objstr_hook;
    Object      *list;
};

static struct MUI_CustomClass *PopPublicScreen_CLASS;

LONG PopPublicScreenStrObjFunc(struct Hook *hook, Object *popup, Object *str)
{
    struct PopPublicScreen_DATA   *data = (struct PopPublicScreen_DATA *)hook->h_Data;
    struct List             *pubscrlist;
    struct PubScreenNode    *pubscrnode;
    STRPTR                   strtext = NULL, listentry;
    LONG                     index;
    struct MUI_PubScreenDesc *desc;
    APTR pfh;
    
    set(data->list,MUIA_List_Quiet,TRUE);

    DoMethod(data->list, MUIM_List_Clear);
    DoMethod(data->list, MUIM_List_InsertSingle, PSD_NAME_DEFAULT, MUIV_List_Insert_Bottom);
    DoMethod(data->list, MUIM_List_InsertSingle, PSD_NAME_FRONTMOST, MUIV_List_Insert_Bottom);

    if (MUIScreenBase != NULL &&
        (pfh = MUIS_OpenPubFile(PSD_FILENAME_USE, MODE_OLDFILE)))
    {
        while ( (desc = MUIS_ReadPubFile(pfh)) )
        {
            DoMethod(data->list, MUIM_List_InsertSingle, desc->Name, MUIV_List_Insert_Bottom);
        }
        MUIS_ClosePubFile(pfh);
    }

    pubscrlist = LockPubScreenList();
    ForeachNode(pubscrlist, pubscrnode)
    {
        DoMethod(data->list, MUIM_List_InsertSingle, (IPTR)pubscrnode->psn_Node.ln_Name, MUIV_List_Insert_Bottom);
    }
    UnlockPubScreenList();

    set(data->list,MUIA_List_Quiet,FALSE);

    get(str, MUIA_String_Contents, &strtext);
    
    for(index = 0; ; index++)
    {
        DoMethod(data->list, MUIM_List_GetEntry, index, (IPTR)&listentry);
        
        if (!listentry)
        {
            set(data->list, MUIA_List_Active, strtext[0] ? MUIV_List_Active_Off : 0);
            break;
        }
        
        if (stricmp(strtext, listentry) == 0)
        {
            set(data->list, MUIA_List_Active, index);
            break;
        }
    }
    
    return TRUE;
}

void PopPublicScreenObjStrFunc(struct Hook *hook, Object *popup, Object *str)
{
    STRPTR listentry;
    
    DoMethod(popup, MUIM_List_GetEntry, MUIV_List_GetEntry_Active, (IPTR)&listentry);

    if (listentry)
    {
        if (strcmp(listentry, PSD_NAME_DEFAULT) == 0)
            set(str, MUIA_String_Contents, "");
        else
            set(str, MUIA_String_Contents, listentry);
    }
}

IPTR PopPublicScreen__OM_NEW(struct IClass *cl, Object *obj, struct opSet *msg)
{
    Object *lv, *list;
    struct PopPublicScreen_DATA *data;
    
    obj = (Object *)DoSuperNewTags
    (
        cl, obj, NULL,
        MUIA_Popobject_Object, (IPTR)(lv = (Object *)ListviewObject,
            MUIA_Listview_List, (IPTR)(list = (Object *)ListObject,
                InputListFrame,
                End),
            End),
        TAG_MORE, (IPTR) msg->ops_AttrList
    );
    
    if (obj)
    {
        data = INST_DATA(cl, obj);
        
        data->list = list;
        
        data->strobj_hook.h_Entry = HookEntry;
        data->strobj_hook.h_SubEntry = (HOOKFUNC)PopPublicScreenStrObjFunc;
        data->strobj_hook.h_Data = data;
        
        data->objstr_hook.h_Entry = HookEntry;
        data->objstr_hook.h_SubEntry = (HOOKFUNC)PopPublicScreenObjStrFunc;
        data->objstr_hook.h_Data = data;
        
        SetAttrs(obj, MUIA_Popobject_StrObjHook, (IPTR)&data->strobj_hook,
                      MUIA_Popobject_ObjStrHook, (IPTR)&data->objstr_hook,
                      TAG_DONE);
                     
                
        DoMethod(lv, MUIM_Notify, MUIA_Listview_DoubleClick, TRUE,
                 (IPTR)obj, 2, MUIM_Popstring_Close, TRUE);
    }
    
    return (IPTR)obj;
}

BOOPSI_DISPATCHER(IPTR, PopPublicScreen_Dispatcher, cl, obj, msg)
{
    switch (msg->MethodID)
    {
        case OM_NEW:
            return PopPublicScreen__OM_NEW(cl, obj, (struct opSet *)msg);
        default:
            return DoSuperMethodA(cl, obj, msg);
    }
}
BOOPSI_DISPATCHER_END

/* At the end: every run left these two classes behind, and as
 * subclasses they kept the library's Slider and Popobject classes in use,
 * so the library could never leave memory. */
void delete_system_classes(void)
{
    if (BubbleSlider_CLASS)
        MUI_DeleteCustomClass(BubbleSlider_CLASS);
    BubbleSlider_CLASS = NULL;
    if (PopPublicScreen_CLASS)
        MUI_DeleteCustomClass(PopPublicScreen_CLASS);
    PopPublicScreen_CLASS = NULL;
}

static int ensure_PopPublicScreen_CLASS(void)
{
    if (PopPublicScreen_CLASS == NULL)
    {
        PopPublicScreen_CLASS = MUI_CreateCustomClass(NULL, MUIC_Popobject, NULL,
            sizeof(struct PopPublicScreen_DATA), PopPublicScreen_Dispatcher);
    }
    return PopPublicScreen_CLASS != NULL;
}

/* Preferences System tab class */

struct MUI_SystemPData
{
    Object *screen_name_string;
    Object *call_psi_button;
    Object *pop_to_front_checkmark;
    Object *hotkey_string;
    Object *show_icon_checkmark;
    Object *show_menu_checkmark;
    Object *on_startup_checkmark;
    Object *arexx_checkmark;
    Object *identity_cycle;
    Object *program_list;
    Object *program_string;
    Object *program_cycle;
    Object *program_add;
    Object *program_remove;
    Object *first_bubble_slider;
    Object *next_bubble_slider;
    struct Hook psiHook;
    struct Hook programHook;
};

static IPTR ExecuteScreenInspectorFunc(struct Hook *hook, Object *caller, void *data)
{
    struct TagItem tags[5];

    (void)hook;
    (void)caller;
    (void)data;

    tags[0].ti_Tag = SYS_Asynch;
    tags[0].ti_Data = TRUE;
    tags[1].ti_Tag = SYS_Input;
    tags[1].ti_Data = 0;
    tags[2].ti_Tag = SYS_Output;
    tags[2].ti_Data = 0;
    tags[3].ti_Tag = NP_StackSize;
    tags[3].ti_Data = AROS_STACKSIZE;
    tags[4].ti_Tag = TAG_DONE;
    tags[4].ti_Data = 0;
    
    if (SystemTagList("sys:prefs/psi", tags) == -1)
    {
        return (IPTR) FALSE;
    }
    
    return (IPTR) TRUE;
}

/* Which MUI programs see and OpenMUI behaves as (ENV:Zune/BehaveAs, read
 * by the library when it is loaded and when these settings are saved or
 * used; muimaster identity.c). Automatic: the MUI each program asks
 * for. The per-program lines win over the setting for all. A line of
 * the list is the line of the variable: "MUI5 YAM". */
static CONST_STRPTR identity_labels[] = {
    NULL, "MUI 3.8", "MUI 4", "MUI 5", NULL };  /* [0]: the catalog's */
static CONST_STRPTR program_labels[] = { "MUI 3.8", "MUI 4", "MUI 5", NULL };
static CONST_STRPTR const identity_words[] = { "Auto", "MUI3.8", "MUI4", "MUI5" };

/* "MUI3.8", "MUI4", "MUI5" (any case, "MUI3" too) at s: 1-3, else 0 */
static int identity_word(CONST_STRPTR s, CONST_STRPTR *end)
{
    int id = 0;

    if ((s[0] | 0x20) != 'm' || (s[1] | 0x20) != 'u' || (s[2] | 0x20) != 'i')
        return 0;
    s += 3;
    if (s[0] == '3') { id = 1; s++; if (s[0] == '.' && s[1] == '8') s += 2; }
    else if (s[0] == '4') { id = 2; s++; }
    else if (s[0] == '5') { id = 3; s++; }
    if (!id || (*s != ' ' && *s != '\t' && *s))
        return 0;
    while (*s == ' ' || *s == '\t') s++;
    if (end) *end = s;
    return id;
}

/* The name part of a list line */
static CONST_STRPTR line_name(CONST_STRPTR line)
{
    CONST_STRPTR name = line;

    return identity_word(line, &name) ? name : line;
}

/* Buttons and the list of the per-program lines: what = 0 a line was
 * chosen (it goes into the gadgets), 1 Add (or change the line of that
 * name), 2 Remove */
static IPTR ProgramFunc(struct Hook *hook, Object *caller, IPTR *params)
{
    struct MUI_SystemPData *data = hook->h_Data;
    LONG active = XGET(data->program_list, MUIA_List_Active);
    STRPTR line = NULL, name;
    char buf[300];
    LONG i;

    (void)caller;
    switch (params[0])
    {
    case 0:
        if (active < 0)
            break;
        DoMethod(data->program_list, MUIM_List_GetEntry, active, (IPTR)&line);
        if (!line)
            break;
        set(data->program_string, MUIA_String_Contents, (IPTR)line_name(line));
        set(data->program_cycle, MUIA_Cycle_Active, identity_word(line, NULL) - 1);
        break;
    case 1:
        name = (STRPTR)XGET(data->program_string, MUIA_String_Contents);
        while (name && (*name == ' ' || *name == '\t')) name++;
        if (!name || !*name)
            break;
        snprintf(buf, sizeof(buf), "%s %s",
            identity_words[XGET(data->program_cycle, MUIA_Cycle_Active) + 1], name);
        for (i = 0; ; i++)
        {
            line = NULL;
            DoMethod(data->program_list, MUIM_List_GetEntry, i, (IPTR)&line);
            if (!line)
                break;
            if (!Stricmp((STRPTR)line_name(line), name))
            {
                DoMethod(data->program_list, MUIM_List_Remove, i);
                break;
            }
        }
        DoMethod(data->program_list, MUIM_List_InsertSingle, (IPTR)buf,
            line ? i : MUIV_List_Insert_Bottom);
        set(data->program_list, MUIA_List_Active, line ? i
            : MUIV_List_Active_Bottom);
        break;
    case 2:
        if (active >= 0)
            DoMethod(data->program_list, MUIM_List_Remove, MUIV_List_Remove_Active);
        break;
    }
    return 0;
}

static IPTR SystemP_New(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct MUI_SystemPData *data;
    struct MUI_SystemPData d;

    if (!ensure_BubbleSlider_CLASS() || !ensure_PopPublicScreen_CLASS())
        return (IPTR)NULL;

    identity_labels[0] = _(MSG_BEHAVE_AUTOMATIC);

    obj = (Object *) DoSuperNewTags
    (
        cl, obj, NULL,
        
        /* Two columns of their own (not a 2-column grid, whose rows
         * took the height of the larger group): the page fits a 640x256
         * screen at the default fonts with the page list beside it */
        MUIA_Group_Horiz, TRUE,

        Child, (IPTR) VGroup,
            Child, (IPTR) VGroup,
                GroupFrameT(_(MSG_PUBLIC_SCREEN)),
                MUIA_Group_VertSpacing, 0,
                Child, (IPTR) VSpace(0),
                /* rows of their own, not one label column: "Pop to front:"
                 * would widen the name's label column by 60 pixels */
                Child, (IPTR) HGroup,
                    Child, (IPTR) Label1(_(MSG_NAME)),
                    Child, NewObject(PopPublicScreen_CLASS->mcc_Class, NULL,
                        MUIA_Popstring_String, (IPTR) (d.screen_name_string = StringObject, MUIA_Frame, MUIV_Frame_String, End),
                        MUIA_Popstring_Button, PopButton(MUII_PopUp),
                        TAG_END),
                    End,
                Child, (IPTR) (d.call_psi_button = SimpleButton(_(MSG_CALL_INSPECTOR))),
                Child, (IPTR) HGroup,
                    Child, (IPTR) Label1(_(MSG_POP_TO_FRONT)),
                    Child, (IPTR) (d.pop_to_front_checkmark = MakeCheck(NULL)),
                    Child, RectangleObject, MUIA_Weight, 1, End,
                    End,
                Child, (IPTR) VSpace(0),
                End,

            Child, (IPTR) VGroup,
                GroupFrameT(_(MSG_INTERFACES)),
                MUIA_Group_VertSpacing, 0,
                Child, (IPTR) VSpace(0),
                Child, (IPTR) HGroup,
                    Child, HSpace(0),
                    Child, (IPTR) Label1(_(MSG_AREXX)),
                    Child, (IPTR) (d.arexx_checkmark = MakeCheck(NULL)),
                    Child, HSpace(0),
                    End,
                Child, (IPTR) VSpace(0),
                End,

            Child, (IPTR) VGroup,
                GroupFrameT(_(MSG_BUBBLE_HELP)),
                MUIA_Group_VertSpacing, 0,
                Child, (IPTR) VSpace(0),
                Child, (IPTR) ColGroup(2),
                    Child, (IPTR) Label1(_(MSG_FIRST_BUBBLE)),
                    Child, (IPTR) (d.first_bubble_slider = NewObject(BubbleSlider_CLASS->mcc_Class, NULL, MUIA_Numeric_Min, 0, MUIA_Numeric_Max, 50, TAG_END)),
                    Child, (IPTR) Label1(_(MSG_NEXT_BUBBLE)),
                    Child, (IPTR) (d.next_bubble_slider = NewObject(BubbleSlider_CLASS->mcc_Class, NULL, MUIA_Numeric_Min, 0, MUIA_Numeric_Max, 50, TAG_END)),
                    End,
                Child, (IPTR) VSpace(0),
                End,
            End,

        Child, (IPTR) VGroup,
            Child, (IPTR) VGroup,
                GroupFrameT(_(MSG_ICONIFICATION)),
                MUIA_Group_VertSpacing, 0,
                Child, (IPTR) VSpace(0),
                Child, (IPTR) ColGroup(2),
                    Child, (IPTR) Label1(_(MSG_HOTKEY)),
                    Child, (IPTR) (d.hotkey_string = MakeString()),
                    Child, (IPTR) Label1(_(MSG_SHOW)),
                    Child, HGroup,
                        Child, (IPTR) (d.show_icon_checkmark = MakeCheck(NULL)),
                        Child, (IPTR) Label1(_(MSG_ICON)),
                        Child, (IPTR) (d.show_menu_checkmark = MakeCheck(NULL)),
                        Child, (IPTR) Label1(_(MSG_MENU)),
                        End,
                    Child, (IPTR) Label1(_(MSG_ON_STARTUP)),
                    Child, HGroup,
                        Child, (IPTR) (d.on_startup_checkmark = MakeCheck(NULL)),
                        Child, RectangleObject, End,
                        End,
                    End,
                Child, (IPTR) VSpace(0),
                End,

            Child, (IPTR) VGroup,
                GroupFrameT(_(MSG_MUI_VERSION)),
                Child, (IPTR) HGroup,
                    Child, (IPTR) Label1(_(MSG_BEHAVE_AS)),
                    Child, (IPTR) (d.identity_cycle = MakeCycle(_(MSG_BEHAVE_AS), identity_labels)),
                    MUIA_ShortHelp, (IPTR) _(MSG_BEHAVE_HELP),
                    End,
                Child, (IPTR) ListviewObject,
                    MUIA_Listview_List, (IPTR) (d.program_list = ListObject,
                        InputListFrame,
                        MUIA_List_ConstructHook, MUIV_List_ConstructHook_String,
                        MUIA_List_DestructHook, MUIV_List_DestructHook_String,
                        End),
                    MUIA_ShortHelp, (IPTR) _(MSG_PROGRAM_LIST_HELP),
                    End,
                Child, (IPTR) HGroup,
                    Child, (IPTR) Label2(_(MSG_PROGRAM)),
                    Child, (IPTR) (d.program_string = MakeString()),
                    End,
                Child, (IPTR) HGroup,
                    Child, (IPTR) (d.program_cycle = MakeCycle(NULL, program_labels)),
                    Child, (IPTR) (d.program_add = SimpleButton(_(MSG_PROGRAM_ADD))),
                    Child, (IPTR) (d.program_remove = SimpleButton(_(MSG_PROGRAM_REMOVE))),
                    End,
                End,
            End,

        TAG_MORE, (IPTR) msg->ops_AttrList);

    if (!obj) return FALSE;
    
    d.psiHook.h_Entry = HookEntry;
    d.psiHook.h_SubEntry = (HOOKFUNC) ExecuteScreenInspectorFunc;
    
    data = INST_DATA(cl, obj);
    *data = d;

    DoMethod(
        d.first_bubble_slider, MUIM_Notify, MUIA_Numeric_Value, MUIV_EveryTime,
        d.next_bubble_slider, (IPTR) 3, MUIM_Set, MUIA_Disabled, MUIV_NotTriggerValue
    );
    
    DoMethod(
        d.call_psi_button, MUIM_Notify, MUIA_Pressed, FALSE,
        obj, (IPTR) 2, MUIM_CallHook, &data->psiHook
    );

    data->programHook.h_Entry = HookEntry;
    data->programHook.h_SubEntry = (HOOKFUNC) ProgramFunc;
    data->programHook.h_Data = data;
    DoMethod(d.program_list, MUIM_Notify, MUIA_List_Active, MUIV_EveryTime,
        obj, (IPTR) 3, MUIM_CallHook, &data->programHook, 0);
    DoMethod(d.program_add, MUIM_Notify, MUIA_Pressed, FALSE,
        obj, (IPTR) 3, MUIM_CallHook, &data->programHook, 1);
    DoMethod(d.program_string, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime,
        obj, (IPTR) 3, MUIM_CallHook, &data->programHook, 1);
    DoMethod(d.program_remove, MUIM_Notify, MUIA_Pressed, FALSE,
        obj, (IPTR) 3, MUIM_CallHook, &data->programHook, 2);
    
    return (IPTR)obj;
}


static IPTR SystemP_ConfigToGadgets(struct IClass *cl, Object *obj,
                                    struct MUIP_Settingsgroup_ConfigToGadgets *msg)
{
    struct MUI_SystemPData *data = INST_DATA(cl, obj);

    ConfigToString(msg->configdata, MUICFG_PublicScreen, data->screen_name_string);
    ConfigToCheckmark(msg->configdata, MUICFG_PublicScreen_PopToFront, data->pop_to_front_checkmark);
    ConfigToString(msg->configdata, MUICFG_Iconification_Hotkey, data->hotkey_string);
    ConfigToCheckmark(msg->configdata, MUICFG_Iconification_ShowIcon, data->show_icon_checkmark);
    ConfigToCheckmark(msg->configdata, MUICFG_Iconification_ShowMenu, data->show_menu_checkmark);
    ConfigToCheckmark(msg->configdata, MUICFG_Iconification_OnStartup, data->on_startup_checkmark);
    ConfigToCheckmark(msg->configdata, MUICFG_Interfaces_EnableARexx, data->arexx_checkmark);
    {
        char *text = AllocVec(ZUNE_BEHAVE_AS_SIZE, MEMF_ANY), *line, *e;
        int id = 0;

        DoMethod(data->program_list, MUIM_List_Clear);
        if (text && GetVar("Zune/BehaveAs", text, ZUNE_BEHAVE_AS_SIZE,
            GVF_GLOBAL_ONLY | GVF_BINARY_VAR) >= 0)
        {
            for (line = text; line; line = e)
            {
                if ((e = strchr(line, '\n')))
                    *e++ = 0;
                if (line == text)
                {
                    CONST_STRPTR s = line;

                    while (*s == ' ') s++;
                    id = identity_word(s, NULL);
                }
                else if (identity_word(line, NULL) && *line_name(line))
                    DoMethod(data->program_list, MUIM_List_InsertSingle,
                        (IPTR)line, MUIV_List_Insert_Bottom);
            }
        }
        FreeVec(text);
        set(data->identity_cycle, MUIA_Cycle_Active, id);
    }
    ConfigToSlider(msg->configdata, MUICFG_BubbleHelp_FirstDelay, data->first_bubble_slider);
    ConfigToSlider(msg->configdata, MUICFG_BubbleHelp_NextDelay, data->next_bubble_slider);

    return 1;
}


static IPTR SystemP_GadgetsToConfig(struct IClass *cl, Object *obj,
                                    struct MUIP_Settingsgroup_GadgetsToConfig *msg)
{
    struct MUI_SystemPData *data = INST_DATA(cl, obj);
    
    StringToConfig(data->screen_name_string, msg->configdata, MUICFG_PublicScreen);
    CheckmarkToConfig(data->pop_to_front_checkmark, msg->configdata, MUICFG_PublicScreen_PopToFront);
    StringToConfig(data->hotkey_string, msg->configdata, MUICFG_Iconification_Hotkey);
    CheckmarkToConfig(data->show_icon_checkmark, msg->configdata, MUICFG_Iconification_ShowIcon);
    CheckmarkToConfig(data->show_menu_checkmark, msg->configdata, MUICFG_Iconification_ShowMenu);
    CheckmarkToConfig(data->on_startup_checkmark, msg->configdata, MUICFG_Iconification_OnStartup);
    CheckmarkToConfig(data->arexx_checkmark, msg->configdata, MUICFG_Interfaces_EnableARexx);
    {
        /* the text zune.c saves as ENV:Zune/BehaveAs */
        char *t = zune_behave_as;
        STRPTR line;
        LONG i, n = 0;

        n = snprintf(t, ZUNE_BEHAVE_AS_SIZE, "%s\n",
            identity_words[XGET(data->identity_cycle, MUIA_Cycle_Active)]);
        for (i = 0; n < ZUNE_BEHAVE_AS_SIZE - 1; i++)
        {
            line = NULL;
            DoMethod(data->program_list, MUIM_List_GetEntry, i, (IPTR)&line);
            if (!line)
                break;
            n += snprintf(t + n, ZUNE_BEHAVE_AS_SIZE - n, "%s\n", line);
        }
    }
    SliderToConfig(data->first_bubble_slider, msg->configdata, MUICFG_BubbleHelp_FirstDelay);
    SliderToConfig(data->next_bubble_slider, msg->configdata, MUICFG_BubbleHelp_NextDelay);
    
    return TRUE;
}

BOOPSI_DISPATCHER(IPTR, SystemP_Dispatcher, cl, obj, msg)
{
    switch (msg->MethodID)
    {
        case OM_NEW: return SystemP_New(cl, obj, (struct opSet *)msg);
        case MUIM_Settingsgroup_ConfigToGadgets: return SystemP_ConfigToGadgets(cl,obj,(APTR)msg);break;
        case MUIM_Settingsgroup_GadgetsToConfig: return SystemP_GadgetsToConfig(cl,obj,(APTR)msg);break;
    }
    
    return DoSuperMethodA(cl, obj, msg);
}
BOOPSI_DISPATCHER_END

/*
 * Class descriptor.
 */
const struct __MUIBuiltinClass _MUIP_System_desc = {
    "System",
    MUIC_Group,
    sizeof(struct MUI_SystemPData),
    (void*)SystemP_Dispatcher
};


static const UBYTE icon32[] =
{
    0x00, 0x00, 0x00, 0x18,  // width
    0x00, 0x00, 0x00, 0x13,  // height
    'B', 'Z', '2', '\0',
    0x00, 0x00, 0x00, 0x6e,  // number of bytes

    0x42, 0x5a, 0x68, 0x39, 0x31, 0x41, 0x59, 0x26, 0x53, 0x59, 0x26, 0xc3,
    0xe9, 0x90, 0x00, 0x03, 0x0e, 0x06, 0x42, 0x80, 0x02, 0x00, 0x01, 0x08,
    0x00, 0x42, 0x40, 0x00, 0x00, 0xb0, 0x00, 0xb6, 0xc8, 0x42, 0x54, 0xc9,
    0x82, 0x92, 0xa1, 0x90, 0x42, 0xa6, 0x9a, 0x3a, 0x2a, 0xdb, 0x65, 0x78,
    0xa8, 0x34, 0xa5, 0xea, 0x52, 0xe1, 0x49, 0xd2, 0x44, 0xc1, 0x51, 0x3d,
    0xa9, 0x33, 0x43, 0x43, 0x4a, 0xad, 0x15, 0x30, 0x41, 0x40, 0xf7, 0x23,
    0x22, 0x44, 0x50, 0x81, 0x2b, 0x08, 0x18, 0x58, 0x95, 0xa7, 0x41, 0x8b,
    0x24, 0x50, 0x60, 0x97, 0x13, 0xa0, 0xd1, 0x42, 0x0d, 0x52, 0x86, 0x20,
    0x62, 0x6c, 0x08, 0xf8, 0xbb, 0x92, 0x29, 0xc2, 0x84, 0x81, 0x36, 0x1f,
    0x4c, 0x80,
};


Object *systemclass_get_icon(void)
{
    return RawimageObject,
        MUIA_Rawimage_Data, icon32,
    End;
}
