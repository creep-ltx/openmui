/* winattrs: every Window attribute OM_GET answers, for windows made with
 * different OM_NEW tags, closed, open, after a move and a resize, asleep,
 * iconified and back. Pointers print as which object or 0/1. The same
 * output on two libraries = the same OM_NEW, OM_GET and OM_SET answers.
 * Build (I = OpenMUI's library build): m68k-amigaos-gcc -m68000 -noixemul \
 *   -Os -DMUI_OBSOLETE -I$I/include -Iworkbench/libs/muimaster/include \
 *   tests/lean/winattrs.c -o winattrs -lamiga
 */
#include <stdio.h>
#include <libraries/mui.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/dos.h>
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

static Object *app, *w1, *w2, *b1, *b2, *root1, *root2;
static const char t1[] = "first", t2[] = "second", st[] = "screen title";

static const struct { ULONG attr; const char *name; int kind; } attrs[] = {
    /* kind 0: number, 1: pointer (named), 2: boolean */
    { MUIA_Window_Activate, "Activate", 0 },
    { MUIA_Window_ActiveObject, "ActiveObject", 1 },
    { MUIA_Window_AltHeight, "AltHeight", 0 },
    { MUIA_Window_AltLeftEdge, "AltLeftEdge", 0 },
    { MUIA_Window_AltTopEdge, "AltTopEdge", 0 },
    { MUIA_Window_AltWidth, "AltWidth", 0 },
    { MUIA_Window_CloseRequest, "CloseRequest", 0 },
    { MUIA_Window_DefaultObject, "DefaultObject", 1 },
    { MUIA_Window_DisableKeys, "DisableKeys", 0 },
    { MUIA_Window_Height, "Height", 0 },
    { MUIA_Window_ID, "ID", 0 },
    { MUIA_Window_IsSubWindow, "IsSubWindow", 0 },
    { MUIA_Window_LeftEdge, "LeftEdge", 0 },
    { MUIA_Window_Menustrip, "Menustrip", 2 },
    { MUIA_Window_Open, "Open", 0 },
    { MUIA_Window_PublicScreen, "PublicScreen", 2 },
    { MUIA_Window_RootObject, "RootObject", 1 },
    { MUIA_Window_Screen, "Screen", 2 },
    { MUIA_Window_ScreenTitle, "ScreenTitle", 1 },
    { MUIA_Window_Sleep, "Sleep", 0 },
    { MUIA_Window_Title, "Title", 1 },
    { MUIA_Window_TopEdge, "TopEdge", 0 },
    { MUIA_Window_Width, "Width", 0 },
    { MUIA_Window_Window, "Window", 2 },
    { MUIA_WindowObject, "WindowObject", 1 },
};

static const char *name(IPTR p)
{
    static char buf[16];
    if (!p) return "0";
    if (p == (IPTR)w1) return "w1";
    if (p == (IPTR)w2) return "w2";
    if (p == (IPTR)b1) return "b1";
    if (p == (IPTR)b2) return "b2";
    if (p == (IPTR)root1) return "root1";
    if (p == (IPTR)root2) return "root2";
    if (p == (IPTR)t1) return "t1";
    if (p == (IPTR)t2) return "t2";
    if (p == (IPTR)st) return "st";
    sprintf(buf, "other");
    return buf;
}

static void dump(Object *w, const char *what)
{
    int i;
    printf("-- %s\n", what);
    for (i = 0; i < sizeof(attrs) / sizeof(attrs[0]); i++)
    {
        IPTR v = 0xdeadbeef;
        ULONG ok = GetAttr(attrs[i].attr, w, &v);
        if (attrs[i].kind == 0)
            printf("%s=%ld(%ld) ", attrs[i].name, (long)v, (long)ok);
        else if (attrs[i].kind == 1)
            printf("%s=%s(%ld) ", attrs[i].name, name(v), (long)ok);
        else
            printf("%s=%s(%ld) ", attrs[i].name, v == 0xdeadbeef ? "unset"
                : v ? "set" : "0", (long)ok);
    }
    printf("\n");
}

int main(void)
{
    ULONG sigs = 0;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject, MUIA_Application_Title, "winattrs",
        SubWindow, w1 = WindowObject,
            MUIA_Window_Title, t1,
            MUIA_Window_ScreenTitle, st,
            MUIA_Window_ID, MAKE_ID('W','A','T','1'),
            MUIA_Window_AltLeftEdge, 10, MUIA_Window_AltTopEdge, 20,
            MUIA_Window_AltWidth, 300, MUIA_Window_AltHeight, -5,
            MUIA_Window_LeftEdge, 30, MUIA_Window_TopEdge, 25,
            MUIA_Window_DisableKeys, 0x5,
            MUIA_Window_IsSubWindow, TRUE,
            MUIA_Window_Opacity, 300,
            WindowContents, root1 = VGroup,
                Child, b1 = SimpleButton("One"),
                Child, StringObject, StringFrame, End,
            End,
            MUIA_Window_DefaultObject, b1,
        End,
        SubWindow, w2 = WindowObject,
            MUIA_Window_Title, t2,
            MUIA_Window_Activate, FALSE,
            MUIA_Window_Width, MUIV_Window_Width_Screen(30),
            MUIA_Window_Height, MUIV_Window_Height_MinMax(50),
            MUIA_Window_RefWindow, w1,
            MUIA_Window_NoMenus, TRUE,
            MUIA_Window_CloseGadget, FALSE,
            WindowContents, root2 = HGroup,
                Child, b2 = SimpleButton("Two"),
            End,
        End,
    End;
    if (!app)
    {
        printf("no application\n");
        return 20;
    }
    dump(w1, "w1 closed");
    dump(w2, "w2 closed");
    set(w1, MUIA_Window_Open, TRUE);
    set(w2, MUIA_Window_Open, TRUE);
    DoMethod(app, MUIM_Application_NewInput, &sigs);
    set(w1, MUIA_Window_ActiveObject, b1);
    dump(w1, "w1 open");
    dump(w2, "w2 open");
    {
        struct Window *nw = NULL;
        get(w1, MUIA_Window_Window, &nw);
        if (nw)
        {
            ChangeWindowBox(nw, 50, 40, nw->Width + 30, nw->Height + 10);
            Delay(10);
            DoMethod(app, MUIM_Application_NewInput, &sigs);
            Delay(10);
            DoMethod(app, MUIM_Application_NewInput, &sigs);
        }
    }
    dump(w1, "w1 moved and sized");
    set(w1, MUIA_Window_Sleep, TRUE);
    set(w1, MUIA_Window_Sleep, TRUE);
    dump(w1, "w1 asleep twice");
    set(w1, MUIA_Window_Sleep, FALSE);
    set(w1, MUIA_Window_Sleep, FALSE);
    set(w1, MUIA_Window_Sleep, FALSE);
    set(w1, MUIA_Window_Title, t2);
    set(w1, MUIA_Window_DefaultObject, NULL);
    dump(w1, "w1 awake, retitled");
    set(app, MUIA_Application_Iconified, TRUE);
    dump(w1, "w1 iconified");
    set(app, MUIA_Application_Iconified, FALSE);
    DoMethod(app, MUIM_Application_NewInput, &sigs);
    dump(w1, "w1 back");
    dump(w2, "w2 back");
    set(w2, MUIA_Window_Open, FALSE);
    dump(w2, "w2 closed again");
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("done\n");
    return 0;
}
