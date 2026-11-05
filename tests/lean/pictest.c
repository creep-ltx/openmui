/* pictest: a window with a brush image (4:), a bitmap background (5:) and
 * a gradient background (7:), for the picture and gradient modules.
 * PICTURE = the picture file (default RAM:pattern.iff). Test tool only. */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>

struct Library *MUIMasterBase;

int main(int argc, char **argv)
{
    Object *app, *win;
    char brush[120], bitmap[120];
    const char *pic = argc > 1 ? argv[1] : "RAM:pattern.iff";
    ULONG sigs = 0;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    snprintf(brush, sizeof(brush), "4:%s", pic);
    snprintf(bitmap, sizeof(bitmap), "5:%s", pic);
    app = ApplicationObject,
        MUIA_Application_Title, "pictest",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "pictest",
            MUIA_Window_ID, 0,
            WindowContents, VGroup,
                Child, TextObject, MUIA_Text_Contents, "brush (4:)", End,
                Child, ImageObject, MUIA_Image_Spec, brush,
                    MUIA_Frame, MUIV_Frame_ImageButton, End,
                Child, TextObject, MUIA_Text_Contents, "bitmap background (5:)",
                    End,
                Child, RectangleObject, MUIA_Background, bitmap,
                    MUIA_FixHeight, 40, MUIA_FixWidth, 160, End,
                Child, TextObject, MUIA_Text_Contents, "gradient (7:)", End,
                Child, RectangleObject, MUIA_Background,
                    "7:v,ffffffff,00000000,00000000-00000000,00000000,ffffffff",
                    MUIA_FixHeight, 20, End,
                End,
            End,
        End;
    if (!app)
    {
        CloseLibrary(MUIMasterBase);
        return 20;
    }
    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, app, 2,
        MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    set(win, MUIA_Window_Open, TRUE);
    while (DoMethod(app, MUIM_Application_NewInput, &sigs)
        != MUIV_Application_ReturnID_Quit)
    {
        if (sigs)
        {
            sigs = Wait(sigs | SIGBREAKF_CTRL_C);
            if (sigs & SIGBREAKF_CTRL_C)
                break;
        }
    }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
