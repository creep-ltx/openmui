/* syscount.h: count the operating system calls a MUI library makes, black box.
 *
 * Included by opbench.c ("opbench count"). Each function in the table below is
 * patched with SetFunction: a 12-byte entry stub (move.l #index,-(sp); jmp
 * sc_common) per function, generated at run time. sc_common counts a call when
 * counting is on (sc_on) and the caller is this task:
 *
 *   total - every call, also those an OS function makes through a library
 *           vector while it runs for us (AllocVec -> AllocMem on OS 3.1,
 *           intuition's gadget drawing -> RectFill, ...)
 *   top   - only calls made while no other counted function runs: the calls
 *           the program and the MUI library ask for themselves
 *   time  - EClock ticks spent inside top-level calls (timer.device's
 *           ReadEClock before and after; "opbench count time" only, since the
 *           two reads add to every call)
 *
 * The BOOPSI entries (NewObjectA, DisposeObject, SetAttrsA, GetAttr,
 * SetGadgetAttrsA, DoGadgetMethodA) and CallHookPkt only call back into the
 * classes (opbench's own set() is a SetAttrsA): they are counted, and what
 * runs inside them still counts as top level.
 *
 * A top-level call's return address is replaced by sc_tramp, which takes the
 * time, clears the depth and returns to the caller. Functions an interrupt
 * may call (Signal, PutMsg, ReplyMsg, Cause) are not patched: the task test
 * would take an interrupt for the task. The vectors are restored at the end.
 */
#include <exec/execbase.h>
#include <exec/memory.h>
#include <graphics/gfxbase.h>

struct sc_fn { UBYTE lib; WORD lvo; const char *name; };

enum { SC_EXEC, SC_GFX, SC_LAYERS, SC_INTUI, SC_UTIL, SC_KEYMAP, SC_CX, SC_NLIBS };
static const char *sc_libnames[SC_NLIBS] = {
    NULL, "graphics.library", "layers.library", "intuition.library",
    "utility.library", "keymap.library", "commodities.library"
};

