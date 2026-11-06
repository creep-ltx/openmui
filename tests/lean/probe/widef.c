/* widef N: a row of N buttons plus a text in each font preset; prints
 * the stage's fonts */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdlib.h>
#include "dumptree.h"

struct Library *MUIMasterBase;
static const char *names[] = { "Inh", "Nor", "Lis", "Tin", "Fix", "Tit", "Big", "But" };
static const LONG presets[] = { MUIV_Font_Inherit, MUIV_Font_Normal, MUIV_Font_List, MUIV_Font_Tiny,
    MUIV_Font_Fixed, MUIV_Font_Title, MUIV_Font_Big, MUIV_Font_Button };

int main(int argc, char **argv)
{
    Object *app, *win, *grp, *t[8];
    int i, n = argc > 1 ? atoi(argv[1]) : 10;
    IPTR open = 0;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject, MUIA_Application_Title, (IPTR)"WideF", MUIA_Application_Base, (IPTR)"WIDEF",
        SubWindow, (IPTR)(win = WindowObject, MUIA_Window_Title, (IPTR)"WideF",
            WindowContents, (IPTR)(grp = HGroup, End), End), End;
    if (!app) return 20;
    for (i = 0; i < 8; i++)
    {
        t[i] = TextObject, MUIA_Font, presets[i], MUIA_Text_Contents, (IPTR)names[i], End;
        DoMethod(grp, OM_ADDMEMBER, (IPTR)t[i]);
    }
    for (i = 0; i < n; i++)
        DoMethod(grp, OM_ADDMEMBER, (IPTR)SimpleButton("Button"));
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    printf("open %ld\n", (long)open);
    if (open)
        for (i = 0; i < 8; i++)
        {
            struct TextFont *f = _font(t[i]);
            printf("%s %s/%d\n", names[i], f->tf_Message.mn_Node.ln_Name, f->tf_YSize);
        }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
