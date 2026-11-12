/* alloccount: how many exec allocations a MUI program's objects cost.
 *
 * Builds the application of tests/native/opbench.c's kind (a window with
 * groups, texts, a cycle, a radio, sliders, a list, a string, buttons, a
 * menu strip) a few times and disposes it, counting this task's AllocMem
 * and FreeMem calls (exec patched with SetFunction for the run; AllocVec
 * and AllocPooled reach AllocMem through the library vector, so they are
 * counted as the AllocMem they make). The same binary runs on MUI 3.8 and
 * on OpenMUI: a black-box count. Output: "alloccount build=N alloc=A
 * bytes=B free=F" per build+dispose, the median of 5 after a warm-up.
 * "alloccount trace" also lists the last build's allocations: size and
 * caller (the caller of AllocVec for AllocVec), with MUI_NewObjectA's
 * address to find the library's code.
 *
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -Wno-pointer-sign -DMUI_OBSOLETE \
 *   -I$I/include -Iworkbench/libs/muimaster/include tests/native/alloccount.c -o alloccount -lamiga
 */
#include <stdio.h>
#include <string.h>
#include <exec/execbase.h>
#include <libraries/gadtools.h>
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
static Object *probe_make_object(LONG type, ...)
{
    va_list params;
    Object *obj;
    va_start(params, type);
    obj = MUI_MakeObjectA(type, (ULONG *)params);
    va_end(params);
    return obj;
}
#undef MUI_MakeObject
#define MUI_MakeObject probe_make_object

/* Globals: the patches below read them by name */
struct Task *me;
volatile ULONG n_alloc, n_bytes, n_free;
APTR old_alloc, old_free, old_allocvec;
/* "alloccount trace": each allocation's size and caller (AllocVec's
 * caller for AllocVec, marked by the size's top bit) */
ULONG rec[3 * 400], *rec_ptr = rec, *rec_end = rec + 3 * 400;

/* The patches: count for this task, then jump to the original with the
 * registers as they came (d0/d1 for AllocMem, a1/d0 for FreeMem) */
APTR count_alloc(void);
APTR count_free(void);
APTR count_allocvec(void);
asm(
    "\t.text\n\t.even\n"
    "_count_alloc:\n"
    "\tmove.l %a0,-(%sp)\n"
    "\tmove.l _me,%a0\n"
    "\tcmp.l 276(%a6),%a0\n"         /* ExecBase->ThisTask */
    "\tbne.s 1f\n"
    "\taddq.l #1,_n_alloc\n"
    "\tadd.l %d0,_n_bytes\n"
    "\tcmp.l #0xf80000,4(%sp)\n"   /* from the ROM: AllocVec's own */
    "\tbcc.s 1f\n"
    "\tmove.l _rec_ptr,%a0\n"
    "\tcmp.l _rec_end,%a0\n"
    "\tbcc.s 1f\n"
    "\tmove.l %d0,(%a0)+\n"
    "\tmove.l 4(%sp),(%a0)+\n"
    "\tmove.l 8(%sp),(%a0)+\n"
    "\tmove.l %a0,_rec_ptr\n"
    "1:\tmove.l (%sp)+,%a0\n"
    "\tmove.l _old_alloc,-(%sp)\n"
    "\trts\n"
    "_count_allocvec:\n"
    "\tmove.l %a0,-(%sp)\n"
    "\tmove.l _me,%a0\n"
    "\tcmp.l 276(%a6),%a0\n"
    "\tbne.s 1f\n"
    "\tmove.l _rec_ptr,%a0\n"
    "\tcmp.l _rec_end,%a0\n"
    "\tbcc.s 1f\n"
    "\tmove.l %d0,(%a0)\n"
    "\tbset #7,(%a0)\n"
    "\taddq.l #4,%a0\n"
    "\tmove.l 4(%sp),(%a0)+\n"
    "\tmove.l 8(%sp),(%a0)+\n"
    "\tmove.l %a0,_rec_ptr\n"
    "1:\tmove.l (%sp)+,%a0\n"
    "\tmove.l _old_allocvec,-(%sp)\n"
    "\trts\n"
    "_count_free:\n"
    "\tmove.l %a0,-(%sp)\n"
    "\tmove.l _me,%a0\n"
    "\tcmp.l 276(%a6),%a0\n"
    "\tbne.s 1f\n"
    "\taddq.l #1,_n_free\n"
    "1:\tmove.l (%sp)+,%a0\n"
    "\tmove.l _old_free,-(%sp)\n"
    "\trts\n");

