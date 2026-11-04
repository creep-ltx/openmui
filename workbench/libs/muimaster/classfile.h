/* Classes built as files of their own (LIBS:Zune/<Name>.mui), loaded the
 * first time a program makes one of their objects, the way MUI 3.8 ships
 * its rarely used classes in MUI:Libs/mui. The class source is unchanged;
 * the file (classfile.c) is a small library whose second vector hands the
 * class descriptions to muimaster.library, which makes the classes through
 * its normal built-in path (support_classes.c). The class code reaches the
 * library's internal functions through a table in the library: the
 * generator (gcc/classfiles.py) lists every library function the class
 * files call, writes the table for the library and one jump per function
 * for the class files, and stamps both with a checksum of the list, so a
 * class file only works with the library it was built with.
 */
#ifndef ZUNE_CLASSFILE_H
#define ZUNE_CLASSFILE_H

#include <exec/types.h>
#include <exec/libraries.h>

#define ZUNE_CLASSFILE_MAGIC 0x5A364346  /* 'Z6CF' */

/* The library bases a class file shares with muimaster.library, in this
 * order. Lazily opened ones may still be NULL; the class code opens them
 * through zune_lazylib_open() (lazylib.h) like the library does. */
#define ZUNE_CLASSFILE_BASES(X) \
    X(SysBase) X(DOSBase) X(UtilityBase) X(GfxBase) X(IntuitionBase) \
    X(LayersBase) X(KeymapBase) X(GadToolsBase) X(LocaleBase) \
    X(CyberGfxBase) X(AslBase) X(CxBase) X(RexxSysBase) X(DataTypesBase) \
    X(IFFParseBase) X(DiskfontBase) X(IconBase) X(WorkbenchBase) \
    X(MathIeeeDoubBasBase) X(MathIeeeDoubTransBase) X(MUIMasterBase)

#define ZUNE_CLASSFILE_COUNTBASE(n) + 1
#define ZUNE_CLASSFILE_NBASES (0 ZUNE_CLASSFILE_BASES(ZUNE_CLASSFILE_COUNTBASE))

struct ZuneClassFileIface
{
    ULONG        magic;         /* ZUNE_CLASSFILE_MAGIC */
    ULONG        id;            /* checksum of the function list */
    ULONG        count;         /* functions in funcs */
    const APTR  *funcs;
    APTR        *bases[ZUNE_CLASSFILE_NBASES];
};

/* In the library: the table (generated) and the loader. */
extern const struct ZuneClassFileIface zune_classfile_iface;
struct __MUIBuiltinClass;
const struct __MUIBuiltinClass *ZUNE_LoadClassFile(CONST_STRPTR classid,
    struct Library *MUIMasterBase);
void ZUNE_CloseClassFiles(struct Library *MUIMasterBase);

/* The id string of every class file contains this. */
#define ZUNE_CLASSFILE_IDTAG "Zune68 class"

#endif /* ZUNE_CLASSFILE_H */
