/* asltrace: what MUI_AllocAslRequest / MUI_AslRequest pass to asl.library.
 * Patches asl's AllocAslRequest and AslRequest in this task's calls,
 * prints the tags, then opens a MUI window and runs a requester from it. */
#include <libraries/mui.h>
#include <libraries/asl.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/asl.h>
#include <proto/utility.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>

struct Library *MUIMasterBase, *AslBase;
static APTR oldalloc, oldreq;
static struct Task *me;

APTR callalloc(APTR f __asm("a3"), ULONG t __asm("d0"), struct TagItem *tl __asm("a0"), struct Library *b __asm("a6"));
__asm(".text\n.globl _callalloc\n_callalloc:\n\tjmp (a3)\n");
BOOL callreq(APTR f __asm("a3"), APTR r __asm("a0"), struct TagItem *tl __asm("a1"), struct Library *b __asm("a6"));
__asm(".text\n.globl _callreq\n_callreq:\n\tjmp (a3)\n");

static void dumptags(const char *what, struct TagItem *tl)
{
    struct TagItem *t, *st = tl;
    printf("%s tags %p:\n", what, (void *)tl);
    while ((t = NextTagItem(&st)))
        printf("  %08lx = %08lx\n", (unsigned long)t->ti_Tag, (unsigned long)t->ti_Data);
}

static APTR myalloc(ULONG t __asm("d0"), struct TagItem *tl __asm("a0"), struct Library *b __asm("a6"))
{
    APTR r;
    if (FindTask(NULL) == me) { printf("AllocAslRequest type %lu\n", (unsigned long)t); dumptags("alloc", tl); }
    r = callalloc(oldalloc, t, tl, b);
    if (FindTask(NULL) == me) printf("  -> %p\n", r);
    return r;
}
static BOOL myreq(APTR r __asm("a0"), struct TagItem *tl __asm("a1"), struct Library *b __asm("a6"))
{
    BOOL x;
    if (FindTask(NULL) == me) { printf("AslRequest %p\n", r); dumptags("request", tl); }
    x = callreq(oldreq, r, tl, b);
    if (FindTask(NULL) == me) printf("  -> %d\n", (int)x);
    return x;
}

int main(int argc, char **argv)
{
    Object *app, *win;
    struct Window *iw = NULL;
    struct FileRequester *fr;
    BOOL ok;
    me = FindTask(NULL);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    if (!(AslBase = OpenLibrary("asl.library", 37))) return 20;
    Forbid();
    oldalloc = SetFunction(AslBase, -48, (APTR)myalloc);
    oldreq = SetFunction(AslBase, -60, (APTR)myreq);
    Permit();
    app = ApplicationObject, MUIA_Application_Title, (IPTR)"AslTrace", MUIA_Application_Base, (IPTR)"ASLTRACE",
        SubWindow, (IPTR)(win = WindowObject, MUIA_Window_Title, (IPTR)"AslTrace",
            WindowContents, (IPTR)(VGroup, Child, (IPTR)SimpleButton("A button"), End), End), End;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Window, &iw);
    printf("window %p\n", (void *)iw);
    fr = MUI_AllocAslRequestTags(ASL_FileRequest, ASLFR_TitleText, (IPTR)"Trace me", ASLFR_DrawersOnly, TRUE, TAG_DONE);
    printf("MUI_AllocAslRequest -> %p\n", (void *)fr);
    fflush(stdout);
    if (fr)
    {
        ok = MUI_AslRequestTags(fr, ASLFR_Window, (IPTR)iw, TAG_DONE);
        printf("MUI_AslRequest -> %d drawer %s file %s\n", (int)ok, fr->fr_Drawer ? (char *)fr->fr_Drawer : "-", fr->fr_File ? (char *)fr->fr_File : "-");
        MUI_FreeAslRequest(fr);
    }
    MUI_DisposeObject(app);
    Forbid();
    SetFunction(AslBase, -48, oldalloc);
    SetFunction(AslBase, -60, oldreq);
    Permit();
    CloseLibrary(AslBase);
    CloseLibrary(MUIMasterBase);
    return 0;
}
