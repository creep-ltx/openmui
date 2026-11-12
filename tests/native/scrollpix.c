/* scrollpix: the pixels of a list and its scrollbar after scrolls, as
 * checksums of the window.
 *
 * A Listview of 100 entries is scrolled a row at a time (MUIM_List_Jump
 * just below the last visible row), a page, back to the top, and its
 * scroller moved by a set of MUIA_List_First; after each step the
 * window's inner area is summed pixel by pixel (ReadPixel). Two libraries
 * that draw the same give the same lines; compare the output of two runs.
 *
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -Wno-pointer-sign -DMUI_OBSOLETE \
 *   -I$I/include -Iworkbench/libs/muimaster/include tests/native/scrollpix.c -o scrollpix -lamiga
 */
#include <stdio.h>
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

static Object *app, *win;

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

    run(2);
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
}

int main(void)
{
    static char text[100][16];
    Object *list;
    ULONG first, visible;
    int i;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject,
        MUIA_Application_Title, "scrollpix",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "scrollpix",
            MUIA_Window_LeftEdge, 20, MUIA_Window_TopEdge, 20,
            MUIA_Window_Height, 120,
            WindowContents, VGroup,
                Child, ListviewObject,
                    MUIA_Listview_List, list = ListObject, InputListFrame,
                        End,
                    End,
                End,
            End,
        End;
    if (!app)
        return 20;
    for (i = 0; i < 100; i++)
    {
        sprintf(text[i], "Entry %03d", i);
        DoMethod(list, MUIM_List_InsertSingle, text[i],
            MUIV_List_Insert_Bottom);
    }
    set(win, MUIA_Window_Open, TRUE);
    run(20);
    sum("open", 0);
    for (i = 1; i <= 8; i++)
    {
        get(list, MUIA_List_First, &first);
        get(list, MUIA_List_Visible, &visible);
        DoMethod(list, MUIM_List_Jump, first + visible);
        sum("row", i);
    }
    for (i = 1; i <= 3; i++)
    {
        get(list, MUIA_List_First, &first);
        get(list, MUIA_List_Visible, &visible);
        DoMethod(list, MUIM_List_Jump, first + 2 * visible - 1);
        sum("page", i);
    }
    for (i = 1; i <= 4; i++)
    {
        set(list, MUIA_List_Active, i * 13);
        sum("active", i);
    }
    DoMethod(list, MUIM_List_Jump, 0);
    sum("top", 0);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
