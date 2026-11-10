/* vlinner: the frame additions (public mad_addleft/right/top/bottom) of a
 * Volumelist against a List and a Dirlist, each in a Listview, with and
 * without InputListFrame and explicit inner spacing, after the window
 * opened.  MUI 3.8's MUI-Demo Listviews window has the Volumelist 2 px
 * wider on the left (objdump: add 6 against Dirlist's 4). */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>

struct Library *MUIMasterBase;

#define N 8
static Object *o[N];
static const char *what[N] = {
    "Volumelist InputListFrame", "Volumelist InputListFrame InnerLeft 2",
    "Volumelist no frame", "List InputListFrame", "Dirlist InputListFrame",
    "Volumelist InputListFrame InnerLeft 0", "Volumelist InputListFrame Format \",\"",
    "Volumelist ReadListFrame",
};

static Object *lv(Object *l)
{
    return ListviewObject, MUIA_Listview_Input, TRUE, MUIA_Listview_List, l, End;
}

int main(void)
{
    Object *app, *win;
    int i;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject, MUIA_Application_Base, (IPTR)"VLINNER",
        SubWindow, win = WindowObject, MUIA_Window_Title, (IPTR)"vlinner",
            WindowContents, ColGroup(4),
                Child, lv(o[0] = VolumelistObject, InputListFrame, End),
                Child, lv(o[1] = VolumelistObject, InputListFrame, MUIA_InnerLeft, 2, End),
                Child, lv(o[2] = VolumelistObject, End),
                Child, lv(o[3] = ListObject, InputListFrame, End),
                Child, lv(o[4] = DirlistObject, InputListFrame, End),
                Child, lv(o[5] = VolumelistObject, InputListFrame, MUIA_InnerLeft, 0, End),
                Child, lv(o[6] = VolumelistObject, InputListFrame, MUIA_List_Format, (IPTR)",", End),
                Child, lv(o[7] = VolumelistObject, ReadListFrame, End),
            End,
        End,
    End;
    if (!app) { printf("no application\n"); return 20; }
    set(win, MUIA_Window_Open, TRUE);
    for (i = 0; i < N; i++)
    {
        struct MUI_AreaData *a = muiAreaData(o[i]);
        printf("%-40s add l %d r %d t %d b %d  w %d\n", what[i], a->mad_addleft,
            a->mad_subwidth - a->mad_addleft, a->mad_addtop,
            a->mad_subheight - a->mad_addtop, a->mad_Box.Width);
    }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
