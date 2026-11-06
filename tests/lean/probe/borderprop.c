/* borderprop: what a Scrollgroup using the window's border scrollers gives
 * them. A window with a Scrollgroup (MUIA_Scrollgroup_UseWinBorder) round a
 * Virtgroup with a 449x240 Text, as in the Virtual demo's first page; prints
 * every Prop object's Entries/Visible/First and the Virtgroup's sizes.
 * Output: RAM:borderprop.txt. Works on MUI 3.8 and Zune. 2026-11-06: the
 * same numbers on both (432/282 and 240/99), so the other knobs of the
 * Virtual demo's border scrollers come from elsewhere. */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <string.h>
#include "dumptree.h"

struct Library *MUIMasterBase;
ULONG __stack = 32768;
static FILE *f;

static void props(Object *o)
{
    struct List *l = NULL;
    int priv;
    if (!strcmp(dt_cls(o, &priv), "Prop.mui"))
    {
        LONG e = 0, v = 0, fi = 0;
        get(o, MUIA_Prop_Entries, &e);
        get(o, MUIA_Prop_Visible, &v);
        get(o, MUIA_Prop_First, &fi);
        fprintf(f, "Prop%s entries %ld visible %ld first %ld box %dx%d\n", priv ? "*" : "",
            e, v, fi, _width(o), _height(o));
    }
    if (!strcmp(dt_cls(o, &priv), "Virtgroup.mui"))
    {
        LONG w = 0, h = 0;
        get(o, MUIA_Virtgroup_Width, &w);
        get(o, MUIA_Virtgroup_Height, &h);
        fprintf(f, "Virtgroup width %ld height %ld box %dx%d inner %dx%d\n", w, h,
            _width(o), _height(o), _mwidth(o), _mheight(o));
    }
    if (get(o, MUIA_Group_ChildList, &l) && l)
    {
        APTR st = l->lh_Head;
        Object *c;
        while ((c = NextObject(&st)))
            props(c);
    }
}

int main(void)
{
    static const char *text = "\33c\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
        "123456789012345678901234567890123456789012345678901234";
    Object *app, *win, *root;
    ULONG open = FALSE;
    if (!(f = fopen("RAM:borderprop.txt", "w"))) return 20;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject, SubWindow, win = WindowObject,
        MUIA_Window_UseRightBorderScroller, TRUE,
        MUIA_Window_UseBottomBorderScroller, TRUE,
        MUIA_Window_Width, 320, MUIA_Window_Height, 130,
        WindowContents, root = VGroup,
            Child, ScrollgroupObject, MUIA_Scrollgroup_UseWinBorder, TRUE,
                MUIA_Scrollgroup_Contents, VirtgroupObject, VirtualFrame,
                    Child, TextObject, MUIA_Background, MUII_TextBack,
                        MUIA_Text_Contents, text, End,
                    End,
                End,
            End, End, End;
    if (!app) return 20;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (open)
        props(root);
    set(win, MUIA_Window_Open, FALSE);
    MUI_DisposeObject(app);
    fclose(f);
    CloseLibrary(MUIMasterBase);
    return 0;
}
