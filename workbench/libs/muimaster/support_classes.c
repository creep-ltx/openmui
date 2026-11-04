/*
    Copyright (C) 2002-2020, The AROS Development Team. All rights reserved.
*/

#include <string.h>
#include <stdio.h>

#include <intuition/classes.h>
#include <clib/alib_protos.h>
#include <exec/resident.h>
#include <exec/execbase.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/muimaster.h>


#include "mui.h"
#include "support.h"
#include "support_classes.h"
#include "muimaster_intern.h"
#include "classfile.h"
#include "classfiles.h"
#include "native_version.h"

/*#define MYDEBUG*/
#include "debug.h"

#if !defined(__AROS__) && !defined(__amigaos4__) && !defined(__MAXON__)
#include "classes/area.h"

/* Global and externally visible: only the trampoline's assembly below
 * refers to it by name, and link-time optimization would otherwise
 * rename a static function and leave _builtinDraw undefined. */
IPTR __attribute__((noinline, used, externally_visible))
builtinDraw(Class *cl __asm("a0"), Object *obj __asm("a2"),
    Msg msg __asm("a1"))
{
    typedef IPTR (*Dispatcher)(Class * __asm("a0"),
        Object * __asm("a2"), Msg __asm("a1"), APTR __asm("a6"));
    Dispatcher dispatch = (Dispatcher)cl->cl_Dispatcher.h_SubEntry;
    IPTR result = dispatch(cl, obj, msg, cl->cl_Dispatcher.h_Data);

    /* Enabled controls take no class walk. Only the outermost built-in
     * draws the pattern, after its inherited content has been painted. */
    if (muiAreaData(obj)->mad_DisableCount)
    {
        Class *derived = OCLASS(obj);

        while (derived && derived->cl_Dispatcher.h_Entry !=
                (HOOKFUNC)builtinDispatcher)
            derived = derived->cl_Super;
        /* MUI 3.8/3.9 Area and Rectangle draw no ghost pattern; custom
         * subclasses of them paint their own disabled appearance */
        if (derived == cl && !(cl->cl_ID &&
                (!strcmp((const char *)cl->cl_ID, MUIC_Area) ||
                !strcmp((const char *)cl->cl_ID, MUIC_Rectangle))))
            ZuneDrawDisabled(obj);
    }
    return result;
}

/* Every method of a built-in class enters here. Only MUIM_Draw needs the
 * frame of builtinDraw; everything else continues into the class
 * dispatcher (cl_Dispatcher.h_SubEntry) with the registers untouched,
 * so the trampoline costs two instructions instead of saving and
 * restoring four registers around a call (perfbench create+dispose,
 * every method call). The dispatchers do not use a6. */
asm(
    "\t.text\n"
    "\t.align 2\n"
    "\t.globl _builtinDispatcher\n"
    "_builtinDispatcher:\n"
    "\tcmpi.l #0x80426f3f,(%a1)\n"      /* MUIM_Draw */
    "\tbeq.s 1f\n"
    "\tmove.l 12(%a0),-(%sp)\n"         /* cl_Dispatcher.h_SubEntry */
    "\trts\n"
    "1:\tjmp _builtinDraw\n"
);
#endif

extern const struct __MUIBuiltinClass _MUI_Settings_desc;

