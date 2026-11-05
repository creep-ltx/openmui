/* framethick W H MODEID [auto|thick|thin]: open a 16-colour custom screen of that size and mode
 * (pens: SHINE white, SHADOW black, BACKGROUND grey, half pens allocated by the library), an
 * MUI window on it with a text 'B' in every frame type 0..10 raised and 1..10 recessed (spec
 * t s 2 2 1 1) and the MUI 3.8 width test buttons; print each frame's offsets and its pixel map
 * (6 left and 6 right columns, as maps.md: W shine, w halfshine, B background, d halfshadow,
 * K shadow) and the buttons' min/default widths. The thickness setting goes into the
 * application's own prefs file ENV:zune/FThick.1.prefs (auto: the file is deleted; a fifth
 * argument keeps it). Build like the other lean tests (GCC 16.2: -Wno-int-conversion, -lmui). */
#include <libraries/mui.h>
#include <graphics/modeid.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MUICFG_Frame_Thickness
#define MUICFG_Frame_Thickness 0x50b
#endif

struct Library *MUIMasterBase;
static const char *specs[21] = {
    "002211", "102211", "202211", "302211", "402211", "502211", "602211", "702211", "802211",
    "902211", "a02211", "112211", "212211", "312211", "412211", "512211", "612211", "712211",
    "812211", "912211", "a12211" };

static char letter(struct MUI_RenderInfo *mri, ULONG pen)
{
    if (pen == mri->mri_Pens[MPEN_SHINE]) return 'W';
    if (pen == mri->mri_Pens[MPEN_HALFSHINE]) return 'w';
    if (pen == mri->mri_Pens[MPEN_BACKGROUND]) return 'B';
    if (pen == mri->mri_Pens[MPEN_HALFSHADOW]) return 'd';
    if (pen == mri->mri_Pens[MPEN_SHADOW]) return 'K';
    return '0' + (pen & 15) > '9' ? 'a' + (pen & 15) - 10 : '0' + (pen & 15);
}

