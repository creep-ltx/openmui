/* asltest: MUI_AllocAslRequest + MUI_AslRequest as PFS3Doctor 0.1b1 used them */
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
    APTR fr;
    LONG ok = -1;
    int ver = argc > 1 ? atoi(argv[1]) : 19;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", ver))) return 20;
    app = ApplicationObject, MUIA_Application_Title, "asltest",
        SubWindow, win = WindowObject, MUIA_Window_Title, "asltest",
            WindowContents, VGroup, Child, TextObject, MUIA_Text_Contents, "asl", End, End, End, End;
    if (!app) { CloseLibrary(MUIMasterBase); return 20; }
    set(win, MUIA_Window_Open, TRUE);
    if (argc > 2 && !strcmp(argv[2], "raw")) {
        AslBase = OpenLibrary("asl.library", 37);
        fr = AllocAslRequestTags(ASL_FileRequest, ASLFR_TitleText, "Save the log as",
            ASLFR_InitialFile, "PFS3Doctor.log", ASLFR_DoSaveMode, TRUE, TAG_DONE);
        Printf("raw alloc %lx\n", (LONG)fr);
        if (fr) { ok = AslRequestTags(fr, ASLFR_Window, (ULONG)_window(win), TAG_DONE);
            Printf("raw request %ld\n", ok); FreeAslRequest(fr); }
        CloseLibrary(AslBase); MUI_DisposeObject(app); CloseLibrary(MUIMasterBase); return 0;
    }
    fr = MUI_AllocAslRequestTags(ASL_FileRequest, ASLFR_TitleText, "Save the log as",
        ASLFR_InitialFile, "PFS3Doctor.log", ASLFR_DoSaveMode, TRUE, TAG_DONE);
    Printf("lib %ld.%ld alloc %lx\n", (LONG)MUIMasterBase->lib_Version, (LONG)MUIMasterBase->lib_Revision, (LONG)fr);
    if (fr) {
        ok = MUI_AslRequestTags(fr, ASLFR_Window, (ULONG)_window(win), TAG_DONE);
        Printf("request %ld file %s\n", ok, (LONG)((struct FileRequester *)fr)->fr_File);
        MUI_FreeAslRequest(fr);
    }
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
