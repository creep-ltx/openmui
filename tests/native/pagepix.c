/* pagepix: the pixels after page switches, as checksums of the window.
 *
 * A Cycle (entries of different widths), a Register group and a page group
 * whose pages have different sizes; each is switched through its pages,
 * at the window's first size and again after a resize (a page switch in a
 * group whose box changed). After every switch the window's inner area is
 * summed pixel by pixel (ReadPixel). Two libraries that draw the same give
 * the same lines; compare the output of two runs. "pagepix N" waits 30
 * seconds after the N-th checksum (for a screenshot), RAM:pagepix.stop
 * says when.
 *
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -Wno-pointer-sign -DMUI_OBSOLETE \
 *   -I$I/include -Iworkbench/libs/muimaster/include tests/native/pagepix.c -o pagepix -lamiga
 */
#include <stdio.h>
#include <stdlib.h>
#include <intuition/intuition.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
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

static Object *app, *win;
static int stop_at = -1, count;

static void run(int ticks)
{
    ULONG sigs = 0;
    int i;

    for (i = 0; i < ticks; i++)
    {
        Delay(1);
        DoMethod(app, MUIM_Application_NewInput, &sigs);
    }
}

static void sum(const char *what, int n)
{
    struct Window *w = NULL;
    ULONG s = 0, x, y;

    get(win, MUIA_Window_Window, &w);
    if (!w)
    {
        printf("%s %d nowindow\n", what, n);
        return;
    }
    for (y = w->BorderTop; y < (ULONG)(w->Height - w->BorderBottom); y++)
        for (x = w->BorderLeft; x < (ULONG)(w->Width - w->BorderRight); x++)
            s = s * 31 + ReadPixel(w->RPort, x, y) + 1;
    printf("%s %d %lux%lu sum=%08lx\n", what, n,
        (ULONG)w->Width, (ULONG)w->Height, s);
    if (++count == stop_at)
    {
        BPTR f = Open("RAM:pagepix.stop", MODE_NEWFILE);

        if (f)
            Close(f);
        printf("STOP\n");
        fflush(stdout);
        Delay(50 * 30);     /* for a screenshot */
    }
}

int main(int argc, char **argv)
{
    static const char *cycles[] = { "One", "A much longer entry", "Mid", NULL };
    static const char *pages[] = { "First", "Second", "Third", NULL };
    Object *cycle, *reg, *pg;
    struct Window *w = NULL;
    int round, i;

    if (argc > 1)
        stop_at = atoi(argv[1]);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject,
        MUIA_Application_Title, "pagepix",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "pagepix",
            MUIA_Window_LeftEdge, 20, MUIA_Window_TopEdge, 20,
            WindowContents, VGroup,
                Child, cycle = CycleObject, MUIA_Cycle_Entries, cycles, End,
                Child, reg = RegisterGroup(pages),
                    Child, VGroup,
                        Child, TextObject, TextFrame,
                            MUIA_Text_Contents, "Page one", End,
                        Child, SimpleButton("Button"),
                        End,
                    Child, VGroup,
                        Child, TextObject, MUIA_Text_Contents, "Two", End,
                        End,
                    Child, HGroup,
                        Child, SimpleButton("A"),
                        Child, SimpleButton("Bee"),
                        Child, SimpleButton("Cee"),
                        End,
                    End,
                Child, pg = PageGroup, GroupFrame,
                    Child, TextObject, MUIA_Text_Contents, "small", End,
                    Child, TextObject, MUIA_Text_Contents,
                        "\33cA wider page\nof two lines", End,
                    Child, RectangleObject, MUIA_FixWidth, 20,
                        MUIA_FixHeight, 8, MUIA_Background, MUII_FILL, End,
                    End,
                End,
            End,
        End;
    if (!app)
        return 20;
    set(win, MUIA_Window_Open, TRUE);
    run(20);
    for (round = 0; round < 2; round++)
    {
        sum("open", round);
        for (i = 1; i <= 3; i++)
        {
            set(cycle, MUIA_Cycle_Active, i % 3);
            run(3);
            sum("cycle", i);
        }
        for (i = 1; i <= 3; i++)
        {
            set(reg, MUIA_Group_ActivePage, i % 3);
            run(3);
            sum("register", i);
        }
        for (i = 1; i <= 3; i++)
        {
            set(pg, MUIA_Group_ActivePage, i % 3);
            run(3);
            sum("pagegroup", i);
        }
        get(win, MUIA_Window_Window, &w);
        if (w)
        {
            ChangeWindowBox(w, w->LeftEdge, w->TopEdge, w->Width + 60,
                w->Height + 30);
            run(30);
        }
    }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