static Object *build(void)
{
    static const char *cycles[] = { "One", "Two", "Three", NULL };
    static const char *radios[] = { "Red", "Green", "Blue", NULL };
    static struct NewMenu menu[] = {
        { NM_TITLE, "Project", 0, 0, 0, 0 },
        { NM_ITEM, "About", "?", 0, 0, 0 },
        { NM_ITEM, "Quit", "Q", 0, 0, 0 },
        { NM_END, NULL, 0, 0, 0, 0 } };

    return ApplicationObject,
        MUIA_Application_Title, "alloccount",
        MUIA_Application_Menustrip, MUI_MakeObject(MUIO_MenustripNM, menu, 0),
        SubWindow, WindowObject,
            MUIA_Window_Title, "alloccount",
            WindowContents, VGroup,
                Child, ColGroup(2),
                    Child, Label2("Name:"), Child, StringObject, StringFrame, End,
                    Child, Label1("Mode:"),
                    Child, CycleObject, MUIA_Cycle_Entries, cycles, End,
                    Child, Label2("Value:"),
                    Child, SliderObject, MUIA_Numeric_Max, 1000, End,
                    End,
                Child, RadioObject, GroupFrame, MUIA_Radio_Entries, radios, End,
                Child, HGroup,
                    Child, ListviewObject,
                        MUIA_Listview_List, ListObject, InputListFrame, End,
                        End,
                    Child, VGroup,
                        Child, TextObject, TextFrame,
                            MUIA_Text_Contents, "Status", End,
                        Child, SimpleButton("_One"),
                        Child, SimpleButton("_Two"),
                        End,
                    End,
                End,
            End,
        End;
}

int main(int argc, char **argv)
{
    ULONG a[5], b[5], f[5];
    int i, j, k;
    Object *app;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    me = FindTask(NULL);
    if ((app = build()))
        MUI_DisposeObject(app);
    Forbid();
    old_alloc = SetFunction((struct Library *)SysBase, -198, (APTR)count_alloc);
    old_free = SetFunction((struct Library *)SysBase, -210, (APTR)count_free);
    old_allocvec = SetFunction((struct Library *)SysBase, -684,
        (APTR)count_allocvec);
    Permit();
    for (i = 0; i < 5; i++)
    {
        n_alloc = n_bytes = n_free = 0;
        rec_ptr = rec;
        if ((app = build()))
            MUI_DisposeObject(app);
        a[i] = n_alloc; b[i] = n_bytes; f[i] = n_free;
    }
    Forbid();
    SetFunction((struct Library *)SysBase, -198, old_alloc);
    SetFunction((struct Library *)SysBase, -210, old_free);
    SetFunction((struct Library *)SysBase, -684, old_allocvec);
    Permit();
    if (argc > 1)
    {
        ULONG *r;

        /* MUI_NewObjectA's address, from the jump table (LVO -30) */
        printf("newobject %08lx\n",
            *(ULONG *)((UBYTE *)MUIMasterBase - 30 + 2));
        for (r = rec; r < rec_ptr; r += 3)
            printf("rec %s %lu %08lx %08lx\n", (r[0] & 0x80000000) ?
                "vec" : "mem", r[0] & 0x7fffffff, r[1], r[2]);
    }
    for (i = 0; i < 5; i++)            /* sort each for the median */
        for (j = i + 1; j < 5; j++)
        {
            if (a[j] < a[i]) { k = a[i]; a[i] = a[j]; a[j] = k; }
            if (b[j] < b[i]) { k = b[i]; b[i] = b[j]; b[j] = k; }
            if (f[j] < f[i]) { k = f[i]; f[i] = f[j]; f[j] = k; }
        }
    printf("alloccount build=5 alloc=%lu bytes=%lu free=%lu (min alloc %lu, max %lu)\n",
        a[2], b[2], f[2], a[0], a[4]);
    CloseLibrary(MUIMasterBase);
    return 0;
}