static const struct sc_fn sc_fns[] = {
    /* graphics: text */
    { SC_GFX, -60, "Text" }, { SC_GFX, -54, "TextLength" },
    { SC_GFX, -690, "TextExtent" }, { SC_GFX, -696, "TextFit" },
    { SC_GFX, -66, "SetFont" }, { SC_GFX, -72, "OpenFont" },
    { SC_GFX, -78, "CloseFont" }, { SC_GFX, -90, "SetSoftStyle" },
    { SC_GFX, -84, "AskSoftStyle" },
    /* graphics: drawing */
    { SC_GFX, -240, "Move" }, { SC_GFX, -246, "Draw" },
    { SC_GFX, -336, "PolyDraw" }, { SC_GFX, -306, "RectFill" },
    { SC_GFX, -810, "EraseRect" },
    { SC_GFX, -324, "WritePixel" }, { SC_GFX, -318, "ReadPixel" },
    { SC_GFX, -234, "SetRast" }, { SC_GFX, -180, "DrawEllipse" },
    { SC_GFX, -252, "AreaMove" }, { SC_GFX, -258, "AreaDraw" },
    { SC_GFX, -264, "AreaEnd" },
    { SC_GFX, -1056, "WriteChunkyPixels" },
    /* graphics: pens and modes */
    { SC_GFX, -342, "SetAPen" }, { SC_GFX, -348, "SetBPen" },
    { SC_GFX, -354, "SetDrMd" }, { SC_GFX, -894, "SetABPenDrMd" },
    { SC_GFX, -1038, "SetRPAttrsA" }, { SC_GFX, -1044, "GetRPAttrsA" },
    { SC_GFX, -984, "SetWriteMask" }, { SC_GFX, -978, "SetOutlinePen" },
    { SC_GFX, -954, "ObtainPen" }, { SC_GFX, -948, "ReleasePen" },
    { SC_GFX, -840, "ObtainBestPenA" }, { SC_GFX, -852, "SetRGB32" },
    { SC_GFX, -198, "InitRastPort" },
    /* graphics: blits and scrolling */
    { SC_GFX, -396, "ScrollRaster" }, { SC_GFX, -1002, "ScrollRasterBF" },
    { SC_GFX, -36, "BltTemplate" }, { SC_GFX, -312, "BltPattern" },
    { SC_GFX, -30, "BltBitMap" }, { SC_GFX, -606, "BltBitMapRastPort" },
    { SC_GFX, -636, "BltMaskBitMapRastPort" }, { SC_GFX, -552, "ClipBlit" },
    { SC_GFX, -300, "BltClear" }, { SC_GFX, -228, "WaitBlit" },
    { SC_GFX, -918, "AllocBitMap" }, { SC_GFX, -924, "FreeBitMap" },
    { SC_GFX, -492, "AllocRaster" }, { SC_GFX, -498, "FreeRaster" },
    { SC_GFX, -270, "WaitTOF" },
    /* graphics: regions and layer locks */
    { SC_GFX, -516, "NewRegion" }, { SC_GFX, -534, "DisposeRegion" },
    { SC_GFX, -510, "OrRectRegion" }, { SC_GFX, -504, "AndRectRegion" },
    { SC_GFX, -522, "ClearRectRegion" }, { SC_GFX, -558, "XorRectRegion" },
    { SC_GFX, -612, "OrRegionRegion" }, { SC_GFX, -624, "AndRegionRegion" },
    { SC_GFX, -528, "ClearRegion" },
    { SC_GFX, -432, "LockLayerRom" }, { SC_GFX, -438, "UnlockLayerRom" },
    /* layers */
    { SC_LAYERS, -174, "InstallClipRegion" },
    { SC_LAYERS, -120, "LockLayerInfo" }, { SC_LAYERS, -138, "UnlockLayerInfo" },
    { SC_LAYERS, -96, "LockLayer" }, { SC_LAYERS, -102, "UnlockLayer" },
    { SC_LAYERS, -78, "BeginUpdate" }, { SC_LAYERS, -84, "EndUpdate" },
    { SC_LAYERS, -216, "DoHookClipRects" },
    /* intuition */
    { SC_INTUI, -354, "BeginRefresh" }, { SC_INTUI, -366, "EndRefresh" },
    { SC_INTUI, -114, "DrawImage" }, { SC_INTUI, -618, "DrawImageState" },
    { SC_INTUI, -630, "EraseImage" }, { SC_INTUI, -108, "DrawBorder" },
    { SC_INTUI, -216, "PrintIText" }, { SC_INTUI, -330, "IntuiTextLength" },
    { SC_INTUI, -432, "RefreshGList" }, { SC_INTUI, -660, "SetGadgetAttrsA" },
    { SC_INTUI, -636, "NewObjectA" }, { SC_INTUI, -642, "DisposeObject" },
    { SC_INTUI, -648, "SetAttrsA" }, { SC_INTUI, -654, "GetAttr" },
    { SC_INTUI, -810, "DoGadgetMethodA" },
    { SC_INTUI, -558, "ObtainGIRPort" }, { SC_INTUI, -564, "ReleaseGIRPort" },
    { SC_INTUI, -606, "OpenWindowTagList" }, { SC_INTUI, -72, "CloseWindow" },
    { SC_INTUI, -150, "ModifyIDCMP" }, { SC_INTUI, -816, "SetWindowPointerA" },
    { SC_INTUI, -450, "ActivateWindow" }, { SC_INTUI, -276, "SetWindowTitles" },
    { SC_INTUI, -318, "WindowLimits" }, { SC_INTUI, -486, "ChangeWindowBox" },
    { SC_INTUI, -438, "AddGList" }, { SC_INTUI, -444, "RemoveGList" },
    { SC_INTUI, -510, "LockPubScreen" }, { SC_INTUI, -516, "UnlockPubScreen" },
    { SC_INTUI, -678, "MakeClass" }, { SC_INTUI, -714, "FreeClass" },
    { SC_INTUI, -264, "SetMenuStrip" }, { SC_INTUI, -54, "ClearMenuStrip" },
    { SC_INTUI, -702, "ResetMenuStrip" },
    { SC_INTUI, -156, "ModifyProp" }, { SC_INTUI, -468, "NewModifyProp" },
    { SC_INTUI, -690, "GetScreenDrawInfo" },
    { SC_INTUI, -696, "FreeScreenDrawInfo" },
    { SC_INTUI, -84, "CurrentTime" },
    { SC_INTUI, -414, "LockIBase" }, { SC_INTUI, -420, "UnlockIBase" },
#ifdef SC_ALLINTUI
    /* every other intuition function (-DSC_ALLINTUI): which one makes a
     * call the program does not make itself (draw1: WaitTOF) */
    { SC_INTUI, -42, "AddGadget" },
    { SC_INTUI, -48, "ClearDMRequest" },
    { SC_INTUI, -60, "ClearPointer" },
    { SC_INTUI, -66, "CloseScreen" },
    { SC_INTUI, -78, "CloseWorkBench" },
    { SC_INTUI, -90, "DisplayAlert" },
    { SC_INTUI, -96, "DisplayBeep" },
    { SC_INTUI, -102, "DoubleClick" },
    { SC_INTUI, -120, "EndRequest" },
    { SC_INTUI, -126, "GetDefPrefs" },
    { SC_INTUI, -132, "GetPrefs" },
    { SC_INTUI, -138, "InitRequester" },
    { SC_INTUI, -144, "ItemAddress" },
    { SC_INTUI, -162, "MoveScreen" },
    { SC_INTUI, -168, "MoveWindow" },
    { SC_INTUI, -174, "OffGadget" },
    { SC_INTUI, -180, "OffMenu" },
    { SC_INTUI, -186, "OnGadget" },
    { SC_INTUI, -192, "OnMenu" },
    { SC_INTUI, -198, "OpenScreen" },
    { SC_INTUI, -204, "OpenWindow" },
    { SC_INTUI, -210, "OpenWorkBench" },
    { SC_INTUI, -222, "RefreshGadgets" },
    { SC_INTUI, -228, "RemoveGadget" },
    { SC_INTUI, -234, "ReportMouse" },
    { SC_INTUI, -240, "Request" },
    { SC_INTUI, -246, "ScreenToBack" },
    { SC_INTUI, -252, "ScreenToFront" },
    { SC_INTUI, -258, "SetDMRequest" },
    { SC_INTUI, -270, "SetPointer" },
    { SC_INTUI, -282, "ShowTitle" },
    { SC_INTUI, -288, "SizeWindow" },
    { SC_INTUI, -294, "ViewAddress" },
    { SC_INTUI, -300, "ViewPortAddress" },
    { SC_INTUI, -306, "WindowToBack" },
    { SC_INTUI, -312, "WindowToFront" },
    { SC_INTUI, -324, "SetPrefs" },
    { SC_INTUI, -336, "WBenchToBack" },
    { SC_INTUI, -342, "WBenchToFront" },
    { SC_INTUI, -348, "AutoRequest" },
    { SC_INTUI, -360, "BuildSysRequest" },
    { SC_INTUI, -372, "FreeSysRequest" },
    { SC_INTUI, -378, "MakeScreen" },
    { SC_INTUI, -384, "RemakeDisplay" },
    { SC_INTUI, -390, "RethinkDisplay" },
    { SC_INTUI, -396, "AllocRemember" },
    { SC_INTUI, -402, "AlohaWorkbench" },
    { SC_INTUI, -408, "FreeRemember" },
    { SC_INTUI, -426, "GetScreenData" },
    { SC_INTUI, -456, "RefreshWindowFrame" },
    { SC_INTUI, -462, "ActivateGadget" },
    { SC_INTUI, -474, "QueryOverscan" },
    { SC_INTUI, -480, "MoveWindowInFrontOf" },
    { SC_INTUI, -492, "SetEditHook" },
    { SC_INTUI, -498, "SetMouseQueue" },
    { SC_INTUI, -504, "ZipWindow" },
    { SC_INTUI, -522, "LockPubScreenList" },
    { SC_INTUI, -528, "UnlockPubScreenList" },
    { SC_INTUI, -534, "NextPubScreen" },
    { SC_INTUI, -540, "SetDefaultPubScreen" },
    { SC_INTUI, -546, "SetPubScreenModes" },
    { SC_INTUI, -552, "PubScreenStatus" },
    { SC_INTUI, -570, "GadgetMouse" },
    { SC_INTUI, -576, "intuitionPrivate1" },
    { SC_INTUI, -582, "GetDefaultPubScreen" },
    { SC_INTUI, -588, "EasyRequestArgs" },
    { SC_INTUI, -594, "BuildEasyRequestArgs" },
    { SC_INTUI, -600, "SysReqHandler" },
    { SC_INTUI, -612, "OpenScreenTagList" },
    { SC_INTUI, -624, "PointInImage" },
    { SC_INTUI, -666, "NextObject" },
    { SC_INTUI, -672, "intuitionPrivate2" },
    { SC_INTUI, -684, "AddClass" },
    { SC_INTUI, -708, "RemoveClass" },
    { SC_INTUI, -720, "intuitionPrivate3" },
    { SC_INTUI, -726, "intuitionPrivate4" },
    { SC_INTUI, -768, "AllocScreenBuffer" },
    { SC_INTUI, -774, "FreeScreenBuffer" },
    { SC_INTUI, -780, "ChangeScreenBuffer" },
    { SC_INTUI, -786, "ScreenDepth" },
    { SC_INTUI, -792, "ScreenPosition" },
    { SC_INTUI, -798, "ScrollWindowRaster" },
    { SC_INTUI, -804, "LendMenus" },
    { SC_INTUI, -822, "TimedDisplayAlert" },
    { SC_INTUI, -828, "HelpControl" },
    { SC_INTUI, -834, "ShowWindow" },
    { SC_INTUI, -840, "HideWindow" },
#endif
    /* exec */
    { SC_EXEC, -198, "AllocMem" }, { SC_EXEC, -210, "FreeMem" },
    { SC_EXEC, -684, "AllocVec" }, { SC_EXEC, -690, "FreeVec" },
    { SC_EXEC, -708, "AllocPooled" }, { SC_EXEC, -714, "FreePooled" },
    { SC_EXEC, -696, "CreatePool" }, { SC_EXEC, -702, "DeletePool" },
    { SC_EXEC, -564, "ObtainSemaphore" }, { SC_EXEC, -570, "ReleaseSemaphore" },
    { SC_EXEC, -372, "GetMsg" }, { SC_EXEC, -318, "Wait" },
    { SC_EXEC, -306, "SetSignal" }, { SC_EXEC, -294, "FindTask" },
    { SC_EXEC, -552, "OpenLibrary" }, { SC_EXEC, -414, "CloseLibrary" },
    { SC_EXEC, -624, "CopyMem" }, { SC_EXEC, -456, "DoIO" },
    { SC_EXEC, -462, "SendIO" },
    /* utility, keymap, commodities */
    { SC_UTIL, -30, "FindTagItem" }, { SC_UTIL, -36, "GetTagData" },
    { SC_UTIL, -48, "NextTagItem" }, { SC_UTIL, -102, "CallHookPkt" },
    { SC_KEYMAP, -42, "MapRawKey" }, { SC_CX, -204, "MatchIX" },
};
#define SC_N (sizeof(sc_fns) / sizeof(sc_fns[0]))

