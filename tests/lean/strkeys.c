/* strkeys: the same keystrokes into String gadgets on any muimaster.library
 * (MUI 3.8 or Zune), each step's contents, cursor and notifications
 * printed, so two libraries can be compared line by line (branch strkey:
 * String's direct key path for programs that get MUI 3.8 against the
 * Textinput-method path before it, and against MUI 3.8). The keys go in
 * through input.device as real key presses; the mouse marking as absolute
 * pointer events. Only MUI 3.8 attributes are used. Built by hand:
 *   m68k-amigaos-gcc -m68000 -noixemul -Os -s -o strkeys tests/lean/strkeys.c
 * Run from a Shell on the test machine with its window open; the output
 * is the Shell's. "strkeys mouse" adds the pointer tests (the screen must
 * be the Workbench, unobscured).
 */
#define NO_INLINE_STDARG
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <intuition/sghooks.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

#define MO(name, ...) \
    ({ ULONG tags[] = {__VA_ARGS__}; MUI_NewObjectA(name, (struct TagItem *)tags); })

struct Library *MUIMasterBase;
ULONG __stack = 65536;

static struct IOStdReq *io;
static Object *app, *win;
static ULONG hook_calls;
static LONG acks, changes;

static ULONG edit_hook(struct Hook *hook __asm("a0"),
    struct SGWork *work __asm("a2"), ULONG *command __asm("a1"))
{
    hook_calls++;
    return TRUE;
}

static ULONG count_hook(struct Hook *hook __asm("a0"),
    Object *obj __asm("a2"), ULONG *params __asm("a1"))
{
    if (params[0] == 1)
        acks++;
    else
        changes++;
    return 0;
}

static void pump(void)
{
    int i;
    for (i = 0; i < 4; i++)
    {
        Delay(2);
        DoMethod(app, MUIM_Application_InputBuffered);
    }
}

static void send(struct InputEvent *event)
{
    io->io_Command = IND_WRITEEVENT;
    io->io_Data = event;
    io->io_Length = sizeof(*event);
    DoIO((APTR)io);
}

/* A key pressed and released, with qualifiers */
static void key(ULONG raw, ULONG qual)
{
    struct InputEvent event = {0};

    event.ie_Class = IECLASS_RAWKEY;
    event.ie_Code = raw;
    event.ie_Qualifier = qual;
    send(&event);
    pump();
    event.ie_Code |= IECODE_UP_PREFIX;
    send(&event);
    pump();
}

/* The qualifier keys themselves (Alt marking) */
static void keydown(ULONG raw, ULONG qual)
{
    struct InputEvent event = {0};

    event.ie_Class = IECLASS_RAWKEY;
    event.ie_Code = raw;
    event.ie_Qualifier = qual;
    send(&event);
    pump();
}

static void keyup(ULONG raw, ULONG qual)
{
    keydown(raw | IECODE_UP_PREFIX, qual);
}

static void mouse(UWORD code, UWORD qual, WORD x, WORD y)
{
    struct InputEvent event = {0};

    event.ie_Class = IECLASS_RAWMOUSE;
    event.ie_Code = code;
    event.ie_Qualifier = qual;
    event.ie_X = x;
    event.ie_Y = y;
    send(&event);
    pump();
}

/* The letters a-z on the standard keymaps, digits, and the editing keys */
static const UBYTE letters[26] = {
    0x20, 0x35, 0x33, 0x22, 0x12, 0x23, 0x24, 0x25, 0x17, 0x26, 0x27, 0x28,
    0x37, 0x36, 0x18, 0x19, 0x10, 0x13, 0x21, 0x14, 0x16, 0x34, 0x11, 0x32,
    0x15, 0x31
};
#define K_SPACE 0x40
#define K_BS 0x41
#define K_DEL 0x46
#define K_RETURN 0x44
#define K_UP 0x4C
#define K_DOWN 0x4D
#define K_RIGHT 0x4E
#define K_LEFT 0x4F
#define K_LALT 0x64
#define SHIFT IEQUALIFIER_LSHIFT
#define CTRL IEQUALIFIER_CONTROL
#define ALT IEQUALIFIER_LALT
#define AMIGAKEY IEQUALIFIER_RCOMMAND

static void type(const char *text)
{
    for (; *text; text++)
    {
        if (*text >= 'a' && *text <= 'z')
            key(letters[*text - 'a'], 0);
        else if (*text >= 'A' && *text <= 'Z')
            key(letters[*text - 'A'], SHIFT);
        else if (*text >= '1' && *text <= '9')
            key(*text - '0', 0);
        else if (*text == '0')
            key(0x0a, 0);
        else if (*text == ' ')
            key(K_SPACE, 0);
    }
}

