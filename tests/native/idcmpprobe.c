/* idcmpprobe: which IDCMP classes and window flags a MUI window has in
 * several situations (black box: Window->IDCMPFlags and ->Flags of the
 * open window): a plain window, one with an object that has a context
 * menu, one that asks for MUIA_Window_NeedsMouseObject, one whose object
 * has bubble help, one with a draggable object, one with a menu strip.
 * idcmpprobe [vN]   (vN: open muimaster.library version N, default 19)
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -DMUI_OBSOLETE idcmpprobe.c -o idcmpprobe -lmui -lamiga
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <intuition/intuition.h>
#include <libraries/gadtools.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#ifndef MUIA_Window_NeedsMouseObject
#define MUIA_Window_NeedsMouseObject 0x8042372a
#endif
struct Library *MUIMasterBase;
static struct NewMenu nm[] = {
    { NM_TITLE, "Context", 0, 0, 0, 0 }, { NM_ITEM, "One", 0, 0, 0, (APTR)1 },
    { NM_END, NULL, 0, 0, 0, 0 } };

static void show(const char *what, Object *app, Object *win)
{
    struct Window *w = NULL;
    ULONG sigs = 0;
    int i;

    set(win, MUIA_Window_Open, TRUE);
    for (i = 0; i < 10; i++) { Delay(2); DoMethod(app, MUIM_Application_NewInput, &sigs); }
    get(win, MUIA_Window_Window, &w);
    if (w)
        printf("%-22s idcmp=%08lx flags=%08lx%s%s%s%s\n", what,
            (unsigned long)w->IDCMPFlags, (unsigned long)w->Flags,
            w->IDCMPFlags & IDCMP_MOUSEMOVE ? " MOUSEMOVE" : "",
            w->IDCMPFlags & IDCMP_MENUVERIFY ? " MENUVERIFY" : "",
            w->Flags & WFLG_REPORTMOUSE ? " REPORTMOUSE" : "",
            w->Flags & WFLG_RMBTRAP ? " RMBTRAP" : "");
    else
        printf("%-22s no window\n", what);
    set(win, MUIA_Window_Open, FALSE);
}

/* idcmpprobe [vN] hold SECONDS: one window with a menu strip, a text with
 * a context menu, a text with bubble help and a string; prints where they
 * are on the screen, then four lines a second: the pointer's place, the
 * window's WFLG_RMBTRAP, its pointer image (MUI 4/5 give a string
 * another), the windows on the screen (a bubble is one more) and the
 * IDCMP classes. Move the pointer from outside meanwhile. */
static int hold(int seconds)
{
    Object *app, *win, *ctx, *help, *str, *strip, *cm;
    struct Window *w = NULL, *o;
    ULONG sigs = 0;
    int i, n;

    cm = MUI_MakeObject(MUIO_MenustripNM, nm, 0);
    strip = MUI_MakeObject(MUIO_MenustripNM, nm, 0);
    app = ApplicationObject, MUIA_Application_Title, "idcmpprobe",
        SubWindow, win = WindowObject, MUIA_Window_Title, "hold",
            MUIA_Window_Menustrip, strip,
            MUIA_Window_LeftEdge, 40, MUIA_Window_TopEdge, 30,
            WindowContents, VGroup,
                Child, ctx = TextObject, TextFrame,
                    MUIA_Text_Contents, "context menu here", MUIA_ContextMenu, cm, End,
                Child, help = TextObject, TextFrame,
                    MUIA_Text_Contents, "bubble help here", MUIA_ShortHelp, "A bubble", End,
                Child, str = StringObject, StringFrame, End,
                Child, SimpleButton("plain button"),
                End,
            End,
        End;
    if (!app) return 20;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Window, &w);
    if (!w) return 20;
    printf("context %d,%d help %d,%d string %d,%d button %d,%d\n",
        w->LeftEdge + _mleft(ctx) + 20, w->TopEdge + _mtop(ctx) + 3,
        w->LeftEdge + _mleft(help) + 20, w->TopEdge + _mtop(help) + 3,
        w->LeftEdge + _mleft(str) + 20, w->TopEdge + _mtop(str) + 3,
        w->LeftEdge + _mleft(str) + 20, w->TopEdge + _mbottom(str) + 12);
    for (i = 0; i < seconds * 4; i++)
    {
        int t;
        for (t = 0; t < 6; t++) { Delay(2); DoMethod(app, MUIM_Application_NewInput, &sigs); }
        for (n = 0, o = w->WScreen->FirstWindow; o; o = o->NextWindow) n++;
        printf("%3d mouse %d,%d rmbtrap=%d pointer=%s windows=%d idcmp=%08lx\n", i,
            w->WScreen->MouseX, w->WScreen->MouseY, !!(w->Flags & WFLG_RMBTRAP),
            w->Pointer ? "own" : "default", n, (unsigned long)w->IDCMPFlags);
    }
    MUI_DisposeObject(app);
    if (cm) MUI_DisposeObject(cm);
    return 0;
}

int main(int argc, char **argv)
{
    Object *app, *w[6], *strip, *cm;
    int v = 19, i;

    if (argc > 1 && argv[1][0] == 'v') v = atoi(argv[1] + 1), argc--, argv++;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", v))) return 20;
    if (argc > 2 && !strcmp(argv[1], "hold"))
    {
        i = hold(atoi(argv[2]));
        CloseLibrary(MUIMasterBase);
        return i;
    }
    cm = MUI_MakeObject(MUIO_MenustripNM, nm, 0);
    strip = MUI_MakeObject(MUIO_MenustripNM, nm, 0);
    app = ApplicationObject, MUIA_Application_Title, "idcmpprobe",
        SubWindow, w[0] = WindowObject, MUIA_Window_Title, "plain",
            WindowContents, VGroup, Child, SimpleButton("Button"),
                Child, StringObject, StringFrame, End, End, End,
        SubWindow, w[1] = WindowObject, MUIA_Window_Title, "context menu",
            WindowContents, VGroup, Child, TextObject, TextFrame,
                MUIA_Text_Contents, "context", MUIA_ContextMenu, cm, End, End, End,
        SubWindow, w[2] = WindowObject, MUIA_Window_Title, "mouse object",
            MUIA_Window_NeedsMouseObject, TRUE,
            WindowContents, VGroup, Child, SimpleButton("Button"), End, End,
        SubWindow, w[3] = WindowObject, MUIA_Window_Title, "bubble",
            WindowContents, VGroup, Child, TextObject, TextFrame,
                MUIA_Text_Contents, "help", MUIA_ShortHelp, "A bubble", End, End, End,
        SubWindow, w[4] = WindowObject, MUIA_Window_Title, "draggable",
            WindowContents, VGroup, Child, TextObject, TextFrame,
                MUIA_Text_Contents, "drag", MUIA_Draggable, TRUE, End, End, End,
        SubWindow, w[5] = WindowObject, MUIA_Window_Title, "menu strip",
            MUIA_Window_Menustrip, strip,
            WindowContents, VGroup, Child, SimpleButton("Button"), End, End,
        End;
    if (!app) { printf("no application\n"); return 20; }
    {
        static const char *const names[] = { "plain", "context menu",
            "needs mouse object", "bubble help", "draggable", "menu strip" };
        for (i = 0; i < 6; i++) show(names[i], app, w[i]);
    }
    MUI_DisposeObject(app);
    if (cm) MUI_DisposeObject(cm);
    CloseLibrary(MUIMasterBase);
    return 0;
}
