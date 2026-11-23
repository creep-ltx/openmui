/* proppix: the pixels of scrollbars after their values change, as
 * checksums of the window.
 *
 * A Listview gets rows one at a time (its scrollbar's knob appears and
 * shrinks), loses some, is cleared and filled again; a Scrollbar and a
 * Prop of their own get MUIA_Prop_Entries, Visible and First one at a
 * time and together, also to the values they have, with the window
 * open; a disabled one too. After each step the window's inner area is
 * summed pixel by pixel (ReadPixel). Two libraries that draw the same
 * give the same lines; compare the output of two runs.
 *
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -Wno-pointer-sign -DMUI_OBSOLETE \
 *   -I$I/include -Iworkbench/libs/muimaster/include tests/native/proppix.c -o proppix -lamiga
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
    static char text[60][16];
    Object *list, *sb, *prop, *dis;
    int i;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject,
        MUIA_Application_Title, "proppix",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "proppix",
            MUIA_Window_LeftEdge, 20, MUIA_Window_TopEdge, 20,
            MUIA_Window_Height, 140, MUIA_Window_Width, 300,
            WindowContents, VGroup,
                Child, HGroup,
                    Child, ListviewObject,
                        MUIA_Listview_List, list = ListObject, InputListFrame,
                            End,
                        End,
                    Child, prop = PropObject, PropFrame,
                        MUIA_Prop_Entries, 100, MUIA_Prop_Visible, 10,
                        MUIA_Prop_First, 0, MUIA_FixWidth, 12, End,
                    End,
                Child, sb = ScrollbarObject, MUIA_Group_Horiz, TRUE,
                    MUIA_Prop_Entries, 50, MUIA_Prop_Visible, 10,
                    MUIA_Prop_First, 5, End,
                Child, dis = ScrollbarObject, MUIA_Group_Horiz, TRUE,
                    MUIA_Prop_Entries, 50, MUIA_Prop_Visible, 10,
                    MUIA_Prop_First, 5, MUIA_Disabled, TRUE, End,
                End,
            End,
        End;
    if (!app)
        return 20;
    set(win, MUIA_Window_Open, TRUE);
    run(20);
    sum("open", 0);
    for (i = 0; i < 60; i++)
    {
        sprintf(text[i], "Entry %03d", i);
        DoMethod(list, MUIM_List_InsertSingle, text[i],
            MUIV_List_Insert_Bottom);
        if (i < 4 || i % 4 == 0)
            sum("insert", i);
    }
    for (i = 0; i < 12; i++)
    {
        DoMethod(list, MUIM_List_Remove, MUIV_List_Remove_Last);
        if (i % 3 == 0)
            sum("remove", i);
    }
    set(list, MUIA_List_First, 20);
    sum("first", 20);
    DoMethod(list, MUIM_List_Remove, MUIV_List_Remove_First);
    sum("removefirst", 0);
    DoMethod(list, MUIM_List_Clear);
    sum("clear", 0);
    for (i = 0; i < 3; i++)
        DoMethod(list, MUIM_List_InsertSingle, text[i], MUIV_List_Insert_Top);
    sum("three", 0);
    set(list, MUIA_List_Quiet, TRUE);
    for (i = 3; i < 50; i++)
        DoMethod(list, MUIM_List_InsertSingle, text[i], MUIV_List_Insert_Bottom);
    set(list, MUIA_List_Quiet, FALSE);
    sum("quiet50", 0);

    for (i = 0; i <= 45; i += 9)
    {
        set(sb, MUIA_Prop_First, i);
        sum("sb first", i);
    }
    set(sb, MUIA_Prop_First, 45);
    sum("sb first again", 45);
    set(sb, MUIA_Prop_Entries, 200);
    sum("sb entries", 200);
    set(sb, MUIA_Prop_Visible, 100);
    sum("sb visible", 100);
    set(sb, MUIA_Prop_Entries, 20);
    sum("sb entries", 20);
    set(sb, MUIA_Prop_Entries, 0);
    sum("sb entries", 0);
    SetAttrs(sb, MUIA_Prop_Entries, 1000, MUIA_Prop_Visible, 7,
        MUIA_Prop_First, 500, TAG_DONE);
    sum("sb all", 0);
    nnset(sb, MUIA_Prop_First, 993);
    sum("sb nnset", 993);
    DoMethod(sb, MUIM_Prop_Increase, 3);
    sum("sb increase", 3);
    DoMethod(sb, MUIM_Prop_Decrease, 400);
    sum("sb decrease", 400);

    for (i = 0; i <= 90; i += 30)
    {
        set(prop, MUIA_Prop_First, i);
        sum("prop first", i);
    }
    set(prop, MUIA_Prop_Entries, 12);
    sum("prop entries", 12);
    set(prop, MUIA_Prop_Visible, 12);
    sum("prop visible", 12);
    set(prop, MUIA_Prop_Entries, 65536 * 3);
    sum("prop entries big", 0);
    set(prop, MUIA_Prop_First, 65536);
    sum("prop first big", 0);

    set(dis, MUIA_Prop_First, 30);
    sum("disabled first", 30);
    set(dis, MUIA_Prop_Entries, 500);
    sum("disabled entries", 500);
    set(dis, MUIA_Disabled, FALSE);
    sum("enabled", 0);
    set(dis, MUIA_Prop_First, 100);
    sum("enabled first", 100);
    set(dis, MUIA_Disabled, TRUE);
    sum("disabled again", 0);

    set(win, MUIA_Window_Open, FALSE);
    set(sb, MUIA_Prop_First, 10);
    set(win, MUIA_Window_Open, TRUE);
    run(20);
    sum("reopened", 0);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
