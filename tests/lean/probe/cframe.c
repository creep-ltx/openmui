/* cframe DIR: OpenMUI's custom frames (the settings' FST_CUSTOM1, a frame.config with
 * up/default and down/default pictures in DIR): sets custom frame 1 to DIR/frame.config
 * in this program's settings, opens a window with Rectangles of frame "c00000" (up) and
 * "c10000" (down) and a toggle button, prints their sizes and keeps the window open 20 s
 * for a screenshot. MUI 3.8 has no custom frames (an OpenMUI/Zune setting). A test frame:
 * tests/lean/probe/cframe-theme (24x24 ILBMs, 6-pixel tiles, 5 pixels inner spacing); put it
 * with PROBE_PUT="cframe-theme/frame.config=RAM:cf/frame.config ..." (probe.sh).
 * Build: m68k-amigaos-gcc -noixemul -Os -s -I<build>/include cframe.c -o cframe -lamiga */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <string.h>

/* OpenMUI's (its libraries/mui.h with -I<build>/include) */
#ifndef MUICFG_CustomFrame_1
#define MUICFG_CustomFrame_1 0x600
#endif

struct Library *MUIMasterBase;
ULONG __stack = 65536;

int main(int argc, char **argv)
{
    static char path[200];
    Object *app, *win, *a, *b, *t, *c;
    IPTR open = 0;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    strcpy(path, argc > 1 ? argv[1] : "RAM:cf");
    AddPart(path, "frame.config", sizeof(path));
    if ((c = MUI_NewObject("Configdata.mui", TAG_DONE)))
    {
        DoMethod(c, MUIM_Configdata_SetString, MUICFG_CustomFrame_1, (IPTR)path);
        DoMethod(c, MUIM_Configdata_Save, (IPTR)"ENV:OpenMUI/CFRM.1.prefs");
        MUI_DisposeObject(c);
    }
    app = ApplicationObject, MUIA_Application_Title, (IPTR)"CFRM", MUIA_Application_Base, (IPTR)"CFRM",
        SubWindow, (IPTR)(win = WindowObject, MUIA_Window_Title, (IPTR)"CFRM", MUIA_Window_ID, 0,
            WindowContents, (IPTR)(HGroup,
                Child, (IPTR)(a = RectangleObject, MUIA_Frame, (IPTR)"c00000", MUIA_FixWidth, 60, MUIA_FixHeight, 30, End),
                Child, (IPTR)(b = RectangleObject, MUIA_Frame, (IPTR)"c10000", MUIA_FixWidth, 60, MUIA_FixHeight, 30, End),
                Child, (IPTR)(t = TextObject, MUIA_Frame, (IPTR)"c00000", MUIA_InputMode, MUIV_InputMode_Toggle,
                    MUIA_Text_Contents, (IPTR)"Toggle", MUIA_Selected, TRUE, End),
                End), End), End;
    if (!app) { printf("no app\n"); return 20; }
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    printf("open %ld: up %dx%d (inner %d,%d), down %dx%d, toggle %dx%d\n", (long)open,
        _width(a), _height(a), _mleft(a) - _left(a), _mtop(a) - _top(a), _width(b), _height(b),
        _width(t), _height(t));
    fflush(stdout);
    Delay(argc > 2 ? 1000 : 50);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
