/* virtsize: the minimum, default and maximum sizes a Virtgroup reports for contents of
 * known sizes (a fixed 100x50 rectangle, a fixed 20x10 one, a free rectangle, a Text, a
 * column of three Texts), each in its own Scrollgroup, dumped (MUI 3.8 against OpenMUI:
 * the HTMLtext demo's Virtgroup subclass had def 28x24 max 9999x9999 on MUI 3.8 and
 * def 54x12 max 58x9999 on OpenMUI, round 3's od-ht-*.txt).
 * Build: m68k-amigaos-gcc -noixemul -Os -s -I<build>/include virtsize.c -o virtsize -lamiga */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include "dumptree.h"

struct Library *MUIMasterBase;

/* a layout hook as a class like HTMLtext has: min 16x12, def 28x24, max 58x40, the
 * children laid out in a row */
static ULONG LayoutFunc(struct Hook *hook __asm("a0"), Object *obj __asm("a2"), struct MUI_LayoutMsg *lm __asm("a1"))
{
    if (lm->lm_Type == MUILM_MINMAX)
    {
        lm->lm_MinMax.MinWidth = 16; lm->lm_MinMax.MinHeight = 12;
        lm->lm_MinMax.DefWidth = 28; lm->lm_MinMax.DefHeight = 24;
        lm->lm_MinMax.MaxWidth = 58; lm->lm_MinMax.MaxHeight = 40;
        return 0;
    }
    if (lm->lm_Type == MUILM_LAYOUT)
    {
        Object *cstate = (Object *)lm->lm_Children->mlh_Head, *child;
        LONG x = 0;
        while ((child = NextObject(&cstate)))
        {
            MUI_Layout(child, x, 0, _minwidth(child), _minheight(child), 0);
            x += _minwidth(child) + 4;
        }
        lm->lm_Layout.Width = x; lm->lm_Layout.Height = 8;
        return TRUE;
    }
    return 0;
}
static struct Hook layout_hook = { { NULL, NULL }, (HOOKFUNC)LayoutFunc, NULL, NULL };

int main(void)
{
    Object *app, *w1;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject, MUIA_Application_Base, "VIRTSIZE",
        SubWindow, w1 = WindowObject, MUIA_Window_Title, "virtsize", MUIA_Window_LeftEdge, 10, MUIA_Window_TopEdge, 12,
            WindowContents, HGroup,
                Child, ScrollgroupObject, MUIA_Scrollgroup_Contents, VirtgroupObject, MUIA_Frame, MUIV_Frame_Group,
                    Child, RectangleObject, MUIA_FixWidth, 100, MUIA_FixHeight, 50, End, End, End,
                Child, ScrollgroupObject, MUIA_Scrollgroup_Contents, VirtgroupObject, MUIA_Frame, MUIV_Frame_Group,
                    Child, RectangleObject, MUIA_FixWidth, 20, MUIA_FixHeight, 10, End, End, End,
                Child, ScrollgroupObject, MUIA_Scrollgroup_Contents, VirtgroupObject, MUIA_Frame, MUIV_Frame_Group,
                    Child, RectangleObject, End, End, End,
                Child, ScrollgroupObject, MUIA_Scrollgroup_Contents, VirtgroupObject, MUIA_Frame, MUIV_Frame_Group,
                    Child, TextObject, MUIA_Text_Contents, "a Text object", End, End, End,
                Child, ScrollgroupObject, MUIA_Scrollgroup_Contents, VirtgroupObject, MUIA_Frame, MUIV_Frame_Group,
                    Child, TextObject, MUIA_Text_Contents, "one", End,
                    Child, TextObject, MUIA_Text_Contents, "two two", End,
                    Child, TextObject, MUIA_Text_Contents, "three", End, End, End,
                Child, ScrollgroupObject, MUIA_Scrollgroup_Contents, VirtgroupObject, MUIA_Frame, MUIV_Frame_Group,
                    MUIA_Group_LayoutHook, &layout_hook,
                    Child, TextObject, MUIA_Text_Contents, "one", End,
                    Child, TextObject, MUIA_Text_Contents, "two", End, End, End,
            End, End,
        End;
    if (!app) return 20;
    set(w1, MUIA_Window_Open, TRUE);
    { Object *r; struct Window *w;
      get(w1, MUIA_Window_RootObject, &r); get(w1, MUIA_Window_Window, &w);
      printf("virtsize (library %d.%d): window %dx%d min %dx%d max %dx%d\n", MUIMasterBase->lib_Version, MUIMasterBase->lib_Revision,
          w ? w->Width : 0, w ? w->Height : 0, w ? w->MinWidth : 0, w ? w->MinHeight : 0, w ? w->MaxWidth : 0, w ? w->MaxHeight : 0);
      dumptree(r, 0); }
    fflush(stdout);
    Delay(50);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
