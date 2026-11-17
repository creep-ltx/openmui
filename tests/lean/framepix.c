/* framepix [thick|thin|auto] [maps]: every frame type's pixels. A 16-colour 640x480 screen (the
 * pens of framethick.c), a window of Rectangles with the frame spec "TS0000" for every type 0..b
 * and a custom one (c), raised and recessed, at seven inner sizes from 1x1 to 12x6, once over the window's background and once inside a group
 * with MUII_SHINEBACK (the corner hints); toggle buttons of the types 1, 2, 3, 4, 6 and 9 that
 * are then selected (the state flip and its redraw) and titled frames of the same types; then
 * each type and state redrawn at its largest object's place into boxes of 1 to 8 x 1 to 6 pixels
 * (where the frames' size checks decide; MUI_Redraw with the box set by hand). Prints
 * one line per object: its spec, box and a checksum of every pixel's pen (FNV-1a), "maps" adds
 * the pixels (W shine, w halfshine, B background, d halfshadow, K shadow, else the pen number).
 * The setting goes into ENV:OpenMUI/FPix.1.prefs as in framethick.c. Compare OpenMUI builds
 * line for line, and MUI 3.8 black-box where its spec strings mean the same.
 * Build: m68k-amigaos-gcc -Os -m68000 -noixemul -s framepix.c -o framepix (probe.sh does). */
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

#ifndef MUICFG_Frame_Thickness
#define MUICFG_Frame_Thickness 0x50b
#endif
#ifndef MUIM_Configdata_SetULong
#define MUIM_Configdata_SetULong 0x8042f4b6
#endif
#ifndef MUIM_Configdata_Save
#define MUIM_Configdata_Save 0x80422c43
#endif
#ifndef MUIC_Configdata
#define MUIC_Configdata "Configdata.mui"
#endif

struct Library *MUIMasterBase;
ULONG __stack = 65536;

#define NTYPES 13
#define NSIZES 7
static const char types[NTYPES + 1] = "0123456789abc";
static const UBYTE sizes[NSIZES][2] = { {1, 1}, {2, 1}, {1, 2}, {3, 2}, {5, 3}, {7, 7}, {12, 6} };
/* the tiny boxes the frames' size checks decide (redrawn in place) */
static const UBYTE tinyw[] = { 1, 2, 3, 4, 5, 6, 8 }, tinyh[] = { 1, 2, 3, 4, 6 };
#define DRAWFRAME (1 << 11) /* OpenMUI's MADF_DRAWFRAME: the frame too */
static const char togtypes[] = "123469";
#define NOBJ (2 * NTYPES * 2 * NSIZES)
#define NTOG 6
static Object *objs[NOBJ], *togs[NTOG], *titled[NTOG];
static char specs[NOBJ][8];

static char letter(struct MUI_RenderInfo *mri, ULONG pen)
{
    if (pen == mri->mri_Pens[MPEN_SHINE]) return 'W';
    if (pen == mri->mri_Pens[MPEN_HALFSHINE]) return 'w';
    if (pen == mri->mri_Pens[MPEN_BACKGROUND]) return 'B';
    if (pen == mri->mri_Pens[MPEN_HALFSHADOW]) return 'd';
    if (pen == mri->mri_Pens[MPEN_SHADOW]) return 'K';
    return (pen & 15) > 9 ? 'a' + (pen & 15) - 10 : '0' + (pen & 15);
}

