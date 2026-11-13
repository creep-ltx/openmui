/* borderarrows: the window border's scroller arrows. A Scrollgroup with
 * MUIA_Scrollgroup_UseWinBorder around a tall and wide Virtgroup; the
 * program clicks each of the four border arrow buttons (input.device
 * events at the gadgets' places) and prints the Virtgroup's Left/Top
 * after each, the gadget IDs, and closes and opens the window again.
 * The same output on two libraries = the same arrows.
 * Build (I = OpenMUI's library build): m68k-amigaos-gcc -m68000 -noixemul \
 *   -Os -DMUI_OBSOLETE -I$I/include -Iworkbench/libs/muimaster/include \
 *   tests/lean/borderarrows.c -o borderarrows -lamiga
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
ULONG __stack = 65536;
static Object *new_object(CONST_STRPTR name, ...)
{
    va_list tags;
    Object *obj;
    va_start(tags, name);
    obj = MUI_NewObjectA(name, (struct TagItem *)tags);
    va_end(tags);
    return obj;
}
#undef MUI_NewObject
#define MUI_NewObject new_object
static Object *make_object(LONG type, ...)
{
    va_list params;
    Object *obj;
    va_start(params, type);
    obj = MUI_MakeObjectA(type, (ULONG *)params);
    va_end(params);
    return obj;
}
#undef MUI_MakeObject
#define MUI_MakeObject make_object

static struct MsgPort *port;
static struct IOStdReq *inreq;
static Object *app, *win, *vg;

static void send_event(struct InputEvent *ie)
{
    ie->ie_NextEvent = NULL;
    inreq->io_Command = IND_WRITEEVENT;
    inreq->io_Data = ie;
    inreq->io_Length = sizeof(*ie);
    DoIO((struct IORequest *)inreq);
}

static void run(int n)
{
    ULONG sigs = 0;
    int i;
    for (i = 0; i < n; i++)
    {
        Delay(2);
        DoMethod(app, MUIM_Application_NewInput, &sigs);
    }
}

static void click(struct Screen *scr, int x, int y)
{
    struct InputEvent ie;
    struct IEPointerPixel pp;

    memset(&ie, 0, sizeof(ie));
    pp.iepp_Screen = scr;
    pp.iepp_Position.X = x;
    pp.iepp_Position.Y = y;
    ie.ie_Class = IECLASS_NEWPOINTERPOS;
    ie.ie_SubClass = IESUBCLASS_PIXEL;
    ie.ie_EventAddress = &pp;
    send_event(&ie);
    run(2);
    memset(&ie, 0, sizeof(ie));
    ie.ie_Class = IECLASS_RAWMOUSE;
    ie.ie_Code = IECODE_LBUTTON;
    ie.ie_Qualifier = IEQUALIFIER_RELATIVEMOUSE;
    send_event(&ie);
    run(2);
    ie.ie_Code = IECODE_LBUTTON | IECODE_UP_PREFIX;
    send_event(&ie);
    run(6);
}

static int getenv_hold(void)
{
    char b[4];
    return GetVar("BAHOLD", b, sizeof(b), 0) > 0;
}

static void report(const char *what)
{
    ULONG l = 0, t = 0;
    get(vg, MUIA_Virtgroup_Left, &l);
    get(vg, MUIA_Virtgroup_Top, &t);
    printf("%s: left %ld top %ld\n", what, (long)l, (long)t);
}

/* the border buttons (BOOPSI gadgets, not system ones) */
static void arrows(void)
{
    struct Window *w = NULL;
    struct Gadget *g;
    int n = 0;

    get(win, MUIA_Window_Window, &w);
    if (!w)
    {
        printf("no window\n");
        return;
    }
    for (g = w->FirstGadget; g; g = g->NextGadget)
    {
        int x, y;
        char what[48];

        if (g->GadgetType & GTYP_SYSGADGET)
            continue;
        printf("gadget id %ld type %lx flags %lx\n", (long)g->GadgetID,
            (long)(g->GadgetType & GTYP_GTYPEMASK), (long)(g->Flags & 0x30));
        /* the arrows: small; a prop has a relative (negative) length */
        if (g->Width <= 0 || g->Height <= 0 || g->Width > 30
            || g->Height > 30)
            continue;
        x = (g->Flags & GFLG_RELRIGHT) ? w->Width - 1 + g->LeftEdge
            : g->LeftEdge;
        y = (g->Flags & GFLG_RELBOTTOM) ? w->Height - 1 + g->TopEdge
            : g->TopEdge;
        x += w->LeftEdge + g->Width / 2;
        y += w->TopEdge + g->Height / 2;
        if (getenv_hold())
            printf("click %ld,%ld\n", (long)x, (long)y);
        click(w->WScreen, x, y);
        sprintf(what, "arrow %ld (id %ld)", (long)n++, (long)g->GadgetID);
        report(what);
    }
}

int main(void)
{
    Object *body;
    int i;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    port = CreateMsgPort();
    inreq = (struct IOStdReq *)CreateIORequest(port, sizeof(*inreq));
    if (!inreq || OpenDevice("input.device", 0, (struct IORequest *)inreq, 0))
        return 20;
    app = ApplicationObject, MUIA_Application_Title, "borderarrows",
        SubWindow, win = WindowObject, MUIA_Window_Title, "borderarrows",
            MUIA_Window_Width, 220, MUIA_Window_Height, 140,
            MUIA_Window_LeftEdge, 40, MUIA_Window_TopEdge, 40,
            MUIA_Window_UseRightBorderScroller, TRUE,
            MUIA_Window_UseBottomBorderScroller, TRUE,
            WindowContents, ScrollgroupObject,
                MUIA_Scrollgroup_UseWinBorder, TRUE,
                MUIA_Scrollgroup_Contents, vg = VirtgroupObject,
                    Child, body = ColGroup(4), End,
                End,
            End,
        End,
    End;
    if (!app)
    {
        printf("no application\n");
        return 20;
    }
    for (i = 0; i < 80; i++)
    {
        char buf[32];
        sprintf(buf, "Cell number %d", i);
        DoMethod(body, OM_ADDMEMBER, SimpleButton(buf));
    }
    set(win, MUIA_Window_Open, TRUE);
    run(5);
    report("opened");
    if (getenv_hold())
    {
        for (i = 0; i < 40; i++)
        {
            run(25);
            report("hold");
        }
    }
    arrows();
    set(win, MUIA_Window_Open, FALSE);
    set(win, MUIA_Window_Open, TRUE);
    run(5);
    report("reopened");
    arrows();
    MUI_DisposeObject(app);
    CloseDevice((struct IORequest *)inreq);
    DeleteIORequest((struct IORequest *)inreq);
    DeleteMsgPort(port);
    CloseLibrary(MUIMasterBase);
    printf("done\n");
    return 0;
}
