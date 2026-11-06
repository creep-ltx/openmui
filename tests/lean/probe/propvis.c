/* propvis: what a List tells its scrollbar. A Listview with N entries in a
 * window of height H; prints the List's MUIA_List_Visible, the height of its
 * entry area, and the Prop's Entries, Visible, First and height. Works on
 * MUI 3.8 and Zune (public fields only, classes found by MUI_GetClass).
 * Output: RAM:propvis.txt. Build: tests/lean/probe like the others. */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <string.h>
#include "dumptree.h"

struct Library *MUIMasterBase;
ULONG __stack = 32768;
static const char *items[70];
static char names[70][16];

static Object *find(Object *o, const char *cls)
{
    struct List *l = NULL;
    int priv;
    if (!strcmp(dt_cls(o, &priv), cls) && !priv) return o;
    if (get(o, MUIA_Group_ChildList, &l) && l)
    {
        APTR st = l->lh_Head;
        Object *c, *r;
        while ((c = NextObject(&st)))
            if ((r = find(c, cls))) return r;
    }
    return NULL;
}

int main(void)
{
    static const int ns[] = { 3, 7, 11, 17, 23, 37, 50, 63 };
    int i, n, h;
    FILE *f = fopen("RAM:propvis.txt", "w");
    if (!f || !(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    for (i = 0; i < 70; i++) { sprintf(names[i], "Line %d", i + 1); items[i] = names[i]; }
    for (n = 0; n < 8; n++)
        for (h = 60; h <= 200; h += 5)
        {
            Object *app, *win, *lv, *list, *prop;
            ULONG open = FALSE;
            items[ns[n]] = NULL;
            app = ApplicationObject, SubWindow, win = WindowObject,
                MUIA_Window_Height, h, MUIA_Window_Width, 160,
                WindowContents, VGroup,
                    Child, lv = ListviewObject, MUIA_Listview_List, list = ListObject,
                        InputListFrame, MUIA_List_SourceArray, items, End, End,
                    End, End, End;
            items[ns[n]] = names[ns[n]];
            if (!app) return 20;
            set(win, MUIA_Window_Open, TRUE);
            get(win, MUIA_Window_Open, &open);
            if (open)
            {
                LONG vis = 0, pe = 0, pv = 0, pf = 0;
                get(list, MUIA_List_Visible, &vis);
                prop = find(lv, "Prop.mui");
                if (prop)
                {
                    get(prop, MUIA_Prop_Entries, &pe);
                    get(prop, MUIA_Prop_Visible, &pv);
                    get(prop, MUIA_Prop_First, &pf);
                }
                {
                    char col[200]; int y, k = 0;
                    struct RastPort *rp = _rp(prop);
                    for (y = _top(prop); y <= _bottom(prop) && k < 198; y++)
                        col[k++] = '0' + ReadPixel(rp, _left(prop) + _width(prop) / 2, y);
                    col[k] = 0;
                    fprintf(f, "%s ", col);
                }
                fprintf(f, "n %2d list %3d (inner %3d) visible %2ld prop %ld/%ld/%ld height %d inner %d\n",
                    ns[n], _height(list), _mheight(list), vis, pe, pv, pf,
                    prop ? _height(prop) : -1, prop ? _mheight(prop) : -1);
            }
            set(win, MUIA_Window_Open, FALSE);
            MUI_DisposeObject(app);
        }
    fclose(f);
    CloseLibrary(MUIMasterBase);
    return 0;
}
