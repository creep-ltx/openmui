/* mccspy: a black-box trace of what a MUI library sends to a program's own classes, in
   order. It patches muimaster's MUI_CreateCustomClass and wraps the dispatcher of every
   class a program creates (the program's classes, not MUI's), MUI_DisposeObject, MUI_Redraw,
   intuition CloseWindow and exec AllocMem/FreeMem (blocks of 2 KB and more), and records
   each call (op, class, task, object, arguments, two return addresses) into a ring of 8192
   records in chip RAM (0x1A0000, or the next free of 0x1BF000, 0x100000, 0x80000) that survives
   a reset. 19 graphics.library drawing calls are logged too when they draw into a chip block of
   8 KB or more the record saw allocated (who drew last into a block freed at the quit). Methods seen too often to matter
   (OM_GET, OM_SET, HandleEvent, HandleInput, the Application input methods) are skipped.
   The same tool runs on MUI 3.8 and on OpenMUI: the two records say what differs.
     mccspy [VER]    run in the background (Ctrl-C stops it; patches stay until then); VER
                     the muimaster version to open (default 19: the MUI 3.8 identity on OpenMUI)
     mccspy read     print the record (after the program's quit, or after the reboot)
   Build: m68k-amigaos-gcc -Os -m68000 -noixemul -s -o mccspy mccspy.c mccspy.s gfxwatch.s */
#include <proto/exec.h>
#include <proto/dos.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <intuition/classes.h>
#include <graphics/rastport.h>
#include <libraries/mui.h>
#include <string.h>
#include <stdio.h>

#define NCLS 128
#define NTASK 16
#define NREC 7000
#define MAGIC 0x4D435350
/* the header, the records right after it (0x1000 in); the first address that AllocAbs gives */
static const ULONG addrs[4] = { 0x1A0000, 0x1BF000, 0x100000, 0x080000 };
#define NBLK 32      /* chip blocks of 8 KB and more seen allocated, watched for drawing calls */
#define NGFX 19

struct rec { UBYTE op, cls, task, pad; ULONG obj, a, b, ret0, ret1; };   /* 24 bytes */
struct hdr {
    ULONG magic, n, ncls, ntask;
    char cname[NCLS][24]; ULONG ccl[NCLS];
    char tname[NTASK][24]; ULONG ttask[NTASK];
};
enum { OP_METHOD = 1, OP_FREE, OP_CLOSEWIN, OP_DISPOSE, OP_REDRAW, OP_CCC, OP_ALLOC, OP_MARK, OP_GFX, OP_ABM };
extern APTR ms_oldabm; void ms_abm(void);

extern APTR gw_old[NGFX];   /* gfxwatch.s: the stubs call gw_check(fn, a0, a1, sp) */
void gw_stub0(void), gw_stub1(void), gw_stub2(void), gw_stub3(void), gw_stub4(void), gw_stub5(void), gw_stub6(void), gw_stub7(void), gw_stub8(void), gw_stub9(void), gw_stub10(void), gw_stub11(void), gw_stub12(void), gw_stub13(void), gw_stub14(void), gw_stub15(void), gw_stub16(void), gw_stub17(void), gw_stub18(void);
static const LONG gfxlvos[NGFX] = { -306, -234, -810, -396, -60, -606, -30, -300, -552, -36, -312, -48, -42, -636, -1098, -246, -786, -774, -1056 };
static const char *gfxnames[NGFX] = { "RectFill", "SetRast", "EraseRect", "ScrollRaster", "Text", "BltBitMapRastPort", "BltBitMap", "BltClear", "ClipBlit", "BltTemplate", "BltPattern", "ClearScreen", "ClearEOL", "BltMaskBitMapRastPort", "ScrollRasterBF", "Draw", "WritePixelArray8", "WritePixelLine8", "WriteChunkyPixels" };
static ULONG blklo[NBLK], blkhi[NBLK]; static int nblk;

