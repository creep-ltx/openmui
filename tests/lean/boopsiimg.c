/* boopsiimg: external BOOPSI image classes in image specs ("3:<name>.image",
 * imspec_boopsi.c). A window with each image at its own size, the first
 * one as a toggle button (selected state) and stretched in a fixed box;
 * prints each image object's size once the window is open, so that Zune
 * and a real MUI can be compared. Run until Ctrl-C or the close gadget.
 *   boopsiimg [SPEC...]   (default: four kmel images from MUI:Images)
 * Build: m68k-amigaos-gcc -noixemul -Os -I<build>/include
 *   -Iworkbench/libs/muimaster/include -o boopsiimg boopsiimg.c -lmui
 * Test tool only. */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>

struct Library *MUIMasterBase;

static const char *const defaults[] = {
    "3:kmel/kmel_arrowdown.image", "3:kmel/kmel_checkmark.image",
    "3:kmel/kmel_cycle.image", "3:kmel/kmel_popdrawer.image",
};

#define MAXIMG 8

int main(int argc, char **argv)
{
    Object *app, *win, *grp, *img[MAXIMG], *button, *stretched;
    const char *specs[MAXIMG];
    int n = 0, i;
    ULONG sigs = 0;

    for (i = 1; i < argc && n < MAXIMG; i++)
        specs[n++] = argv[i];
    if (!n)
        for (i = 0; i < 4; i++)
            specs[n++] = defaults[i];
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    app = ApplicationObject,
        MUIA_Application_Title, "boopsiimg",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "boopsiimg",
            MUIA_Window_ID, 0,
            WindowContents, VGroup,
                Child, grp = ColGroup(2), End,
                Child, HGroup,
                    Child, TextObject, MUIA_Text_Contents, "toggle", End,
                    Child, button = ImageObject, MUIA_Image_Spec, specs[0],
                        MUIA_InputMode, MUIV_InputMode_Toggle,
                        MUIA_Frame, MUIV_Frame_ImageButton, End,
                    Child, TextObject, MUIA_Text_Contents, "40x24", End,
                    Child, stretched = ImageObject, MUIA_Image_Spec, specs[0],
                        MUIA_Image_FreeHoriz, TRUE, MUIA_Image_FreeVert, TRUE,
                        MUIA_FixWidth, 40, MUIA_FixHeight, 24, End,
                End,
            End,
        End,
    End;
    if (!app)
    {
        CloseLibrary(MUIMasterBase);
        return 20;
    }
    for (i = 0; i < n; i++)
    {
        Object *label = TextObject, MUIA_Text_Contents, specs[i], End;

        img[i] = ImageObject, MUIA_Image_Spec, specs[i], End;
        if (label && img[i])
        {
            DoMethod(grp, OM_ADDMEMBER, label);
            DoMethod(grp, OM_ADDMEMBER, img[i]);
        }
        else
            img[i] = NULL;
    }
    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, app, 2,
        MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    set(win, MUIA_Window_Open, TRUE);
    for (i = 0; i < n; i++)
        if (img[i])
            printf("%s %ldx%ld\n", specs[i], (long)_mwidth(img[i]),
                (long)_mheight(img[i]));
    printf("toggle %ldx%ld stretched %ldx%ld\n", (long)_mwidth(button),
        (long)_mheight(button), (long)_mwidth(stretched),
        (long)_mheight(stretched));
    fflush(stdout);
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