/* Read by the stubs by name */
struct Task *sc_me;
volatile UBYTE sc_on, sc_timing;
volatile ULONG sc_depth, sc_cur, sc_ret;
struct EClockVal sc_t0, sc_t1;
ULONG sc_old[SC_N], sc_total[SC_N], sc_top[SC_N], sc_time[SC_N], sc_pass[SC_N];
static ULONG sc_atotal[SC_N], sc_atop[SC_N], sc_atime[SC_N];
static struct Library *sc_bases[SC_NLIBS];
static UWORD *sc_stubs;
static int sc_active;

void sc_common(void);
void sc_tramp(void);
asm(
    "\t.text\n\t.even\n"
    "_sc_common:\n"
    "\tmovem.l %d0/%a0,-(%sp)\n"          /* sp: d0 a0 index return */
    "\ttst.b _sc_on\n"
    "\tbeq.w 9f\n"
    "\tmove.l 4:w,%a0\n"
    "\tmove.l 276(%a0),%a0\n"              /* ExecBase->ThisTask */
    "\tcmp.l _sc_me,%a0\n"
    "\tbne.w 9f\n"
    "\tmove.l 8(%sp),%d0\n"
    "\tlsl.l #2,%d0\n"
    "\tlea _sc_total,%a0\n"
    "\taddq.l #1,0(%a0,%d0.l)\n"
    "\ttst.l _sc_depth\n"
    "\tbne.w 9f\n"
    "\tlea _sc_top,%a0\n"
    "\taddq.l #1,0(%a0,%d0.l)\n"
    "\tcmp.l _sc_rectidx,%d0\n"          /* RectFill: the pixels it fills */
    "\tbne.s 3f\n"
    "\tmovem.l %d1-%d3/%a1,-(%sp)\n"      /* the caller's d0 is at 16(sp) */
    "\tmove.l %d3,-(%sp)\n"
    "\tmove.l %d2,-(%sp)\n"
    "\tmove.l %d1,-(%sp)\n"
    "\tmove.l 28(%sp),-(%sp)\n"
    "\tjsr _sc_rect\n"
    "\tlea 16(%sp),%sp\n"
    "\tmovem.l (%sp)+,%d1-%d3/%a1\n"
    "\tmove.l 8(%sp),%d0\n"
    "\tlsl.l #2,%d0\n"
    "3:\tcmp.l _sc_who,%d0\n"            /* "who=NAME": record the callers */
    "\tbne.s 2f\n"
    "\tmovem.l %d1/%a1,-(%sp)\n"
    "\tpea 20(%sp)\n"                     /* the return address */
    "\tjsr _sc_record\n"
    "\taddq.l #4,%sp\n"
    "\tmovem.l (%sp)+,%d1/%a1\n"
    "\tmove.l 8(%sp),%d0\n"
    "\tlsl.l #2,%d0\n"
    "2:\tlea _sc_pass,%a0\n"             /* a call back into MUI's classes: */
    "\ttst.l 0(%a0,%d0.l)\n"             /* what it calls counts as top too */
    "\tbne.w 9f\n"
    "\tmove.l %d0,_sc_cur\n"
    "\tmove.l 12(%sp),_sc_ret\n"
    "\tmove.l #_sc_tramp,12(%sp)\n"
    "\tmove.l #1,_sc_depth\n"
    "\ttst.b _sc_timing\n"
    "\tbeq.w 9f\n"
    "\tmovem.l %d1/%a1/%a6,-(%sp)\n"
    "\tlea _sc_t0,%a0\n"
    "\tmove.l _TimerBase,%a6\n"
    "\tjsr -60(%a6)\n"                     /* ReadEClock */
    "\tmovem.l (%sp)+,%d1/%a1/%a6\n"
    "9:\tmove.l 8(%sp),%d0\n"
    "\tlsl.l #2,%d0\n"
    "\tlea _sc_old,%a0\n"
    "\tmove.l 0(%a0,%d0.l),8(%sp)\n"     /* the index slot -> the old function */
    "\tmovem.l (%sp)+,%d0/%a0\n"
    "\trts\n"
    "_sc_tramp:\n"
    "\tsubq.l #4,%sp\n"
    "\tmovem.l %d0-%d1/%a0-%a1/%a6,-(%sp)\n"
    "\ttst.b _sc_timing\n"
    "\tbeq.s 1f\n"
    "\tlea _sc_t1,%a0\n"
    "\tmove.l _TimerBase,%a6\n"
    "\tjsr -60(%a6)\n"
    "\tmove.l _sc_t1+4,%d0\n"
    "\tsub.l _sc_t0+4,%d0\n"
    "\tmove.l _sc_cur,%d1\n"
    "\tlea _sc_time,%a0\n"
    "\tadd.l %d0,0(%a0,%d1.l)\n"
    "1:\tclr.l _sc_depth\n"
    "\tmove.l _sc_ret,20(%sp)\n"
    "\tmovem.l (%sp)+,%d0-%d1/%a0-%a1/%a6\n"
    "\trts\n");