extern APTR ms_old[NCLS];                       /* the classes' original dispatchers */
extern APTR ms_oldccc, ms_olddispose, ms_oldredraw, ms_oldclosewin, ms_oldalloc, ms_oldfree;
extern APTR ms_stubs[NCLS];
void ms_ccc(void), ms_dispose(void), ms_redraw(void), ms_closewin(void), ms_alloc(void), ms_free(void);

static struct hdr *H; static struct rec *R;
static volatile int stopped;

static int inram(ULONG v)
{
    struct MemHeader *mh;
    if (v >= 0xF80000 && v < 0x1000000) return 1;
    for (mh = (struct MemHeader *)SysBase->MemList.lh_Head; mh->mh_Node.ln_Succ;
         mh = (struct MemHeader *)mh->mh_Node.ln_Succ)
        if (v >= (ULONG)mh->mh_Lower + 8 && v < (ULONG)mh->mh_Upper) return 1;
    return 0;
}
static int isret(ULONG v)
{
    UWORD w2, w4, w6;
    if (v & 1 || v < 0x400 || !inram(v)) return 0;
    w2 = *(UWORD *)(v - 2); w4 = *(UWORD *)(v - 4); w6 = *(UWORD *)(v - 6);
    if ((w2 & 0xFFF8) == 0x4E90) return 1;
    if ((w4 & 0xFFF8) == 0x4EA8 || (w4 & 0xFFF8) == 0x4EB0 || w4 == 0x4EB8 || w4 == 0x4EBA || w4 == 0x4EBB) return 1;
    if (w4 == 0x6100) return 1;
    if ((w2 & 0xFF00) == 0x6100 && (w2 & 0xFF) != 0 && (w2 & 0xFF) != 0xFF) return 1;
    if (w6 == 0x4EB9 || w6 == 0x61FF) return 1;
    return 0;
}
/* the first two RAM return addresses on the stack (not ROM: a library's caller matters) */
static void rets(ULONG *sp, ULONG *out)
{
    struct Task *t = SysBase->ThisTask; ULONG i; int k = 0;
    out[0] = out[1] = 0;
    for (i = 0; i < 200 && k < 2 && (APTR)(sp + i) < t->tc_SPUpper; i++)
        if (sp[i] < 0xF80000 && isret(sp[i])) out[k++] = sp[i];
}
static int taskidx(void)
{
    struct Task *t = SysBase->ThisTask; ULONG i;
    for (i = 0; i < H->ntask; i++) if (H->ttask[i] == (ULONG)t) return i;
    if (H->ntask < NTASK) {
        const char *s = t->tc_Node.ln_Name ? t->tc_Node.ln_Name : "?";
        H->ttask[H->ntask] = (ULONG)t;
        strncpy(H->tname[H->ntask], s, 23); H->tname[H->ntask][23] = 0;
        return H->ntask++;
    }
    return NTASK - 1;
}
static void record(int op, int cls, ULONG obj, ULONG a, ULONG b, ULONG *sp)
{
    struct rec *r; ULONG rr[2];
    if (stopped) return;
    Disable();
    r = &R[H->n % NREC]; H->n++;
    Enable();
    r->op = op; r->cls = cls; r->task = taskidx(); r->pad = 0;
    r->obj = obj; r->a = a; r->b = b;
    if (sp) { rets(sp, rr); r->ret0 = rr[0]; r->ret1 = rr[1]; } else r->ret0 = r->ret1 = 0;
}