static const struct __MUIBuiltinClass *const builtins[] = {
    &_MUI_Notify_desc,
    &_MUI_Family_desc,
    &_MUI_Application_desc,
    &_MUI_Window_desc,
    &_MUI_Area_desc,
    &_MUI_Rectangle_desc,
    &_MUI_Group_desc,
#if ZUNE_BUILTIN_SELECTGROUP
    &_MUI_Selectgroup_desc,
#endif
#if ZUNE_BUILTIN_SELECTORPANEL
    &_MUI_SelectorPanel_desc,
#endif
#if ZUNE_BUILTIN_FILEPANEL
    &_MUI_Filepanel_desc,
#endif
#if ZUNE_BUILTIN_FONTPANEL
    &_MUI_Fontpanel_desc,
    &_MUI_Fontdisplay_desc,
#endif
#if ZUNE_BUILTIN_KEYADJUST
    &_MUI_Keyadjust_desc,
#endif
    &_MUI_Image_desc,
    &_MUI_Configdata_desc,
    &_MUI_Text_desc,
#if ZUNE_BUILTIN_TEXTDATA
    &_MUI_Textdata_desc,
#endif
#if ZUNE_BUILTIN_TITLE
    &_MUI_Title_desc,
#endif
    &_MUI_Numeric_desc,
    &_MUI_Slider_desc,
    &_MUI_String_desc,
    ZUNE_BOOPSI_DESC & _MUI_Prop_desc,
    &_MUI_Scrollbar_desc,
    &_MUI_Register_desc,
    &_MUI_Menuitem_desc,
    &_MUI_Menu_desc,
    &_MUI_Menustrip_desc,
    ZUNE_VIRTGROUP_DESC ZUNE_SCROLLGROUP_DESC & _MUI_Scrollbutton_desc,
    &_MUI_Semaphore_desc,
#if ZUNE_BUILTIN_PROCESS
    &_MUI_Process_desc,
#endif
    &_MUI_Dataspace_desc,
#if ZUNE_BUILTIN_DATAMAP
    &_MUI_Datamap_desc,
    &_MUI_Objectmap_desc,
#endif
    &_MUI_Bitmap_desc,
    &_MUI_Bodychunk_desc,
    &_MUI_ChunkyImage_desc,
    &_MUI_Cycle_desc,
    &_MUI_Popstring_desc,
    &_MUI_Listview_desc,
    &_MUI_List_desc,
    ZUNE_FLOATTEXT_DESC
    ZUNE_POPASL_DESC & _MUI_Popobject_desc,
    ZUNE_GAUGE_DESC
        ZUNE_ABOUTMUI_DESC
        (&_MUI_Settings_desc),
        ZUNE_SETTINGSGROUP_DESC
        ZUNE_IMAGEADJUST_DESC
        ZUNE_POPIMAGE_DESC
        ZUNE_SCALE_DESC
        ZUNE_RADIO_DESC
        ZUNE_ICONLISTVIEW_DESC
        ZUNE_BALANCE_DESC
        ZUNE_COLORFIELD_DESC
        ZUNE_COLORADJUST_DESC
        ZUNE_IMAGEDISPLAY_DESC
        ZUNE_PENDISPLAY_DESC
        ZUNE_PENADJUST_DESC ZUNE_POPPEN_DESC & _MUI_Mccprefs_desc,
    ZUNE_FRAMEDISPLAY_DESC
        ZUNE_POPFRAME_DESC
        ZUNE_FRAMEADJUST_DESC
        ZUNE_VOLUMELIST_DESC
        ZUNE_DIRLIST_DESC
        ZUNE_NUMERICBUTTON_DESC
        ZUNE_POPLIST_DESC
        ZUNE_CRAWLING_DESC
        ZUNE_POPSCREEN_DESC
        ZUNE_LEVELMETER_DESC
        ZUNE_KNOB_DESC
        ZUNE_DTPIC_DESC
        ZUNE_PALETTE_DESC
        ZUNE_PANEL_DESC
        ZUNE_PANELGROUP_DESC
        ZUNE_DRAGHANDLE_DESC
        ZUNE_PANELTITLE_DESC
};

static BOOL zune_is_mui_file(CONST_STRPTR name)
{
    size_t len = strlen(name);

    return len > 4 && !Stricmp(name + len - 4, ".mui");
}

/* Zune68's own .mui files say so in their id string; those built before
 * 2026-11-04 (Pixmap.mui 20.29) said "AROS native AmigaOS port". */
static BOOL zune_is_own_id(CONST_STRPTR id)
{
    return id && (strstr((const char *)id, "Zune68")
        || strstr((const char *)id, "AROS"));
}

/* The Resident tag in a loaded file: it points at itself once relocated. */
static const struct Resident *zune_find_resident(BPTR seglist)
{
    BPTR seg;

    for (seg = seglist; seg; seg = *(BPTR *)BADDR(seg))
    {
        UWORD *p = (UWORD *)((UBYTE *)BADDR(seg) + 4);
        UWORD *end = (UWORD *)((UBYTE *)BADDR(seg) - 4
            + ((ULONG *)BADDR(seg))[-1]) - sizeof(struct Resident) / 2;

        for (; p <= end; p++)
            if (*p == RTC_MATCHWORD
                && ((const struct Resident *)p)->rt_MatchTag
                    == (const struct Resident *)p)
                return (const struct Resident *)p;
    }
    return NULL;
}

/* Is LIBS:<path> (a ".mui") Zune68's? Decided from the library in memory
 * by that name (OpenLibrary would return it, whatever the drawer), else
 * from the file's Resident tag, without running its code: opening a real
 * MUI .mui runs its init, which opens muimaster.library = Zune. */
