/* ctxdrag: Area's context menu and drag and drop, black box, the same
 * binary on MUI 3.8 and OpenMUI. A window with a button that has a
 * context menu (two items, the second a checkmark), a draggable text and
 * a drop target (a custom Text subclass accepting every drop). The
 * program moves the pointer and presses the buttons through input.device
 * and runs its own MUI loop between the steps, then prints what reached
 * it: the context menu's trigger and the item's checkmark, the target's
 * DragQuery/DragBegin/DragDrop/DragFinish, a short click that must not
 * start a drag, a right click beside the button.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -I<build>/include \
 *        -Wl,-u,___stkinit ctxdrag.c -o ctxdrag -lamiga
 */
#include <exec/types.h>
#include <exec/io.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <intuition/intuition.h>
#include <libraries/mui.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <stdio.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

static struct IOStdReq *io;

static IPTR xget(Object *o, ULONG attr)
{
    IPTR v = 0;

    get(o, attr, &v);
    return v;
}
static Object *app;
static struct Screen *scr;

static void ev(UBYTE cls, UWORD code, UWORD qual, WORD x, WORD y)
{
    struct InputEvent ie = {0};
    struct IEPointerPixel pp;

    ie.ie_Class = cls;
    ie.ie_Code = code;
    ie.ie_Qualifier = qual;
    if (cls == IECLASS_NEWPOINTERPOS)
    {
        pp.iepp_Screen = scr;
        pp.iepp_Position.X = x;
        pp.iepp_Position.Y = y;
        ie.ie_SubClass = IESUBCLASS_PIXEL;
        ie.ie_EventAddress = &pp;
    }
    io->io_Command = IND_WRITEEVENT;
    io->io_Data = &ie;
    io->io_Length = sizeof(ie);
    DoIO((struct IORequest *)io);
}

/* the program's own loop for about n ticks */
static void pump(int n)
{
    ULONG sigs = 0;

    while (n-- > 0)
    {
        DoMethod(app, MUIM_Application_NewInput, (IPTR)&sigs);
        Delay(1);
        DoMethod(app, MUIM_Application_NewInput, (IPTR)&sigs);
    }
}

static void at(Object *o, WORD dx, WORD dy, UWORD qual)
{
    struct Window *w = (struct Window *)xget(o, MUIA_Window);

    ev(IECLASS_NEWPOINTERPOS, IECODE_NOBUTTON, qual,
        w->LeftEdge + _mleft(o) + dx, w->TopEdge + _mtop(o) + dy);
    pump(3);
}

struct TargetData { int dummy; };

static IPTR Target(struct IClass *cl __asm("a0"), Object *obj __asm("a2"),
    Msg msg __asm("a1"))
{
    switch (msg->MethodID)
    {
    case MUIM_DragQuery:
        printf("target: DragQuery\n");
        return MUIV_DragQuery_Accept;
    case MUIM_DragBegin:
        printf("target: DragBegin\n");
        break;
    case MUIM_DragFinish:
        printf("target: DragFinish\n");
        break;
    case MUIM_DragDrop:
        printf("target: DragDrop\n");
        return 0;
    }
    return DoSuperMethodA(cl, obj, msg);
}