/* called by the dispatcher stubs: ms_log(n, cl, obj, msg, sp) */
void ms_log(ULONG n, APTR cl, APTR obj, Msg msg, ULONG *sp)
{
    ULONG id = msg->MethodID;
    /* OM_SET on a Virtgroup subclass (Voyager's page) is kept: its first tag and value */
    if (id == OM_SET && H->cname[n][0] == 'V') {
        struct TagItem *t = ((struct opSet *)msg)->ops_AttrList;
        record(OP_METHOD, n, (ULONG)obj, id, t ? (t->ti_Tag << 16 | (t->ti_Data & 0xffff)) : 0, sp);
        return;
    }
    if (id == OM_GET || id == OM_SET || id == MUIM_HandleEvent || id == MUIM_HandleInput
        || id == MUIM_Application_NewInput || id == MUIM_Application_Input
        || id == MUIM_Application_InputBuffered || id == OM_NOTIFY || id == OM_UPDATE)
        return;
    record(OP_METHOD, n, (ULONG)obj, id, id == OM_NEW ? 0 : ((ULONG *)msg)[1], NULL);
}
/* after MUI_CreateCustomClass: name is the superclass name, mcc the result */
void ms_ccc_done(const char *name, struct MUI_CustomClass *mcc)
{
    ULONG i;
    if (!mcc || !mcc->mcc_Class) { record(OP_CCC, 0xFF, 0, (ULONG)name, 0, NULL); return; }
    if (H->ncls >= NCLS) return;
    i = H->ncls++;
    H->ccl[i] = (ULONG)mcc->mcc_Class;
    if ((ULONG)name < 0x10000000 && name) { strncpy(H->cname[i], name, 23); H->cname[i][23] = 0; }
    else { strcpy(H->cname[i], "?"); }
    ms_old[i] = (APTR)mcc->mcc_Class->cl_Dispatcher.h_Entry;
    mcc->mcc_Class->cl_Dispatcher.h_Entry = (HOOKFUNC)ms_stubs[i];
    record(OP_CCC, i, (ULONG)mcc->mcc_Class, (ULONG)ms_old[i], mcc->mcc_Class->cl_InstSize, NULL);
}
void ms_log_dispose(APTR obj, ULONG *sp) { record(OP_DISPOSE, 0, (ULONG)obj, 0, 0, sp); }
void ms_log_redraw(APTR obj, ULONG flags, ULONG *sp) { record(OP_REDRAW, 0, (ULONG)obj, flags, 0, sp); }
void ms_log_closewin(APTR win, ULONG *sp) { record(OP_CLOSEWIN, 0, (ULONG)win, 0, 0, sp); }
void ms_log_alloc(APTR block, ULONG size, ULONG flags, ULONG *sp)
{
    if (size < 2048) return;
    record(OP_ALLOC, 0, (ULONG)block, size, flags, sp);
    if (block && size >= 8192 && (ULONG)block < 0x200000) {   /* a chip block: watch it */
        int i; Disable();
        for (i = 0; i < nblk; i++) if (blklo[i] == (ULONG)block) break;
        if (i == nblk && nblk < NBLK) nblk++;
        if (i < NBLK) { blklo[i] = (ULONG)block; blkhi[i] = (ULONG)block + size; }
        Enable();
    }
}
void ms_log_free(APTR block, ULONG size, ULONG *sp)
{ if (size >= 2048) record(OP_FREE, 0, (ULONG)block, size, 0, sp); }
/* graphics AllocBitMap(width d0, height d1, depth d2): the sizes a program asks for */
void ms_log_abm(ULONG w, ULONG h, ULONG d, ULONG *sp) { record(OP_ABM, 0, w, h, d, sp); }
/* a drawing call: log it when its destination lies in a watched chip block (gfxwatch's decode:
   the RastPort is a1 for most, a0 for the pixel-array calls, BltBitMap's a1 is the BitMap,
   BltClear's a1 the memory) */
void gw_check(ULONG fn, ULONG a0, ULONG a1, ULONG *sp)
{
    ULONG target = 0; int i;
    if (fn >= 16) { struct RastPort *rp = (struct RastPort *)a0; if (!rp || !rp->BitMap) return; target = (ULONG)rp->BitMap->Planes[0]; }
    else if (!a1) return;
    else if (fn == 6) { struct BitMap *bm = (struct BitMap *)a1; target = (ULONG)bm->Planes[0]; }
    else if (fn == 7) target = a1;
    else { struct RastPort *rp = (struct RastPort *)a1; if (!rp->BitMap) return; target = (ULONG)rp->BitMap->Planes[0]; }
    if (!target) return;
    for (i = 0; i < nblk; i++)
        if (target >= blklo[i] && target < blkhi[i]) { record(OP_GFX, fn, target, blklo[i], fn >= 16 ? a0 : a1, sp); return; }
}

