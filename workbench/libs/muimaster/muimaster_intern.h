/*
    Copyright (C) 2002-2025, The AROS Development Team. All rights reserved.
*/

#ifndef MUIMASTER_INTERN_H
#define MUIMASTER_INTERN_H

#ifndef EXEC_TYPES_H
#   include <exec/types.h>
#endif
#ifndef EXEC_LIBRARIES_H
#   include <exec/libraries.h>
#endif
#ifndef EXEC_MEMORY_H
#   include <exec/memory.h>
#endif
#ifndef INTUITION_CLASSES_H
#   include <intuition/classes.h>
#endif
#ifndef INTUITION_INTUITIONBASE_H
#   include <intuition/intuitionbase.h>
#endif
#ifndef GRAPHICS_GFXBASE_H
#   include <graphics/gfxbase.h>
#endif
#ifndef DOS_DOS_H
#   include <dos/dos.h>
#endif
#ifndef UTILITY_UTILITY_H
#   include <utility/utility.h>
#endif
#ifndef EXEC_SEMAPHORES_H
#   include <exec/semaphores.h>
#endif

#ifdef __AROS__
#   ifndef AROS_ASMCALL_H
#       include <aros/asmcall.h>
#   endif
#else
#   include "support_amigaos.h"
#endif

#include "mui.h"
#include "textengine.h"
#include "prefs.h"
#include "penspec.h"


#include "classfiles.h"   /* generated: ZUNE_CLASSFILE_NFILES */

/* The MUI a program sees and Zune behaves as (identity.c) */
#define ZUNE_ID_MUI38 1
#define ZUNE_ID_MUI4  2
#define ZUNE_ID_MUI5  3
/* Programs (tasks) whose identity is remembered while they have the
 * library open (12 bytes each); more are served by the rules without a
 * record */
#define ZUNE_ID_OPENERS 32

struct MUIMasterBase_intern
{
    struct Library              library;
#ifndef __AROS__
    /* On AROS these fields are handled by the system */
    struct ExecBase             *sysbase;
    BPTR                        seglist;

    /* On AROS autoopened libraries are used */
    struct DosLibrary           *dosbase;
    struct UtilityBase          *utilitybase;
    struct Library              *aslbase;
    struct GfxBase              *gfxbase;
    struct Library              *layersbase;
    struct IntuitionBase        *intuibase;
    struct Library              *cxbase;
    struct RxsLib               *rxsbase;
    struct Library              *keymapbase;
    struct Library              *gadtoolsbase;
    struct Library              *iffparsebase;
    struct Library              *diskfontbase;
    struct Library              *iconbase;
    struct Library              *cybergfxbase;
    struct Library              *workbenchbase;
#ifdef HAVE_COOLIMAGES
    struct Library              *coolimagesbase;
#endif
    
/*  struct Library              *datatypesbase; */
#endif /* __AROS__ */

    struct TextFont             *topaz8font;
    struct SignalSemaphore      ZuneSemaphore; /* Used when accessing global data */
    APTR                        SpecialMemory;

    /* Borrowed built-in classes, cleared before any expunge attempt. */
    Class                       *BuiltinClassCache[32];
    struct SignalSemaphore      LazyLibSemaphore; /* zune_lazylib_open() */
    ULONG                       LazyLibFailed;    /* 1 << ZUNE_LIB_* */
    BPTR                        ClassFiles[ZUNE_CLASSFILE_NFILES + 1]; /* classfile.h: private LoadSeg copies */
    /* External classes (.mcc/.mcp/.mui) this library opened, one open
     * each, kept while loaded as MUI does (support_classes.c) */
    struct MinList              ExternalClasses;
    BOOL                        ReleasingExternal; /* LibClose: no expunge now */
    /* What MUI's private LVO -264 returns (classes/application.c) */
    Object                      *Applist;
    struct MUI_CustomClass      *ApplistClass;
    /* Which MUI each program gets (identity.c): the copies of this base
     * reporting MUI 3.8 and MUI 4 (this base itself reports MUI 5), the
     * "Behave as" settings, the programs that have this library open and
     * the identity of the program whose external class is being loaded */
    struct Library              *IdCopy[2];
    APTR                        IdPrograms;   /* parsed per-program list */
    UBYTE                       IdGlobal;     /* ZUNE_ID_* or 0: automatic */
    UBYTE                       IdLoader;     /* ZUNE_ID_* or 0 */
    struct ZuneOpener {
        struct Task *task;
        APTR splower;   /* another task at the same address: not this */
        ULONG name;     /* the program's name hashed: nor a later one */
        UWORD count;
        UBYTE identity;
        UBYTE nested;   /* inside this library's own open/close/remove
                         * of a class module: its opens of this library
                         * on the program's task are not the program's */
    }                           IdOpeners[ZUNE_ID_OPENERS];
    /* The last three stay last: the checks in tests/ find them from the
     * end of the base (class list at -28, default pens at -4) */
    struct MinList              BuiltinClasses;
    struct MinList              Applications;
    struct MUI_PenSpec          *defaultPens;
};


