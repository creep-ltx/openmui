/* drawcost: what the OS charges for the drawing a MUI window does, on the
 * screen opbench uses (640x480, depth 2, hires interlace).
 *
 * No MUI: plain intuition, graphics and layers calls, each timed with the
 * EClock (emulated time on an emulator), N calls per line, printed as
 * microseconds per call. What it answers (round `draw1`):
 *   - a window's opening with the layer's default backfill (cleared to pen 0
 *     inside OpenWindowTagList), with LAYERS_NOBACKFILL, and with a hook that
 *     paints a pen: what OpenWindowTagList and CloseWindow cost each way
 *   - one RectFill over the whole inside of such a window
 *   - one-pixel lines: RectFill and Move/Draw, vertical and horizontal
 *   - Text: JAM1 and JAM2, with and without a clip region installed
 *   - the cost of repeated SetAPen/SetDrMd/SetFont/SetSoftStyle, of
 *     InstallClipRegion, InitRastPort, TextLength, TextExtent, TextFit
 *
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os tests/native/drawcost.c \
 *          -o drawcost -lamiga
 * Run:   drawcost [N]     (default N 50)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <devices/timer.h>
#include <intuition/intuition.h>
#include <graphics/gfxmacros.h>
#include <graphics/modeid.h>
#include <graphics/regions.h>
#include <graphics/layers.h>
#include <graphics/rpattr.h>
#include <utility/hooks.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/layers.h>
#include <proto/timer.h>

struct Device *TimerBase;
ULONG __stack = 32768;
static ULONG freq;
static int N = 50;

static ULONG now(void)
{
    struct EClockVal ev;

    freq = ReadEClock(&ev);
    return ev.ev_lo;
}

static void report(const char *what, ULONG ticks, int n)
{
    /* microseconds per call, one decimal */
    ULONG us10 = (ULONG)(((unsigned long long)ticks * 10000000ULL) / freq / n);

    printf("%-34s %6lu.%lu us\n", what, (unsigned long)(us10 / 10),
        (unsigned long)(us10 % 10));
}

struct bfmsg { struct Layer *layer; struct Rectangle bounds; LONG ox, oy; };

static ULONG pen_hook_func(struct Hook *h __asm("a0"), struct RastPort *rp __asm("a2"),
    struct bfmsg *m __asm("a1"))
{
    struct RastPort r = *rp;

    r.Layer = NULL;
    SetAPen(&r, (ULONG)h->h_Data);
    SetDrMd(&r, JAM2);
    RectFill(&r, m->bounds.MinX, m->bounds.MinY, m->bounds.MaxX, m->bounds.MaxY);
    return 0;
}
static struct Hook pen_hook = { { 0 }, (HOOKFUNC)pen_hook_func, 0, (APTR)2 };

static struct Window *openwin(struct Screen *scr, ULONG bftag, APTR bf)
{
    return OpenWindowTags(NULL, WA_Left, 20, WA_Top, 20, WA_InnerWidth, 600,
        WA_InnerHeight, 400, WA_Title, (ULONG)"drawcost",
        WA_CustomScreen, (ULONG)scr, WA_SizeGadget, TRUE, WA_DragBar, TRUE,
        WA_DepthGadget, TRUE, WA_CloseGadget, TRUE, WA_Activate, TRUE,
        WA_SmartRefresh, TRUE, WA_NewLookMenus, TRUE, bftag, (ULONG)bf,
        TAG_DONE);
}