/* "who=NAME": each top-level call of NAME, its return address and the
 * next return addresses into the library found on the stack (a guess: any
 * long within 256 KB of the library's MUI_NewObjectA gate that points into
 * code), printed as "who OP ADDR ADDR ..." with "whobase ADDR" (the gate's
 * address) for tests/lean/scripts/syscount-who.py */
LONG sc_who = -1;

/* The pixels the top-level RectFill calls fill (their rectangles' areas),
 * reported per operation as "_pixels": how much a toolkit paints */
LONG sc_rectidx = -1;
static ULONG sc_pix, sc_apix, sc_lastarea;
static WORD sc_lastrect[4];

void sc_rect(LONG x0, LONG y0, LONG x1, LONG y1)
{
    LONG w = (WORD)x1 - (WORD)x0 + 1, h = (WORD)y1 - (WORD)y0 + 1;

    sc_lastrect[0] = x0; sc_lastrect[1] = y0;
    sc_lastrect[2] = x1; sc_lastrect[3] = y1;
    sc_lastarea = 0;
    if (w > 0 && h > 0)
        sc_pix += sc_lastarea = w * h;
}
static ULONG sc_lo, sc_hi;
#define SC_RECS 600
#define SC_DEPTH 6
static ULONG sc_rec[SC_RECS][SC_DEPTH], sc_recarea[SC_RECS];
static WORD sc_recrect[SC_RECS][4];   /* who=RectFill: the rectangle */
static int sc_nrec, sc_nrec_kept;
static const char *sc_whoname;   /* "who=NAME" */

