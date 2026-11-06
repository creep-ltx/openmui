/* smaller: one window with everything the lean edition's "smaller library"
 * step (roadmap phase 3) moved into modules, to compare its pixels with
 * the build before: the rounded frame types 5, 7, 8, A and B (Zframes.mui),
 * the vector images MUII_Drawer to MUII_TapeDown (Zimages.mui), and a
 * button whose press opens a menu strip with MUIM_Menustrip_Popup
 * (Zmenu.mui; the pop-up stays until Esc or a click). The window has an ID,
 * so the title bar gadgets (Zwintitle.mui) show when the settings switch
 * them on. Test tool only.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -I<build>/include \
 *        -Wl,-u,___stkinit smaller.c -o smaller -lamiga */
#include <libraries/mui.h>
#include <libraries/gadtools.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 32768;

static Object *framed(const char *spec)
{
    return TextObject, MUIA_Frame, (IPTR)spec, MUIA_Text_Contents, spec,
        MUIA_Text_PreParse, "\33c", MUIA_InnerLeft, 4, MUIA_InnerRight, 4,
        End;
}

static Object *image(ULONG spec)
{
    return ImageObject, MUIA_Image_Spec, spec, MUIA_Frame, MUIV_Frame_None,
        End;
}

static struct NewMenu menu[] = {
    { NM_TITLE, "Pop", 0, 0, 0, 0 },
    { NM_ITEM, "First item", 0, 0, 0, (APTR)1 },
    { NM_ITEM, "Second item", 0, 0, 0, (APTR)2 },
    { NM_END, NULL, 0, 0, 0, 0 }
};

int main(void)
{
    Object *app, *win, *pop, *strip;
    ULONG sigs = 0, id;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    strip = MUI_MakeObject(MUIO_MenustripNM, (IPTR)menu, 0);
    app = ApplicationObject,
        MUIA_Application_Title, "smaller",
        MUIA_Application_Base, "SMALLER",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, "smaller",
            MUIA_Window_ID, MAKE_ID('S', 'M', 'L', 'R'),
            WindowContents, VGroup,
                Child, HGroup,
                    Child, framed("502222"), Child, framed("702222"),
                    Child, framed("802222"), Child, framed("A02222"),
                    Child, framed("B02222"),
                    End,
                Child, HGroup,
                    Child, image(MUII_Drawer), Child, image(MUII_HardDisk),
                    Child, image(MUII_Disk), Child, image(MUII_Chip),
                    Child, image(MUII_Volume), Child, image(MUII_Network),
                    Child, image(MUII_Assign),
                    End,
                Child, HGroup,
                    Child, image(MUII_TapePlay), Child, image(MUII_TapePlayBack),
                    Child, image(MUII_TapePause), Child, image(MUII_TapeStop),
                    Child, image(MUII_TapeRecord), Child, image(MUII_TapeUp),
                    Child, image(MUII_TapeDown),
                    End,
                Child, pop = SimpleButton("_Pop-up menu"),
                End,
            End,
        End;
    if (!app)
    {
        if (strip)
            MUI_DisposeObject(strip);
        CloseLibrary(MUIMasterBase);
        return 20;
    }
    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, app, 2,
        MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    DoMethod(pop, MUIM_Notify, MUIA_Pressed, FALSE, app, 2,
        MUIM_Application_ReturnID, 1);
    set(win, MUIA_Window_Open, TRUE);
    while ((id = DoMethod(app, MUIM_Application_NewInput, &sigs))
        != MUIV_Application_ReturnID_Quit)
    {
        if (id == 1 && strip)
            DoMethod(strip, MUIM_Menustrip_Popup, (IPTR)pop, 0, 10, 10);
        if (sigs)
        {
            sigs = Wait(sigs | SIGBREAKF_CTRL_C);
            if (sigs & SIGBREAKF_CTRL_C)
                break;
        }
    }
    MUI_DisposeObject(app);
    if (strip)
        MUI_DisposeObject(strip);
    CloseLibrary(MUIMasterBase);
    return 0;
}