static int step;
static void show(Object *str, const char *what)
{
    STRPTR text = NULL;
    ULONG pos = 0, ack = 0;

    GetAttr(MUIA_String_Contents, str, (ULONG *)&text);
    GetAttr(MUIA_String_BufferPos, str, &pos);
    GetAttr(MUIA_String_Acknowledge, str, &ack);
    printf("%2d %-28s [%s] pos=%lu changes=%ld acks=%ld ack=[%s]\n", ++step,
        what, text ? (char *)text : "NULL", pos, changes, acks,
        ack ? (char *)ack : "");
}

static void activate(Object *str)
{
    SetAttrs(win, MUIA_Window_ActiveObject, str, TAG_DONE);
    pump();
}

int main(int argc, char **argv)
{
    static struct Hook hook, counter;
    Object *plain, *accept, *reject, *integer, *secret, *maxlen, *advance,
        *next, *hooked, *group;
    struct MsgPort *port;
    BOOL with_mouse = argc > 1 && !strcmp(argv[1], "mouse");

    setvbuf(stdout, NULL, _IONBF, 0);
    MUIMasterBase = OpenLibrary("muimaster.library", 0);
    if (!MUIMasterBase)
        return 20;
    hook.h_Entry = (HOOKFUNC)edit_hook;
    counter.h_Entry = (HOOKFUNC)count_hook;
    plain = MO(MUIC_String, MUIA_Frame, MUIV_Frame_String, MUIA_CycleChain, 1,
        MUIA_String_MaxLen, 40, MUIA_String_Contents, (ULONG)"hello world",
        TAG_DONE);
    accept = MO(MUIC_String, MUIA_Frame, MUIV_Frame_String, MUIA_CycleChain, 1,
        MUIA_String_MaxLen, 40, MUIA_String_Accept, (ULONG)"0123456789",
        TAG_DONE);
    reject = MO(MUIC_String, MUIA_Frame, MUIV_Frame_String, MUIA_CycleChain, 1,
        MUIA_String_MaxLen, 40, MUIA_String_Reject, (ULONG)"xX", TAG_DONE);
    integer = MO(MUIC_String, MUIA_Frame, MUIV_Frame_String, MUIA_CycleChain, 1,
        MUIA_String_MaxLen, 40, MUIA_String_Integer, 42, TAG_DONE);
    secret = MO(MUIC_String, MUIA_Frame, MUIV_Frame_String, MUIA_CycleChain, 1,
        MUIA_String_MaxLen, 40, MUIA_String_Secret, TRUE, TAG_DONE);
    maxlen = MO(MUIC_String, MUIA_Frame, MUIV_Frame_String, MUIA_CycleChain, 1,
        MUIA_String_MaxLen, 6, TAG_DONE);
    advance = MO(MUIC_String, MUIA_Frame, MUIV_Frame_String, MUIA_CycleChain, 1,
        MUIA_String_MaxLen, 40, MUIA_String_AdvanceOnCR, TRUE, TAG_DONE);
    next = MO(MUIC_String, MUIA_Frame, MUIV_Frame_String, MUIA_CycleChain, 1,
        MUIA_String_MaxLen, 40, TAG_DONE);
    hooked = MO(MUIC_String, MUIA_Frame, MUIV_Frame_String, MUIA_CycleChain, 1,
        MUIA_String_MaxLen, 40, MUIA_String_EditHook, (ULONG)&hook,
        MUIA_String_Contents, (ULONG)"hook", TAG_DONE);
    group = MO(MUIC_Group, MUIA_Group_Child, (ULONG)plain,
        MUIA_Group_Child, (ULONG)accept, MUIA_Group_Child, (ULONG)reject,
        MUIA_Group_Child, (ULONG)integer, MUIA_Group_Child, (ULONG)secret,
        MUIA_Group_Child, (ULONG)maxlen, MUIA_Group_Child, (ULONG)advance,
        MUIA_Group_Child, (ULONG)next, MUIA_Group_Child, (ULONG)hooked,
        TAG_DONE);
    win = MO(MUIC_Window, MUIA_Window_Title, (ULONG)"strkeys",
        MUIA_Window_LeftEdge, 40, MUIA_Window_TopEdge, 30,
        MUIA_Window_Width, 300, MUIA_Window_RootObject, (ULONG)group,
        TAG_DONE);
    app = MO(MUIC_Application, MUIA_Application_Window, (ULONG)win, TAG_DONE);
    if (!app)
    {
        printf("no application\n");
        return 20;
    }
    DoMethod(plain, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime,
        (ULONG)plain, 3, MUIM_CallHook, (ULONG)&counter, 0);
    DoMethod(plain, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime,
        (ULONG)plain, 3, MUIM_CallHook, (ULONG)&counter, 1);
    SetAttrs(win, MUIA_Window_Open, TRUE, TAG_DONE);
    pump();
    port = CreateMsgPort();
    io = port ? (APTR)CreateIORequest(port, sizeof(*io)) : NULL;
    if (!io || OpenDevice("input.device", 0, (APTR)io, 0))
        return 20;
    printf("library %u.%u\n", MUIMasterBase->lib_Version,
        MUIMasterBase->lib_Revision);

    /* 1. Typing, cursor and deleting in the plain String */
    activate(plain);
    show(plain, "active");
    type("ab");                     show(plain, "type ab at end");
    key(K_LEFT, 0); key(K_LEFT, 0); show(plain, "left left");
    type("Z");                      show(plain, "type Z");
    key(K_BS, 0);                   show(plain, "backspace");
    key(K_DEL, 0);                  show(plain, "del");
    key(K_LEFT, SHIFT);             show(plain, "shift-left (line start)");
    key(K_RIGHT, SHIFT);            show(plain, "shift-right (line end)");
    key(K_LEFT, CTRL);              show(plain, "ctrl-left (word)");
    key(K_LEFT, CTRL);              show(plain, "ctrl-left (word)");
    key(K_RIGHT, CTRL);             show(plain, "ctrl-right (word)");
    key(K_DEL, SHIFT);              show(plain, "shift-del (to end)");
    type("x");                      show(plain, "type x");
    key(K_BS, SHIFT);               show(plain, "shift-backspace (to start)");
    type("one two three");          show(plain, "type one two three");
    key(letters['a' - 'a'], CTRL);  show(plain, "ctrl-a");
    key(letters['z' - 'a'], CTRL);  show(plain, "ctrl-z");
    key(K_LEFT, 0); key(K_LEFT, 0); key(K_LEFT, 0);
    key(letters['k' - 'a'], CTRL);  show(plain, "left x3, ctrl-k");
    key(letters['u' - 'a'], CTRL);  show(plain, "ctrl-u");
    type("abc def");                show(plain, "type abc def");
    key(K_RETURN, 0);               show(plain, "return");
    activate(plain);
    key(K_UP, 0); key(K_DOWN, 0);   show(plain, "up down");
    key(K_LEFT, 0); key(K_LEFT, 0);
    key(letters['x' - 'a'], CTRL);  show(plain, "left x2, ctrl-x");
    type("cut me");                 show(plain, "type cut me");

    /* 2. Marking with the keys (Alt) and the clipboard */
    key(K_LEFT, 0); key(K_LEFT, 0); show(plain, "left x2");
    keydown(K_LALT, ALT);
    key(K_LEFT, ALT); key(K_LEFT, ALT); key(K_LEFT, ALT);
    keyup(K_LALT, 0);               show(plain, "alt + left x3 (mark)");
    key(letters['c' - 'a'], AMIGAKEY); show(plain, "amiga-c (copy)");
    key(K_RIGHT, SHIFT);            show(plain, "shift-right");
    key(letters['v' - 'a'], AMIGAKEY); show(plain, "amiga-v (paste)");
    keydown(K_LALT, ALT);
    key(K_LEFT, ALT); key(K_LEFT, ALT);
    keyup(K_LALT, 0);               show(plain, "alt + left x2 (mark)");
    key(letters['x' - 'a'], AMIGAKEY); show(plain, "amiga-x (cut)");
    keydown(K_LALT, ALT);
    key(K_LEFT, ALT); key(K_LEFT, ALT);
    keyup(K_LALT, 0);
    type("Q");                      show(plain, "alt + left x2, type Q");
    keydown(K_LALT, ALT);
    key(K_LEFT, ALT); key(K_LEFT, ALT);
    keyup(K_LALT, 0);
    key(K_BS, 0);                   show(plain, "alt + left x2, backspace");
    keydown(K_LALT, ALT);
    key(K_LEFT, ALT); key(K_LEFT, ALT);
    keyup(K_LALT, 0);
    key(K_RIGHT, 0);                show(plain, "alt + left x2, right");
    keydown(K_LALT, ALT);
    key(K_LEFT, ALT); key(K_LEFT, ALT);
    keyup(K_LALT, 0);
    key(K_DEL, 0);                  show(plain, "alt + left x2, del");
    key(letters['z' - 'a'], AMIGAKEY); show(plain, "amiga-z");

    /* 3. Accept, Reject, Integer, Secret, MaxLen, AdvanceOnCR, EditHook */
    activate(accept);
    type("a1b2c3");                 show(accept, "accept digits: a1b2c3");
    activate(reject);
    type("axbxc");                  show(reject, "reject x: axbxc");
    activate(integer);
    show(integer, "integer 42");
    key(K_BS, 0); type("7");        show(integer, "backspace, 7");
    key(K_RETURN, 0);
    {
        ULONG value = 0;
        GetAttr(MUIA_String_Integer, integer, &value);
        printf("   integer value %lu\n", value);
    }
    activate(secret);
    type("pass"); key(K_LEFT, 0); key(K_BS, 0);
                                    show(secret, "secret: pass, left, bs");
    activate(maxlen);
    type("abcdefgh");               show(maxlen, "maxlen 6: abcdefgh");
    key(K_LEFT, 0); type("x");      show(maxlen, "maxlen 6: left, x");
    activate(advance);
    type("go"); key(K_RETURN, 0);
    {
        ULONG active = 0;
        GetAttr(MUIA_Window_ActiveObject, win, &active);
        printf("   advance on CR: active is %s\n", active == (ULONG)next ?
            "the next String" : active == (ULONG)advance ? "the same" :
            active ? "another object" : "none");
    }
    show(advance, "advance: go, return");
    activate(hooked);
    hook_calls = 0;
    type("ab"); key(K_BS, 0); key(K_LEFT, 0); key(K_RETURN, 0);
    show(hooked, "hook: ab, bs, left, return");
    printf("   edit hook called %lu times\n", hook_calls);

    /* 4. The mouse: a click sets the cursor, a drag marks */
    if (with_mouse)
    {
        struct Window *w = NULL;

        GetAttr(MUIA_Window_Window, win, (ULONG *)&w);
        WORD x0 = w->LeftEdge + _mleft(plain), y = w->TopEdge
            + (_mtop(plain) + _mbottom(plain)) / 2;
        WORD cw = _font(plain)->tf_XSize;

        activate(plain);
        SetAttrs(plain, MUIA_String_Contents, (ULONG)"click and drag",
            TAG_DONE);
        pump();
        mouse(IECODE_NOBUTTON, 0, x0 + cw * 3 + 1, y);
        mouse(IECODE_LBUTTON, 0, x0 + cw * 3 + 1, y);
        mouse(IECODE_LBUTTON | IECODE_UP_PREFIX, IEQUALIFIER_LEFTBUTTON,
            x0 + cw * 3 + 1, y);
        show(plain, "click at char 3");
        type("X");                  show(plain, "type X");
        mouse(IECODE_NOBUTTON, 0, x0 + cw * 2 + 1, y);
        mouse(IECODE_LBUTTON, 0, x0 + cw * 2 + 1, y);
        mouse(IECODE_NOBUTTON, IEQUALIFIER_LEFTBUTTON, x0 + cw * 5 + 1, y);
        mouse(IECODE_NOBUTTON, IEQUALIFIER_LEFTBUTTON, x0 + cw * 8 + 1, y);
        mouse(IECODE_LBUTTON | IECODE_UP_PREFIX, IEQUALIFIER_LEFTBUTTON,
            x0 + cw * 8 + 1, y);
        show(plain, "drag chars 2 to 8");
        type("Y");                  show(plain, "type Y over the mark");
        mouse(IECODE_NOBUTTON, 0, x0 + cw * 1 + 1, y);
        mouse(IECODE_LBUTTON, 0, x0 + cw * 1 + 1, y);
        mouse(IECODE_NOBUTTON, IEQUALIFIER_LEFTBUTTON, x0 + cw * 4 + 1, y);
        mouse(IECODE_LBUTTON | IECODE_UP_PREFIX, IEQUALIFIER_LEFTBUTTON,
            x0 + cw * 4 + 1, y);
        show(plain, "drag chars 1 to 4");
        key(K_BS, 0);               show(plain, "backspace over the mark");
        mouse(IECODE_NOBUTTON, 0, x0 + cw * 1 + 1, y);
        mouse(IECODE_LBUTTON, 0, x0 + cw * 1 + 1, y);
        mouse(IECODE_NOBUTTON, IEQUALIFIER_LEFTBUTTON, x0 + cw * 4 + 1, y);
        mouse(IECODE_LBUTTON | IECODE_UP_PREFIX, IEQUALIFIER_LEFTBUTTON,
            x0 + cw * 4 + 1, y);
        show(plain, "drag chars 1 to 4 again");
        key(K_LEFT, 0);             show(plain, "left off the mark");
        /* a screenshot now shows the mark: the file RAM:strkeys-shot says */
        {
            BPTR f = Open("RAM:strkeys-shot", MODE_NEWFILE);
            if (f)
                Close(f);
        }
        Delay(400);
    }

    CloseDevice((APTR)io);
    DeleteIORequest((APTR)io);
    DeleteMsgPort(port);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("done\n");
    return 0;
}