void sc_record(ULONG *p)
{
    int i, n = 0;
    ULONG *r;

    if (sc_nrec >= SC_RECS)
        return;
    sc_recarea[sc_nrec] = sc_who == sc_rectidx ? sc_lastarea : 0;
    memcpy(sc_recrect[sc_nrec], sc_lastrect, sizeof(sc_lastrect));
    r = sc_rec[sc_nrec++];
    r[n++] = p[0];
    for (i = 1; i < 96 && n < SC_DEPTH; i++)
        if (p[i] >= sc_lo && p[i] < sc_hi && !(p[i] & 1))
            r[n++] = p[i];
    while (n < SC_DEPTH)
        r[n++] = 0;
}

static struct Library *sc_base(int lib)
{
    return lib == SC_EXEC ? (struct Library *)SysBase : sc_bases[lib];
}

/* Patch every function; 0 if the stubs could not be made */
static int sc_install(int timing)
{
    UWORD *p;
    unsigned i;

    for (i = 1; i < SC_NLIBS; i++)
        if (!(sc_bases[i] = OpenLibrary(sc_libnames[i], 37)))
            printf("syscount: no %s\n", sc_libnames[i]);
    if (!(sc_stubs = AllocMem(SC_N * 12, MEMF_PUBLIC)))
        return 0;
    for (i = 0, p = sc_stubs; i < SC_N; i++)
    {
        *p++ = 0x2f3c;                              /* move.l #i,-(sp) */
        *p++ = 0; *p++ = i;
        *p++ = 0x4ef9;                              /* jmp sc_common */
        *p++ = (ULONG)sc_common >> 16; *p++ = (ULONG)sc_common & 0xffff;
    }
    CacheClearU();
    for (i = 0; i < SC_N; i++)
    {
        static const char *pass[] = { "NewObjectA", "DisposeObject", "SetAttrsA",
            "GetAttr", "SetGadgetAttrsA", "DoGadgetMethodA", "CallHookPkt", NULL };
        const char **q;

        for (q = pass; *q; q++)
            if (!strcmp(*q, sc_fns[i].name))
                sc_pass[i] = 1;
    }
    sc_me = FindTask(NULL);
    for (i = 0; i < SC_N; i++)
        if (!strcmp(sc_fns[i].name, "RectFill"))
            sc_rectidx = i * 4;
    sc_timing = timing;
    if (sc_whoname)
    {
        /* MUI_NewObjectA's entry: the jmp at LVO -30 */
        ULONG gate = *(ULONG *)((UBYTE *)MUIMasterBase - 30 + 2);

        sc_lo = gate > 0x40000 ? gate - 0x40000 : 0;
        sc_hi = gate + 0x40000;
        printf("whobase %08lx\n", (unsigned long)gate);
        for (i = 0; i < SC_N; i++)
            if (!strcmp(sc_fns[i].name, sc_whoname))
                sc_who = i * 4;
    }
    Forbid();
    for (i = 0; i < SC_N; i++)
        if (sc_base(sc_fns[i].lib))
            sc_old[i] = (ULONG)SetFunction(sc_base(sc_fns[i].lib), sc_fns[i].lvo,
                (APTR)(sc_stubs + i * 6));
    Permit();
    sc_active = 1;
    return 1;
}

