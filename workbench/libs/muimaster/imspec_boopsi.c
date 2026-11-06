/*
 * "3:<name>.image" image specs: an external BOOPSI image class, as MUI's
 * autodoc of MUIA_Image_Spec describes "3:" ("the name of an external
 * boopsi image class"). The class is a class library file, e.g.
 * LIBS:Zune/Images/kmel/kmel_arrowdown.image: the name is looked for in
 * the image drawers (IMSPEC_EXTERNAL_PREFIX..., or as it is when it has a
 * volume), the file opened by its path with OpenLibrary() (exec counts the
 * opens: one per image spec that is set up), an object made of its public
 * class (the file name, then the file name without ".image", as image
 * class libraries name their classes) or of the class a ClassLibrary base
 * carries, and both given back at cleanup. Standard AmigaOS BOOPSI, written
 * from the RKRM and the NDK, not from MUI.
 *
 * The sizes: image classes written for MUI answer a method of their own
 * with their minimum, maximum and default size and keep the render info
 * for MUI's pens (ZUNE_IMAGECLASS_SIZES; found in the dispatcher of the
 * kmel images, which ship with MUI 5 in MUI:Images/kmel). MUI 5 shows such
 * an image at its minimum (measured: kmel_arrowdown 4x4, kmel_cycle 16x8,
 * tests/lean/boopsiimg.c). Any other class has the size its object has
 * (an imageclass object without IA_Width/IA_Height is 80x40).
 *
 * Part of the picture module (classes/zpicture.c): only programs that use
 * such a spec load it. imspec.c draws the object itself.
 */
#include <exec/execbase.h>
#include <exec/memory.h>
#include <dos/dosextens.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/imageclass.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <clib/alib_protos.h>
#include <string.h>
#include <stdio.h>

#include "support.h"
#include "mui.h"

/* The size method of image classes written for MUI: the message's longs
 * at 12..32 are filled in (minimum, maximum, default width and height),
 * the one at 36 is the MUI_RenderInfo the object draws with; the answer
 * is the method's own id. */
#define ZUNE_IMAGECLASS_SIZES 0x80427157

struct ZuneImageclassSizes
{
    ULONG MethodID;
    ULONG unused[2];
    LONG min_width, min_height, max_width, max_height, def_width, def_height;
    struct MUI_RenderInfo *mri;
};

static CONST_STRPTR const boopsi_roots[] = {
    IMSPEC_EXTERNAL_PREFIX,
#ifdef IMSPEC_EXTERNAL_PREFIX2
    IMSPEC_EXTERNAL_PREFIX2,
#endif
};
#define BOOPSI_ROOTS (sizeof(boopsi_roots) / sizeof(boopsi_roots[0]))

static Object *boopsi_new(Class *cl, CONST_STRPTR id, struct DrawInfo *dri)
{
    struct TagItem tags[] = {
        { SYSIA_DrawInfo, (IPTR)dri },
        { TAG_DONE, 0 }
    };

    return NewObjectA(cl, (ClassID)id, tags);
}

static UWORD clamp_size(LONG v, UWORD otherwise)
{
    return v <= 0 ? otherwise : v > MUI_MAXMAX ? MUI_MAXMAX : (UWORD)v;
}

BOOL zmod_boopsi_open(CONST_STRPTR name, struct MUI_RenderInfo *mri,
    struct Library **basep, Object **objp, UWORD *sizes)
{
    struct DrawInfo *dri = mri->mri_DrawInfo;
    struct ZuneImageclassSizes msg;
    struct Process *me = (struct Process *)FindTask(NULL);
    struct Library *lib = NULL;
    Object *o = NULL;
    char path[256], id[64];
    APTR oldwin;
    int r, n;

    *basep = NULL;
    *objp = NULL;
    /* Lock and LoadSeg need a process (images are set up in the
     * program's own) */
    if (!name || !name[0] || me->pr_Task.tc_Node.ln_Type != NT_PROCESS)
        return FALSE;
    /* no "please insert volume MUI" when MUI: is not assigned */
    oldwin = me->pr_WindowPtr;
    me->pr_WindowPtr = (APTR)-1;
    for (r = strchr((const char *)name, ':') ? -1 : 0;
        !lib && r < (int)BOOPSI_ROOTS; r++)
    {
        BPTR lock;

        if (r < 0)
            snprintf(path, sizeof(path), "%s", name);
        else
            snprintf(path, sizeof(path), "%s%s", boopsi_roots[r], name);
        /* only a file that is there: OpenLibrary would search LIBS: for
         * the bare name otherwise */
        if ((lock = Lock((STRPTR)path, ACCESS_READ)))
        {
            UnLock(lock);
            lib = OpenLibrary((STRPTR)path, 0);
        }
        if (r < 0)
            break;
    }
    me->pr_WindowPtr = oldwin;
    if (!lib)
        return FALSE;

    snprintf(id, sizeof(id), "%s", FilePart((STRPTR)name));
    o = boopsi_new(NULL, id, dri);
    n = strlen(id);
    if (!o && n > 6 && !strcmp(id + n - 6, ".image"))
    {
        id[n - 6] = 0;
        o = boopsi_new(NULL, id, dri);
    }
    if (!o && lib->lib_PosSize >= (UWORD)sizeof(struct ClassLibrary))
    {
        Class *cl = ((struct ClassLibrary *)lib)->cl_Class;

        if (cl && !((IPTR)cl & 1) && TypeOfMem(cl))
            o = boopsi_new(cl, NULL, dri);
    }
    if (!o)
    {
        CloseLibrary(lib);
        return FALSE;
    }
    /* min width, min height, max width, max height */
    memset(&msg, 0, sizeof(msg));
    msg.MethodID = ZUNE_IMAGECLASS_SIZES;
    msg.mri = mri;
    if (DoMethodA(o, (Msg)&msg) == ZUNE_IMAGECLASS_SIZES)
    {
        sizes[0] = clamp_size(msg.min_width, 1);
        sizes[1] = clamp_size(msg.min_height, 1);
        sizes[2] = clamp_size(msg.max_width, MUI_MAXMAX);
        sizes[3] = clamp_size(msg.max_height, MUI_MAXMAX);
    }
    else
    {
        struct Image *im = (struct Image *)o;

        sizes[0] = clamp_size(im->Width, 8);
        sizes[1] = clamp_size(im->Height, 8);
        sizes[2] = MUI_MAXMAX;
        sizes[3] = MUI_MAXMAX;
    }
    *basep = lib;
    *objp = o;
    return TRUE;
}

void zmod_boopsi_close(struct Library *base, Object *obj)
{
    if (obj)
        DisposeObject(obj);
    if (base)
        CloseLibrary(base);
}