int main(int argc, char **argv)
{
    static UWORD pens[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, (UWORD)~0 };
    /* DETAIL BLOCK TEXT SHINE SHADOW FILL FILLTEXT BACKGROUND HIGHLIGHT BARDETAIL BARBLOCK BARTRIM */
    static ULONG cols[] = { 12L << 16 | 0,
        0x55555555, 0x55555555, 0x55555555,  0x22222222, 0x22222222, 0x66666666,
        0xffffffff, 0, 0,                    0xffffffff, 0xffffffff, 0xffffffff,
        0, 0, 0,                             0, 0, 0xffffffff,
        0x33333333, 0x33333333, 0x33333333,  0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa,
        0x11111111, 0x88888888, 0x11111111,  0x99999999, 0x22222222, 0x99999999,
        0x44444444, 0x44444444, 0x99999999,  0x66666666, 0x22222222, 0x22222222, 0 };
    static const char *labels[8] = { "B", "Abc", "WWW", "iii", "_Ok", "Button", "ButtonText", "Button" };
    Object *app, *win, *frames[21], *buttons[8], *row1, *row2, *row3;
    struct Screen *scr;
    struct Window *iw = NULL;
    int i, x, y, w = argc > 1 ? atoi(argv[1]) : 640, h = argc > 2 ? atoi(argv[2]) : 256;
    ULONG mode = argc > 3 ? strtoul(argv[3], NULL, 16) : HIRES_KEY;
    const char *th = argc > 4 ? argv[4] : "auto";

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    if (strcmp(th, "auto")) {
        Object *c = MUI_NewObject(MUIC_Configdata, TAG_DONE);
        if (c) {
            DoMethod(c, MUIM_Configdata_SetULong, MUICFG_Frame_Thickness, strcmp(th, "thick") ? 2 : 1);
            DoMethod(c, MUIM_Configdata_Save, (IPTR)"ENV:zune/FThick.1.prefs");
            MUI_DisposeObject(c);
        }
    } else
        DeleteFile("ENV:zune/FThick.1.prefs");
    scr = OpenScreenTags(NULL, SA_Width, w, SA_Height, h, SA_DisplayID, mode, SA_Depth, 4,
        SA_Pens, (IPTR)pens, SA_SharePens, TRUE, SA_Colors32, (IPTR)cols, SA_Title, (IPTR)"FThick",
        SA_Type, CUSTOMSCREEN, TAG_DONE);
    if (!scr) { printf("no screen\n"); CloseLibrary(MUIMasterBase); return 20; }
    row1 = HGroup, End; row2 = HGroup, End; row3 = HGroup, End;
    app = ApplicationObject, MUIA_Application_Title, (IPTR)"FThick", MUIA_Application_Base, (IPTR)"FThick",
        SubWindow, (IPTR)(win = WindowObject, MUIA_Window_Title, (IPTR)"FThick", MUIA_Window_Screen, (IPTR)scr,
            MUIA_Window_ID, 0,
            WindowContents, (IPTR)VGroup, Child, (IPTR)row1, Child, (IPTR)row2, Child, (IPTR)row3, End, End), End;
    if (!app) { printf("no app\n"); CloseScreen(scr); CloseLibrary(MUIMasterBase); return 20; }
    for (i = 0; i < 21; i++) {
        frames[i] = TextObject, MUIA_Frame, (IPTR)specs[i], MUIA_Background, MUII_BACKGROUND,
            MUIA_Text_Contents, (IPTR)"B", MUIA_Text_PreParse, (IPTR)"\33c", MUIA_Text_SetMax, TRUE, End;
        DoMethod(i < 11 ? row1 : row2, OM_ADDMEMBER, (IPTR)frames[i]);
    }
    for (i = 0; i < 8; i++) {
        buttons[i] = i < 7 ? SimpleButton(labels[i]) :
            TextObject, ButtonFrame, MUIA_Text_Contents, (IPTR)labels[i], MUIA_Text_PreParse, (IPTR)"\33l",
                MUIA_Background, MUII_ButtonBack, MUIA_InputMode, MUIV_InputMode_RelVerify, End;
        DoMethod(row3, OM_ADDMEMBER, (IPTR)buttons[i]);
    }
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Window, &iw);
    if (!iw) { printf("no window\n"); goto out; }
    Delay(25);
    {
        struct MUI_RenderInfo *mri = muiRenderInfo(frames[0]);
        printf("screen %dx%d mode %08lx setting %s thinframes %ld window %ldx%ld\n", w, h, mode, th,
            (long)((mri->mri_Flags & MUIMRI_THINFRAMES) != 0), (long)iw->Width, (long)iw->Height);
        for (i = 0; i < 8; i++)
            printf("button \"%s\"%s min %ld def %ld height %ld add L%ld sw%ld\n", labels[i], i == 7 ? " left" : "",
                (long)_minwidth(buttons[i]), (long)_defwidth(buttons[i]), (long)_minheight(buttons[i]),
                (long)_addleft(buttons[i]), (long)_subwidth(buttons[i]));
        for (i = 0; i < 21; i++) {
            Object *o = frames[i];
            printf("## spec %s add L%ld T%ld sw%ld sh%ld box %ldx%ld\n", specs[i], (long)_addleft(o),
                (long)_addtop(o), (long)_subwidth(o), (long)_subheight(o), (long)_width(o), (long)_height(o));
            for (y = _top(o); y <= _bottom(o); y++) {
                char line[20], *p = line;
                for (x = _left(o); x < _left(o) + 6; x++) *p++ = letter(mri, ReadPixel(iw->RPort, x, y));
                *p++ = '.'; *p++ = '.';
                for (x = _right(o) - 5; x <= _right(o); x++) *p++ = letter(mri, ReadPixel(iw->RPort, x, y));
                *p = 0;
                printf("%s\n", line);
            }
        }
    }
out:
    fflush(stdout);
    MUI_DisposeObject(app);
    CloseScreen(scr);
    if (argc < 6) DeleteFile("ENV:zune/FThick.1.prefs");
    CloseLibrary(MUIMasterBase);
    return 0;
}