static BOOL zune_mui_file_is_own(CONST_STRPTR path)
{
    struct Library *lib;
    const struct Resident *rt;
    TEXT file[260];
    BOOL own = FALSE, inmem = FALSE;
    BPTR seg;

    Forbid();
    lib = (struct Library *)FindName(&SysBase->LibList, FilePart(path));
    if (lib)
    {
        inmem = TRUE;
        own = zune_is_own_id(lib->lib_IdString);
    }
    Permit();
    if (inmem)
        return own;
    if (FindTask(NULL)->tc_Node.ln_Type != NT_PROCESS)
        return FALSE;
    snprintf(file, sizeof(file), "LIBS:%s", path);
    if (!(seg = LoadSeg(file)))
        return FALSE;
    rt = zune_find_resident(seg);
    own = rt && zune_is_own_id(rt->rt_IdString);
    UnLoadSeg(seg);
    return own;
}

Class *ZUNE_GetExternalClass(CONST_STRPTR classname,
    struct Library *MUIMasterBase)
{
    struct Library *mcclib = NULL;
    struct MUI_CustomClass *mcc = NULL;
    CONST_STRPTR const *pathptr;
    TEXT s[255];

    static CONST_STRPTR const searchpaths[] = {
        "Zune/%s",
        "Classes/Zune/%s",
        "MUI/%s",
        NULL,
    };

    for (pathptr = searchpaths; *pathptr; pathptr++)
    {
        snprintf(s, 255, *pathptr, classname);

        D(bug("Trying opening of %s\n", s));

        /* A ".mui" is a built-in class of a muimaster.library. Real
         * MUI's own (MUI:Libs/mui/Pixmap.mui, Balance.mui, ...) work only
         * inside real MUI: open only Zune68's. */
        if (zune_is_mui_file(classname) && !zune_mui_file_is_own(s))
        {
            D(bug("%s is not Zune68's, skipped\n", s));
            continue;
        }

        if ((mcclib = OpenLibrary(s, 0)))
        {
            D(bug("Calling MCC Query. Librarybase at 0x%p\n", mcclib));

            mcc = MCC_Query(0);
            if (!mcc)
                mcc = MCC_Query(1);     /* MCP? */

            if (mcc)
            {
                if (mcc->mcc_Class)
                {
                    mcc->mcc_Module = mcclib;
                    D(bug("Successfully opened %s as external class\n",
                            classname));

                    return mcc->mcc_Class;
                }
            }

            CloseLibrary(mcclib);
        }
    }

    D(bug("Failed to open external class %s\n", classname));
    return NULL;
}

/**************************************************************************/
static Class *ZUNE_FindBuiltinClass(CONST_STRPTR classid, struct Library *MUIMasterBase)
{
    struct MUIMasterBase_intern *intZuneBase = (struct MUIMasterBase_intern *)MUIMasterBase;
    Class *cl = NULL, *cl2;

    cl2 = intZuneBase->BuiltinClassCache[((UBYTE)classid[0] ^ (UBYTE)classid[1]) & 31];
    if (cl2 && (cl2->cl_ID == classid || !strcmp(cl2->cl_ID, classid)))
        return cl2;

    ForeachNode(&intZuneBase->BuiltinClasses, cl2)
    {
        if (cl2->cl_ID == classid ||
            (cl2->cl_ID[0] == classid[0] && !strcmp(cl2->cl_ID, classid)))
        {
            cl = cl2;
            break;
        }
    }

    return cl;
}

#if ZUNE_CLASSFILE_NFILES
/* Class name -> class file, and the files (generated: classfiles.h) */
static const struct { CONST_STRPTR name; UBYTE file; } classfile_classes[] = {
#define ZCF_CLASS(name, file) { name, file },
    ZUNE_CLASSFILE_CLASSES(ZCF_CLASS)
};
static CONST_STRPTR const classfile_files[] = {
#define ZCF_FILE(name) name,
    ZUNE_CLASSFILE_FILES(ZCF_FILE)
};

static const struct __MUIBuiltinClass *classfile_query(struct Library *lib,
    CONST_STRPTR classid)
{
    register const struct __MUIBuiltinClass *res __asm("d0");
    register const struct ZuneClassFileIface *a0 __asm("a0") =
        &zune_classfile_iface;
    register CONST_STRPTR a1 __asm("a1") = classid;
    register struct Library *a6 __asm("a6") = lib;

    __asm volatile ("jsr %%a6@(-36:W)"
        : "=r" (res), "+r" (a0), "+r" (a1)
        : "r" (a6)
        : "d1", "cc", "memory");
    return res;
}