static void dump(struct RastPort *rp, Object *o, const char *name, int maps)
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
    const char *th = argc > 1 ? argv[1] : "auto";
    int maps = argc > 2 && !strcmp(argv[2], "maps");
    Object *app, *win, *grid[2], *row;
    struct Screen *scr;
    struct Window *iw = NULL;
    char name[32];
    int i, g, t, s, z, n;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    if (strcmp(th, "auto"))
    {
        Object *c = MUI_NewObject(MUIC_Configdata, TAG_DONE);
        if (c)
        {
            DoMethod(c, MUIM_Configdata_SetULong, MUICFG_Frame_Thickness, strcmp(th, "thick") ? 2 : 1);
            DoMethod(c, MUIM_Configdata_Save, (IPTR)"ENV:OpenMUI/FPix.1.prefs");
            MUI_DisposeObject(c);
        }
    }
    else
        DeleteFile("ENV:OpenMUI/FPix.1.prefs");
    scr = OpenScreenTags(NULL, SA_Width, 640, SA_Height, 480, SA_DisplayID, HIRESLACE_KEY,
        SA_Depth, 4, SA_Pens, (IPTR)pens, SA_SharePens, TRUE, SA_Colors32, (IPTR)cols,
        SA_Title, (IPTR)"FPix", SA_Type, CUSTOMSCREEN, TAG_DONE);
    if (!scr) { printf("no screen\n"); CloseLibrary(MUIMasterBase); return 20; }
    grid[0] = ColGroup(2 * NSIZES), MUIA_Group_Spacing, 2, End;
    grid[1] = ColGroup(2 * NSIZES), MUIA_Group_Spacing, 2, MUIA_Background, MUII_SHINEBACK, End;
    row = HGroup, End;
    app = ApplicationObject, MUIA_Application_Title, (IPTR)"FPix", MUIA_Application_Base, (IPTR)"FPix",
        SubWindow, (IPTR)(win = WindowObject, MUIA_Window_Title, (IPTR)"FPix",
            MUIA_Window_Screen, (IPTR)scr, MUIA_Window_ID, 0,
            WindowContents, (IPTR)VGroup, Child, (IPTR)HGroup, Child, (IPTR)grid[0],
                Child, (IPTR)grid[1], End, Child, (IPTR)row, End, End), End;
    if (!app) { printf("no app\n"); CloseScreen(scr); CloseLibrary(MUIMasterBase); return 20; }
    for (n = g = 0; g < 2; g++)
        for (t = 0; t < NTYPES; t++)
            for (s = 0; s < 2; s++)
                for (z = 0; z < NSIZES; z++, n++)
                {
                    /* no sprintf: libnix's takes %d as a word */
                    strcpy(specs[n], "000000");
                    specs[n][0] = types[t];
                    specs[n][1] = '0' + s;
                    objs[n] = RectangleObject, MUIA_Frame, (IPTR)specs[n],
                        MUIA_FixWidth, sizes[z][0], MUIA_FixHeight, sizes[z][1], End;
                    DoMethod(grid[g], OM_ADDMEMBER, (IPTR)objs[n]);
                }
    for (i = 0; i < NTOG; i++)
    {
        static char tspec[NTOG][8];
        strcpy(tspec[i], "002211");
        tspec[i][0] = togtypes[i];
        togs[i] = TextObject, MUIA_Frame, (IPTR)tspec[i], MUIA_Background, MUII_ButtonBack,
            MUIA_InputMode, MUIV_InputMode_Toggle, MUIA_Text_Contents, (IPTR)"Tg", End;
        titled[i] = VGroup, MUIA_Frame, (IPTR)tspec[i], MUIA_FrameTitle, (IPTR)"Ti",
            Child, (IPTR)RectangleObject, MUIA_FixWidth, 20, MUIA_FixHeight, 4, End, End;
        DoMethod(row, OM_ADDMEMBER, (IPTR)togs[i]);
        DoMethod(row, OM_ADDMEMBER, (IPTR)titled[i]);
    }
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Window, &iw);
    if (!iw) { printf("no window\n"); goto out; }
    Delay(25);
    printf("setting %s thinframes %ld window %ldx%ld\n", th,
        (long)((muiRenderInfo(objs[0])->mri_Flags & MUIMRI_THINFRAMES) != 0),
        (long)iw->Width, (long)iw->Height);
    for (n = 0; n < NOBJ; n++)
    {
        sprintf(name, "%s s%ld g%ld", specs[n], (long)(n % NSIZES),
            (long)(n / (NTYPES * 2 * NSIZES)));
        dump(iw->RPort, objs[n], name, maps);
    }
    for (i = 0; i < NTOG; i++)
    {
        strcpy(name, "toggle 0");
        name[7] = togtypes[i];
        dump(iw->RPort, togs[i], name, maps);
        strcpy(name, "titled 0");
        name[7] = togtypes[i];
        dump(iw->RPort, titled[i], name, maps);
    }
    for (i = 0; i < NTOG; i++)
        set(togs[i], MUIA_Selected, TRUE);
    Delay(5);
    for (i = 0; i < NTOG; i++)
    {
        strcpy(name, "selected 0");
        name[9] = togtypes[i];
        dump(iw->RPort, togs[i], name, maps);
    }
    /* each type and state redrawn into tiny boxes at the place of its
     * largest object (cleared first), the old box dumped */
    for (n = 0; n < 2 * NTYPES * NSIZES; n += NSIZES)
    {
        Object *o = objs[n + NSIZES - 1];
        WORD w = _width(o), h = _height(o);
        int a, b;

        for (a = 0; a < sizeof(tinyw); a++)
            for (b = 0; b < sizeof(tinyh); b++)
            {
                SetAPen(iw->RPort, 0);
                RectFill(iw->RPort, _left(o), _top(o), _left(o) + w - 1, _top(o) + h - 1);
                _width(o) = tinyw[a];
                _height(o) = tinyh[b];
                MUI_Redraw(o, MADF_DRAWOBJECT | DRAWFRAME);
                _width(o) = w;
                _height(o) = h;
                sprintf(name, "tiny %s %ldx%ld", specs[n], (long)tinyw[a], (long)tinyh[b]);
                dump(iw->RPort, o, name, maps);
            }
    }
out:
    fflush(stdout);
    MUI_DisposeObject(app);
    CloseScreen(scr);
    DeleteFile("ENV:OpenMUI/FPix.1.prefs");
    CloseLibrary(MUIMasterBase);
    return 0;
}