static const char *mname(ULONG id)
{
    switch (id) {
    case OM_NEW: return "OM_NEW"; case OM_DISPOSE: return "OM_DISPOSE"; case OM_ADDMEMBER: return "OM_ADDMEMBER";
    case OM_REMMEMBER: return "OM_REMMEMBER"; case OM_SET: return "OM_SET(tag:val)";
    case MUIM_Setup: return "Setup"; case MUIM_Cleanup: return "Cleanup"; case MUIM_Show: return "Show";
    case MUIM_Hide: return "Hide"; case MUIM_Draw: return "Draw"; case MUIM_Layout: return "Layout";
    case MUIM_AskMinMax: return "AskMinMax"; case MUIM_DrawBackground: return "DrawBackground";
    case MUIM_Window_Setup: return "Window_Setup"; case MUIM_Window_Cleanup: return "Window_Cleanup";
    case MUIM_Notify: return "Notify"; case MUIM_Application_ReturnID: return "ReturnID";
    case 0x80429e07: return "ConnectParent"; case 0x80429e08: return "DisconnectParent";
    case MUIM_Group_InitChange: return "InitChange"; case MUIM_Group_ExitChange: return "ExitChange";
    case MUIM_Set: return "Set"; case MUIM_CallHook: return "CallHook"; case MUIM_MultiSet: return "MultiSet";
    case MUIM_Application_PushMethod: return "PushMethod"; 
    case MUIM_GoActive: return "GoActive"; case MUIM_GoInactive: return "GoInactive";
    case MUIM_Window_SetCycleChain: return "SetCycleChain"; case MUIM_Window_AddEventHandler: return "AddEH";
    case MUIM_Window_RemEventHandler: return "RemEH"; case MUIM_Application_Load: return "Load";
    case MUIM_Application_Save: return "Save"; case MUIM_Export: return "Export"; case MUIM_Import: return "Import";
    case MUIM_Application_AddInputHandler: return "AddIH"; case MUIM_Application_RemInputHandler: return "RemIH";
    case MUIM_KillNotify: return "KillNotify"; case MUIM_KillNotifyObj: return "KillNotifyObj";
    case MUIM_Window_Snapshot: return "Snapshot"; case MUIM_Application_ShowHelp: return "ShowHelp";
    case MUIM_Application_OpenConfigWindow: return "OpenConfigWindow";
    case MUIM_Application_UnpushMethod: return "UnpushMethod";
    case MUIM_Application_SetMenuCheck: return "SetMenuCheck"; case MUIM_Application_SetMenuState: return "SetMenuState";
    case MUIM_Application_GetMenuCheck: return "GetMenuCheck"; case MUIM_Application_GetMenuState: return "GetMenuState";
    case MUIM_List_Insert: return "List_Insert"; case MUIM_List_InsertSingle: return "List_InsertSingle";
    case MUIM_List_Clear: return "List_Clear"; case MUIM_List_Redraw: return "List_Redraw";
    case MUIM_List_GetEntry: return "List_GetEntry"; case MUIM_List_Remove: return "List_Remove";
    case MUIM_Group_Sort: return "Group_Sort"; case MUIM_Group_ExitChange2: return "ExitChange2";
    case MUIM_CreateDragImage: return "CreateDragImage"; case MUIM_DeleteDragImage: return "DeleteDragImage";
    case MUIM_DragQuery: return "DragQuery"; case MUIM_DragDrop: return "DragDrop";
    case MUIM_ContextMenuBuild: return "ContextMenuBuild"; case MUIM_CreateBubble: return "CreateBubble";
    case MUIM_CreateShortHelp: return "CreateShortHelp"; case MUIM_DeleteShortHelp: return "DeleteShortHelp";
    
    }
    return NULL;
}

