/* bgfill: which parts of a group with its own MUIA_Background are painted, with
 * and without MUIA_FillArea, with plain children (no background of their own),
 * nested plain groups, a layout hook with a pad, and as the window's root.
 * Black box for UP-MCC's UP-Panel. Columns (left to right), each black "2:0,0,0":
 *   A FillArea FALSE, B FillArea TRUE, C FillArea FALSE + layout hook (pad 8),
 * Each column holds a Rectangle with MUIA_FillArea FALSE (6 high, under the 10 high one).
 *   D grey-blue "2:30..,50..,90.." FillArea FALSE (pen snapping).
 * Window 2: the same kind of group as the window's root (FillArea FALSE).
 * Argument HOLD keeps the windows open until a key (Ctrl-C). */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <string.h>
#include "dumptree.h"

struct Library *MUIMasterBase;
unsigned long __stack = 65536;
static struct Hook hook;
static Object *inner_c;

static ULONG lay(struct Hook *h __asm("a0"), Object *obj __asm("a2"), struct MUI_LayoutMsg *lm __asm("a1"))
{
    (void)h; (void)obj;
    switch (lm->lm_Type) {
    case MUILM_MINMAX:
        lm->lm_MinMax.MinWidth = _minwidth(inner_c) + 16;
        lm->lm_MinMax.MinHeight = _minheight(inner_c) + 16;
        lm->lm_MinMax.DefWidth = _defwidth(inner_c) + 16;
        lm->lm_MinMax.DefHeight = _defheight(inner_c) + 16;
        lm->lm_MinMax.MaxWidth = MUI_MAXMAX;
        lm->lm_MinMax.MaxHeight = MUI_MAXMAX;
        return 0;
    case MUILM_LAYOUT:
        MUI_Layout(inner_c, 8, 8, lm->lm_Layout.Width - 16, lm->lm_Layout.Height - 16, 0);
        return TRUE;
    }
    return MUILM_UNKNOWN;
}

static Object *kids(void)
{
    return VGroup, MUIA_FillArea, FALSE,
        Child, TextObject, MUIA_Text_Contents, "\0333" "plain", End,
        Child, RectangleObject, MUIA_FixHeight, 10, End,
        Child, RectangleObject, MUIA_FixHeight, 6, MUIA_FillArea, FALSE, End,
        Child, HGroup, MUIA_FillArea, FALSE,
            Child, TextObject, MUIA_Text_Contents, "\0333" "n1", End,
            Child, TextObject, MUIA_Text_Contents, "\0333" "n2", End, End,
        Child, VGroup, Child, TextObject, MUIA_Text_Contents, "\0333" "dflt", End, End,
        End;
}

static Object *col(const char *spec, BOOL fill)
{
    return VGroup, MUIA_Background, spec, MUIA_FillArea, fill, MUIA_Group_Spacing, 6,
        Child, TextObject, MUIA_Text_Contents, "\0333" "top", End,
        Child, kids(),
        Child, TextObject, MUIA_Text_Contents, "\0333" "bottom", End,
        End;
}

int main(int argc, char **argv)
{
    static const char *black = "2:00000000,00000000,00000000";
    static const char *blue = "2:30303030,50505050,90909090";
    Object *app, *w1, *w2, *c;
    int hold = argc > 1 && !strcmp(argv[1], "HOLD");
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    hook.h_Entry = (HOOKFUNC)lay;
    inner_c = kids();
    c = VGroup, MUIA_Background, black, MUIA_FillArea, FALSE, MUIA_Group_Spacing, 0,
        MUIA_Group_LayoutHook, &hook, Child, inner_c, End;
    app = ApplicationObject, MUIA_Application_Base, "BGFILL",
        SubWindow, w1 = WindowObject, MUIA_Window_Title, "bgfill", MUIA_Window_LeftEdge, 10, MUIA_Window_TopEdge, 20,
            MUIA_Window_Width, 400, MUIA_Window_Height, 130,
            WindowContents, HGroup, MUIA_Group_Spacing, 8,
                Child, col(black, FALSE),
                Child, col(black, TRUE),
                Child, c,
                Child, col(blue, FALSE),
            End, End,
        SubWindow, w2 = WindowObject, MUIA_Window_Title, "root", MUIA_Window_LeftEdge, 420, MUIA_Window_TopEdge, 20,
            MUIA_Window_Width, 160, MUIA_Window_Height, 130,
            WindowContents, VGroup, MUIA_Background, black, MUIA_FillArea, FALSE,
                Child, TextObject, MUIA_Text_Contents, "\0333" "root", End,
                Child, kids(),
            End, End,
        End;
    if (!app) { printf("no app\n"); return 20; }
    set(w1, MUIA_Window_Open, TRUE);
    set(w2, MUIA_Window_Open, TRUE);
    { Object *r; get(w1, MUIA_Window_RootObject, &r); dumptree(r, 0); }
    fflush(stdout);
    if (hold) Wait(SIGBREAKF_CTRL_C); else Delay(150);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
