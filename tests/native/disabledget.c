/* disabledget: how a window asks whether an event handler's object is
 * disabled, black box.
 *
 * A window holds three objects of a Rectangle subclass, each with a
 * MOUSEBUTTONS event handler: N (normal), D (MUIA_Disabled TRUE) and L
 * (enabled, but its OM_GET answers MUIA_Disabled with TRUE: a lie). A left
 * click (down and up) over N is written into input.device. Counted: the
 * MUIM_HandleEvent calls with a mouse button per object, and the OM_GETs of
 * MUIA_Disabled each object receives while the click is handled. If the
 * toolkit asks the objects with OM_GET, L gets no events and every object
 * gets OM_GETs; if it reads its own state, L gets the events as N does.
 * The same binary runs on MUI 3.8, MUI 5 and OpenMUI (version 19).
 *
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -Wno-pointer-sign -DMUI_OBSOLETE -I$I/include \
 *   -Iworkbench/libs/muimaster/include tests/native/disabledget.c -o disabledget -lamiga
 * Output: "disabledget N=e/g D=e/g L=e/g" (events / OM_GETs of MUIA_Disabled)
 */
#include <stdio.h>
#include <string.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <intuition/intuition.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdarg.h>

struct Library *MUIMasterBase;
static Object *probe_new_object(CONST_STRPTR name, ...)
{
    va_list tags;
    Object *obj;
    va_start(tags, name);
    obj = MUI_NewObjectA(name, (struct TagItem *)tags);
    va_end(tags);
    return obj;
}
#undef MUI_NewObject
#define MUI_NewObject probe_new_object
static ULONG events[3], dgets[3];
static int counting;
static struct MsgPort *inport;
static struct IOStdReq *inreq;

struct Data { struct MUI_EventHandlerNode ehn; ULONG which; };

static ULONG dispatch(struct IClass *cl __asm("a0"), Object *obj __asm("a2"),
    Msg msg __asm("a1"))
{
    struct Data *data = INST_DATA(cl, obj);

    switch (msg->MethodID)
    {
    case OM_NEW:
        if ((obj = (Object *)DoSuperMethodA(cl, obj, msg)))
        {
            data = INST_DATA(cl, obj);
            data->which = GetTagData(TAG_USER + 1, 0,
                ((struct opSet *)msg)->ops_AttrList);
        }
        return (ULONG)obj;
    case OM_GET:
        if (((struct opGet *)msg)->opg_AttrID == MUIA_Disabled)
        {
            if (counting)
                dgets[data->which]++;
            if (data->which == 2)
            {
                *((struct opGet *)msg)->opg_Storage = TRUE;
                return TRUE;
            }
        }
        break;
    case MUIM_Setup:
        if (!DoSuperMethodA(cl, obj, msg))
            return FALSE;
        data->ehn.ehn_Events = IDCMP_MOUSEBUTTONS;
        data->ehn.ehn_Object = obj;
        data->ehn.ehn_Class = cl;
        DoMethod(_win(obj), MUIM_Window_AddEventHandler, &data->ehn);
        return TRUE;
    case MUIM_Cleanup:
        DoMethod(_win(obj), MUIM_Window_RemEventHandler, &data->ehn);
        break;
    case MUIM_HandleEvent:
    {
        struct MUIP_HandleEvent *he = (struct MUIP_HandleEvent *)msg;
        if (he->imsg && he->imsg->Class == IDCMP_MOUSEBUTTONS)
            events[data->which]++;
        return 0;
    }
    }
    return DoSuperMethodA(cl, obj, msg);
}

static void send(struct InputEvent *ie)
{
    inreq->io_Command = IND_WRITEEVENT;
    inreq->io_Data = ie;
    inreq->io_Length = sizeof(*ie);
    DoIO((struct IORequest *)inreq);
}

static void run(Object *app, int ticks)
{
    ULONG sigs = 0;
    int i;

    for (i = 0; i < ticks; i++)
    {
        Delay(1);
        DoMethod(app, MUIM_Application_NewInput, &sigs);
    }
}

int main(void)
{
    struct MUI_CustomClass *rect;
    Object *app, *win, *n;
    struct Window *w = NULL;
    struct InputEvent ie;
    struct IEPointerPixel pp;
    ULONG l = 0, t = 0, wd = 0, h = 0;
    int rc = 20;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    rect = MUI_CreateCustomClass(NULL, MUIC_Rectangle, NULL,
        sizeof(struct Data), (APTR)dispatch);
    inport = CreateMsgPort();
    inreq = (struct IOStdReq *)CreateIORequest(inport, sizeof(*inreq));
    if (!rect || !inreq
        || OpenDevice("input.device", 0, (struct IORequest *)inreq, 0))
        goto out;
    app = ApplicationObject,
        MUIA_Application_Title, "disabledget",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "disabledget",
            WindowContents, VGroup,
                Child, n = NewObject(rect->mcc_Class, NULL,
                    TAG_USER + 1, 0, MUIA_FixHeight, 30, MUIA_FixWidth, 120,
                    TAG_DONE),
                Child, NewObject(rect->mcc_Class, NULL,
                    TAG_USER + 1, 1, MUIA_FixHeight, 30, MUIA_Disabled, TRUE,
                    TAG_DONE),
                Child, NewObject(rect->mcc_Class, NULL,
                    TAG_USER + 1, 2, MUIA_FixHeight, 30, TAG_DONE),
                End,
            End,
        End;
    if (!app)
        goto out;
    set(win, MUIA_Window_Open, TRUE);
    run(app, 25);
    set(win, MUIA_Window_Activate, TRUE);
    run(app, 25);
    get(win, MUIA_Window_Window, &w);
    get(n, MUIA_LeftEdge, &l);
    get(n, MUIA_TopEdge, &t);
    get(n, MUIA_Width, &wd);
    get(n, MUIA_Height, &h);
    if (w)
    {
        memset(&ie, 0, sizeof(ie));
        pp.iepp_Screen = w->WScreen;
        pp.iepp_Position.X = w->LeftEdge + l + wd / 2;
        pp.iepp_Position.Y = w->TopEdge + t + h / 2;
        ie.ie_Class = IECLASS_NEWPOINTERPOS;
        ie.ie_SubClass = IESUBCLASS_PIXEL;
        ie.ie_EventAddress = &pp;
        send(&ie);
        run(app, 10);
        counting = 1;
        memset(&ie, 0, sizeof(ie));
        ie.ie_Class = IECLASS_RAWMOUSE;
        ie.ie_Code = IECODE_LBUTTON;
        ie.ie_Qualifier = IEQUALIFIER_RELATIVEMOUSE;
        send(&ie);
        run(app, 10);
        ie.ie_Code = IECODE_LBUTTON | IECODE_UP_PREFIX;
        send(&ie);
        run(app, 10);
        counting = 0;
    }
    printf("disabledget N=%lu/%lu D=%lu/%lu L=%lu/%lu\n",
        events[0], dgets[0], events[1], dgets[1], events[2], dgets[2]);
    rc = 0;
    MUI_DisposeObject(app);
out:
    if (inreq)
    {
        if (inreq->io_Device)
            CloseDevice((struct IORequest *)inreq);
        DeleteIORequest((struct IORequest *)inreq);
    }
    if (inport) DeleteMsgPort(inport);
    if (rect) MUI_DeleteCustomClass(rect);
    CloseLibrary(MUIMasterBase);
    return rc;
}