int main(int argc, char **argv)
{
    struct MUI_CustomClass *tc;
    struct MsgPort *port;
    Object *win, *button, *drag, *target, *menu, *item1, *item2;
    LONG trig = 0;

    setvbuf(stdout, NULL, _IOLBF, 0);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    if (!(port = CreateMsgPort())
        || !(io = (struct IOStdReq *)CreateIORequest(port, sizeof(*io)))
        || OpenDevice("input.device", 0, (struct IORequest *)io, 0))
        return 20;
    tc = MUI_CreateCustomClass(NULL, MUIC_Text, NULL,
        sizeof(struct TargetData), (APTR)Target);
    if (!tc)
        return 20;

    menu = MenustripObject,
        MUIA_Family_Child, MenuObject, MUIA_Menu_Title, "Ctx",
            MUIA_Family_Child, item1 = MenuitemObject,
                MUIA_Menuitem_Title, "First", End,
            MUIA_Family_Child, item2 = MenuitemObject,
                MUIA_Menuitem_Title, "Check",
                MUIA_Menuitem_Checkit, TRUE, End,
            End,
        End;
    app = ApplicationObject,
        MUIA_Application_Title, "ctxdrag",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "ctxdrag",
            MUIA_Window_LeftEdge, 20, MUIA_Window_TopEdge, 30,
            MUIA_Window_Width, 300, MUIA_Window_Height, 160,
            WindowContents, VGroup,
                Child, button = TextObject, ButtonFrame,
                    MUIA_Text_Contents, "\33cMenu here",
                    MUIA_InputMode, MUIV_InputMode_RelVerify,
                    MUIA_ContextMenu, menu, End,
                Child, HGroup,
                    Child, drag = TextObject, TextFrame,
                        MUIA_Text_Contents, "Drag me",
                        MUIA_InputMode, MUIV_InputMode_RelVerify,
                        MUIA_Draggable, TRUE, End,
                    Child, target = NewObject(tc->mcc_Class, NULL,
                        TextFrame, MUIA_Text_Contents, "Drop here",
                        MUIA_Dropable, TRUE, TAG_DONE),
                    End,
                End,
            End,
        End;
    if (!app)
    {
        printf("no application\n");
        return 20;
    }
    DoMethod(button, MUIM_Notify, MUIA_ContextMenuTrigger, MUIV_EveryTime,
        MUIV_Notify_Self, 3, MUIM_WriteLong, MUIV_TriggerValue, &trig);
    set(win, MUIA_Window_Open, TRUE);
    scr = (struct Screen *)xget(win, MUIA_Window_Screen);
    pump(25);

    /* the context menu: right button down on the button, down onto the
     * items, released on the second; then again on the first */
    {
        int pass;

        for (pass = 0; pass < 2; pass++)
        {
            int k;

            trig = 0;
            at(button, 10, 2, 0);
            ev(IECLASS_RAWMOUSE, IECODE_RBUTTON, IEQUALIFIER_RBUTTON, 0, 0);
            pump(10);
            if (argc > 1)
            {
                /* "show": the open menu held for a screenshot */
                printf("menu open\n");
                pump(250);
            }
            /* down onto the menu's title, right into its items, down to
             * the second item in the first pass */
            for (k = 1; k <= 10; k++)
                at(button, 10, 2 + k * 3, IEQUALIFIER_RBUTTON);
            for (k = 1; k <= 10; k++)
                at(button, 10 + k * 6, 32, IEQUALIFIER_RBUTTON);
            for (k = 1; k <= (pass ? 0 : 5); k++)
                at(button, 70, 32 + k * 2, IEQUALIFIER_RBUTTON);
            if (argc > 1)
            {
                printf("items open\n");
                pump(250);
            }
            pump(5);
            ev(IECLASS_RAWMOUSE, IECODE_RBUTTON | IECODE_UP_PREFIX, 0, 0, 0);
            pump(15);
            printf("menu pass %d: trigger %s, check item %ld\n", pass,
                trig == (LONG)item1 ? "First" : trig == (LONG)item2 ? "Check"
                : trig ? "other" : "none",
                (long)xget(item2, MUIA_Menuitem_Checked));
        }
    }
    /* a right click beside the objects (the group): nothing */
    at(button, -3, 40, 0);
    ev(IECLASS_RAWMOUSE, IECODE_RBUTTON, IEQUALIFIER_RBUTTON, 0, 0);
    pump(5);
    ev(IECLASS_RAWMOUSE, IECODE_RBUTTON | IECODE_UP_PREFIX, 0, 0, 0);
    pump(5);
    printf("right click beside: trigger %s\n", trig ? "yes" : "none");

    /* a short click on the draggable text: no drag, a press */
    at(drag, 5, 2, 0);
    ev(IECLASS_RAWMOUSE, IECODE_LBUTTON, IEQUALIFIER_LEFTBUTTON, 0, 0);
    pump(3);
    at(drag, 6, 2, IEQUALIFIER_LEFTBUTTON);
    printf("short move: selected %ld pressed %ld\n",
        (long)xget(drag, MUIA_Selected), (long)xget(drag, MUIA_Pressed));
    ev(IECLASS_RAWMOUSE, IECODE_LBUTTON | IECODE_UP_PREFIX, 0, 0, 0);
    pump(5);
    printf("short click: selected %ld\n", (long)xget(drag, MUIA_Selected));

    /* a drag onto the target */
    at(drag, 5, 2, 0);
    ev(IECLASS_RAWMOUSE, IECODE_LBUTTON, IEQUALIFIER_LEFTBUTTON, 0, 0);
    pump(3);
    {
        int k;
        LONG tx = _mleft(target) + 10 - _mleft(drag);

        for (k = 1; k <= 8; k++)
            at(drag, 5 + tx * k / 8, 2, IEQUALIFIER_LEFTBUTTON);
    }
    pump(10);
    ev(IECLASS_RAWMOUSE, IECODE_LBUTTON | IECODE_UP_PREFIX, 0, 0, 0);
    pump(20);
    printf("after drag: selected %ld pressed %ld\n",
        (long)xget(drag, MUIA_Selected), (long)xget(drag, MUIA_Pressed));

    set(win, MUIA_Window_Open, FALSE);
    MUI_DisposeObject(app);
    MUI_DisposeObject(menu);
    MUI_DeleteCustomClass(tc);
    CloseDevice((struct IORequest *)io);
    DeleteIORequest((struct IORequest *)io);
    DeleteMsgPort(port);
    CloseLibrary(MUIMasterBase);
    printf("done\n");
    return 0;
}
