/* ghost [MODE]: disabled objects, for comparing their look.
 *  row 1: button disabled at creation, enabled button, string disabled,
 *         checkmark disabled, cycle disabled
 *  group 2 (framed, titled) disabled at creation: button, string, slider
 *  row 3: button disabled after the window opened
 *  list disabled (listview with 3 entries)
 * MODE 1: also put the window to sleep after 1 s (MUIA_Window_Sleep).
 * MODE 2: disable the whole root group after opening. */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <stdlib.h>

struct Library *MUIMasterBase;
static const char *cyc[] = { "One", "Two", NULL };
static const char *ents[] = { "Alpha", "Beta", "Gamma", NULL };

int main(int argc, char **argv)
{
    Object *app, *win, *b3, *root, *lv, *lst;
    int mode = argc > 1 ? atoi(argv[1]) : 0;
    int secs = argc > 2 ? atoi(argv[2]) : 20;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject, MUIA_Application_Title, (IPTR)"Ghost", MUIA_Application_Base, (IPTR)"GHOST",
        SubWindow, (IPTR)(win = WindowObject, MUIA_Window_Title, (IPTR)"Ghost",
            WindowContents, (IPTR)(root = VGroup,
                Child, (IPTR)(HGroup,
                    Child, (IPTR)(TextObject, ButtonFrame, MUIA_Background, MUII_ButtonBack,
                        MUIA_Text_Contents, (IPTR)"\33cDisabled", MUIA_InputMode, MUIV_InputMode_RelVerify,
                        MUIA_Disabled, TRUE, End),
                    Child, (IPTR)SimpleButton("Enabled"),
                    Child, (IPTR)(StringObject, StringFrame, MUIA_String_Contents, (IPTR)"String", MUIA_Disabled, TRUE, End),
                    Child, (IPTR)(ImageObject, ImageButtonFrame, MUIA_Image_Spec, MUII_CheckMark,
                        MUIA_InputMode, MUIV_InputMode_Toggle, MUIA_Selected, TRUE, MUIA_Background, MUII_ButtonBack,
                        MUIA_ShowSelState, FALSE, MUIA_Disabled, TRUE, End),
                    Child, (IPTR)(CycleObject, MUIA_Cycle_Entries, (IPTR)cyc, MUIA_Disabled, TRUE, End),
                    End),
                Child, (IPTR)(HGroup, GroupFrameT("Group disabled"), MUIA_Disabled, TRUE,
                    Child, (IPTR)SimpleButton("In group"),
                    Child, (IPTR)(StringObject, StringFrame, MUIA_String_Contents, (IPTR)"Str", End),
                    Child, (IPTR)(SliderObject, MUIA_Numeric_Value, 5, End),
                    End),
                Child, (IPTR)(HGroup,
                    Child, (IPTR)(b3 = SimpleButton("Later")),
                    Child, (IPTR)(TextObject, TextFrame, MUIA_Background, MUII_TextBack, MUIA_Text_Contents, (IPTR)"Text", MUIA_Disabled, TRUE, End),
                    End),
                Child, (IPTR)(lv = ListviewObject, MUIA_Disabled, TRUE, MUIA_Listview_List, (IPTR)(lst = ListObject, InputListFrame,
                    MUIA_List_SourceArray, (IPTR)ents, End), End),
                End),
            End), End;
    if (!app) { printf("no app\n"); return 20; }
    set(win, MUIA_Window_Open, TRUE);
    Delay(50);
    set(b3, MUIA_Disabled, TRUE);
    if (mode == 1) { Delay(50); set(win, MUIA_Window_Sleep, TRUE); }
    if (mode == 2) { Delay(50); set(root, MUIA_Disabled, TRUE); }
    printf("ready\n"); fflush(stdout);
    Delay(secs * 50);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