int main(int argc, char **argv)
{
    struct timerequest tr;
    struct Screen *scr;
    struct Window *w;
    struct RastPort *rp, tmp;
    struct Region *reg;
    struct TextExtent te;
    struct TextFont *f;
    ULONG t;
    int i, k;
    static const char s9[] = "Entry one";
    static const char s5[] = "Value";

    if (argc > 1) N = atoi(argv[1]);
    if (N < 1) N = 1;
    if (OpenDevice(TIMERNAME, UNIT_ECLOCK, (struct IORequest *)&tr, 0))
        return 20;
    TimerBase = tr.tr_node.io_Device;
    scr = OpenScreenTags(NULL, SA_Width, 640, SA_Height, 480, SA_Depth, 2,
        SA_DisplayID, HIRESLACE_KEY, SA_Title, (ULONG)"drawcost", TAG_DONE);
    if (!scr)
        return 20;
    /* settle the screen */
    Delay(25);
    printf("N=%d\n", N);

    /* Opening and closing, three ways (N/10 times each) */
    for (k = 0; k < 3; k++)
    {
        ULONG to = 0, tc = 0, tf = 0;
        int n = N / 10 < 3 ? 3 : N / 10;
        ULONG tag = k == 0 ? TAG_IGNORE : WA_BackFill;
        APTR bf = k == 1 ? (APTR)LAYERS_NOBACKFILL : (APTR)&pen_hook;
        static const char *nm[] = { "default backfill", "LAYERS_NOBACKFILL",
            "pen hook" };
        char line[64];

        for (i = 0; i < n; i++)
        {
            t = now();
            w = openwin(scr, tag, bf);
            to += now() - t;
            if (!w)
                return 20;
            t = now();
            SetAPen(w->RPort, 2);
            RectFill(w->RPort, w->BorderLeft, w->BorderTop,
                w->Width - w->BorderRight - 1, w->Height - w->BorderBottom - 1);
            tf += now() - t;
            t = now();
            CloseWindow(w);
            tc += now() - t;
        }
        sprintf(line, "open, %s", nm[k]);
        report(line, to, n);
        sprintf(line, "close, %s", nm[k]);
        report(line, tc, n);
        sprintf(line, "fill inside 600x400 (%s)", nm[k]);
        report(line, tf, n);
    }

    w = openwin(scr, TAG_IGNORE, NULL);
    if (!w)
        return 20;
    rp = w->RPort;
    SetAPen(rp, 1);
    SetDrMd(rp, JAM1);

    t = now();
    for (i = 0; i < N; i++)
        RectFill(rp, 100, 50, 100, 69);
    report("vline 20 RectFill", now() - t, N);
    t = now();
    for (i = 0; i < N; i++) { Move(rp, 102, 50); Draw(rp, 102, 69); }
    report("vline 20 Move+Draw", now() - t, N);
    t = now();
    for (i = 0; i < N; i++)
        RectFill(rp, 110, 50, 209, 50);
    report("hline 100 RectFill", now() - t, N);
    t = now();
    for (i = 0; i < N; i++) { Move(rp, 110, 52); Draw(rp, 209, 52); }
    report("hline 100 Move+Draw", now() - t, N);
    t = now();
    for (i = 0; i < N; i++)
        RectFill(rp, 110, 60, 209, 79);
    report("box 100x20 RectFill", now() - t, N);
    t = now();
    for (i = 0; i < N; i++)
        RectFill(rp, 110, 60, 409, 159);
    report("box 300x100 RectFill", now() - t, N);
    t = now();
    for (i = 0; i < N; i++)
        WritePixel(rp, 300, 30);
    report("WritePixel", now() - t, N);

    t = now();
    for (i = 0; i < N; i++)
        SetAPen(rp, 1);
    report("SetAPen same", now() - t, N);
    t = now();
    for (i = 0; i < N; i++)
        SetAPen(rp, i & 1 ? 1 : 2);
    report("SetAPen change", now() - t, N);
    t = now();
    for (i = 0; i < N; i++)
        SetDrMd(rp, JAM1);
    report("SetDrMd same", now() - t, N);
    f = rp->Font;
    t = now();
    for (i = 0; i < N; i++)
        SetFont(rp, f);
    report("SetFont same", now() - t, N);
    t = now();
    for (i = 0; i < N; i++)
        SetSoftStyle(rp, 0, AskSoftStyle(rp));
    report("SetSoftStyle 0", now() - t, N);
    t = now();
    for (i = 0; i < N; i++)
        InitRastPort(&tmp);
    report("InitRastPort", now() - t, N);
    t = now();
    for (i = 0; i < N; i++)
        TextLength(rp, s9, 9);
    report("TextLength 9", now() - t, N);
    t = now();
    for (i = 0; i < N; i++)
        TextExtent(rp, s9, 9, &te);
    report("TextExtent 9", now() - t, N);
    t = now();
    for (i = 0; i < N; i++)
        TextFit(rp, s9, 9, &te, NULL, 1, 200, 20);
    report("TextFit 9", now() - t, N);

    SetAPen(rp, 1);
    SetDrMd(rp, JAM1);
    t = now();
    for (i = 0; i < N; i++) { Move(rp, 100, 200); Text(rp, s9, 9); }
    report("Text 9 JAM1", now() - t, N);
    t = now();
    for (i = 0; i < N; i++) { Move(rp, 100, 220); Text(rp, s5, 5); }
    report("Text 5 JAM1", now() - t, N);
    SetBPen(rp, 0);
    SetDrMd(rp, JAM2);
    t = now();
    for (i = 0; i < N; i++) { Move(rp, 100, 240); Text(rp, s9, 9); }
    report("Text 9 JAM2", now() - t, N);
    SetDrMd(rp, JAM1);
    SetSoftStyle(rp, FSF_BOLD, FSF_BOLD);
    t = now();
    for (i = 0; i < N; i++) { Move(rp, 100, 260); Text(rp, s9, 9); }
    report("Text 9 JAM1 bold (algo)", now() - t, N);
    SetSoftStyle(rp, 0, FSF_BOLD);
    rp->Mask = 0x03;
    t = now();
    for (i = 0; i < N; i++) { Move(rp, 100, 280); Text(rp, s9, 9); }
    report("Text 9 JAM1 mask 3", now() - t, N);
    rp->Mask = 0xff;

    reg = NewRegion();
    if (reg)
    {
        struct Rectangle r = { 20, 20, 500, 300 };
        struct Region *old;

        OrRectRegion(reg, &r);
        t = now();
        for (i = 0; i < N; i++)
        {
            old = InstallClipRegion(w->WLayer, reg);
            InstallClipRegion(w->WLayer, old);
        }
        report("InstallClipRegion on+off (pair)", now() - t, N);
        old = InstallClipRegion(w->WLayer, reg);
        t = now();
        for (i = 0; i < N; i++) { Move(rp, 100, 300); Text(rp, s9, 9); }
        report("Text 9 JAM1, clip installed", now() - t, N);
        t = now();
        for (i = 0; i < N; i++)
            RectFill(rp, 110, 60, 209, 79);
        report("box 100x20 RectFill, clip", now() - t, N);
        InstallClipRegion(w->WLayer, old);
        DisposeRegion(reg);
    }
    t = now();
    for (i = 0; i < N; i++)
    {
        LockLayerInfo(&scr->LayerInfo);
        UnlockLayerInfo(&scr->LayerInfo);
    }
    report("LockLayerInfo+Unlock", now() - t, N);

    CloseWindow(w);
    CloseScreen(scr);
    CloseDevice((struct IORequest *)&tr);
    return 0;
}
