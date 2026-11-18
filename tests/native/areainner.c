/* Where MUI keeps MUIA_InnerLeft/Top/Right/Bottom and the frame title in its private area data (80-107). */
#include <stdio.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
struct Library *MUIMasterBase;
ULONG __stack = 65536;
static void dump(const char *n, Object *o)
{
    UBYTE *ad = (UBYTE *)muiAreaData(o); int i;
    printf("%s:", n);
    for (i = 80; i < 108; i++) printf(" %d:%02x", i, ad[i]);
    printf("\n");
}
static void line(Object *o)
{
    UBYTE *ad = (UBYTE *)muiAreaData(o);
    printf("RESULT inner %02x %02x %02x %02x ctrl %02x\n", ad[88], ad[89], ad[90], ad[91], ad[96]);
}
int main(void)
{
    Object *app, *win, *t, *g;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject, MUIA_Application_Title, "innerprobe",
        SubWindow, win = WindowObject, MUIA_Window_Title, "innerprobe",
            WindowContents, g = VGroup, MUIA_Frame, MUIV_Frame_Group, MUIA_FrameTitle, "Title",
                Child, t = TextObject, MUIA_Frame, MUIV_Frame_Text, MUIA_Text_Contents, "hello",
                    MUIA_InnerLeft, 3, MUIA_InnerTop, 5, MUIA_InnerRight, 7, MUIA_InnerBottom, 9,
                    MUIA_ControlChar, 'q',
                End,
            End,
        End,
    End;
    if (!app) return 20;
    set(win, MUIA_Window_Open, TRUE);
    printf("title ptr %08lx\n", (ULONG)"Title");
    line(t); dump("text", t); dump("group", g);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
