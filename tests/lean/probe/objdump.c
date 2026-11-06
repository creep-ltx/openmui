/* objdump [TITLE]: dump the MUI object tree of every open window whose
 * title starts with TITLE; the windows come from zspy (run it first). */
#include <libraries/mui.h>
#include <intuition/intuitionbase.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <string.h>
#include "dumptree.h"
#include "zspy.h"

struct Library *MUIMasterBase;

int main(int argc, char **argv)
{
    struct ZSpy *spy;
    int i;
    const char *t = argc > 1 ? argv[1] : "";
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    if (!(spy = (struct ZSpy *)FindPort(ZSPY_NAME))) { printf("no zspy\n"); return 10; }
    for (i = 0; i < ZSPY_MAX; i++)
    {
        Object *wo = spy->obj[i], *root = NULL;
        struct Window *w = NULL;
        if (!wo || spy->kind[i] != 1) continue;
        get(wo, MUIA_Window_Window, &w);
        if (!w || !w->Title || strncmp((char *)w->Title, t, strlen(t))) continue;
        printf("== %s: %dx%d min %dx%d max %dx%d borders l%d t%d r%d b%d\n", (char *)w->Title,
            w->Width, w->Height, w->MinWidth, w->MinHeight, w->MaxWidth, w->MaxHeight,
            w->BorderLeft, w->BorderTop, w->BorderRight, w->BorderBottom);
        if (get(wo, MUIA_Window_RootObject, &root) && root)
            dumptree(root, 0);
    }
    CloseLibrary(MUIMasterBase);
    return 0;
}
