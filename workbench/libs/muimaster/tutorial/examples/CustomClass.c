/*
    A custom class: Meter, a subclass of Area that draws a bar and
    changes its value when the user clicks in it.

    Shows the parts every custom class has (docs/reference/custom-classes.md):
    the dispatcher, a private attribute, OM_NEW/OM_SET/OM_GET,
    MUIM_AskMinMax, MUIM_Setup/MUIM_Cleanup (an event handler), MUIM_HandleEvent
    and MUIM_Draw.  The class is private to the program (no library base),
    made with MUI_CreateCustomClass() and deleted before the library is closed.

    Build (see the reference):
      m68k-amigaos-gcc -noixemul -Os -Iinclude -o CustomClass CustomClass.c -lmui
*/

#include <exec/types.h>
#include <intuition/classusr.h>
#include <intuition/intuition.h>
#include <utility/tagitem.h>
#include <libraries/mui.h>

#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/utility.h>
#include <proto/muimaster.h>

struct Library *MUIMasterBase;

/* A private attribute: MUIB_MUI-based ids are MUI's; programs use TAG_USER */
#define MUIA_Meter_Value    (TAG_USER | 0x4d300001)   /* isg  LONG, 0..100 */

struct Meter_DATA
{
    LONG value;
    struct MUI_EventHandlerNode ehn;    /* registered in Setup */
};

static IPTR Meter_New(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct Meter_DATA *data;

    obj = (Object *) DoSuperMethodA(cl, obj, (Msg) msg);
    if (!obj)
        return 0;

    data = INST_DATA(cl, obj);
    data->value = GetTagData(MUIA_Meter_Value, 0, msg->ops_AttrList);
    return (IPTR) obj;
}

static IPTR Meter_Set(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct Meter_DATA *data = INST_DATA(cl, obj);
    struct TagItem *tag;
    struct TagItem *tags = msg->ops_AttrList;

    while ((tag = NextTagItem(&tags)))
    {
        if (tag->ti_Tag == MUIA_Meter_Value)
        {
            LONG v = (LONG) tag->ti_Data;

            if (v < 0)
                v = 0;
            if (v > 100)
                v = 100;
            if (v != data->value)
            {
                data->value = v;
                /* redraw only the changed object, not its frame */
                MUI_Redraw(obj, MADF_DRAWUPDATE);
            }
            else
                tag->ti_Tag = TAG_IGNORE;  /* no change, no notification */
        }
    }
    return DoSuperMethodA(cl, obj, (Msg) msg);
}

static IPTR Meter_Get(struct IClass *cl, Object *obj, struct opGet *msg)
{
    struct Meter_DATA *data = INST_DATA(cl, obj);

    if (msg->opg_AttrID == MUIA_Meter_Value)
    {
        *msg->opg_Storage = (IPTR) data->value;
        return TRUE;
    }
    return DoSuperMethodA(cl, obj, (Msg) msg);
}

static IPTR Meter_AskMinMax(struct IClass *cl, Object *obj,
    struct MUIP_AskMinMax *msg)
{
    /* The superclass fills in what the frame and inner spacing need;
       the class adds what its content needs. */
    DoSuperMethodA(cl, obj, (Msg) msg);
    msg->MinMaxInfo->MinWidth += 50;
    msg->MinMaxInfo->DefWidth += 100;
    msg->MinMaxInfo->MaxWidth += MUI_MAXMAX;   /* may grow without limit */
    msg->MinMaxInfo->MinHeight += 8;
    msg->MinMaxInfo->DefHeight += 12;
    msg->MinMaxInfo->MaxHeight += 12;          /* a bar does not get taller */
    return 0;
}

static IPTR Meter_Setup(struct IClass *cl, Object *obj, Msg msg)
{
    struct Meter_DATA *data = INST_DATA(cl, obj);

    if (!DoSuperMethodA(cl, obj, msg))
        return FALSE;

    /* Ask the window for mouse button events, only while the object is
       shown and enabled (MUI_EHF_GUIMODE) */
    data->ehn.ehn_Priority = 0;
    data->ehn.ehn_Flags = MUI_EHF_GUIMODE;
    data->ehn.ehn_Object = obj;
    data->ehn.ehn_Class = cl;
    data->ehn.ehn_Events = IDCMP_MOUSEBUTTONS;
    DoMethod(_win(obj), MUIM_Window_AddEventHandler, (IPTR) &data->ehn);
    return TRUE;
}

static IPTR Meter_Cleanup(struct IClass *cl, Object *obj, Msg msg)
{
    struct Meter_DATA *data = INST_DATA(cl, obj);

    DoMethod(_win(obj), MUIM_Window_RemEventHandler, (IPTR) &data->ehn);
    return DoSuperMethodA(cl, obj, msg);
}

static IPTR Meter_HandleEvent(struct IClass *cl, Object *obj,
    struct MUIP_HandleEvent *msg)
{
    struct IntuiMessage *imsg = msg->imsg;

