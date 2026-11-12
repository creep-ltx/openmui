/* gforward: which Group attributes a set() on a group passes on to its child groups.
 * A parent VGroup with two child VGroups (one with an explicit spacing of 1); each
 * attribute is set on the parent alone, then the children are read back. With the
 * window open, the children's boxes show whether MUIA_Group_Horiz was passed on. */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>

struct Library *MUIMasterBase;
unsigned long __stack = 65536;

static void show(const char *what, Object *p, Object *a, Object *b)
{
    IPTR v[6] = {0};
    get(p, MUIA_Group_HorizSpacing, &v[0]); get(p, MUIA_Group_VertSpacing, &v[1]);
    get(a, MUIA_Group_HorizSpacing, &v[2]); get(a, MUIA_Group_VertSpacing, &v[3]);
    get(b, MUIA_Group_HorizSpacing, &v[4]); get(b, MUIA_Group_VertSpacing, &v[5]);
    printf("%-22s parent %ld/%ld  a %ld/%ld  b %ld/%ld\n", what, (long)v[0], (long)v[1], (long)v[2], (long)v[3], (long)v[4], (long)v[5]);
}

int main(void)
{
    Object *app, *win, *p, *a, *b;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject, MUIA_Application_Base, "GFORWARD",
        SubWindow, win = WindowObject, MUIA_Window_Title, "gforward",
            WindowContents, p = VGroup,
                Child, a = VGroup, MUIA_Group_Spacing, 1,
                    Child, TextObject, MUIA_Text_Contents, "a1", End,
                    Child, TextObject, MUIA_Text_Contents, "a2", End, End,
                Child, b = VGroup,
                    Child, TextObject, MUIA_Text_Contents, "b1", End,
                    Child, TextObject, MUIA_Text_Contents, "b2", End, End,
            End, End,
        End;
    if (!app) { printf("no app\n"); return 20; }
    show("new", p, a, b);
    set(win, MUIA_Window_Open, TRUE);
    show("open", p, a, b);
    SetAttrs(p, MUIA_Group_HorizSpacing, 7, TAG_DONE); show("set HorizSpacing 7", p, a, b);
    SetAttrs(p, MUIA_Group_VertSpacing, 9, TAG_DONE); show("set VertSpacing 9", p, a, b);
    SetAttrs(p, MUIA_Group_Spacing, 5, TAG_DONE); show("set Spacing 5", p, a, b);
    set(win, MUIA_Window_Open, FALSE);
    SetAttrs(p, MUIA_Group_Horiz, TRUE, TAG_DONE);
    set(win, MUIA_Window_Open, TRUE);
    printf("after Horiz on parent: a %d,%d %dx%d  b %d,%d %dx%d\n", _left(a), _top(a), _width(a), _height(a), _left(b), _top(b), _width(b), _height(b));
    {   Object *t1 = NULL; struct List *l = NULL; Object *st, *c; get(a, MUIA_Group_ChildList, &l); st = (Object *)l->lh_Head;
        while ((c = NextObject(&st))) printf("  a child %d,%d %dx%d\n", _left(c), _top(c), _width(c), _height(c)); (void)t1; }
    set(win, MUIA_Window_Open, FALSE);
    SetAttrs(p, MUIA_Group_Columns, 2, TAG_DONE);
    set(win, MUIA_Window_Open, TRUE);
    {   struct List *l = NULL; Object *st, *c; get(a, MUIA_Group_ChildList, &l); st = (Object *)l->lh_Head;
        while ((c = NextObject(&st))) printf("  after Columns 2: a child %d,%d %dx%d\n", _left(c), _top(c), _width(c), _height(c)); }
    Delay(50);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