static void readrec(void)
{
    ULONG i, first, n;
    if (H->magic != MAGIC) { printf("no record (magic %08lx)\n", H->magic); return; }
    n = H->n < NREC ? H->n : NREC; first = H->n < NREC ? 0 : H->n % NREC;
    printf("mccspy: %lu calls, %lu kept; %lu classes, %lu tasks\n", H->n, n, H->ncls, H->ntask);
    for (i = 0; i < H->ncls; i++) printf("  class %lu: %s cl %08lx\n", i, H->cname[i], H->ccl[i]);
    for (i = 0; i < H->ntask; i++) printf("  task %lu: %s %08lx\n", i, H->tname[i], H->ttask[i]);
    for (i = 0; i < n; i++) {
        struct rec *r = &R[(first + i) % NREC]; const char *m; ULONG seq = H->n - n + i;
        printf("%6lu t%lu ", seq, (ULONG)r->task);
        switch (r->op) {
        case OP_METHOD:
            m = mname(r->a);
            if (m) printf("M %-12s obj %08lx cls %lu(%s) arg %08lx%s%08lx %08lx\n", m, r->obj, (ULONG)r->cls, H->cname[r->cls], r->b, r->ret0 ? " from " : "", r->ret0, r->ret1);
            else printf("M %08lx     obj %08lx cls %lu(%s) arg %08lx\n", r->a, r->obj, (ULONG)r->cls, H->cname[r->cls], r->b);
            break;
        case OP_FREE: printf("FreeMem  %08lx size %lu from %08lx %08lx\n", r->obj, r->a, r->ret0, r->ret1); break;
        case OP_ALLOC: printf("AllocMem %08lx size %lu flags %lx from %08lx %08lx\n", r->obj, r->a, r->b, r->ret0, r->ret1); break;
        case OP_CLOSEWIN: printf("CloseWindow %08lx from %08lx %08lx\n", r->obj, r->ret0, r->ret1); break;
        case OP_DISPOSE: printf("MUI_DisposeObject %08lx from %08lx %08lx\n", r->obj, r->ret0, r->ret1); break;
        case OP_REDRAW: printf("MUI_Redraw %08lx flags %lu from %08lx %08lx\n", r->obj, r->a, r->ret0, r->ret1); break;
        case OP_CCC: printf("CreateCustomClass -> cls %lu(%s) cl %08lx dispatcher %08lx instsize %lu\n", (ULONG)r->cls, r->cls < NCLS ? H->cname[r->cls] : "failed", r->obj, r->a, r->b); break;
        case OP_MARK: printf("MARK %lu\n", r->a); break;
        case OP_ABM: printf("AllocBitMap %lux%lu depth %lu from %08lx %08lx\n", r->obj, r->a, r->b, r->ret0, r->ret1); break;
        case OP_GFX: printf("GFX %s into %08lx (block %08lx) arg %08lx from %08lx %08lx\n", r->cls < NGFX ? gfxnames[r->cls] : "?", r->obj, r->a, r->b, r->ret0, r->ret1); break;
        default: printf("op %d\n", r->op);
        }
    }
}

