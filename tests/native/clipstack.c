/* clipstack: which region MUI_RemoveClipping (MUI_RemoveClipRegion) installs, black box.
 * MUI 3.8 and MUI 5 install the MUI region pushed before the removed one, or none when the stack is
 * empty: a region the program installed itself between two pushes, or before the first, is not
 * put back although MUI_AddClipping returned it as the handle.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -Wl,-u,___stkinit clipstack.c -o clipstack -lamiga
 * Output (MUI 3.8, MUI 5): "nested 1", "between: handle 1 own 0 first 1", "base: handle 1 own 0 none 1" */
#include <stdio.h>
#include <libraries/mui.h>
#include <graphics/regions.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/layers.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
struct Library *MUIMasterBase;
ULONG __stack = 65536;
static void install(struct MUI_RenderInfo *mri, struct Region *r)
{
    LockLayerInfo(&mri->mri_Screen->LayerInfo);
    InstallClipRegion(mri->mri_Window->WLayer, r);
    UnlockLayerInfo(&mri->mri_Screen->LayerInfo);
}
int main(void)
{
    Object *app, *win, *root;
    struct MUI_RenderInfo *mri;
    struct Layer *l;
    struct Region *own, *first;
    struct Rectangle rc = {0, 0, 30, 30};
    APTR h1, h2;
    ULONG open = 0;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject, MUIA_Application_Title, "clipstack",
        SubWindow, win = WindowObject, MUIA_Window_Title, "clipstack",
            WindowContents, root = VGroup, Child, SimpleButton("Button"), End,
        End, End;
    if (!app) return 20;
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    if (!open || !(own = NewRegion())) return 20;
    OrRectRegion(own, &rc);
    mri = muiRenderInfo(root);
    l = mri->mri_Window->WLayer;
    h1 = MUI_AddClipping(mri, 0, 0, 100, 100);
    first = l->ClipRegion;
    h2 = MUI_AddClipping(mri, 10, 10, 20, 20);
    MUI_RemoveClipping(mri, h2);
    printf("nested %ld\n", (long)(l->ClipRegion == first));
    install(mri, own);
    h2 = MUI_AddClipping(mri, 5, 5, 50, 50);
    MUI_RemoveClipping(mri, h2);
    printf("between: handle %ld own %ld first %ld\n", (long)(h2 == own),
        (long)(l->ClipRegion == own), (long)(l->ClipRegion == first));
    install(mri, first);
    MUI_RemoveClipping(mri, h1);
    install(mri, own);
    h1 = MUI_AddClipping(mri, 0, 0, 100, 100);
    MUI_RemoveClipping(mri, h1);
    printf("base: handle %ld own %ld none %ld\n", (long)(h1 == own),
        (long)(l->ClipRegion == own), (long)(l->ClipRegion == NULL));
    install(mri, NULL);
    DisposeRegion(own);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
