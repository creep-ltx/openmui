/* stage [W H MODE [FONT SIZE [NCOL [TICKS]]]]: the font of each MUIV_Font_
 * preset, a titled and an untitled group, an empty list and a row of 3
 * buttons, dumped; NCOL more buttons in a column push the window into a
 * fitting stage; on a screen of its own (with FONT as its font) when W H
 * are given. Build: m68k-amigaos-gcc -noixemul -Os -s -I<build>/include
 * stage.c -o stage -lamiga */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdlib.h>
#include "dumptree.h"

struct Library *MUIMasterBase;
static const char *names[] = { "Inherit", "Normal", "List", "Tiny", "Fixed", "Title", "Big", "Button" };
static const LONG presets[] = { MUIV_Font_Inherit, MUIV_Font_Normal, MUIV_Font_List, MUIV_Font_Tiny,
    MUIV_Font_Fixed, MUIV_Font_Title, MUIV_Font_Big, MUIV_Font_Button };

int main(int argc, char **argv)
{
    Object *col, *app, *win, *grp, *t[8], *root;
    struct Screen *scr = NULL;
    IPTR open = 0;
    int i;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    if (argc > 2) scr = dt_screen(atoi(argv[1]), atoi(argv[2]), argc > 3 ? strtoul(argv[3], NULL, 16) : 0x8000,
        argc > 5 ? argv[4] : NULL, argc > 5 ? atoi(argv[5]) : 0);
    app = ApplicationObject, MUIA_Application_Title, (IPTR)"FontTest", MUIA_Application_Base, (IPTR)"FONTTEST",
        SubWindow, (IPTR)(win = WindowObject, MUIA_Window_Title, (IPTR)"FontTest",
            scr ? MUIA_Window_Screen : TAG_IGNORE, (IPTR)scr,
            WindowContents, (IPTR)(root = VGroup,
                Child, (IPTR)(grp = VGroup, End),
                Child, (IPTR)(col = VGroup, End),
                Child, (IPTR)(HGroup, GroupFrameT("Title"), Child, (IPTR)SimpleButton("In"), End),
                Child, (IPTR)(VGroup, GroupFrame, Child, (IPTR)SimpleButton("Untitled"), End),
                Child, (IPTR)(ListviewObject, MUIA_Listview_List, (IPTR)(ListObject, InputListFrame, End), End),
                Child, (IPTR)(HGroup, Child, (IPTR)SimpleButton("One"), Child, (IPTR)SimpleButton("Two"),
                    Child, (IPTR)SimpleButton("Three"), End),
                End), End), End;
    if (!app) return 20;
    for (i = 0; i < 8; i++)
    {
        t[i] = TextObject, MUIA_Font, presets[i], MUIA_Text_Contents, (IPTR)names[i], End;
        DoMethod(grp, OM_ADDMEMBER, (IPTR)t[i]);
    }
    for (i = 0; i < (argc > 6 ? atoi(argv[6]) : 0); i++) DoMethod(col, OM_ADDMEMBER, (IPTR)SimpleButton("Col"));
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (open)
    {
        for (i = 0; i < 8; i++)
        {
            struct TextFont *f = _font(t[i]);
            printf("%-8s %s/%d baseline %d\n", names[i], f && f->tf_Message.mn_Node.ln_Name ? f->tf_Message.mn_Node.ln_Name : "?", f ? f->tf_YSize : 0, f ? f->tf_Baseline : 0);
        }
        dumptree(root, 0);
    }
    Delay(argc > 7 ? atoi(argv[7]) : 50);
    MUI_DisposeObject(app);
    if (scr) CloseScreen(scr);
    CloseLibrary(MUIMasterBase);
    return 0;
}
