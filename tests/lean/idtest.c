/* idtest: which MUI a program gets from Zune (identity.c, roadmap phase 8).
 *
 *   idtest VERSION [AGAIN] [wait]
 *
 * Opens muimaster.library with VERSION (0 = OldOpenLibrary), prints the
 * version of the base it got, the base in the library list, and the
 * identity Zune gave the application object it makes (Zune's private
 * MUI_GlobalInfo byte: 1 MUI 3.8, 2 MUI 4, 3 MUI 5; this test knows the
 * private layout, programs must not). AGAIN opens it a second time with
 * another version (a program's later opens keep its first identity).
 * "wait" keeps it all open until Ctrl-C, so that two can run at once,
 * and prints again at the end. Built by hand:
 *   m68k-amigaos-gcc -m68000 -noixemul -Os -s -o idtest tests/lean/idtest.c -lmui
 */
#include <exec/execbase.h>
#include <dos/dos.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Library *MUIMasterBase;

static int identity_of(Object *app)
{
    /* Zune's struct MUI_GlobalInfo: the first byte of priv0 */
    UBYTE *gi = (UBYTE *)muiGlobalInfo(app);
    return gi ? gi[0] : -1;
}

static void report(const char *what, struct Library *b, Object *app)
{
    struct Library *shared;

    Forbid();
    shared = (struct Library *)FindName(&SysBase->LibList, "muimaster.library");
    Permit();
    printf("%s: base %08lx %d.%d (list: %08lx %d.%d) app identity %d\n", what,
        (ULONG)b, b->lib_Version, b->lib_Revision, (ULONG)shared,
        shared ? shared->lib_Version : 0, shared ? shared->lib_Revision : 0,
        app ? identity_of(app) : 0);
}

int main(int argc, char **argv)
{
    LONG v = argc > 1 ? atol(argv[1]) : 19, again = -1;
    BOOL wait = FALSE;
    struct Library *second = NULL;
    Object *app;
    int i;

    for (i = 2; i < argc; i++)
        if (!strcmp(argv[i], "wait"))
            wait = TRUE;
        else
            again = atol(argv[i]);
    MUIMasterBase = v ? OpenLibrary("muimaster.library", v)
        : OldOpenLibrary("muimaster.library");
    if (!MUIMasterBase)
    {
        printf("asked %ld: no library\n", v);
        return 20;
    }
    app = MUI_NewObject(MUIC_Application, MUIA_Application_Title, "idtest",
        MUIA_Application_Base, "IDTEST", MUIA_Application_UseCommodities, FALSE,
        MUIA_Application_UseRexx, FALSE, TAG_DONE);
    printf("asked %ld ", v);
    report("got", MUIMasterBase, app);
    if (again >= 0 && (second = OpenLibrary("muimaster.library", again)))
    {
        printf("asked again %ld ", again);
        report("got", second, NULL);
    }
    if (wait)
    {
        Wait(SIGBREAKF_CTRL_C);
        printf("at the end ");
        report("still", MUIMasterBase, app);
    }
    if (app)
        MUI_DisposeObject(app);
    if (second)
        CloseLibrary(second);
    CloseLibrary(MUIMasterBase);
    return 0;
}
