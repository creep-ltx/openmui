/* opbench: per-operation timing of whichever muimaster.library is installed.
 *
 * The same binary runs on MUI 3.8 and on OpenMUI, so it uses only classes,
 * attributes and methods MUI 3.8 documents (it asks for version 19). Every
 * operation is timed with the EClock (emulated time on an emulator), REPS
 * samples of a batch, and printed as the median per operation:
 *
 *   NAME  us/op=MEDIAN  min=..  max=..  batch=N
 *
 * Operations that a user causes (a button click, typing, opening a menu) are
 * written into input.device as real input events and the program's own MUI
 * loop runs until MUI has done what the event asks, so both libraries take
 * the same path from the event to the result. A wait that does not end in 3
 * seconds prints "NAME timeout" and the run goes on.
 *
 * Build (GCC 16.2 or bebbo 6.5; I = OpenMUI's library build):
 *   m68k-amigaos-gcc -m68000 -noixemul -Os -DMUI_OBSOLETE -I$I/include \
 *     -Iworkbench/libs/muimaster/include tests/native/opbench.c -o opbench -lamiga
 * Run: opbench [REPS]   (on its own 640x480 depth-2 screen; default 9 reps)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <devices/timer.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <intuition/intuition.h>
#include <libraries/gadtools.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <graphics/modeid.h>
#include <proto/timer.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdarg.h>

struct Library *MUIMasterBase;
static Object *bench_new_object(CONST_STRPTR name, ...)
{
    va_list tags;
    Object *obj;
    va_start(tags, name);
    obj = MUI_NewObjectA(name, (struct TagItem *)tags);
    va_end(tags);
    return obj;
}
#undef MUI_NewObject
#define MUI_NewObject bench_new_object
static Object *bench_make_object(LONG type, ...)
{
    va_list params;
    Object *obj;
    va_start(params, type);
    obj = MUI_MakeObjectA(type, (ULONG *)params);
    va_end(params);
    return obj;
}
#undef MUI_MakeObject
#define MUI_MakeObject bench_make_object

struct Device *TimerBase;
ULONG __stack = 65536;

#define MAXREPS 31
static int reps = 9;
static ULONG samples[MAXREPS];
static ULONG eclock_freq;

static const char *cycles[] = { "First", "Second", "Third", NULL };
static const char *radios[] = { "Alpha", "Beta", "Gamma", NULL };
static const char *pages[] = { "One", "Two", "Three", NULL };
static const char *entries10[] = {
    "Entry one", "Entry two", "Entry three", "Entry four", "Entry five",
    "Entry six", "Entry seven", "Entry eight", "Entry nine", "Entry ten",
    NULL
};
#define NBIG 1000
static char bigtext[NBIG][12];
static const char *big[NBIG + 1], *shuffled[NBIG + 1];

static struct NewMenu menu[] = {
    { NM_TITLE, "Project", 0, 0, 0, 0 },
    { NM_ITEM, "Open...", "O", 0, 0, 0 }, { NM_ITEM, "Save", "S", 0, 0, 0 },
    { NM_ITEM, "Save as...", "A", 0, 0, 0 }, { NM_ITEM, NM_BARLABEL, 0, 0, 0, 0 },
    { NM_ITEM, "Print", "P", 0, 0, 0 }, { NM_ITEM, "About...", "?", 0, 0, 0 },
    { NM_ITEM, NM_BARLABEL, 0, 0, 0, 0 }, { NM_ITEM, "Quit", "Q", 0, 0, 0 },
    { NM_TITLE, "Edit", 0, 0, 0, 0 },
    { NM_ITEM, "Cut", "X", 0, 0, 0 }, { NM_ITEM, "Copy", "C", 0, 0, 0 },
    { NM_ITEM, "Paste", "V", 0, 0, 0 }, { NM_ITEM, "Select all", 0, 0, 0, 0 },
    { NM_TITLE, "Settings", 0, 0, 0, 0 },
    { NM_ITEM, "Icons", 0, CHECKIT | MENUTOGGLE | CHECKED, 0, 0 },
    { NM_ITEM, "Save settings", 0, 0, 0, 0 }, { NM_ITEM, "MUI...", 0, 0, 0, 0 },
    { NM_END, 0, 0, 0, 0, 0 }
};

static Object *app, *win, *root, *list, *slider, *numeric, *text, *string;
static Object *cycle, *radio, *button, *status;
static Object *win2, *root2, *reg, *pop, *poplist;
static struct Screen *scr;
static struct MsgPort *inport;
static struct IOStdReq *inreq;
static struct timerequest *treq;
static struct MsgPort *tport;
static ULONG sigs;
static volatile LONG pressed;

static ULONG now(void)
{
    struct EClockVal ev;

    eclock_freq = ReadEClock(&ev);
    return ev.ev_lo;
}

static int cmp(const void *a, const void *b)
{
    ULONG x = *(const ULONG *)a, y = *(const ULONG *)b;

    return x < y ? -1 : x > y;
}

/* One line per operation: median, min and max per operation in us */
static void report(const char *name, ULONG ops)
{
    ULONG d = (eclock_freq / 1000) * ops;   /* ticks per ms, per op */

    qsort(samples, reps, sizeof(samples[0]), cmp);
#define US(t) ((unsigned long)(((unsigned long long)(t) * 1000ULL * 10 + d / 2) / d))
    printf("%-18s us/op=%7lu.%lu min=%lu.%lu max=%lu.%lu batch=%lu\n", name,
        US(samples[reps / 2]) / 10, US(samples[reps / 2]) % 10,
        US(samples[0]) / 10, US(samples[0]) % 10,
        US(samples[reps - 1]) / 10, US(samples[reps - 1]) % 10,
        (unsigned long)ops);
}

