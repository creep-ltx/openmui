/* winreqsize: the window size after only MUIA_Window_Height, then only MUIA_Window_Width, was set
 * between two opens (black box; MUI 3.8 and MUI 5: the requested height, then the requested width
 * with that height kept; OpenMUI before 2026-11-26 gave the minimum height).
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -Wl,-u,___stkinit winreqsize.c -o winreqsize -lamiga
 * Output: "first: WxH", "height150: WxH", "width300: WxH" (the intuition window's outer size)
 * MUI 3.8: 72x42, 72x150, 300x150 (OS 3.1, topaz 8; MUI 5 on 3.2.1: 80x57, 80x150, 300x150) */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
struct Library *MUIMasterBase;
ULONG __stack = 65536;
static Object *app;
static void sizes(Object *win, const char *what)
{
    struct Window *w = NULL;
    int i;
    set(win, MUIA_Window_Open, TRUE);
    for (i = 0; i < 4; i++) { DoMethod(app, MUIM_Application_InputBuffered); Delay(2); }
    get(win, MUIA_Window_Window, &w);
    printf("%s: %ldx%ld\n", what, w ? (long)w->Width : -1L, w ? (long)w->Height : -1L);
    set(win, MUIA_Window_Open, FALSE);
}
int main(void)
{
    Object *win;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject, MUIA_Application_Title, "winreqsize",
        SubWindow, win = WindowObject, MUIA_Window_Title, "winreqsize",
            WindowContents, VGroup, Child, SimpleButton("Button"), Child, RectangleObject, End, End,
        End, End;
    if (!app) return 20;
    sizes(win, "first");
    set(win, MUIA_Window_Height, 150);
    sizes(win, "height150");
    set(win, MUIA_Window_Width, 300);
    sizes(win, "width300");
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