int main(int argc, char **argv)
{
    struct Library *MUIMasterBase, *IntuitionBase2, *GfxBase2;
    ULONG i;
    static void (*const stubs[NGFX])(void) = { gw_stub0, gw_stub1, gw_stub2, gw_stub3, gw_stub4, gw_stub5, gw_stub6, gw_stub7, gw_stub8, gw_stub9, gw_stub10, gw_stub11, gw_stub12, gw_stub13, gw_stub14, gw_stub15, gw_stub16, gw_stub17, gw_stub18 };
    H = NULL;
    for (i = 0; i < 4 && !H; i++) if (((struct hdr *)addrs[i])->magic == MAGIC) H = (struct hdr *)addrs[i];
    if (H) R = (struct rec *)((ULONG)H + 0x1000);
    if (argc > 1 && argv[1][0] == 'r') { if (!H) { printf("no record\n"); return 5; } readrec(); return 0; }
    if (argc > 1 && (argv[1][0] < '0' || argv[1][0] > '9') && argv[1][0] != 'm') { printf("mccspy [VER|read|mark N]\n"); return 5; }
    if (argc > 1 && argv[1][0] == 'm') {    /* mccspy mark N: a marker in the record */
        ULONG v = argc > 2 ? (ULONG)(argv[2][0] - '0') : 0;
        if (H->magic != MAGIC) return 10;
        record(OP_MARK, 0, 0, v, 0, NULL); return 0;
    }
    H = NULL;
    for (i = 0; i < 4 && !H; i++)
        if (AllocAbs(0x1000 + NREC * sizeof(struct rec), (APTR)addrs[i])) H = (struct hdr *)addrs[i];
    if (!H) { printf("mccspy: no free chip RAM at any of the four addresses\n"); return 10; }
    R = (struct rec *)((ULONG)H + 0x1000);
    for (i = 0; i < 4; i++) if ((struct hdr *)addrs[i] != H) ((struct hdr *)addrs[i])->magic = 0;   /* one record only */
    memset(H, 0, sizeof(*H)); H->magic = MAGIC;
    printf("mccspy: record at %08lx\n", (ULONG)H);
    /* the version asked for matters on OpenMUI: each identity (MUI 3.8 = 19, MUI 4 = 20,
     * MUI 5 = 21) has its own jump table copy, shared by the programs asking for it;
     * patch the one the program under test uses (default 19) */
    MUIMasterBase = OpenLibrary("muimaster.library", argc > 1 ? (ULONG)(argv[1][0] - '0') * 10 + (argv[1][1] - '0') : 19);
    if (!MUIMasterBase) { printf("mccspy: no muimaster.library\n"); return 10; }
    IntuitionBase2 = OpenLibrary("intuition.library", 0);
    GfxBase2 = OpenLibrary("graphics.library", 0);
    Forbid();
    for (i = 0; i < NGFX; i++) gw_old[i] = SetFunction(GfxBase2, gfxlvos[i], (APTR)stubs[i]);
    ms_oldabm = SetFunction(GfxBase2, -918, (APTR)ms_abm);
    ms_oldccc = SetFunction(MUIMasterBase, -108, (APTR)ms_ccc);
    ms_olddispose = SetFunction(MUIMasterBase, -36, (APTR)ms_dispose);
    ms_oldredraw = SetFunction(MUIMasterBase, -102, (APTR)ms_redraw);
    ms_oldclosewin = SetFunction(IntuitionBase2, -72, (APTR)ms_closewin);
    ms_oldalloc = SetFunction((struct Library *)SysBase, -198, (APTR)ms_alloc);
    ms_oldfree = SetFunction((struct Library *)SysBase, -210, (APTR)ms_free);
    Permit();
    printf("mccspy: recording (Ctrl-C stops)\n"); fflush(stdout);
    while (!(SetSignal(0, 0) & SIGBREAKF_CTRL_C)) Delay(10);
    stopped = 1;
    Forbid();
    SetFunction(MUIMasterBase, -108, ms_oldccc);
    SetFunction(MUIMasterBase, -36, ms_olddispose);
    SetFunction(MUIMasterBase, -102, ms_oldredraw);
    SetFunction(IntuitionBase2, -72, ms_oldclosewin);
    SetFunction((struct Library *)SysBase, -198, ms_oldalloc);
    SetFunction((struct Library *)SysBase, -210, ms_oldfree);
    for (i = 0; i < NGFX; i++) SetFunction(GfxBase2, gfxlvos[i], gw_old[i]);
    SetFunction(GfxBase2, -918, ms_oldabm);
    /* a class still alive and still patched gets its dispatcher back (a deleted one is left alone) */
    for (i = 0; i < H->ncls; i++)
        if (((struct IClass *)H->ccl[i])->cl_Dispatcher.h_Entry == (HOOKFUNC)ms_stubs[i])
            ((struct IClass *)H->ccl[i])->cl_Dispatcher.h_Entry = (HOOKFUNC)ms_old[i];
    Permit();
    CloseLibrary(GfxBase2); CloseLibrary(IntuitionBase2); CloseLibrary(MUIMasterBase);
    printf("mccspy: stopped, %lu calls\n", H->n);
    return 0;
}