/* ---- MUI loop with a 3 s guard ---------------------------------------- */
static int timedout;

static void guard_start(void)
{
    treq->tr_node.io_Command = TR_ADDREQUEST;
    treq->tr_time.tv_secs = 3;
    treq->tr_time.tv_micro = 0;
    SendIO((struct IORequest *)treq);
}

static void guard_stop(void)
{
    if (!CheckIO((struct IORequest *)treq))
        AbortIO((struct IORequest *)treq);
    WaitIO((struct IORequest *)treq);
}

/* Run the application's input loop until cond() holds */
static int run_until(int (*cond)(void))
{
    guard_start();
    for (;;)
    {
        if (DoMethod(app, MUIM_Application_NewInput, &sigs) ==
                (ULONG)MUIV_Application_ReturnID_Quit)
            break;
        if (cond())
            break;
        if (CheckIO((struct IORequest *)treq))
        {
            timedout = 1;
            break;
        }
        if (sigs)
            Wait(sigs | (1UL << tport->mp_SigBit));
    }
    guard_stop();
    return !timedout;
}

/* Let everything pending be handled (outside the timed parts) */
static void settle(void)
{
    int i;

    for (i = 0; i < 5; i++)
    {
        Delay(2);
        DoMethod(app, MUIM_Application_InputBuffered);
        DoMethod(app, MUIM_Application_NewInput, &sigs);
    }
}

/* ---- input events ----------------------------------------------------- */
static void send_event(struct InputEvent *ie)
{
    ULONG s, m;

    CurrentTime(&s, &m);
    ie->ie_TimeStamp.tv_secs = s;
    ie->ie_TimeStamp.tv_micro = m;
    ie->ie_NextEvent = NULL;
    inreq->io_Command = IND_WRITEEVENT;
    inreq->io_Data = ie;
    inreq->io_Length = sizeof(*ie);
    DoIO((struct IORequest *)inreq);
}

static void pointer_to(int x, int y)
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
}

static void mouse_button(UWORD code)
{
    struct InputEvent ie;

    memset(&ie, 0, sizeof(ie));
    ie.ie_Class = IECLASS_RAWMOUSE;
    ie.ie_Code = code;
    ie.ie_Qualifier = IEQUALIFIER_RELATIVEMOUSE;
    send_event(&ie);
}

