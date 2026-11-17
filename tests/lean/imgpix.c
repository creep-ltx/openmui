/* imgpix [maps]: every built-in vector image's pixels. A 16-colour 640x480 screen (framepix.c's
 * pens), Image objects "1:0" to "1:23" (arrows, check mark, radio, cycle, pop-ups, drawer, disks,
 * tape buttons) free in both directions, normal and selected, at their minimum size, 5x3 larger
 * and 36x30, the first ten also inside a MUII_SHINEBACK group, and the four arrows at 72x40 and
 * 40x72 (shapes wider or taller than 32 pixels). One line per object: its spec, state, box and a
 * checksum of every pixel's pen; "maps" adds the pixels. Compare OpenMUI builds line for line.
 * Build: as framepix.c. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>


struct Library *MUIMasterBase;
ULONG __stack = 65536;

static const UBYTE mins[24][2] = {
    {10, 5}, {10, 5}, {4, 8}, {4, 8}, {13, 7}, {17, 9}, {15, 8}, {10, 10}, {12, 10}, {12, 9},
    {13, 8}, {13, 8}, {12, 8}, {12, 5}, {12, 5}, {13, 8}, {13, 8},
    {4, 7}, {4, 7}, {2, 7}, {6, 7}, {7, 7}, {7, 4}, {7, 4} };
#define NOBJ (24 * 3 * 2 + 10 * 2 + 8)
static Object *objs[NOBJ];
static char names[NOBJ][24];

static char letter(struct MUI_RenderInfo *mri, ULONG pen)
{
    if (pen == mri->mri_Pens[MPEN_SHINE]) return 'W';
    if (pen == mri->mri_Pens[MPEN_HALFSHINE]) return 'w';
    if (pen == mri->mri_Pens[MPEN_BACKGROUND]) return 'B';
    if (pen == mri->mri_Pens[MPEN_HALFSHADOW]) return 'd';
    if (pen == mri->mri_Pens[MPEN_SHADOW]) return 'K';
    return (pen & 15) > 9 ? 'a' + (pen & 15) - 10 : '0' + (pen & 15);
}

static void dumpx(struct RastPort *rp, Object *o, const char *name, int maps)
{
    struct MUI_RenderInfo *mri = muiRenderInfo(o);
    ULONG h = 2166136261UL;
    LONG x, y;

    for (y = _top(o); y <= _bottom(o); y++)
        for (x = _left(o); x <= _right(o); x++)
            h = (h ^ letter(mri, ReadPixel(rp, x, y))) * 16777619UL;
    printf("%s %ldx%ld %08lx\n", name, (long)_width(o), (long)_height(o), (unsigned long)h);
    if (maps)
        for (y = _top(o); y <= _bottom(o); y++)
        {
            for (x = _left(o); x <= _right(o); x++)
                putchar(letter(mri, ReadPixel(rp, x, y)));
            putchar('\n');
        }
}


static Object *img(int n, int w, int h, int sel, int i)
{
    static char spec[NOBJ][8];

    sprintf(spec[i], "1:%ld", (long)n);
    sprintf(names[i], "%s %s", spec[i], sel ? "sel" : "nor");
    return objs[i] = ImageObject, MUIA_Image_Spec, (IPTR)spec[i], MUIA_Image_FreeHoriz, TRUE,
        MUIA_Image_FreeVert, TRUE, MUIA_FixWidth, w, MUIA_FixHeight, h, MUIA_Selected, sel,
        MUIA_InputMode, MUIV_InputMode_Toggle, End;
}

int main(int argc, char **argv)
{
    static UWORD pens[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, (UWORD)~0 };
    static ULONG cols[] = { 12L << 16 | 0,
        0x55555555, 0x55555555, 0x55555555,  0x22222222, 0x22222222, 0x66666666,
        0xffffffff, 0, 0,                    0xffffffff, 0xffffffff, 0xffffffff,
        0, 0, 0,                             0, 0, 0xffffffff,
        0x33333333, 0x33333333, 0x33333333,  0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa,
        0x11111111, 0x88888888, 0x11111111,  0x99999999, 0x22222222, 0x99999999,
        0x44444444, 0x44444444, 0x99999999,  0x66666666, 0x22222222, 0x22222222, 0 };
    int maps = argc > 1 && !strcmp(argv[1], "maps");
    Object *app, *win, *grid, *shine, *big;
    struct Screen *scr;
    struct Window *iw = NULL;
    int i, n, z, s;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    scr = OpenScreenTags(NULL, SA_Width, 640, SA_Height, 480, SA_DisplayID, HIRESLACE_KEY,
        SA_Depth, 4, SA_Pens, (IPTR)pens, SA_SharePens, TRUE, SA_Colors32, (IPTR)cols,
        SA_Title, (IPTR)"IPix", SA_Type, CUSTOMSCREEN, TAG_DONE);
    if (!scr) { printf("no screen\n"); CloseLibrary(MUIMasterBase); return 20; }
    grid = ColGroup(18), MUIA_Group_Spacing, 1, End;
    shine = ColGroup(10), MUIA_Group_Spacing, 1, MUIA_Background, MUII_SHINEBACK, End;
    big = HGroup, MUIA_Group_Spacing, 1, End;
    app = ApplicationObject, MUIA_Application_Title, (IPTR)"IPix", MUIA_Application_Base, (IPTR)"IPix",
        SubWindow, (IPTR)(win = WindowObject, MUIA_Window_Title, (IPTR)"IPix",
            MUIA_Window_Screen, (IPTR)scr, MUIA_Window_ID, 0,
            WindowContents, (IPTR)VGroup, Child, (IPTR)grid, Child, (IPTR)shine,
                Child, (IPTR)big, End, End), End;
    if (!app) { printf("no app\n"); CloseScreen(scr); CloseLibrary(MUIMasterBase); return 20; }
    for (i = n = 0; n < 24; n++)
        for (z = 0; z < 3; z++)
            for (s = 0; s < 2; s++, i++)
                DoMethod(grid, OM_ADDMEMBER, (IPTR)img(n, z == 2 ? 36 : mins[n][0] + 5 * z,
                    z == 2 ? 30 : mins[n][1] + 3 * z, s, i));
    for (n = 0; n < 10; n++)
        for (s = 0; s < 2; s++, i++)
            DoMethod(shine, OM_ADDMEMBER, (IPTR)img(n, mins[n][0] + 2, mins[n][1] + 1, s, i));
    for (n = 0; n < 4; n++)
        for (z = 0; z < 2; z++, i++)
            DoMethod(big, OM_ADDMEMBER, (IPTR)img(n, z ? 40 : 72, z ? 72 : 40, 0, i));
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Window, &iw);
    if (!iw) { printf("no window\n"); goto out; }
    Delay(25);
    printf("window %ldx%ld\n", (long)iw->Width, (long)iw->Height);
    for (i = 0; i < NOBJ; i++)
        dumpx(iw->RPort, objs[i], names[i], maps);
out:
    fflush(stdout);
    MUI_DisposeObject(app);
    CloseScreen(scr);
    CloseLibrary(MUIMasterBase);
    return 0;
}
