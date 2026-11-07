/* asltest: MUI_AllocAslRequest + MUI_AslRequest as PFS3Doctor 0.1b1 used them
 * asltest VERSION [raw|nowin|intui] [macro]: the parent window is
 * MUIA_Window_Window, with "macro" _window(win) as PFS3Doctor passed it */
#include <libraries/mui.h>
#include <libraries/asl.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/asl.h>
#include <string.h>
struct Library *AslBase;
#include <stdlib.h>
struct Library *MUIMasterBase;
int main(int argc, char **argv)
{
    Object *app, *win;
    struct Window *zw;
    APTR fr;
    LONG ok = -1;
    int ver = argc > 1 ? atoi(argv[1]) : 19;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", ver))) return 20;
    app = ApplicationObject, MUIA_Application_Title, "asltest",
        SubWindow, win = WindowObject, MUIA_Window_Title, "asltest",
            WindowContents, VGroup, Child, TextObject, MUIA_Text_Contents, "asl", End, End, End, End;
    if (!app) { CloseLibrary(MUIMasterBase); return 20; }
    set(win, MUIA_Window_Open, TRUE);
    /* the parent: MUIA_Window_Window, or with "macro" as the last argument
     * _window(win), which is defined for area objects only (on a Window
     * object it reads past the object's notify data) */
    zw = (struct Window *)XGET(win, MUIA_Window_Window);
    Printf("MUIA_Window_Window %lx, _window(win) %lx\n", (LONG)zw, (LONG)_window(win));
    if (!strcmp(argv[argc - 1], "macro")) zw = _window(win);
    /* raw: asl.library called directly with the Zune window as parent;
     * nowin: without a parent window; intui: an Intuition window of our
     * own on the same screen as parent */
    if (argc > 2 && (!strcmp(argv[2], "raw") || !strcmp(argv[2], "nowin")
        || !strcmp(argv[2], "intui"))) {
        struct Window *w = zw, *own = NULL;
        AslBase = OpenLibrary("asl.library", 37);
        if (w) Printf("parent window %lx screen %lx rport font %lx\n", (LONG)w,
            (LONG)w->WScreen, (LONG)w->RPort->Font);
        if (!strcmp(argv[2], "nowin")) w = NULL;
        else if (!strcmp(argv[2], "intui") && zw)
            w = own = OpenWindowTags(NULL, WA_PubScreen, (ULONG)zw->WScreen,
                WA_Width, 200, WA_Height, 80, WA_Title, (ULONG)"own", WA_DragBar, TRUE, TAG_DONE);
        fr = AllocAslRequestTags(ASL_FileRequest, ASLFR_TitleText, "Save the log as",
            ASLFR_InitialFile, "PFS3Doctor.log", ASLFR_DoSaveMode, TRUE, TAG_DONE);
        Printf("raw alloc %lx\n", (LONG)fr);
        if (fr) { SetIoErr(0); ok = w ? AslRequestTags(fr, ASLFR_Window, (ULONG)w, TAG_DONE)
                : AslRequestTags(fr, TAG_DONE);
            Printf("raw request %ld ioerr %ld\n", ok, IoErr()); FreeAslRequest(fr); }
        if (own) CloseWindow(own);
        CloseLibrary(AslBase); MUI_DisposeObject(app); CloseLibrary(MUIMasterBase); return 0;
    }
    fr = MUI_AllocAslRequestTags(ASL_FileRequest, ASLFR_TitleText, "Save the log as",
        ASLFR_InitialFile, "PFS3Doctor.log", ASLFR_DoSaveMode, TRUE, TAG_DONE);
    Printf("lib %ld.%ld alloc %lx\n", (LONG)MUIMasterBase->lib_Version, (LONG)MUIMasterBase->lib_Revision, (LONG)fr);
    if (fr) {
        ok = MUI_AslRequestTags(fr, ASLFR_Window, (ULONG)zw, TAG_DONE);
        Printf("request %ld file %s\n", ok, (LONG)((struct FileRequester *)fr)->fr_File);
        MUI_FreeAslRequest(fr);
    }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