/* Called under ZuneSemaphore. The file stays open until the library's
 * expunge has freed its classes (ZUNE_CloseClassFiles). */
const struct __MUIBuiltinClass *ZUNE_LoadClassFile(CONST_STRPTR classid,
    struct Library *MUIMasterBase)
{
    struct MUIMasterBase_intern *base = MUIMB(MUIMasterBase);
    const struct __MUIBuiltinClass *desc;
    struct Library *lib;
    unsigned i;
    UBYTE file;

    for (i = 0; i < sizeof(classfile_classes) / sizeof(classfile_classes[0]); i++)
        if (!strcmp(classfile_classes[i].name, classid))
            break;
    if (i == sizeof(classfile_classes) / sizeof(classfile_classes[0]))
        return NULL;
    file = classfile_classes[i].file;
    if (!(lib = base->ClassFiles[file]))
    {
        TEXT path[48];

        snprintf(path, sizeof(path), "Zune/%s", classfile_files[file]);
        if (!(lib = OpenLibrary(path, ZUNE68_LIBRARY_VERSION)))
            return NULL;
        /* Only Zune68's own class files have the query vector */
        if (!lib->lib_IdString || lib->lib_NegSize < 36
            || !strstr((const char *)lib->lib_IdString, ZUNE_CLASSFILE_IDTAG))
        {
            CloseLibrary(lib);
            return NULL;
        }
        base->ClassFiles[file] = lib;
    }
    desc = classfile_query(lib, classid);
    if (desc && strcmp(desc->name, classid))
        desc = NULL;
    return desc;
}

/* After the classes are freed: close the class files. */
void ZUNE_CloseClassFiles(struct Library *MUIMasterBase)
{
    struct MUIMasterBase_intern *base = MUIMB(MUIMasterBase);
    unsigned i;

    for (i = 0; i < ZUNE_CLASSFILE_NFILES; i++)
    {
        if (base->ClassFiles[i])
            CloseLibrary(base->ClassFiles[i]);
        base->ClassFiles[i] = NULL;
    }
}
#endif

static Class *ZUNE_MakeBuiltinClass(CONST_STRPTR classid,
    struct Library *MUIMasterBase)
{
    int i;
    Class *cl = NULL;
    const struct __MUIBuiltinClass *desc = NULL;

    D(bug("Makeing Builtinclass %s\n", classid));

    for (i = 0; i < sizeof(builtins) / sizeof(builtins[0]); i++)
    {
        if (!strcmp(builtins[i]->name, classid))
        {
            desc = builtins[i];
            break;
        }
    }
#if ZUNE_CLASSFILE_NFILES
    if (!desc)
        desc = ZUNE_LoadClassFile(classid, MUIMasterBase);
#endif
    if (desc)
    {
        Class *supercl;
        ClassID superclid;

        /* Cached classes do not own library opens. Expunge checks
           references, objects and subclasses before reclaiming them. */
        if (strcmp(desc->supername, ROOTCLASS) == 0)
        {
            superclid = ROOTCLASS;
            supercl = NULL;
        }
        else
        {
            superclid = NULL;
            supercl = MUI_GetClass(desc->supername);

            if (!supercl)
                return NULL;
        }

        cl = MakeClass(desc->name, superclid, supercl, desc->datasize, 0);
        /* MakeClass owns the subclass link on success. The temporary
           lookup reference is no longer needed, including on failure. */
        if (supercl)
            MUI_FreeClass(supercl);
        if (cl)
        {
#if defined(__MAXON__) || defined(__amigaos4__)
            cl->cl_Dispatcher.h_Entry = desc->dispatcher;
#else
#ifdef __AROS__
            cl->cl_Dispatcher.h_Entry = (HOOKFUNC) metaDispatcher;
#else
            cl->cl_Dispatcher.h_Entry = (HOOKFUNC) builtinDispatcher;
#endif
            cl->cl_Dispatcher.h_SubEntry = desc->dispatcher;
#endif
            /* Use this as a reference counter */
            cl->cl_Dispatcher.h_Data = MUIMasterBase;
            cl->cl_UserData = 0;
        }
    }

    return cl;
}