#undef MUIMB
void zune_vector_free_templates(void); /* imspec_vector.c, at expunge */
void zune_title_gadgets_free(void);    /* classes/window.c, at expunge */
#define MUIMB(b)	((struct MUIMasterBase_intern *)b)

/* The stages in which MUI 3.8 sets a window larger than the screen up
 * again (measured with a row and a column of buttons on a 640x256
 * screen): each keeps what the one before it changed */
#define FALLBACK_HELVETICA11 1  /* helvetica 11 fonts */
#define FALLBACK_HELVETICA9  2  /* helvetica 9 fonts */
#define FALLBACK_SPACING     3  /* group and window spacing 2 beside, 1 above */
#define FALLBACK_NOINNER     4  /* no inner frame spacing */
#define FALLBACK_TOPAZ       5  /* topaz 8 fonts, the last stage */
/* The highest of these stages any window of an application needed:
 * MUI 3.8 opens the application's later windows in it too. Kept in the
 * global info's first field, which MUI names private and Zune does not
 * otherwise use: its lowest byte. Its highest byte is the MUI Zune
 * behaves as for the application (ZUNE_ID_*, identity.c) */
#define MGI_FALLBACK(mgi) (((UBYTE *)&(mgi)->priv0)[3])
#define MGI_IDENTITY(mgi) (((UBYTE *)&(mgi)->priv0)[0])

#ifndef __AROS__
extern struct Library *MUIMasterBase;
/* Which MUI Zune behaves as for a program (identity.c): the one its
 * application object was made with, or, with no object at hand, the one
 * of the calling task. MUI 3.9+ behaviour only when it is not MUI 3.8. */
UBYTE zune_task_identity(void);
#define ZUNE_MUI38() (zune_task_identity() == ZUNE_ID_MUI38)
#define ZUNE_APP_MUI38(obj) \
    (MGI_IDENTITY(muiGlobalInfo(obj)) == ZUNE_ID_MUI38)
/* MUI 4 or 5 behaviour where it differs from MUI 3.8's (an identity not
 * set, 0, behaves as MUI 3.8) */
#define ZUNE_APP_MUI45(obj) \
    (MGI_IDENTITY(muiGlobalInfo(obj)) >= ZUNE_ID_MUI4)
struct Library *zune_identity_open(ULONG version);
void zune_identity_close(struct Library *base);
void zune_identity_load(void);     /* read ENV:OpenMUI/BehaveAs (a process) */
/* The record nesting around this library's own open, close and removal
 * of class modules (identity.c); zune_module_close and _remove do one
 * close or removal inside it, in their own frames, with register
 * arguments: the callers' frames stay as they were */
void zune_identity_nest(register LONG delta __asm("d0"));
void zune_identity_release(void);  /* LibClose's release of the classes */
/* mui_newobject.c: the child objects of a tag list nothing has read yet */
void zune_dispose_children(struct TagItem *tags __asm("a0"));
/* support_classes.c: MakeClass(id, superid, super, size, 0) with OS 3.1's
 * raised subclass count put right when MakeClass's own allocation failed */
struct IClass *zune_make_class(CONST_STRPTR id __asm("a0"),
    CONST_STRPTR superid __asm("a1"), struct IClass *super __asm("d1"),
    ULONG size __asm("d0"));
void zune_module_close(register struct Library *lib __asm("a1"));
void zune_module_remove(register struct Library *lib __asm("a1"));
void zune_identity_free(void);     /* at expunge */
BOOL zune_is_master(struct Library *lib); /* this base or one of its copies */
extern struct Library *MUIScreenBase;
extern struct Library *CyberGfxBase;
BOOL ZUNE_FreeBuiltinClasses(struct Library *base);
void ZUNE_ReleaseExternalClasses(struct Library *base);
BOOL ZUNE_FreeExternalClass(Class *cl, struct Library *base);
/* classes/application.c: MUI's private application list object */
Object *zune_applist_get(void);
void zune_applist_free(void);
/* classes/application.c: run fn(cl, obj, msg) on a spare stack of the
 * application of obj (a Notify object: its global info's) when little
 * stack is left (68k native builds) */
IPTR zune_deep_call(IPTR (*fn)(struct IClass *, Object *, Msg),
    struct IClass *cl, Object *obj, Msg msg);
/* classes/popstring.c: a Popstring subclass's OM_NEW sets Popstring's hooks
 * through the superclass, in Popstring's frame */
void Popstring_SetHooks(struct IClass *cl, Object *obj, struct Hook *open,
    struct Hook *close, IPTR toggle);
#endif

#endif /* MUIMASTER_INTERN_H */