static void key(UWORD code)
{
    struct InputEvent ie;

    memset(&ie, 0, sizeof(ie));
    ie.ie_Class = IECLASS_RAWKEY;
    ie.ie_Code = code;
    send_event(&ie);
    ie.ie_Code = code | IECODE_UP_PREFIX;
    send_event(&ie);
}

/* The centre of an object, in screen coordinates */
static void centre(Object *o, Object *w, int *x, int *y)
{
    struct Window *nw = NULL;
    ULONG l = 0, t = 0, wd = 0, h = 0;

    get(w, MUIA_Window_Window, &nw);
    get(o, MUIA_LeftEdge, &l);
    get(o, MUIA_TopEdge, &t);
    get(o, MUIA_Width, &wd);
    get(o, MUIA_Height, &h);
    *x = (nw ? nw->LeftEdge : 0) + (WORD)l + (WORD)wd / 2;
    *y = (nw ? nw->TopEdge : 0) + (WORD)t + (WORD)h / 2;
}

/* ---- the objects -------------------------------------------------------- */
static struct Hook presshook;

static ULONG press_func(void)
{
    pressed++;
    return 0;
}

static Object *menustrip(void)
{
    ULONG params[2];

    params[0] = (ULONG)menu;
    params[1] = 0;
    return MUI_MakeObjectA(MUIO_MenustripNM, params);
}

static Object *build(void)
{
    return ApplicationObject,
        MUIA_Application_Title, "opbench",
        MUIA_Application_Base, "OPBENCH",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "opbench",
            MUIA_Window_Screen, scr,
            MUIA_Window_LeftEdge, 20,
            MUIA_Window_TopEdge, 20,
            MUIA_Window_Width, 600,
            MUIA_Window_Height, 400,
            MUIA_Window_Menustrip, menustrip(),
            WindowContents, root = VGroup,
                MUIA_FixWidth, 600,
                MUIA_FixHeight, 400,
                Child, HGroup, GroupFrameT("Controls"),
                    Child, ColGroup(2),
                        Child, Label2("Name:"),
                        Child, string = StringObject, StringFrame,
                            MUIA_String_MaxLen, 120, End,
                        Child, Label1("Mode:"),
                        Child, cycle = CycleObject, MUIA_Cycle_Entries, cycles, End,
                        Child, Label1("Check:"),
                        Child, CheckMark(TRUE),
                        Child, Label2("Value:"),
                        Child, numeric = SliderObject,
                            MUIA_Numeric_Max, 1000, End,
                        End,
                    Child, radio = RadioObject, GroupFrame,
                        MUIA_Radio_Entries, radios, End,
                    End,
                Child, HGroup,
                    Child, ListviewObject,
                        MUIA_Listview_List, list = ListObject,
                            InputListFrame, End,
                        End,
                    Child, VGroup,
                        Child, text = TextObject, TextFrame,
                            MUIA_Text_Contents, "Status", End,
                        Child, slider = SliderObject,
                            MUIA_Numeric_Max, 1000, End,
                        Child, GaugeObject, GaugeFrame,
                            MUIA_Gauge_Horiz, TRUE, End,
                        Child, HGroup,
                            Child, button = SimpleButton("_One"),
                            Child, SimpleButton("_Two"),
                            Child, SimpleButton("_Three"),
                            End,
                        Child, status = TextObject, TextFrame,
                            MUIA_Text_Contents, "-", End,
                        End,
                    End,
                Child, HGroup, GroupFrameT("Footer"),
                    Child, TextObject, MUIA_Text_Contents,
                        "\33cA centered footer line", End,
                    Child, HSpace(0),
                    Child, SimpleButton("OK"),
                    Child, SimpleButton("Cancel"),
                    End,
                End,
            End,
        SubWindow, win2 = WindowObject,
            MUIA_Window_Title, "opbench 2",
            MUIA_Window_Screen, scr,
            MUIA_Window_LeftEdge, 30,
            MUIA_Window_TopEdge, 40,
            MUIA_Window_Width, 360,
            MUIA_Window_Height, 240,
            WindowContents, root2 = VGroup,
                Child, reg = RegisterGroup(pages),
                    Child, VGroup,
                        Child, StringObject, StringFrame, End,
                        Child, HGroup,
                            Child, SimpleButton("Alpha"),
                            Child, SimpleButton("Beta"),
                            End,
                        Child, TextObject, TextFrame,
                            MUIA_Text_Contents, "Page one", End,
                        End,
                    Child, VGroup,
                        Child, CycleObject, MUIA_Cycle_Entries, cycles, End,
                        Child, SliderObject, MUIA_Numeric_Max, 100, End,
                        Child, CheckMark(FALSE),
                        End,
                    Child, ListviewObject,
                        MUIA_Listview_List, ListObject, InputListFrame,
                            MUIA_List_SourceArray, entries10, End,
                        End,
                    End,
                Child, pop = PopobjectObject,
                    MUIA_Popstring_String, StringObject, StringFrame, End,
                    MUIA_Popstring_Button, PopButton(MUII_PopUp),
                    MUIA_Popobject_Object, ListviewObject,
                        MUIA_Listview_List, poplist = ListObject, InputListFrame,
                            MUIA_List_SourceArray, entries10, End,
                        End,
                    End,
                End,
            End,
        End;
}