Class *ZUNE_GetBuiltinClass(CONST_STRPTR id, struct Library *base)
{
    Class *cl;
    if (!id || !*id) return NULL;
    /* Cache hits need no semaphore: the class list only changes under
     * the semaphore inside Forbid (ZUNE_AddBuiltinClass, expunge), so a
     * Forbid section sees a consistent cache entry and reference count.
     * Two semaphore operations per object (here and in MUI_FreeClass)
     * would be a visible share of tree creation. */
    Forbid();
    cl = MUIMB(base)->BuiltinClassCache[((UBYTE)id[0] ^ (UBYTE)id[1]) & 31];
    if (cl && (cl->cl_ID == id || !strcmp(cl->cl_ID, id)))
    {
        ++cl->cl_UserData;
        Permit();
        return cl;
    }
    Permit();
    ObtainSemaphore(&MUIMB(base)->ZuneSemaphore);
    cl = ZUNE_FindBuiltinClass(id, base);
    if (!cl) {
        cl = ZUNE_MakeBuiltinClass(id, base);
        if (cl) ZUNE_AddBuiltinClass(cl, base);
    }
    if (cl)
    {
        MUIMB(base)->BuiltinClassCache[((UBYTE)id[0] ^ (UBYTE)id[1]) & 31] = cl;
        ++cl->cl_UserData;
    }
    ReleaseSemaphore(&MUIMB(base)->ZuneSemaphore);
    return cl;
}

/* Reclaim cached classes only at expunge, never from disposal callbacks.
 * Classes enter the list after their superclasses, so walk backwards.
 * Keep the library resident if any reference, object or subclass survives.
 */
BOOL ZUNE_FreeBuiltinClasses(struct Library *mb)
{
    struct MUIMasterBase_intern *base = (struct MUIMasterBase_intern *)mb;
    struct MinNode *node, *previous;
    BOOL empty;

    /* Expunge runs under Exec's Forbid. Do not wait and allow another
       task to reopen the library after LibExpunge checked its open count. */
    if (!AttemptSemaphore(&base->ZuneSemaphore))
        return FALSE;
    /* Some classes can be freed even if expunge later finds a live class.
       Drop all borrowed cache entries before walking the ownership list. */
    memset(base->BuiltinClassCache, 0, sizeof(base->BuiltinClassCache));
    node = base->BuiltinClasses.mlh_TailPred;
    while (node->mln_Pred)
    {
        Class *cl = (Class *)node;

        previous = node->mln_Pred;
        if (!cl->cl_UserData && !cl->cl_ObjectCount && !cl->cl_SubclassCount)
        {
            ZUNE_RemoveBuiltinClass(cl, mb);
            if (!FreeClass(cl))
            {
                /* Keep a refused class reachable in its original position. */
                Insert((struct List *)&base->BuiltinClasses,
                    (struct Node *)cl, (struct Node *)previous);
                cl->cl_Flags |= CLF_INLIST;
            }
        }
        node = previous;
    }
    empty = base->BuiltinClasses.mlh_Head->mln_Succ == NULL;
    ReleaseSemaphore(&base->ZuneSemaphore);
    return empty;
}

/*
 * metaDispatcher - puts h_Data in A6 and calls real dispatcher
 */

#ifdef __AROS__
AROS_UFH3(IPTR, metaDispatcher,
    AROS_UFHA(struct IClass *, cl, A0),
    AROS_UFHA(Object *, obj, A2),
    AROS_UFHA(Msg, msg, A1))
{
    AROS_USERFUNC_INIT

    return AROS_UFC4(IPTR, cl->cl_Dispatcher.h_SubEntry,
        AROS_UFPA(Class *, cl, A0),
        AROS_UFPA(Object *, obj, A2),
        AROS_UFPA(Msg, msg, A1),
        AROS_UFPA(APTR, cl->cl_Dispatcher.h_Data, A6));

    AROS_USERFUNC_EXIT
}

#else
IPTR metaDispatcher(Class *cl __asm("a0"), Object *obj __asm("a2"),
    Msg msg __asm("a1"))
{
    typedef IPTR (*Dispatcher)(Class * __asm("a0"),
        Object * __asm("a2"), Msg __asm("a1"), APTR __asm("a6"));
    Dispatcher dispatch = (Dispatcher)cl->cl_Dispatcher.h_SubEntry;
    return dispatch(cl, obj, msg, cl->cl_Dispatcher.h_Data);
}
#endif
