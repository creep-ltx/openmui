/* keyoffers: how often a raw key's handlers are called, black box.
 *
 * A window holds a group G (a Group subclass with a key handler) around an
 * object A (a Rectangle subclass with a key handler, the active object),
 * and an object O elsewhere (a key handler too). Nobody eats a key. A key
 * press and a release are written into input.device; every MUIM_HandleEvent
 * with a raw key is counted per object and per press/release. The same
 * binary runs on MUI 3.8 and on OpenMUI (it asks for version 19 only).
 *
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -Wno-pointer-sign -DMUI_OBSOLETE -I$I/include \
 *   -Iworkbench/libs/muimaster/include tests/native/keyoffers.c -o keyoffers -lamiga
 * Output: "keyoffers A=d/u G=d/u O=d/u order=..../...." (calls for the
 * press / the release, and the order of the calls; O's handler has a higher
 * priority than G's, so G before O means a walk of A's parents)
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
/* MUI_NewObject without the stdarg stub library (as opbench.c) */
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
static ULONG counts[3][2];
static char order[2][16];
static int norder[2];
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
    case MUIM_Setup:
        if (!DoSuperMethodA(cl, obj, msg))
            return FALSE;
        data->ehn.ehn_Events = IDCMP_RAWKEY;
        data->ehn.ehn_Object = obj;
        data->ehn.ehn_Class = cl;
        /* O before G in the handler list: a key offered to G before O
         * comes from a walk of the active object's parents */
        data->ehn.ehn_Priority = data->which == 2 ? 10 : 0;
        DoMethod(_win(obj), MUIM_Window_AddEventHandler, &data->ehn);
        return TRUE;
    case MUIM_Cleanup:
        DoMethod(_win(obj), MUIM_Window_RemEventHandler, &data->ehn);
        break;
    case MUIM_HandleEvent:
    {
        struct MUIP_HandleEvent *he = (struct MUIP_HandleEvent *)msg;
        if (he->imsg && he->imsg->Class == IDCMP_RAWKEY
            && (he->imsg->Code & 0x7f) == 0x20)
        {
            int up = (he->imsg->Code & IECODE_UP_PREFIX) ? 1 : 0;
            counts[data->which][up]++;
            if (norder[up] < 15)
                order[up][norder[up]++] = "AGO"[data->which];
        }
        return 0;
    }
    }
    return DoSuperMethodA(cl, obj, msg);
}

static void key(UWORD code)
{
    struct InputEvent ie;

    memset(&ie, 0, sizeof(ie));
    ie.ie_Class = IECLASS_RAWKEY;
    ie.ie_Code = code;
    inreq->io_Command = IND_WRITEEVENT;
    inreq->io_Data = &ie;
    inreq->io_Length = sizeof(ie);
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
    struct MUI_CustomClass *rect, *group;
    Object *app, *win, *a, *g, *o;
    int rc = 20;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    rect = MUI_CreateCustomClass(NULL, MUIC_Rectangle, NULL,
        sizeof(struct Data), (APTR)dispatch);
    group = MUI_CreateCustomClass(NULL, MUIC_Group, NULL,
        sizeof(struct Data), (APTR)dispatch);
    inport = CreateMsgPort();
    inreq = (struct IOStdReq *)CreateIORequest(inport, sizeof(*inreq));
    if (!rect || !group || !inreq
        || OpenDevice("input.device", 0, (struct IORequest *)inreq, 0))
        goto out;
    app = ApplicationObject,
        MUIA_Application_Title, "keyoffers",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "keyoffers",
            WindowContents, VGroup,
                Child, g = NewObject(group->mcc_Class, NULL,
                    TAG_USER + 1, 1, GroupFrame,
                    Child, a = NewObject(rect->mcc_Class, NULL,
                        TAG_USER + 1, 0, MUIA_CycleChain, 1,
                        MUIA_FixHeight, 20, TAG_DONE),
                    TAG_DONE),
                Child, o = NewObject(rect->mcc_Class, NULL,
                    TAG_USER + 1, 2, MUIA_FixHeight, 20, TAG_DONE),
                End,
            End,
        End;
    if (!app)
        goto out;
    set(win, MUIA_Window_Open, TRUE);
    run(app, 25);
    set(win, MUIA_Window_Activate, TRUE);
    run(app, 25);
    set(win, MUIA_Window_ActiveObject, a);
    run(app, 10);
    key(0x20);
    run(app, 10);
    key(0x20 | IECODE_UP_PREFIX);
    run(app, 10);
    printf("keyoffers A=%lu/%lu G=%lu/%lu O=%lu/%lu order=%s/%s\n",
        counts[0][0], counts[0][1], counts[1][0], counts[1][1],
        counts[2][0], counts[2][1], order[0], order[1]);
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
    if (group) MUI_DeleteCustomClass(group);
    if (rect) MUI_DeleteCustomClass(rect);
    CloseLibrary(MUIMasterBase);
    return rc;
}
