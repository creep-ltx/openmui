/* lvrows: Listviews around a List, a List with MUIA_List_MinLineHeight 10,
 * a Listtree.mcc and a Listtree.mcc with MinLineHeight 10, each object's
 * min/def/max dumped (dumptree.h). AmIRC's server list is a Listtree in a
 * Listview: MUI 3.8 gives it 3 rows minimum and 8 by default. The last two:
 * a List and a Listtree in helvetica 13. */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/diskfont.h>
#include <proto/graphics.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include "dumptree.h"

struct Library *MUIMasterBase, *DiskfontBase;
static struct TextAttr hv13 = { (STRPTR)"helvetica.font", 13, 0, 0 };

static Object *lv(Object *list)
{
    return list ? ListviewObject, MUIA_Listview_List, (IPTR)list, End : NULL;
}

int main(void)
{
    Object *app, *win, *root, *l[6];
    struct TextFont *tf = NULL;
    IPTR open = 0;
    int i;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    l[0] = lv(ListObject, InputListFrame, End);
    l[1] = lv(ListObject, InputListFrame, MUIA_List_MinLineHeight, 10, End);
    l[2] = lv(MUI_NewObject("Listtree.mcc", InputListFrame, TAG_DONE));
    l[3] = lv(MUI_NewObject("Listtree.mcc", InputListFrame, MUIA_List_MinLineHeight, 10, TAG_DONE));
    if ((DiskfontBase = OpenLibrary("diskfont.library", 36))) tf = OpenDiskFont(&hv13);
    l[4] = tf ? lv(ListObject, InputListFrame, MUIA_Font, (IPTR)tf, End) : NULL;
    l[5] = tf ? lv(MUI_NewObject("Listtree.mcc", InputListFrame, MUIA_Font, (IPTR)tf, TAG_DONE)) : NULL;
    root = HGroup, End;
    for (i = 0; i < 6; i++)
        if (l[i]) DoMethod(root, OM_ADDMEMBER, (IPTR)l[i]);
        else printf("object %d not made\n", i);
    app = ApplicationObject, MUIA_Application_Title, (IPTR)"LVRows", MUIA_Application_Base, (IPTR)"LVROWS",
        SubWindow, (IPTR)(win = WindowObject, MUIA_Window_Title, (IPTR)"LVRows",
            WindowContents, (IPTR)root, End), End;
    if (!app) return 20;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (open)
        dumptree(root, 0);
    Delay(50);
    MUI_DisposeObject(app);
    if (tf) CloseFont(tf);
    if (DiskfontBase) CloseLibrary(DiskfontBase);
    CloseLibrary(MUIMasterBase);
    return 0;
}