static Object *build_small(void)
{
    return ApplicationObject,
        MUIA_Application_Title, "opbench small",
        MUIA_Application_Base, "OPBENCH",
        End;
}

/* ---- conditions for run_until ------------------------------------------- */
static LONG want;
static int cond_pressed(void) { return pressed >= want; }
static int cond_strlen(void)
{
    STRPTR s = NULL;

    get(string, MUIA_String_Contents, &s);
    return s && (LONG)strlen(s) == want;
}
static LONG want_w;
static int cond_width(void)
{
    ULONG w = 0;

    get(root2, MUIA_Width, &w);
    return (LONG)w == want_w;
}
static int cond_nowindow(void)
{
    struct Window *w = NULL;

    get(win, MUIA_Window_Window, &w);
    return w == NULL;
}
static int cond_window(void)
{
    struct Window *w = NULL;

    get(win, MUIA_Window_Window, &w);
    return w != NULL;
}
static int cond_open(void)
{
    ULONG o = 0;

    get(win, MUIA_Window_Open, &o);
    return o != 0;
}

/* "opbench prof NAME COUNT": every operation runs 3 times, NAME COUNT times
 * after a pause, for tests/lean/pcsample.c to sample it */
static const char *prof_name;
static int prof_count;

/* Wait (not running: the sampler skips a waiting task) until RAM:opbench.go
 * exists, so the samples hold the profiled operation only */
static void prof_wait(const char *name)
{
    BPTR l;

    printf("profiling %s: waiting for RAM:opbench.go\n", name);
    if ((l = Open("RAM:opbench.ready", MODE_NEWFILE)))
        Close(l);
    while (!(l = Lock("RAM:opbench.go", ACCESS_READ)))
        Delay(5);
    UnLock(l);
}

#define TIMED(name, ops, ...) do { \
        int i_, n_ = reps, keep_ = reps; \
        if (prof_name && !strcmp(prof_name, name)) { \
            n_ = prof_count; prof_wait(name); } \
        timedout = 0; \
        for (i_ = 0; i_ < n_; i_++) { ULONG t_ = now(); __VA_ARGS__; \
            if (i_ < MAXREPS) samples[i_] = now() - t_; } \
        if (n_ < reps) reps = n_; else if (n_ > MAXREPS) reps = MAXREPS; else reps = n_; \
        if (timedout) printf("%-18s timeout\n", name); else report(name, ops); \
        reps = keep_; \
        if (prof_name && !strcmp(prof_name, name)) goto done; \
    } while (0)

static void list_fill(Object *l, const char **arr, int quiet)
{
    if (quiet) set(l, MUIA_List_Quiet, TRUE);
    DoMethod(l, MUIM_List_Clear);
    DoMethod(l, MUIM_List_Insert, arr, -1, MUIV_List_Insert_Bottom);
    if (quiet) set(l, MUIA_List_Quiet, FALSE);
}

