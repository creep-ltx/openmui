/* aboutcrawl: a window with a Crawling object (scrolling credits) and the
 * "About MUI" window (MUIM_Application_AboutMUI, the Aboutmui class), the
 * two classes MUI 3.8 loads from MUI:Libs/mui and OpenMUI from LIBS:OpenMUI
 * since branch smaller2. Both stay open for SECONDS (default 8), then the
 * program quits. Built by hand:
 *   m68k-amigaos-gcc -m68000 -noixemul -Os -s -o aboutcrawl tests/lean/aboutcrawl.c
 */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef IPTR
#define IPTR ULONG
#endif
#ifndef MUIC_Crawling
#define MUIC_Crawling "Crawling.mcc"
#endif

struct Library *MUIMasterBase;

int main(int argc, char **argv)
{
    Object *app, *win, *crawl;
    LONG secs = argc > 1 ? atol(argv[1]) : 8;
    ULONG open = 0;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19)))
        return 20;
    crawl = MUI_NewObject(MUIC_Crawling,
        TextFrame,
        MUIA_FixHeightTxt, "\n\n",
        MUIA_Group_Child, MUI_NewObject(MUIC_Text,
            MUIA_Text_Contents, "\033cCrawling line one\n\033cline two\n"
                "\033cline three\n\033cline four\n\033cline five",
            TAG_DONE),
        TAG_DONE);
    printf("crawling %s\n", crawl ? "made" : "not made");
    app = MUI_NewObject(MUIC_Application,
        MUIA_Application_Title, "aboutcrawl",
        MUIA_Application_Base, "ABOUTCRAWL",
        MUIA_Application_Window, win = MUI_NewObject(MUIC_Window,
            MUIA_Window_Title, "aboutcrawl",
            MUIA_Window_ID, 0,
            MUIA_Window_RootObject, MUI_NewObject(MUIC_Group,
                MUIA_Group_Child, MUI_NewObject(MUIC_Text,
                    MUIA_Text_Contents, "Crawling and About MUI", TAG_DONE),
                crawl ? MUIA_Group_Child : TAG_IGNORE, crawl,
                TAG_DONE),
            TAG_DONE),
        TAG_DONE);
    if (!app)
    {
        printf("no application\n");
        CloseLibrary(MUIMasterBase);
        return 20;
    }
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    printf("window %s\n", open ? "open" : "not open");
    DoMethod(app, MUIM_Application_AboutMUI, win);
    for (secs *= 10; secs > 0; secs--)
    {
        ULONG sigs = 0;
        DoMethod(app, MUIM_Application_NewInput, &sigs);
        Delay(5);
    }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    printf("done\n");
    return 0;
}