static void sc_remove(void)
{
    unsigned i, lost = 0;

    if (!sc_active)
        return;
    sc_on = 0;
    Forbid();
    for (i = 0; i < SC_N; i++)
        if (sc_old[i])
        {
            struct Library *b = sc_base(sc_fns[i].lib);
            APTR now_ = (APTR)*(ULONG *)((UBYTE *)b + sc_fns[i].lvo + 2);

            if (now_ == (APTR)(sc_stubs + i * 6))
                SetFunction(b, sc_fns[i].lvo, (APTR)sc_old[i]);
            else
                lost++;
        }
    Permit();
    Delay(25);                  /* let any task inside a stub leave it */
    if (lost)                   /* patched over by someone: keep everything */
    {
        printf("syscount: %u vectors patched over, waiting forever\n", lost);
        Wait(0);
    }
    FreeMem(sc_stubs, SC_N * 12);
    for (i = 1; i < SC_NLIBS; i++)
        if (sc_bases[i])
            CloseLibrary(sc_bases[i]);
    sc_active = 0;
}

/* start of a timed part: forget what was counted before it */
static void sc_start(void)
{
    if (!sc_active)
        return;
    sc_on = 0;
    memset(sc_total, 0, sizeof(sc_total));
    memset(sc_top, 0, sizeof(sc_top));
    memset(sc_time, 0, sizeof(sc_time));
    sc_nrec = sc_nrec_kept;
    sc_pix = 0;
    sc_on = 1;
}

