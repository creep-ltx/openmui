/* cyclechain: the window's cycle chain and gadget IDs, black box. Prints
 * which object MUIA_Window_ActiveObject Next/Prev reach (from none, from
 * each object, past the ends), with one object disabled and one hidden,
 * after closing and opening the window again, after objects left and
 * joined the chain (a page switch), with MUIM_Window_SetCycleChain's
 * explicit order, and the gadget IDs of a window's border scrollers and
 * a listview's prop. The same output on two libraries = the same chain.
 * Build (I = OpenMUI's library build): m68k-amigaos-gcc -m68000 -noixemul \
 *   -Os -DMUI_OBSOLETE -I$I/include -Iworkbench/libs/muimaster/include tests/lean/cyclechain.c -o cyclechain -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
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

#define N 6
static Object *o[N], *win, *pages;

static int which(Object *a)
{
    int i;
    for (i = 0; i < N; i++)
        if (o[i] == a) return i;
    return a ? 99 : -1;
}

static int active(void)
{
    Object *a = NULL;
    get(win, MUIA_Window_ActiveObject, &a);
    return which(a);
}

static void walk(const char *what, IPTR dir, int steps)
{
    int i;
    printf("%s:", what);
    for (i = 0; i < steps; i++)
    {
        set(win, MUIA_Window_ActiveObject, dir);
        printf(" %d", active());
    }
    printf("\n");
}

static void from_each(void)
{
    int i;
    for (i = 0; i < N; i++)
    {
        set(win, MUIA_Window_ActiveObject, o[i]);
        printf("from %d:", i);
        set(win, MUIA_Window_ActiveObject, MUIV_Window_ActiveObject_Next);
        printf(" next %d", active());
        set(win, MUIA_Window_ActiveObject, o[i]);
        set(win, MUIA_Window_ActiveObject, MUIV_Window_ActiveObject_Prev);
        printf(" prev %d\n", active());
    }
}

static void gadgets(void)
{
    struct Window *w = NULL;
    struct Gadget *g;
    get(win, MUIA_Window_Window, &w);
    printf("gadget ids:");
    for (g = w ? w->FirstGadget : NULL; g; g = g->NextGadget)
        if (!(g->GadgetType & GTYP_SYSGADGET))
            printf(" %d", g->GadgetID);
    printf("\n");
}

int main(void)
{
    Object *app;
    int i;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject,
        MUIA_Application_Title, "cyclechain",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "cyclechain",
            MUIA_Window_UseRightBorderScroller, TRUE,
            MUIA_Window_UseBottomBorderScroller, TRUE,
            WindowContents, VGroup,
                Child, o[0] = SimpleButton("_A"),
                Child, o[1] = StringObject, StringFrame, MUIA_CycleChain, 1, End,
                Child, o[2] = SimpleButton("_C"),
                Child, pages = PageGroup,
                    Child, o[3] = SimpleButton("_D"),
                    Child, o[4] = SimpleButton("_E"),
                End,
                Child, o[5] = ListviewObject, MUIA_CycleChain, 1,
                    MUIA_Listview_List, ListObject, InputListFrame, End,
                End,
            End,
        End,
    End;
    if (!app)
    {
        printf("no application\n");
        CloseLibrary(MUIMasterBase);
        return 20;
    }
    for (i = 0; i < N; i++)
        set(o[i], MUIA_CycleChain, 1);
    set(win, MUIA_Window_Open, TRUE);
    printf("open %d\n", (int)XGET(win, MUIA_Window_Open));
    gadgets();
    walk("next from none", MUIV_Window_ActiveObject_Next, 8);
    set(win, MUIA_Window_ActiveObject, MUIV_Window_ActiveObject_None);
    walk("prev from none", MUIV_Window_ActiveObject_Prev, 8);
    from_each();

    set(o[2], MUIA_Disabled, TRUE);
    walk("C disabled, next", MUIV_Window_ActiveObject_Next, 7);
    walk("C disabled, prev", MUIV_Window_ActiveObject_Prev, 7);
    set(o[2], MUIA_Disabled, FALSE);

    set(pages, MUIA_Group_ActivePage, 1);
    walk("page E, next", MUIV_Window_ActiveObject_Next, 7);
    walk("page E, prev", MUIV_Window_ActiveObject_Prev, 7);

    set(win, MUIA_Window_Open, FALSE);
    set(win, MUIA_Window_Open, TRUE);
    gadgets();
    set(win, MUIA_Window_ActiveObject, MUIV_Window_ActiveObject_None);
    walk("reopened, next", MUIV_Window_ActiveObject_Next, 7);

    set(win, MUIA_Window_Open, FALSE);
    set(o[1], MUIA_CycleChain, 0);
    set(win, MUIA_Window_Open, TRUE);
    set(win, MUIA_Window_ActiveObject, MUIV_Window_ActiveObject_None);
    walk("B out, next", MUIV_Window_ActiveObject_Next, 7);
    set(o[1], MUIA_CycleChain, 1);

    DoMethod(win, MUIM_Window_SetCycleChain, o[5], o[2], o[0], o[1], NULL);
    set(win, MUIA_Window_ActiveObject, MUIV_Window_ActiveObject_None);
    walk("explicit, next", MUIV_Window_ActiveObject_Next, 7);
    walk("explicit, prev", MUIV_Window_ActiveObject_Prev, 7);
    set(win, MUIA_Window_Open, FALSE);
    set(win, MUIA_Window_Open, TRUE);
    set(win, MUIA_Window_ActiveObject, MUIV_Window_ActiveObject_None);
    walk("explicit reopened, next", MUIV_Window_ActiveObject_Next, 7);
    gadgets();

    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("done\n");
    return 0;
}
