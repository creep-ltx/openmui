/* Copyright (C) 2002-2020, The AROS Development Team.
 * Native library loader, based on Dirk Stoecker's library example.
 */
#include <proto/exec.h>
#include <exec/resident.h>
#include <exec/execbase.h>
#include <dos/var.h>
#include <proto/dos.h>
#include "muimaster_intern.h"
#include "native_version.h"
#include "gates.h"
struct ExecBase *SysBase;
ULONG L_InitLib(struct Library *base);
void L_ExpungeLib(struct Library *base);
#ifdef MUIMASTER_DROPIN
#define LIBNAME "muimaster.library"
#else
#define LIBNAME "zunemaster.library"
#endif
#define IDSTRING "$VER: " LIBNAME " " ZUNE68_LIBRARY_VERSION_STRING \
    " (" ZUNE68_LIBRARY_DATE ") Zune68 " ZUNE68_RELEASE_STAGE "\r\n"

/* The fourth vector, reserved: exec never calls it and it returns 0.
 * Zune prefs calls it with D0 'ZnID' from its own process after saving
 * ENV:Zune/BehaveAs, to have the library read the settings again
 * (identity.c: Open runs under Forbid and cannot read them itself). */
#define ZUNE_RELOAD_MAGIC 0x5A6E4944 /* 'ZnID' */
ULONG LibReserved(ULONG magic __asm("d0"))
{
    if (magic == ZUNE_RELOAD_MAGIC)
        zune_identity_load();
    return 0;
}
void MUI_Priv1(void) { }
void MUI_Priv2(void) { }
void MUI_Priv3(void) { }
void MUI_Priv4(void) { }
void MUI_Priv5(void) { }
void MUI_Priv6(void) { }
void MUIP_GetVirtualRect(Object *obj __asm("a0"), LONG *rect __asm("a1"));
/* MUI 3.8's jump table goes down to LVO -306 (lib_NegSize 308), MUI 5's
 * further; the slots past MUIP_GetVirtualRect are private there. Programs
 * from MUI's own authors call some of them: AmIRC 3.6 calls -264 at start
 * and sends a method to what it returns (classes/application.c,
 * zune_applist_get). A table that ended at -240 sent that call into
 * whatever memory lay below the library base: a guru (8000000B,
 * 80000008, 80000004, by what happened to be there), which on OS 3.1
 * reboots the machine. Every slot MUI 3.8 has now exists; those whose
 * purpose is unknown return 0. */
ULONG MUI_PrivNone(void) { return 0; }
Object *MUI_PrivApplist(void) { return zune_applist_get(); }
/* Exec calls Open of the base in its library list (version in D0), and
 * returns what Open returns: the base for the program's identity
 * (identity.c). Close and Expunge come through that base, so through a
 * copy too: both work on the shared base. */