    if (imsg && imsg->Class == IDCMP_MOUSEBUTTONS && imsg->Code == SELECTDOWN
        && imsg->MouseX >= _mleft(obj) && imsg->MouseX <= _mright(obj)
        && imsg->MouseY >= _mtop(obj) && imsg->MouseY <= _mbottom(obj))
    {
        LONG v = (imsg->MouseX - _mleft(obj)) * 100 / _mwidth(obj);

        set(obj, MUIA_Meter_Value, v);
        return MUI_EventHandlerRC_Eat;
    }
    return 0;
}

static IPTR Meter_Draw(struct IClass *cl, Object *obj, struct MUIP_Draw *msg)
{
    struct Meter_DATA *data = INST_DATA(cl, obj);
    struct RastPort *rp;
    LONG filled;

    /* The superclass draws the frame and the background when the flags ask
       for it; it also updates msg->flags. */
    DoSuperMethodA(cl, obj, (Msg) msg);

    if (!(msg->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE)))
        return 0;

    rp = _rp(obj);
    filled = _mwidth(obj) * data->value / 100;

    SetAPen(rp, _pens(obj)[MPEN_FILL]);
    if (filled > 0)
        RectFill(rp, _mleft(obj), _mtop(obj), _mleft(obj) + filled - 1,
            _mbottom(obj));
    SetAPen(rp, _pens(obj)[MPEN_BACKGROUND]);
    if (filled < _mwidth(obj))
        RectFill(rp, _mleft(obj) + filled, _mtop(obj), _mright(obj),
            _mbottom(obj));
    return 0;
}

/* The dispatcher: the class in a0, the object in a2, the message in a1.
   Everything it does not know goes to the superclass. */
static IPTR Meter_Dispatcher(struct IClass *cl __asm("a0"),
    Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    switch (msg->MethodID)
    {
    case OM_NEW:          return Meter_New(cl, obj, (struct opSet *) msg);
    case OM_SET:          return Meter_Set(cl, obj, (struct opSet *) msg);
    case OM_GET:          return Meter_Get(cl, obj, (struct opGet *) msg);
    case MUIM_AskMinMax:  return Meter_AskMinMax(cl, obj, (APTR) msg);
    case MUIM_Setup:      return Meter_Setup(cl, obj, msg);
    case MUIM_Cleanup:    return Meter_Cleanup(cl, obj, msg);
    case MUIM_HandleEvent:return Meter_HandleEvent(cl, obj, (APTR) msg);
    case MUIM_Draw:       return Meter_Draw(cl, obj, (APTR) msg);
    }
    return DoSuperMethodA(cl, obj, msg);
}

int main(void)
{
    struct MUI_CustomClass *mcc;
    Object *app, *win, *slider, *meter;
    int rc = 20;

    /* Ask for 19 (MUI 3.8 behaviour) unless the program uses MUI 4/5 calls;
       see docs/reference/README.md "Opening the library" */
    MUIMasterBase = OpenLibrary("muimaster.library", 19);
    if (!MUIMasterBase)
        return rc;

    /* base NULL: a class private to this program */
    mcc = MUI_CreateCustomClass(NULL, MUIC_Area, NULL,
        sizeof(struct Meter_DATA), (APTR) Meter_Dispatcher);
    if (mcc)
    {
        app = ApplicationObject,
            MUIA_Application_Title, (IPTR) "CustomClass",
            MUIA_Application_Version, (IPTR) "$VER: CustomClass 1.0 (18.11.26)",
            MUIA_Application_Base, (IPTR) "CUSTOMCLASS",
            SubWindow, win = WindowObject,
                MUIA_Window_Title, (IPTR) "Meter",
                MUIA_Window_ID, MAKE_ID('M', 'E', 'T', 'R'),
                WindowContents, VGroup,
                    Child, meter = NewObject(mcc->mcc_Class, NULL,
                        MUIA_Frame, MUIV_Frame_Gauge,
                        MUIA_Meter_Value, 30,
                        TAG_DONE),
                    Child, slider = SliderObject,
                        MUIA_Numeric_Min, 0,
                        MUIA_Numeric_Max, 100,
                        MUIA_Numeric_Value, 30,
                    End,
                End,
            End,
        End;

        if (app)
        {
            ULONG sigs = 0;

            /* the slider drives the meter; a click in the meter moves the
               slider */
            DoMethod(slider, MUIM_Notify, MUIA_Numeric_Value, MUIV_EveryTime,
                meter, 3, MUIM_Set, MUIA_Meter_Value, MUIV_TriggerValue);
            DoMethod(meter, MUIM_Notify, MUIA_Meter_Value, MUIV_EveryTime,
                slider, 3, MUIM_Set, MUIA_Numeric_Value, MUIV_TriggerValue);
            DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
                app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);

            set(win, MUIA_Window_Open, TRUE);
            while (DoMethod(app, MUIM_Application_NewInput, (IPTR) &sigs)
                != MUIV_Application_ReturnID_Quit)
            {
                if (sigs)
                {
                    sigs = Wait(sigs | SIGBREAKF_CTRL_C);
                    if (sigs & SIGBREAKF_CTRL_C)
                        break;
                }
            }
            rc = 0;
            MUI_DisposeObject(app);     /* disposes the window and the meter */
        }
        MUI_DeleteCustomClass(mcc);     /* only after every object is gone */
    }
    CloseLibrary(MUIMasterBase);
    return rc;
}
