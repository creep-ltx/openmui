/* imgclass: what an external BOOPSI image class gives without MUI.
 *   imgclass FILE [CLASSNAME]
 * Opens the class library by path, makes an object of its public class
 * (CLASSNAME, else the file name without ".image") with no tags, with
 * SYSIA_DrawInfo, and with IA_Width/IA_Height 0, and prints the Image's
 * Width/Height and what IM_DOMAINFRAME (V44) answers for the minimum,
 * nominal and maximum size. For comparing Zune's "3:" image sizes with a
 * real MUI's, black-box. Test tool only. */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/imageclass.h>
#include <intuition/screens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <string.h>

static void show(const char *what, Object *o, struct DrawInfo *dri, struct Screen *s)
{
    struct Image *im = (struct Image *)o;
    struct impDomainFrame d;
    LONG w;

    if (!o)
    {
        printf("%-12s no object\n", what);
        return;
    }
    printf("%-12s Width %d Height %d", what, im->Width, im->Height);
    for (w = IDOMAIN_MINIMUM; w <= IDOMAIN_MAXIMUM; w++)
    {
        memset(&d, 0, sizeof(d));
        d.MethodID = IM_DOMAINFRAME;
        d.imp_DrInfo = dri;
        d.imp_RPort = &s->RastPort;
        d.imp_Which = w;
        if (DoMethodA(o, (Msg)&d))
            printf("  domain%ld %dx%d", (long)w, d.imp_Domain.Width,
                d.imp_Domain.Height);
    }
    printf("\n");
    DisposeObject(o);
}

int main(int argc, char **argv)
{
    struct Library *lib;
    struct Screen *s;
    struct DrawInfo *dri;
    char id[64];
    int n;

    if (argc < 2)
        return 10;
    if (!(lib = OpenLibrary(argv[1], 0)))
    {
        printf("cannot open %s\n", argv[1]);
        return 10;
    }
    if (argc > 2)
        snprintf(id, sizeof(id), "%s", argv[2]);
    else
    {
        snprintf(id, sizeof(id), "%s", FilePart(argv[1]));
        n = strlen(id);
        if (n > 6 && !strcmp(id + n - 6, ".image"))
            id[n - 6] = 0;
    }
    printf("%s: %s %d.%d, class %s\n", argv[1], lib->lib_Node.ln_Name,
        lib->lib_Version, lib->lib_Revision, id);
    s = LockPubScreen(NULL);
    dri = GetScreenDrawInfo(s);
    show("no tags", NewObject(NULL, id, TAG_DONE), dri, s);
    show("DrawInfo", NewObject(NULL, id, SYSIA_DrawInfo, dri, TAG_DONE), dri, s);
    show("0x0", NewObject(NULL, id, IA_Width, 0, IA_Height, 0, TAG_DONE), dri, s);
    show("1x1", NewObject(NULL, id, IA_Width, 1, IA_Height, 1, TAG_DONE), dri, s);
    show("DrawInfo 1x1", NewObject(NULL, id, SYSIA_DrawInfo, dri, IA_Width, 1,
        IA_Height, 1, TAG_DONE), dri, s);
    {
        /* a size set after OM_NEW */
        Object *o = NewObject(NULL, id, SYSIA_DrawInfo, dri, TAG_DONE);

        if (o)
            SetAttrs(o, IA_Width, 1, IA_Height, 1, TAG_DONE);
        show("set 1x1", o, dri, s);
    }
    show("size 0", NewObject(NULL, id, SYSIA_DrawInfo, dri, SYSIA_Size, 0, TAG_DONE), dri, s);
    FreeScreenDrawInfo(s, dri);
    UnlockPubScreen(NULL, s);
    CloseLibrary(lib);
    return 0;
}
