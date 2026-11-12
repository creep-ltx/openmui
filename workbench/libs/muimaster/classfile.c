/* One class file, e.g. LIBS:Zune/Floattext.mui (see classfile.h).
 * Built once per class source with
 *   -DZCF_NAME=\"Floattext.mui\" -DZCF_DESCS=_MUI_Floattext_desc
 *   -DZCF_DESC_PTRS=&_MUI_Floattext_desc
 * (both may list several descriptions, comma separated).
 * Vector 5 (-30) is the usual MCC_Query and returns NULL: this is not a
 * custom class. Vector 6 (-36) gives muimaster.library a class description
 * by name, once the library's interface has been checked.
 * The library does not OpenLibrary the file: it LoadSegs a private copy
 * and calls vector 6 through the Resident tag's function table
 * (support_classes.c), because the globals below (bases, jump table)
 * belong to one library, and so that no .mui of the same name in the
 * library list (a real MUI's) is ever taken for it. The library vectors stay so that the file
 * is still a well-formed library (version, the id string).
 */
#include <exec/resident.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include "classfile.h"
#include "native_version.h"
#include "build-date.h"

struct __MUIBuiltinClass
{
    CONST_STRPTR name;
    CONST_STRPTR supername;
    ULONG datasize;
    ULONG (*dispatcher)();
};

#define ZCF_STR2(x) #x
#define ZCF_STR(x) ZCF_STR2(x)

extern const struct __MUIBuiltinClass ZCF_DESCS;
static const struct __MUIBuiltinClass *const descs[] = { ZCF_DESC_PTRS };

/* cfbases.c, and the generated jump table (classstubs.s) */
void zune_cf_setbases(const struct ZuneClassFileIface *iface);
extern const struct ZuneClassFileIface *zune_cf_iface;
extern const ULONG zune_cf_id, zune_cf_count;

struct ClassFileBase
{
    struct Library lib;
    UWORD pad;
    BPTR seglist;
};

static const char libname[] = ZCF_NAME;
static const char libid[] = ZCF_NAME " " ZUNE68_LIBRARY_VERSION_STRING
    " (" ZUNE68_LIBRARY_DATE ") " ZUNE_CLASSFILE_IDTAG "\r\n";
const char zune_cf_verstag[] = "$VER: " ZCF_NAME " "
    ZUNE68_LIBRARY_VERSION_STRING " (" ZUNE68_LIBRARY_DATE ") "
    ZUNE_CLASSFILE_IDTAG;

static void freebase(struct ClassFileBase *b)
{
    FreeMem((UBYTE *)b - b->lib.lib_NegSize,
        b->lib.lib_NegSize + b->lib.lib_PosSize);
}

static struct Library *cf_open(struct ClassFileBase *b __asm("a6"))
{
    b->lib.lib_OpenCnt++;
    b->lib.lib_Flags &= ~LIBF_DELEXP;
    return &b->lib;
}

static BPTR cf_expunge(struct ClassFileBase *b __asm("a6"))
{
    BPTR seg;

    if (b->lib.lib_OpenCnt)
    {
        b->lib.lib_Flags |= LIBF_DELEXP;
        return 0;
    }
    seg = b->seglist;
    Remove(&b->lib.lib_Node);
    freebase(b);
    return seg;
}

static BPTR cf_close(struct ClassFileBase *b __asm("a6"))
{
    if (--b->lib.lib_OpenCnt == 0)
    {
        /* The library that used the classes has freed them: another
         * muimaster.library may use this file next. */
        zune_cf_iface = NULL;
        if (b->lib.lib_Flags & LIBF_DELEXP)
            return cf_expunge(b);
    }
    return 0;
}

static ULONG cf_null(void)
{
    return 0;
}

static const struct __MUIBuiltinClass *cf_query(
    const struct ZuneClassFileIface *iface __asm("a0"),
    CONST_STRPTR name __asm("a1"), struct ClassFileBase *b __asm("a6"))
{
    unsigned i;

    if (!iface || iface->magic != ZUNE_CLASSFILE_MAGIC
        || iface->id != zune_cf_id || iface->count != zune_cf_count)
        return NULL;
    /* One library per copy: the bases and functions are global. The
     * library loads a private copy each, so this only refuses a second
     * library on a copy opened as a shared library. */
    if (zune_cf_iface && zune_cf_iface != iface)
        return NULL;
    zune_cf_iface = iface;
    zune_cf_setbases(iface);
    for (i = 0; i < sizeof(descs) / sizeof(descs[0]); i++)
    {
        const char *a = (const char *)descs[i]->name, *c = (const char *)name;

        while (*a && *a == *c)
            a++, c++;
        if (*a == *c)
            return descs[i];
    }
    return NULL;
}

static struct Library *cf_init(struct ClassFileBase *b __asm("d0"),
    BPTR seglist __asm("a0"), struct ExecBase *sys __asm("a6"))
{
    /* Opened as a shared library (OpenLibrary of LIBS:Zune/X.mui: the
     * external-class path tries it when the private load failed for
     * lack of memory), the bases were never copied in: cf_expunge's
     * Remove() and FreeMem() at the next flush went through a NULL
     * SysBase and reset the machine (fault sweep, stability round 2).
     * From address 4, not from a6: reading the a6 argument makes GCC
     * save a6 (a frame of 4 bytes where there was none). */
    (void)sys;
    SysBase = *(struct ExecBase **)4;
    b->seglist = seglist;
    b->lib.lib_Node.ln_Type = NT_LIBRARY;
    b->lib.lib_Node.ln_Name = (char *)libname;
    b->lib.lib_Flags = LIBF_SUMUSED | LIBF_CHANGED;
    b->lib.lib_Version = ZUNE68_LIBRARY_VERSION;
    b->lib.lib_Revision = ZUNE68_LIBRARY_REVISION;
    b->lib.lib_IdString = (APTR)libid;
    return &b->lib;
}

static const APTR functable[] = {
    (APTR)cf_open, (APTR)cf_close, (APTR)cf_expunge, (APTR)cf_null,
    (APTR)cf_null, (APTR)cf_query, (APTR)-1
};

static const ULONG inittab[4] = {
    sizeof(struct ClassFileBase), (ULONG)functable, 0, (ULONG)cf_init
};

const struct Resident zune_cf_romtag = {
    RTC_MATCHWORD, (struct Resident *)&zune_cf_romtag,
    (APTR)(&zune_cf_romtag + 1), RTF_AUTOINIT, ZUNE68_LIBRARY_VERSION,
    NT_LIBRARY, 0, (char *)libname, (char *)libid, (APTR)inittab
};