/* end of a timed part: add it to the operation's sums */
static void sc_stop(void)
{
    unsigned i;

    if (!sc_active)
        return;
    sc_on = 0;
    for (i = 0; i < SC_N; i++)
    {
        sc_atotal[i] += sc_total[i];
        sc_atop[i] += sc_top[i];
        sc_atime[i] += sc_time[i];
    }
    sc_nrec_kept = sc_nrec;
    sc_apix += sc_pix;
}

/* "calls NAME | Fn top total us | ...": per operation, two decimals */
static void sc_report(const char *name, ULONG ops)
{
    unsigned i;
    ULONG tk = eclock_freq / 1000;        /* ticks per ms */

    if (!sc_active)
        return;
    printf("calls %s", name);
    for (i = 0; i < SC_N; i++)
        if (sc_atotal[i])
        {
            ULONG top = sc_atop[i] * 100 / ops, tot = sc_atotal[i] * 100 / ops;
            ULONG us = (ULONG)(((unsigned long long)sc_atime[i] * 1000000ULL)
                / ((unsigned long long)tk * 1000ULL) / ops);

            printf(" | %s %lu.%02lu %lu.%02lu %lu", sc_fns[i].name,
                top / 100, top % 100, tot / 100, tot % 100, us);
        }
    printf(" | _pixels %lu 0 0", (unsigned long)(sc_apix / ops));
    printf("\n");
    for (i = 0; i < (unsigned)sc_nrec; i++)
    {
        int k;

        printf("who %s", name);
        if (sc_who == sc_rectidx)
            printf(" a=%lu r=%d,%d,%d,%d", (unsigned long)sc_recarea[i],
                sc_recrect[i][0], sc_recrect[i][1], sc_recrect[i][2],
                sc_recrect[i][3]);
        for (k = 0; k < SC_DEPTH && sc_rec[i][k]; k++)
            printf(" %08lx", (unsigned long)sc_rec[i][k]);
        printf("\n");
    }
    sc_nrec = sc_nrec_kept = 0;
    sc_apix = 0;
    memset(sc_atotal, 0, sizeof(sc_atotal));
    memset(sc_atop, 0, sizeof(sc_atop));
    memset(sc_atime, 0, sizeof(sc_atime));
}
