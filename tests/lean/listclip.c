/* Pixels of lists whose drawing may leave the entry area: text wider than
 * the list, a cut-off last row, columns with bars and a title, styled
 * text. Prints one checksum per pixel row of the window after opening,
 * after moving the cursor (with and without scrolling), after MUI_Redraw
 * and after a relayout. Compare the output of two library builds.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -I<build>/include
 *        -Wl,-u,___stkinit listclip.c -o listclip -lmui
 */
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
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

static const char *plain[] = {
    "Short", "A very long entry that is much wider than the list itself, it goes on",
    "Third", "Fourth entry", "Fifth", "Sixth with more text than fits in here",
    "Seventh", "Eighth", "Ninth", "Tenth", "Eleventh", "Twelfth", "Thirteenth",
    "Fourteenth", "Fifteenth", "Sixteenth", "Seventeenth", "Eighteenth", NULL
};
static const char *styled[] = {
    "\33bBold entry that is wider than the list", "\33iItalic entry, also too wide here",
    "\33cCentred", "\33rRight", "\33uUnderlined", "\0338Pen 8", "Plain", "Plain 2",
    "Plain 3", "Plain 4", "Plain 5", "Plain 6", NULL
};
static const char *cols[] = {
    "One|Alpha|First column text", "Two|Beta, a wider cell than the column|x",
    "Three|Gamma|A third column that runs past the right edge of the list",
    "Four|Delta|y", "Five|Epsilon|z", "Six|Zeta|w", "Seven|Eta|v", "Eight|Theta|u",
    "Nine|Iota|t", "Ten|Kappa|s", NULL
};

static char cellbuf[3][64];
static const char *titles[] = { "Name", "Greek", "Text" };

static LONG display_func(struct Hook *h __asm("a0"), char **array __asm("a2"),
    char *entry __asm("a1"))
{
    int c = 0;
    const char *p = entry;

    if (!entry)
    {
        array[0] = (char *)titles[0];
        array[1] = (char *)titles[1];
        array[2] = (char *)titles[2];
        return 0;
    }
    while (c < 3)
    {
        const char *e = strchr(p, '|');
        int n = e ? e - p : (int)strlen(p);

        if (n > 63) n = 63;
        memcpy(cellbuf[c], p, n);
        cellbuf[c][n] = 0;
        array[c] = cellbuf[c];
        c++;
        if (!e) break;
        p = e + 1;
    }
    while (c < 3) array[c++] = "";
    return 0;
}

static struct Hook display_hook = { { NULL, NULL }, (HOOKFUNC)display_func, NULL, NULL };

static struct Window *window;

static void dump(const char *phase)
{
    struct RastPort *rp = window->RPort;
    int x, y;

    WaitBlit();
    for (y = 0; y < window->Height; y++)
    {
        ULONG sum = 0;

        for (x = 0; x < window->Width; x++)
            sum = sum * 31 + ReadPixel(rp, x, y) + 1;
        printf("%s %d %08lx\n", phase, y, (unsigned long)sum);
    }
}

static Object *mklist(const char **e, int kind, Object **out)
{
    Object *l;

    if (kind == 2)
        l = MUI_NewObject(MUIC_List, InputListFrame,
            MUIA_List_Format, (IPTR)"BAR,BAR,",
            MUIA_List_Title, TRUE,
            MUIA_List_DisplayHook, (IPTR)&display_hook,
            TAG_DONE);
    else
        l = MUI_NewObject(MUIC_List, InputListFrame, TAG_DONE);
    *out = l;
    return MUI_NewObject(MUIC_Listview, MUIA_Listview_List, (IPTR)l,
        MUIA_Weight, 100 + kind * 7, TAG_DONE);
}

int main(void)
{
    Object *app, *win, *lists[6], *lv[6], *root;
    const char **data[6] = { plain, plain, cols, styled, plain, cols };
    int kinds[6] = { 0, 0, 2, 0, 0, 2 };
    int i, rc = 20;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    for (i = 0; i < 6; i++)
        lv[i] = mklist(data[i], kinds[i], &lists[i]);
    app = MUI_NewObject(MUIC_Application,
        MUIA_Application_Title, (IPTR)"listclip",
        MUIA_Application_UseCommodities, FALSE,
        MUIA_Application_UseRexx, FALSE,
        SubWindow, (IPTR)(win = MUI_NewObject(MUIC_Window,
            MUIA_Window_Title, (IPTR)"listclip",
            MUIA_Window_LeftEdge, 10, MUIA_Window_TopEdge, 20,
            MUIA_Window_Width, 280, MUIA_Window_Height, 220,
            WindowContents, (IPTR)(root = MUI_NewObject(MUIC_Group,
                MUIA_Group_Columns, 2,
                Child, (IPTR)lv[0], Child, (IPTR)lv[1],
                Child, (IPTR)lv[2], Child, (IPTR)lv[3],
                Child, (IPTR)lv[4], Child, (IPTR)lv[5],
                TAG_DONE)),
            TAG_DONE)),
        TAG_DONE);
    if (!app)
        goto lib;
    for (i = 0; i < 6; i++)
        DoMethod(lists[i], MUIM_List_Insert, data[i], -1, MUIV_List_Insert_Bottom);
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Window, &window);
    if (!window)
        goto app;
    Delay(10);
    DoMethod(app, MUIM_Application_InputBuffered);
    dump("open");
    for (i = 0; i < 6; i++)
        set(lists[i], MUIA_List_Active, 1);
    dump("active1");
    for (i = 0; i < 6; i++)
        set(lists[i], MUIA_List_Active, 5);
    dump("active5");
    for (i = 0; i < 6; i++)
        set(lists[i], MUIA_List_Active, MUIV_List_Active_Bottom);
    dump("bottom");
    for (i = 0; i < 6; i++)
        set(lists[i], MUIA_List_Active, MUIV_List_Active_Up);
    dump("up");
    for (i = 0; i < 6; i++)
        set(lists[i], MUIA_List_Active, MUIV_List_Active_Top);
    dump("top");
    for (i = 0; i < 6; i++)
        DoMethod(lists[i], MUIM_List_Select, 2, MUIV_List_Select_On, NULL);
    dump("select2");
    MUI_Redraw(root, MADF_DRAWOBJECT);
    dump("redraw");
    DoMethod(root, MUIM_Group_InitChange);
    DoMethod(root, MUIM_Group_ExitChange);
    dump("relayout");
    rc = 0;
app:
    MUI_DisposeObject(app);
lib:
    CloseLibrary(MUIMasterBase);
    printf("rc=%d\n", rc);
    return rc;
}