struct Library *LibOpen(struct MUIMasterBase_intern *base __asm("a6"),
    ULONG version __asm("d0"))
{
    base->library.lib_Flags &= ~LIBF_DELEXP;
    ++base->library.lib_OpenCnt;
    return zune_identity_open(version);
}
BPTR LibExpunge(struct MUIMasterBase_intern *base __asm("a6"))
{
    BPTR segment;

    if (&base->library != MUIMasterBase)
        return 0;   /* through a copy: the shared base decides */
    /* Unused external classes go first, as MUI's do on a flush: they
     * hold this library open and keep its classes in use */
    ZUNE_ReleaseExternalClasses(&base->library);
    if (base->library.lib_OpenCnt || base->ReleasingExternal
        || (base->ExternalClasses.mlh_Head
            && !IsListEmpty((struct List *)&base->ExternalClasses))) {
        base->library.lib_Flags |= LIBF_DELEXP;
        return 0;
    }
    /* MUI's private application list object holds a Semaphore subclass */
    zune_applist_free();
    if (!ZUNE_FreeBuiltinClasses(&base->library)) {
        base->library.lib_Flags |= LIBF_DELEXP;
        return 0;
    }
    segment = base->seglist;
    Remove((struct Node *)base);
    zune_identity_free();
    L_ExpungeLib(&base->library);
    FreeMem((UBYTE *)base - base->library.lib_NegSize,
        base->library.lib_NegSize + base->library.lib_PosSize);
    return segment;
}
BPTR LibClose(struct MUIMasterBase_intern *base __asm("a6"))
{
    zune_identity_close(&base->library);
    base = MUIMB(MUIMasterBase);
    --base->library.lib_OpenCnt;
    /* A program closing the library gives up the external classes nobody
     * uses any more: they hold this library open (each class library
     * opens it), so waiting for this library's expunge would wait for
     * ever. Closed, they leave memory at the next flush and close this
     * library, which then releases those that only they used. */
    ZUNE_ReleaseExternalClasses(&base->library);
    /* Not while external classes are being closed: one of them closing
     * this library ends inside that release, whose caller goes on */
    if (!base->library.lib_OpenCnt &&
        (base->library.lib_Flags & LIBF_DELEXP) && !base->ReleasingExternal)
        return LibExpunge(base);
    return 0;
}
struct Library *LibInit(BPTR segment __asm("a0"),
    struct MUIMasterBase_intern *base __asm("d0"),
    struct ExecBase *exec __asm("a6"))
{
    SysBase = exec;
    base->sysbase = exec;
    base->seglist = segment;
    base->library.lib_Node.ln_Type = NT_LIBRARY;
    base->library.lib_Node.ln_Name = LIBNAME;
    base->library.lib_Flags = LIBF_SUMUSED | LIBF_CHANGED;
    base->library.lib_Version = ZUNE68_LIBRARY_VERSION;
    base->library.lib_Revision = ZUNE68_LIBRARY_REVISION;
    base->library.lib_IdString = IDSTRING;
    if (L_InitLib(&base->library))
    {
        /* The "Behave as" settings (identity.c); this base reports
         * MUI 5, each opener gets the base of its identity */
        zune_identity_load();
        return &base->library;
    }
    FreeMem((UBYTE *)base - base->library.lib_NegSize,
        base->library.lib_NegSize + base->library.lib_PosSize);
    return NULL;
}
#define MUI_VECTOR(name) (APTR)Gate_##name
static const APTR LibVectors[] = {
  (APTR) LibOpen,
  (APTR) LibClose,
  (APTR) LibExpunge,
  (APTR) LibReserved,
  MUI_VECTOR(MUI_NewObjectA),
  MUI_VECTOR(MUI_DisposeObject),
  MUI_VECTOR(MUI_RequestA),
  MUI_VECTOR(MUI_AllocAslRequest),
  MUI_VECTOR(MUI_AslRequest),
  MUI_VECTOR(MUI_FreeAslRequest),
  MUI_VECTOR(MUI_Error),
  MUI_VECTOR(MUI_SetError),
  MUI_VECTOR(MUI_GetClass),
  MUI_VECTOR(MUI_FreeClass),
  MUI_VECTOR(MUI_RequestIDCMP),
  MUI_VECTOR(MUI_RejectIDCMP),
  MUI_VECTOR(MUI_Redraw),
  MUI_VECTOR(MUI_CreateCustomClass),
  MUI_VECTOR(MUI_DeleteCustomClass),
  MUI_VECTOR(MUI_MakeObjectA),
  MUI_VECTOR(MUI_Layout),
  (APTR) MUI_Priv1,
  (APTR) MUI_Priv2,
  (APTR) MUI_Priv3,
  (APTR) MUI_Priv4,
  MUI_VECTOR(MUI_ObtainPen),
  MUI_VECTOR(MUI_ReleasePen),
  MUI_VECTOR(MUI_AddClipping),
  MUI_VECTOR(MUI_RemoveClipping),
  MUI_VECTOR(MUI_AddClipRegion),
  MUI_VECTOR(MUI_RemoveClipRegion),
  MUI_VECTOR(MUI_BeginRefresh),
  MUI_VECTOR(MUI_EndRefresh),
  (APTR) MUI_Priv5,
  (APTR) MUI_Priv6,
  MUI_VECTOR(MUI_Show),
  MUI_VECTOR(MUI_Hide),
  MUI_VECTOR(MUI_LayoutObj),
  MUI_VECTOR(MUI_Offset),
  (APTR) MUIP_GetVirtualRect,
  (APTR) MUI_PrivNone,          /* -246 */
  (APTR) MUI_PrivNone,          /* -252 */
  (APTR) MUI_PrivNone,          /* -258 */
  (APTR) MUI_PrivApplist,       /* -264 */
  (APTR) MUI_PrivNone,          /* -270 */
  (APTR) MUI_PrivNone,          /* -276 */
  (APTR) MUI_PrivNone,          /* -282 */
  (APTR) MUI_PrivNone,          /* -288 */
  (APTR) MUI_PrivNone,          /* -294 */
  (APTR) MUI_PrivNone,          /* -300 */
  (APTR) MUI_PrivNone,          /* -306 */
  (APTR) -1
};


const ULONG LibInitTable[] = {
    sizeof(struct MUIMasterBase_intern), (ULONG)LibVectors, 0, (ULONG)LibInit
};
const struct Resident RomTag = {
    RTC_MATCHWORD, (struct Resident *)&RomTag,
    (struct Resident *)(&RomTag + 1), RTF_AUTOINIT,
    ZUNE68_LIBRARY_VERSION, NT_LIBRARY, 0, LIBNAME, IDSTRING,
    (APTR)LibInitTable
};
