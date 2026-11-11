/* stringml: String's drawing that is in the MUI 4/5 module since branch
 * slim2: a Multiline String (MUIA_Textinput_Multiline) without and with a
 * marked range, a third one active with its cursor, and an empty String
 * with MUI 5's placeholder (MUIA_String_Placeholder). The window must look
 * the same on every library build. Waits for Ctrl-C (wasabi break), then
 * prints the cursor position a click into the first one gave. Built by hand:
 *   m68k-amigaos-gcc -m68000 -noixemul -Os -s -o stringml tests/lean/stringml.c
 */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>
#include <clib/alib_protos.h>
#include <stdio.h>

#ifndef IPTR
#define IPTR ULONG
#endif
/* Textinput.mcc's tag base, TAG_USER | (1307 << 16) + 0x712 */
#define TI(x) (0x851B0712UL + (x))
#define MULTILINE TI(100)
#define MARKSTART TI(133)
#define MARKEND TI(134)
#define PLACEHOLDER 0x8042ae65

struct Library *MUIMasterBase;

int main(void)
{
    Object *app, *win, *s1, *s2, *s3, *s4;
    static const char text[] = "First line\nSecond, longer line\nthird";

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject,
        MUIA_Application_Title, "stringml",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "stringml",
            MUIA_Window_ID, 0,
            WindowContents, VGroup,
                Child, s1 = StringObject, StringFrame,
                    MULTILINE, TRUE, MUIA_FixHeightTxt, "\n\n\n",
                    MUIA_String_Contents, text, End,
                Child, s2 = StringObject, StringFrame,
                    MULTILINE, TRUE, MUIA_FixHeightTxt, "\n\n\n",
                    MUIA_String_Contents, text,
                    MARKSTART, 6, MARKEND, 17, End,
                Child, s3 = StringObject, StringFrame,
                    MULTILINE, TRUE, MUIA_FixHeightTxt, "\n\n\n",
                    MUIA_String_Contents, text, End,
                Child, s4 = StringObject, StringFrame,
                    PLACEHOLDER, "Type here", End,
                End,
            End,
        End;
    if (!app)
    {
        CloseLibrary(MUIMasterBase);
        return 20;
    }
    set(win, MUIA_Window_Open, TRUE);
    set(win, MUIA_Window_ActiveObject, s3);
    {
        ULONG sigs = 0;

        while (DoMethod(app, MUIM_Application_NewInput, (IPTR)&sigs)
            != MUIV_Application_ReturnID_Quit)
            if (sigs && ((sigs = Wait(sigs | SIGBREAKF_CTRL_C))
                    & SIGBREAKF_CTRL_C))
                break;
    }
    {
        IPTR pos = 0;
        get(s3, TI(127), &pos);
        printf("cursor %lu\n", (unsigned long)pos);
    }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
