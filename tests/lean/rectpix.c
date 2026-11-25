/* rectpix [colour [thick|thin|auto [maps]]]: Rectangle's pixels. The bars (MUIA_Rectangle_HBar,
 * MUIA_Rectangle_VBar) with and without a bar title (a long and a short one), and a plain
 * Rectangle, each at six widths and four heights, on framepix.c's 16-colour screen; the group
 * title colour (MUICFG_GroupTitle_Color 0 standard, 1 hilite, 2 3D, 3 outline) and the frame
 * thickness set through the application's own settings file. One line per object: its kind,
 * box and a checksum of every pixel's pen; "maps" adds the pixels. Compare OpenMUI builds line
 * for line (trim1: Rectangle's Draw rewritten).
 * Build: m68k-amigaos-gcc -Os -m68000 -noixemul -s rectpix.c -o rectpix (probe.sh does). */
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

#ifndef MUICFG_GroupTitle_Color
#define MUICFG_GroupTitle_Color 0x10
#endif
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

#define NKINDS 6
#define NW 6
#define NH 4
#define NOBJ (NKINDS * NW * NH)
static const UBYTE ws[NW] = { 1, 3, 8, 21, 44, 90 }, hs[NH] = { 1, 4, 9, 15 };
static const char *kinds[NKINDS] = { "plain", "hbar", "vbar", "hbar-long", "hbar-short", "vbar-title" };
static Object *objs[NOBJ];

static char letter(struct MUI_RenderInfo *mri, ULONG pen)
{
    if (pen == mri->mri_Pens[MPEN_SHINE]) return 'W';
    if (pen == mri->mri_Pens[MPEN_HALFSHINE]) return 'w';
    if (pen == mri->mri_Pens[MPEN_BACKGROUND]) return 'B';
    if (pen == mri->mri_Pens[MPEN_HALFSHADOW]) return 'd';
    if (pen == mri->mri_Pens[MPEN_SHADOW]) return 'K';
    if (pen == mri->mri_Pens[MPEN_TEXT]) return 'T';
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

static Object *make(int k, int w, int h)
{
    static const char *titles[NKINDS] = { NULL, NULL, NULL, "Speed: 20", "Ti", "Vt" };
    BOOL hb = k == 1 || k == 3 || k == 4, vb = k == 2 || k == 5;

    return RectangleObject, MUIA_Rectangle_HBar, hb, MUIA_Rectangle_VBar, vb,
        titles[k] ? MUIA_Rectangle_BarTitle : TAG_IGNORE, (IPTR)titles[k],
        MUIA_FixWidth, w, MUIA_FixHeight, h, End;
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
    int colour = argc > 1 ? atoi(argv[1]) : 0;
    const char *th = argc > 2 ? argv[2] : "auto";
    int maps = argc > 3 && !strcmp(argv[3], "maps");
    Object *app, *win, *grid, *c;
    struct Screen *scr;
    struct Window *iw = NULL;
    char name[48];
    int k, a, b, n;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    if ((c = MUI_NewObject(MUIC_Configdata, TAG_DONE)))
    {
        DoMethod(c, MUIM_Configdata_SetULong, MUICFG_GroupTitle_Color, colour);
        if (strcmp(th, "auto"))
            DoMethod(c, MUIM_Configdata_SetULong, MUICFG_Frame_Thickness, strcmp(th, "thick") ? 2 : 1);
        DoMethod(c, MUIM_Configdata_Save, (IPTR)"ENV:OpenMUI/RPix.1.prefs");
        MUI_DisposeObject(c);
    }
    scr = OpenScreenTags(NULL, SA_Width, 640, SA_Height, 480, SA_DisplayID, HIRESLACE_KEY,
        SA_Depth, 4, SA_Pens, (IPTR)pens, SA_SharePens, TRUE, SA_Colors32, (IPTR)cols,
        SA_Title, (IPTR)"RPix", SA_Type, CUSTOMSCREEN, TAG_DONE);
    if (!scr) { printf("no screen\n"); CloseLibrary(MUIMasterBase); return 20; }
    grid = ColGroup(NW), MUIA_Group_Spacing, 2, End;
    app = ApplicationObject, MUIA_Application_Title, (IPTR)"RPix", MUIA_Application_Base, (IPTR)"RPix",
        SubWindow, (IPTR)(win = WindowObject, MUIA_Window_Title, (IPTR)"RPix",
            MUIA_Window_Screen, (IPTR)scr, MUIA_Window_ID, 0,
            WindowContents, (IPTR)grid, End), End;
    if (!app) { printf("no app\n"); CloseScreen(scr); CloseLibrary(MUIMasterBase); return 20; }
    for (n = k = 0; k < NKINDS; k++)
        for (b = 0; b < NH; b++)
            for (a = 0; a < NW; a++, n++)
                DoMethod(grid, OM_ADDMEMBER, (IPTR)(objs[n] = make(k, ws[a], hs[b])));
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Window, &iw);
    if (!iw) { printf("no window\n"); goto out; }
    Delay(25);
    printf("colour %ld setting %s thinframes %ld window %ldx%ld\n", (long)colour, th,
        (long)((muiRenderInfo(objs[0])->mri_Flags & MUIMRI_THINFRAMES) != 0),
        (long)iw->Width, (long)iw->Height);
    for (n = 0; n < NOBJ; n++)
    {
        sprintf(name, "%s w%ld h%ld", kinds[n / (NW * NH)], (long)ws[n % NW], (long)hs[(n / NW) % NH]);
        dump(iw->RPort, objs[n], name, maps);
    }
    /* redrawn alone over a cleared box: the same pixels without the neighbours' */
    for (n = 0; n < NOBJ; n++)
    {
        Object *o = objs[n];
        SetAPen(iw->RPort, 0);
        RectFill(iw->RPort, _left(o), _top(o), _right(o), _bottom(o));
        MUI_Redraw(o, MADF_DRAWOBJECT);
        sprintf(name, "redraw %s w%ld h%ld", kinds[n / (NW * NH)], (long)ws[n % NW], (long)hs[(n / NW) % NH]);
        dump(iw->RPort, o, name, maps);
    }
out:
    MUI_DisposeObject(app);
    CloseScreen(scr);
    DeleteFile("ENV:OpenMUI/RPix.1.prefs");
    CloseLibrary(MUIMasterBase);
    return 0;
}