int main(int argc, char **argv)
{
    struct timerequest tr;
    ULONG open = 0;
    int i, j, rc = 20, x, y;
    static char buf[40];

    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc > 3 && !strcmp(argv[1], "prof"))
    {
        prof_name = argv[2];
        prof_count = atoi(argv[3]);
        reps = 3;
    }
    else if (argc > 1)
        reps = atoi(argv[1]);
    if (reps < 3) reps = 3;
    if (reps > MAXREPS) reps = MAXREPS;
    for (i = 0; i < NBIG; i++)
    {
        sprintf(bigtext[i], "Entry %04d", i);
        big[i] = bigtext[i];
    }
    for (i = 0; i < NBIG; i++)                      /* a fixed shuffle */
        shuffled[i] = big[(i * 389 + 17) % NBIG];
    big[NBIG] = shuffled[NBIG] = NULL;

    if (OpenDevice(TIMERNAME, UNIT_ECLOCK, (struct IORequest *)&tr, 0))
        return 20;
    TimerBase = tr.tr_node.io_Device;
    tport = CreateMsgPort();
    treq = (struct timerequest *)CreateIORequest(tport, sizeof(*treq));
    inport = CreateMsgPort();
    inreq = (struct IOStdReq *)CreateIORequest(inport, sizeof(*inreq));
    if (!treq || !inreq || OpenDevice(TIMERNAME, UNIT_VBLANK, (struct IORequest *)treq, 0))
        return 20;
    if (OpenDevice("input.device", 0, (struct IORequest *)inreq, 0))
        return 20;
    presshook.h_Entry = (HOOKFUNC)press_func;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        goto nolib;
    printf("library %d.%d eclock=%lu reps=%d\n", MUIMasterBase->lib_Version,
        MUIMasterBase->lib_Revision, (unsigned long)(now(), eclock_freq), reps);
    scr = OpenScreenTags(NULL, SA_Width, 640, SA_Height, 480, SA_Depth, 2,
        SA_DisplayID, HIRESLACE_KEY, SA_Title, "opbench", TAG_DONE);
    if (!scr) goto done;

    /* Warm class caches and prefs before measuring */
    if ((app = build())) MUI_DisposeObject(app);
    if ((app = build_small())) MUI_DisposeObject(app);

    TIMED("prefs+app (empty)", 1, {
        app = build_small(); if (!app) goto done; MUI_DisposeObject(app); });
    app = NULL;
    TIMED("app+windows create", 1, {
        app = build(); if (!app) goto done; MUI_DisposeObject(app); });

    if (!(app = build()))
        goto done;
    DoMethod(slider, MUIM_Notify, MUIA_Numeric_Value, MUIV_EveryTime,
        numeric, 3, MUIM_Set, MUIA_Numeric_Value, MUIV_TriggerValue);
    DoMethod(button, MUIM_Notify, MUIA_Pressed, FALSE,
        status, 3, MUIM_Set, MUIA_Text_Contents, "Pressed");
    DoMethod(button, MUIM_Notify, MUIA_Pressed, FALSE,
        app, 2, MUIM_CallHook, &presshook);
    list_fill(list, entries10, 0);
    for (i = 0; i < 19; i++)
        DoMethod(list, MUIM_List_Insert, entries10, -1, MUIV_List_Insert_Bottom);

    TIMED("window open+close", 1, {
        set(win, MUIA_Window_Open, TRUE);
        get(win, MUIA_Window_Open, &open);
        set(win, MUIA_Window_Open, FALSE); });
    if (!open) goto done;

    set(win, MUIA_Window_Open, TRUE);
    settle();
    {
        struct Window *nw = NULL;

        get(win, MUIA_Window_Window, &nw);
        if (nw)
            printf("window idcmp=%08lx flags=%08lx\n",
                (unsigned long)nw->IDCMPFlags, (unsigned long)nw->Flags);
    }

    TIMED("full redraw", 10, {
        for (j = 0; j < 10; j++) MUI_Redraw(root, MADF_DRAWOBJECT); });
    TIMED("relayout", 5, {
        for (j = 0; j < 5; j++) {
            DoMethod(root, MUIM_Group_InitChange);
            DoMethod(root, MUIM_Group_ExitChange); } });
    TIMED("slider set", 100, {
        for (j = 0; j < 100; j++) set(numeric, MUIA_Numeric_Value, (i_ * 100 + j * 37) % 1000); });
    TIMED("notify chain", 100, {
        for (j = 0; j < 100; j++) set(slider, MUIA_Numeric_Value, (i_ * 100 + j) % 1000); });
    TIMED("cycle set", 50, {
        for (j = 0; j < 50; j++) set(cycle, MUIA_Cycle_Active, (j + 1) % 3); });
    TIMED("radio set", 50, {
        for (j = 0; j < 50; j++) set(radio, MUIA_Radio_Active, (j + 1) % 3); });
    TIMED("text set", 100, {
        for (j = 0; j < 100; j++) { sprintf(buf, "Status %d", j); set(text, MUIA_Text_Contents, buf); } });

    /* Lists: inserts with the list shown, scrolling, sorting */
    TIMED("list insert 1", 50, {
        DoMethod(list, MUIM_List_Clear);
        t_ = now();
        for (j = 0; j < 50; j++)
            DoMethod(list, MUIM_List_Insert, &big[j], 1, MUIV_List_Insert_Bottom); });
    TIMED("list insert 100", 1, {
        DoMethod(list, MUIM_List_Clear);
        t_ = now();
        DoMethod(list, MUIM_List_Insert, big, 100, MUIV_List_Insert_Bottom); });
    TIMED("list insert 1000", 1, {
        DoMethod(list, MUIM_List_Clear);
        t_ = now();
        DoMethod(list, MUIM_List_Insert, big, NBIG, MUIV_List_Insert_Bottom); });
    {
        ULONG count = 0;

        get(list, MUIA_List_Entries, &count);
        printf("list-entries=%lu\n", (unsigned long)count);
    }
    TIMED("list active", 50, {
        for (j = 0; j < 50; j++) set(list, MUIA_List_Active, (j * 7) % 200); });
    set(list, MUIA_List_Active, MUIV_List_Active_Off);
    TIMED("list scroll line", 40, {
        DoMethod(list, MUIM_List_Jump, 0);
        t_ = now();
        for (j = 0; j < 40; j++) {
            ULONG first = 0, vis = 0;
            get(list, MUIA_List_First, &first);
            get(list, MUIA_List_Visible, &vis);
            DoMethod(list, MUIM_List_Jump, first + vis); } });
    TIMED("list scroll page", 20, {
        DoMethod(list, MUIM_List_Jump, 0);
        t_ = now();
        for (j = 0; j < 20; j++) {
            ULONG first = 0, vis = 0;
            get(list, MUIA_List_First, &first);
            get(list, MUIA_List_Visible, &vis);
            DoMethod(list, MUIM_List_Jump, first + 2 * vis - 1); } });
    {
        ULONG first = 0;

        get(list, MUIA_List_First, &first);
        printf("list-first=%lu\n", (unsigned long)first);
    }
    TIMED("list sort 1000", 1, {
        list_fill(list, shuffled, 1);
        t_ = now();
        DoMethod(list, MUIM_List_Sort); });
    {
        STRPTR e = NULL;

        DoMethod(list, MUIM_List_GetEntry, 0, &e);
        printf("list-sorted-first=%s\n", e ? (char *)e : "(null)");
    }

    /* User input through input.device */
    set(win, MUIA_Window_Activate, TRUE);
    settle();
    centre(button, win, &x, &y);
    pointer_to(x, y);
    settle();
    pressed = 0;
    TIMED("button click", 1, {
        want = pressed + 1;
        mouse_button(IECODE_LBUTTON);
        mouse_button(IECODE_LBUTTON | IECODE_UP_PREFIX);
        run_until(cond_pressed); });
    printf("presses=%ld\n", (long)pressed);

    set(win, MUIA_Window_ActiveObject, string);
    settle();
    TIMED("string type", 40, {
        set(string, MUIA_String_Contents, "");
        DoMethod(app, MUIM_Application_InputBuffered);
        t_ = now();
        for (j = 1; j <= 40 && !timedout; j++) {
            want = j;
            key(0x20);                       /* "a" */
            run_until(cond_strlen); } });
    TIMED("string backspace", 20, {
        set(string, MUIA_String_Contents, "abcdefghijklmnopqrstuvwxyz");
        set(string, MUIA_String_BufferPos, 26);
        DoMethod(app, MUIM_Application_InputBuffered);
        t_ = now();
        for (j = 1; j <= 20 && !timedout; j++) {
            want = 26 - j;
            key(0x41);                       /* backspace */
            run_until(cond_strlen); } });
    set(win, MUIA_Window_ActiveObject, MUIV_Window_ActiveObject_None);
    settle();

    /* The menu: right button down over the window, then up */
    centre(root, win, &x, &y);
    pointer_to(x, y);
    settle();
    TIMED("menu open+close", 1, {
        mouse_button(IECODE_RBUTTON);
        mouse_button(IECODE_RBUTTON | IECODE_UP_PREFIX);
        DoMethod(app, MUIM_Application_InputBuffered); });

    /* Iconify and back: the window closes, comes back and is drawn */
    TIMED("iconify", 1, {
        set(app, MUIA_Application_Iconified, FALSE);
        run_until(cond_window);
        settle();
        t_ = now();
        set(app, MUIA_Application_Iconified, TRUE);
        run_until(cond_nowindow);
        DoMethod(app, MUIM_Application_InputBuffered); });
    TIMED("uniconify", 1, {
        set(app, MUIA_Application_Iconified, TRUE);
        run_until(cond_nowindow);
        settle();
        t_ = now();
        set(app, MUIA_Application_Iconified, FALSE);
        run_until(cond_window);
        DoMethod(app, MUIM_Application_InputBuffered); });
    settle();

    /* The second window: register pages, a popup, a resize */
    set(win, MUIA_Window_Open, FALSE);
    set(win2, MUIA_Window_Open, TRUE);
    get(win2, MUIA_Window_Open, &open);
    settle();
    if (open)
    {
        struct Window *nw = NULL;

        TIMED("register page", 30, {
            for (j = 0; j < 30; j++) set(reg, MUIA_Group_ActivePage, (j + 1) % 3); });
        set(reg, MUIA_Group_ActivePage, 0);
        settle();
        TIMED("popup open+close", 1, {
            DoMethod(pop, MUIM_Popstring_Open);
            DoMethod(app, MUIM_Application_InputBuffered);
            DoMethod(pop, MUIM_Popstring_Close, TRUE);
            DoMethod(app, MUIM_Application_InputBuffered); });
        settle();
        get(win2, MUIA_Window_Window, &nw);
        if (nw)
        {
            WORD w0 = nw->Width, h0 = nw->Height;
            ULONG rw0 = 0;

            get(root2, MUIA_Width, &rw0);
            TIMED("window resize", 2, {
                want_w = rw0 + 80;
                ChangeWindowBox(nw, nw->LeftEdge, nw->TopEdge, w0 + 80, h0 + 60);
                run_until(cond_width);
                want_w = rw0;
                ChangeWindowBox(nw, nw->LeftEdge, nw->TopEdge, w0, h0);
                run_until(cond_width); });
        }
        set(win2, MUIA_Window_Open, FALSE);
    }
    else
        puts("window 2 did not open");
    rc = 0;
done:
    if (app)
        MUI_DisposeObject(app);
    if (scr) CloseScreen(scr);
    CloseLibrary(MUIMasterBase);
nolib:
    CloseDevice((struct IORequest *)inreq);
    CloseDevice((struct IORequest *)treq);
    CloseDevice((struct IORequest *)&tr);
    printf("rc=%d\n", rc);
    return rc;
}
