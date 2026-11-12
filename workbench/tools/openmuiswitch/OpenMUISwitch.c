/*
 * OpenMUISwitch - switch between OpenMUI and a real MUI, and uninstall
 * OpenMUI. Part of the lean edition's installer (dist/Install).
 *
 * The installer records every file it puts on the system in
 * <home>/Data/Manifest (home: SYS:OpenMUI unless HOME= says otherwise, or
 * the drawer this program was started from):
 *
 *   ZUNE68K-MANIFEST 1   (the name of the first editions' records, kept so
 *                        that an earlier installation is still removed)
 *   LIBS <dir>          the LIBS: directory Zune's libraries went to
 *   NEW <path>          a file Zune put where there was none
 *   REPLACED <path>     a file Zune replaced; the old one follows as
 *   BACKUP <name>       <home>/Data/Backup/<name>
 *   DIR <path>          a drawer the installer made
 *
 * MUI:     every OpenMUI file in the LIBS directory (muimaster.library,
 *          LIBS:OpenMUI/..., the classes in LIBS:MUI; zunemaster.library and
 *          LIBS:Zune/... in an earlier edition's record) is moved to
 *          <home>/Data/Parked and the user's own files it had replaced are
 *          put back: after a reboot programs get the real MUI.
 * OPENMUI (or ZUNE): the other way round.
 * UNINSTALL: OpenMUI's files are deleted, the user's own put back, the
 *          drawers the installer made removed when empty; with SETTINGS
 *          also ENV:OpenMUI and ENVARC:OpenMUI (and an earlier edition's
 *          ENV:Zune and ENVARC:Zune).
 * MIGRATE [FILE]: for the installer, updating an earlier edition. The
 *          settings in ENVARC:Zune and ENV:Zune are moved to ENVARC:OpenMUI
 *          and ENV:OpenMUI (a file of the same name already there is not
 *          replaced: the old one stays where it is); FILE, the earlier
 *          edition's class drawer (LIBS:Zune) once its record has taken its
 *          own files away: what is left there is the user's, moved to
 *          <home>/Data/Backup and written into the record as REPLACED with
 *          no file of OpenMUI's in its place, so that Uninstall and "Use
 *          real MUI" put it back.
 * STATUS:  what is installed where (Shell).
 *
 * zunemaster.library, the second name earlier editions installed the
 * library under, is not used any more: it is handled only as the record
 * says (deleted, put back from the backup, parked), like any other file,
 * and STATUS names one that is left in a LIBS: directory.
 * ISZUNE FILE: return code 0 when FILE is a Zune68 library, 5 when it is
 *          not, 10 when it cannot be read (for the installer).
 * LIBSDIR N: sets ENV:OpenMUI-LibsDir to the Nth directory of the LIBS:
 *          assign; return code 5 when there is none (for the installer).
 *
 * From Workbench the icon's ACTION tool type chooses (MUI, ZUNE or
 * UNINSTALL); questions and results come as requesters.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/var.h>
#include <dos/dostags.h>
#include <intuition/intuition.h>
#include <workbench/startup.h>
#include <workbench/workbench.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/icon.h>
#include <proto/locale.h>
#include <string.h>

#ifndef IPTR
#define IPTR ULONG
#endif

#define openmuiswitch_ARRAY
#include "openmuiswitch_strings.h"
#include "catalogs/catalog_version.h"

/* ASCII case-insensitive compares (utility.library's are not needed) */
static int lower(int c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

static int Strnicmp(const char *a, const char *b, int n)
{
    for (; n > 0; n--, a++, b++)
    {
        int d = lower((UBYTE)*a) - lower((UBYTE)*b);

        if (d || !*a)
            return d;
    }
    return 0;
}

#define Stricmp(a, b) Strnicmp((const char *)(a), (const char *)(b), 0x7fff)

static const char version[] __attribute__((used)) ="$VER: OpenMUISwitch 1.2 (12.11.2026) OpenMUI 0.1b1";

struct IntuitionBase *IntuitionBase;
struct Library *IconBase;
struct LocaleBase *LocaleBase;
static struct Catalog *catalog;
static BOOL catalog_tried;

/* A message: from the catalog when locale.library and one for the user's
 * language are there, else the English built in */
static const char *msg(ULONG id)
{
    if (!catalog_tried)
    {
        catalog_tried = TRUE;
        if ((LocaleBase = (struct LocaleBase *)OpenLibrary((STRPTR)"locale.library", 38)))
            catalog = OpenCatalog(NULL, (STRPTR)"System/Tools/OpenMUISwitch.catalog",
                OC_Version, CATALOG_VERSION, TAG_DONE);
    }
    if (catalog)
        return (const char *)GetCatalogStr(catalog, id,
            openmuiswitch_Array[id].cca_Str);
    return (const char *)openmuiswitch_Array[id].cca_Str;
}

static void close_catalog(void)
{
    if (catalog)
        CloseCatalog(catalog);
    if (LocaleBase)
        CloseLibrary((struct Library *)LocaleBase);
    catalog = NULL;
    LocaleBase = NULL;
}
extern struct WBStartup *_WBenchMsg;

#define MAXENT 600
#define PATHLEN 256

enum { E_NEW, E_REPLACED, E_DIR };

struct Ent
{
    UBYTE kind;
    char *path;
    char *backup;
};

static struct Ent ents[MAXENT];
static int nents;
static char *libsdir;
static char *manifest;
static char home[PATHLEN];
static BOOL quiet, fromwb, home_later;

/* Messages: printed in a Shell, collected for one requester from Workbench */
static char report[2048];
static int reportlen;

static void say_n(const char *s, int n)
{
    if (!fromwb)
    {
        if (!quiet)
            while (n-- > 0)
                FPutC(Output(), (UBYTE)*s++);
        return;
    }
    while (n-- > 0 && reportlen < (int)sizeof(report) - 1)
        report[reportlen++] = *s++;
    report[reportlen] = 0;
}

static void say(ULONG id)
{
    const char *s = msg(id);

    say_n(s, strlen(s));
}

/* A message with one %s */
static void say1(ULONG id, const char *arg)
{
    const char *s = msg(id), *at = strstr(s, "%s");

    if (!at)
    {
        say_n(s, strlen(s));
        return;
    }
    say_n(s, at - s);
    say_n(arg, strlen(arg));
    say_n(at + 2, strlen(at + 2));
}

static LONG ask(const char *text, const char *gadgets)
{
    struct EasyStruct es;

    if (!fromwb || !IntuitionBase)
        return 1;
    es.es_StructSize = sizeof(es);
    es.es_Flags = 0;
    es.es_Title = (UBYTE *)"OpenMUI";
    es.es_TextFormat = (UBYTE *)"%s";
    es.es_GadgetFormat = (UBYTE *)gadgets;
    return EasyRequest(NULL, &es, NULL, (IPTR)text);
}

static void join(char *buf, const char *a, const char *b)
{
    strncpy(buf, a, PATHLEN - 1);
    buf[PATHLEN - 1] = 0;
    AddPart((STRPTR)buf, (STRPTR)b, PATHLEN);
}

static BOOL exists(const char *path, LONG *type)
{
    BPTR lock = Lock((STRPTR)path, ACCESS_READ);
    struct FileInfoBlock *fib;

    if (!lock)
        return FALSE;
    if (type && (fib = AllocDosObject(DOS_FIB, NULL)))
    {
        *type = Examine(lock, fib) ? fib->fib_DirEntryType : 0;
        FreeDosObject(DOS_FIB, fib);
    }
    UnLock(lock);
    return TRUE;
}

/* Copy then delete, for a move between volumes; keeps the protection bits */
static BOOL copyfile(const char *from, const char *to)
{
    BPTR in, out;
    LONG n, prot = 0;
    BOOL ok = FALSE;
    APTR buf;
    struct FileInfoBlock *fib;

    if (!(buf = AllocVec(8192, MEMF_ANY)))
        return FALSE;
    if ((in = Open((STRPTR)from, MODE_OLDFILE)))
    {
        if ((fib = AllocDosObject(DOS_FIB, NULL)))
        {
            if (ExamineFH(in, fib))
                prot = fib->fib_Protection;
            FreeDosObject(DOS_FIB, fib);
        }
        if ((out = Open((STRPTR)to, MODE_NEWFILE)))
        {
            ok = TRUE;
            while ((n = Read(in, buf, 8192)) > 0)
                if (Write(out, buf, n) != n)
                {
                    ok = FALSE;
                    break;
                }
            if (n < 0)
                ok = FALSE;
            Close(out);
            if (!ok)
                DeleteFile((STRPTR)to);
            else
                SetProtection((STRPTR)to, prot);
        }
        Close(in);
    }
    FreeVec(buf);
    return ok;
}

/* Make the drawer a file goes into when it is missing (a file put back
 * into an earlier edition's LIBS:Zune, which is gone). TRUE if made. */
static BOOL make_parent(const char *path)
{
    char dir[PATHLEN];
    char *end;
    BPTR lock;

    strncpy(dir, path, PATHLEN - 1);
    dir[PATHLEN - 1] = 0;
    end = (char *)PathPart((STRPTR)dir);
    if (end == dir)
        return FALSE;
    *end = 0;
    if ((lock = Lock((STRPTR)dir, ACCESS_READ)))
    {
        UnLock(lock);
        return FALSE;
    }
    if (!(lock = CreateDir((STRPTR)dir)))
        return FALSE;
    UnLock(lock);
    return TRUE;
}

static BOOL move(const char *from, const char *to)
{
    LONG err;

    if (Rename((STRPTR)from, (STRPTR)to))
        return TRUE;
    err = IoErr();
    if (err != ERROR_RENAME_ACROSS_DEVICES && make_parent(to))
    {
        if (Rename((STRPTR)from, (STRPTR)to))
            return TRUE;
        err = IoErr();
    }
    if (err == ERROR_RENAME_ACROSS_DEVICES
        && (make_parent(to), copyfile(from, to)))
        return DeleteFile((STRPTR)from) || TRUE;
    return FALSE;
}

/* Does the file contain "OpenMUI" (this edition) or "Zune68" (earlier
 * editions, Zune68k lean)? 1 yes, 0 no, -1 unreadable */
static int file_is_zune(const char *path)
{
    static const char key[] = "Zune68";
    BPTR fh = Open((STRPTR)path, MODE_OLDFILE);
    UBYTE *buf;
    LONG n, keep = 0, i;
    int found = 0;

    if (!fh)
        return -1;
    if (!(buf = AllocVec(4096 + 8, MEMF_ANY)))
    {
        Close(fh);
        return -1;
    }
    while (!found && (n = Read(fh, buf + keep, 4096)) > 0)
    {
        n += keep;
        for (i = 0; i + 6 <= n; i++)
            if ((buf[i] == 'Z' && !memcmp(buf + i, key, 6))
                || (buf[i] == 'O' && i + 7 <= n && !memcmp(buf + i, "OpenMUI", 7)))
            {
                found = 1;
                break;
            }
        keep = n < 6 ? n : 6;
        memmove(buf, buf + n - keep, keep);
    }
    FreeVec(buf);
    Close(fh);
    return found;
}

/* The $VER text of a library file ("muimaster.library 19.35 (12.02.97)") */
static void file_version(const char *path, char *out, int size)
{
    BPTR fh = Open((STRPTR)path, MODE_OLDFILE);
    UBYTE *buf;
    LONG n, i, j;

    out[0] = 0;
    if (!fh)
        return;
    if ((buf = AllocVec(4096 + 256, MEMF_ANY)))
    {
        LONG keep = 0;

        while (!out[0] && (n = Read(fh, buf + keep, 4096)) > 0)
        {
            n += keep;
            for (i = 0; i + 6 <= n; i++)
                if (buf[i] == '$' && !memcmp(buf + i, "$VER: ", 6))
                {
                    if (i + 200 > n && n == keep + 4096)
                        break;  /* read on, keep the string whole */
                    for (j = 0, i += 6; i < n && j < size - 1
                        && buf[i] >= ' '; )
                        out[j++] = buf[i++];
                    out[j] = 0;
                    break;
                }
            keep = n < 250 ? n : 250;
            memmove(buf, buf + n - keep, keep);
        }
        FreeVec(buf);
    }
    Close(fh);
}

/* The directories of the LIBS: assign, first one first */
static int libs_dirs(char names[][PATHLEN], int max)
{
    struct DosList *dl;
    BPTR locks[8];
    int n = 0, i;

    dl = LockDosList(LDF_ASSIGNS | LDF_READ);
    if ((dl = FindDosEntry(dl, (STRPTR)"LIBS", LDF_ASSIGNS)))
    {
        struct AssignList *al;

        if (dl->dol_Type == DLT_DIRECTORY && dl->dol_Lock)
            locks[n++] = DupLock(dl->dol_Lock);
        for (al = dl->dol_misc.dol_assign.dol_List; al && n < 8 && n < max;
            al = al->al_Next)
            if (al->al_Lock)
                locks[n++] = DupLock(al->al_Lock);
    }
    UnLockDosList(LDF_ASSIGNS | LDF_READ);
    for (i = 0; i < n; i++)
    {
        if (!locks[i] || !NameFromLock(locks[i], (STRPTR)names[i], PATHLEN))
            names[i][0] = 0;
        UnLock(locks[i]);
    }
    return n;
}

/* ---- the record ---- */

static char *dupstr(const char *s, int len)
{
    char *d = AllocVec(len + 1, MEMF_ANY);

    if (d)
    {
        CopyMem((APTR)s, d, len);
        d[len] = 0;
    }
    return d;
}

static BOOL read_manifest(void)
{
    char path[PATHLEN], *line, *end;
    BPTR fh;
    LONG size;
    struct FileInfoBlock *fib;

    join(path, home, "Data/Manifest");
    if (!(fh = Open((STRPTR)path, MODE_OLDFILE)))
        return FALSE;
    size = 0;
    if ((fib = AllocDosObject(DOS_FIB, NULL)))
    {
        if (ExamineFH(fh, fib))
            size = fib->fib_Size;
        FreeDosObject(DOS_FIB, fib);
    }
    if (size <= 0 || !(manifest = AllocVec(size + 1, MEMF_ANY))
        || Read(fh, manifest, size) != size)
    {
        Close(fh);
        return FALSE;
    }
    Close(fh);
    manifest[size] = 0;
    if (strncmp(manifest, "ZUNE68K-MANIFEST", 16))
        return FALSE;
    for (line = manifest; *line; line = end)
    {
        int len;

        for (end = line; *end && *end != '\n'; end++)
            ;
        len = end - line;
        while (len && (line[len - 1] == '\r' || line[len - 1] == ' '))
            len--;
        if (*end)
            end++;
        if (len > 5 && !strncmp(line, "LIBS ", 5))
            libsdir = dupstr(line + 5, len - 5);
        else if (len > 4 && !strncmp(line, "NEW ", 4) && nents < MAXENT)
        {
            ents[nents].kind = E_NEW;
            ents[nents++].path = dupstr(line + 4, len - 4);
        }
        else if (len > 9 && !strncmp(line, "REPLACED ", 9) && nents < MAXENT)
        {
            ents[nents].kind = E_REPLACED;
            ents[nents++].path = dupstr(line + 9, len - 9);
        }
        else if (len > 7 && !strncmp(line, "BACKUP ", 7) && nents
            && ents[nents - 1].kind == E_REPLACED)
            ents[nents - 1].backup = dupstr(line + 7, len - 7);
        else if (len > 4 && !strncmp(line, "DIR ", 4) && nents < MAXENT)
        {
            ents[nents].kind = E_DIR;
            ents[nents++].path = dupstr(line + 4, len - 4);
        }
    }
    return libsdir != NULL;
}

/* Is the entry's file in the LIBS directory (so that it is switched)? */
static BOOL in_libs(struct Ent *e)
{
    int len = strlen(libsdir);

    return e->kind != E_DIR && !Strnicmp(e->path, libsdir, len)
        && (e->path[len] == '/' || libsdir[len - 1] == ':');
}

static void parked_name(char *buf, int i)
{
    char name[16];
    int n = 0, v = i;
    char digits[8];

    do
        digits[n++] = '0' + v % 10;
    while ((v /= 10) && n < 7);
    name[0] = 'P';
    for (v = 1; n; )
        name[v++] = digits[--n];
    name[v] = 0;
    join(buf, home, "Data/Parked");
    AddPart((STRPTR)buf, (STRPTR)name, PATHLEN);
}

static void backup_name(char *buf, struct Ent *e)
{
    join(buf, home, "Data/Backup");
    AddPart((STRPTR)buf, (STRPTR)e->backup, PATHLEN);
}

static BOOL parked(void)
{
    char path[PATHLEN];

    join(path, home, "Data/State");
    return exists(path, NULL);
}

static void set_parked(BOOL on)
{
    char path[PATHLEN];
    BPTR fh;

    join(path, home, "Data/State");
    if (!on)
    {
        DeleteFile((STRPTR)path);
        return;
    }
    if ((fh = Open((STRPTR)path, MODE_NEWFILE)))
    {
        Write(fh, "MUI\n", 4);
        Close(fh);
    }
}

/* ---- the moves, undone in reverse when one fails ---- */

struct Op { char *from, *to; };
static struct Op ops[2 * MAXENT];
static int nops;

static BOOL op_move(const char *from, const char *to)
{
    if (!exists(from, NULL))
        return TRUE;    /* nothing there (removed by hand): nothing to move */
    if (exists(to, NULL))
    {
        say1(MSG_NOT_MOVED, to);
        return FALSE;
    }
    if (!move(from, to))
    {
        say1(MSG_COULD_NOT_MOVE, from);
        return FALSE;
    }
    if (nops < 2 * MAXENT)
    {
        ops[nops].from = dupstr(from, strlen(from));
        ops[nops].to = dupstr(to, strlen(to));
        nops++;
    }
    return TRUE;
}

static void undo_ops(void)
{
    while (nops--)
        if (ops[nops].from && ops[nops].to)
            move(ops[nops].to, ops[nops].from);
    nops = 0;
}

/* After parking, will programs find a real MUI? */
static BOOL real_mui_after_park(void)
{
    static char dirs[8][PATHLEN];
    char path[PATHLEN];
    int i, n;

    for (i = 0; i < nents; i++)
        if (ents[i].kind == E_REPLACED && ents[i].backup && in_libs(&ents[i])
            && !Stricmp(FilePart((STRPTR)ents[i].path),
                (STRPTR)"muimaster.library"))
        {
            backup_name(path, &ents[i]);
            if (file_is_zune(path) == 0)
                return TRUE;
        }
    n = libs_dirs(dirs, 8);
    for (i = 0; i < n; i++)
    {
        if (!dirs[i][0])
            continue;
        join(path, dirs[i], "muimaster.library");
        if (file_is_zune(path) == 0)
        {
            /* the LIBS directory's own copy is Zune's, parked below */
            BPTR a = Lock((STRPTR)dirs[i], ACCESS_READ),
                b = Lock((STRPTR)libsdir, ACCESS_READ);
            BOOL same = a && b && SameLock(a, b) == LOCK_SAME;

            UnLock(a);
            UnLock(b);
            if (!same)
                return TRUE;
        }
    }
    return FALSE;
}

static int do_park(void)
{
    char p[PATHLEN], b[PATHLEN];
    int i;

    if (parked())
    {
        say(MSG_REAL_ALREADY);
        return 0;
    }
    if (!real_mui_after_park())
    {
        say(MSG_NO_REAL_MUI);
        return 5;
    }
    if (ask(msg(MSG_ASK_USE_MUI), msg(MSG_GADGETS_USE_MUI)) != 1)
        return 5;
    join(p, home, "Data/Parked");
    UnLock(CreateDir((STRPTR)p));
    for (i = 0; i < nents; i++)
    {
        struct Ent *e = &ents[i];

        if (!in_libs(e))
            continue;
        parked_name(p, i);
        if (!op_move(e->path, p))
            goto fail;
        if (e->kind == E_REPLACED && e->backup)
        {
            backup_name(b, e);
            if (!op_move(b, e->path))
                goto fail;
        }
    }
    set_parked(TRUE);
    say(MSG_USING_MUI);
    return 0;
fail:
    undo_ops();
    say(MSG_NOTHING_CHANGED);
    return 10;
}

static int do_unpark(void)
{
    char p[PATHLEN], b[PATHLEN];
    int i;

    if (!parked())
    {
        say(MSG_OPENMUI_ALREADY);
        return 0;
    }
    for (i = nents - 1; i >= 0; i--)
    {
        struct Ent *e = &ents[i];

        if (!in_libs(e))
            continue;
        if (e->kind == E_REPLACED && e->backup)
        {
            backup_name(b, e);
            if (!op_move(e->path, b))
                goto fail;
        }
        parked_name(p, i);
        if (!op_move(p, e->path))
            goto fail;
    }
    set_parked(FALSE);
    say(MSG_USING_OPENMUI);
    return 0;
fail:
    undo_ops();
    say(MSG_NOTHING_CHANGED);
    return 10;
}

/* The names in a drawer (files and drawers), NUL after NUL; NULL if none */
static char *dir_names(const char *path, LONG *usedp)
{
    BPTR lock = Lock((STRPTR)path, ACCESS_READ);
    struct FileInfoBlock *fib;
    char *names = NULL;
    LONG used = 0, size = 0;

    *usedp = 0;
    if (!lock)
        return NULL;
    if ((fib = AllocDosObject(DOS_FIB, NULL)))
    {
        if (Examine(lock, fib) && fib->fib_DirEntryType > 0)
            while (ExNext(lock, fib))
            {
                LONG len = strlen((char *)fib->fib_FileName) + 1;

                if (used + len > size)
                {
                    char *grown = AllocVec(size + 1024, MEMF_ANY);

                    if (!grown)
                        break;
                    if (names)
                    {
                        CopyMem(names, grown, used);
                        FreeVec(names);
                    }
                    names = grown;
                    size += 1024;
                }
                strcpy(names + used, (char *)fib->fib_FileName);
                used += len;
            }
        FreeDosObject(DOS_FIB, fib);
    }
    UnLock(lock);
    *usedp = used;
    return names;
}

/* Delete a drawer and everything in it (the settings) */
static void delete_tree(const char *path)
{
    char sub[PATHLEN];
    LONG used, at;
    char *names = dir_names(path, &used);

    if (names)
    {
        for (at = 0; at < used; at += strlen(names + at) + 1)
        {
            LONG type = 0;

            join(sub, path, names + at);
            if (exists(sub, &type) && type > 0)
                delete_tree(sub);
            else
                DeleteFile((STRPTR)sub);
        }
        FreeVec(names);
    }
    DeleteFile((STRPTR)path);
}

/* The drawer we run from is locked by Workbench until we end: a script
 * started now removes it a moment later, if it is empty by then. */
static void delete_home_later(void)
{
    char script[] = "T:OpenMUI-remove";
    BPTR fh, in, out;

    if (!(fh = Open((STRPTR)script, MODE_NEWFILE)))
        return;
    /* A Workbench window of the drawer locks it too: try for two minutes */
    FPrintf(fh, (STRPTR)"FailAt 21\nSet openmui-n 0\nLab again\nWait 3\n"
        "Delete >NIL: \"%s/Data/Backup\" \"%s/Data/Parked\" QUIET\n"
        "Delete >NIL: \"%s/Data\" QUIET\nDelete >NIL: \"%s\" QUIET\n"
        "If NOT EXISTS \"%s\"\n  Skip done\nEndIf\n"
        "Set openmui-n `Eval $openmui-n + 1`\n"
        "If $openmui-n LT 40 VAL\n  Skip again BACK\nEndIf\n"
        "Lab done\nUnSet openmui-n\nDelete >NIL: T:OpenMUI-remove QUIET\n",
        (IPTR)home, (IPTR)home, (IPTR)home, (IPTR)home, (IPTR)home);
    Close(fh);
    in = Open((STRPTR)"NIL:", MODE_OLDFILE);
    out = Open((STRPTR)"NIL:", MODE_NEWFILE);
    /* The script gets a copy of our current drawer, which is the drawer
     * it is to delete: give it another */
    {
        BPTR ram = Lock((STRPTR)"T:", ACCESS_READ);
        BPTR old = ram ? CurrentDir(ram) : 0;

        if (!in || !out || SystemTags((STRPTR)"C:Execute T:OpenMUI-remove",
                SYS_Input, in, SYS_Output, out, SYS_Asynch, TRUE,
                NP_StackSize, 8192, TAG_DONE) == -1)
        {
            if (in)
                Close(in);
            if (out)
                Close(out);
        }
        if (ram)
        {
            CurrentDir(old);
            UnLock(ram);
        }
    }
}

static int do_uninstall(BOOL settings, BOOL askfirst)
{
    char p[PATHLEN], b[PATHLEN];
    int i, kept = 0, restored = 0, removed = 0;
    BOOL wasparked = parked();

    if (askfirst)
    {
        LONG r = ask(msg(MSG_ASK_REMOVE), msg(MSG_GADGETS_REMOVE));

        if (r == 0)
            return 5;
        if (r == 2)
            settings = TRUE;
    }
    for (i = nents - 1; i >= 0; i--)
    {
        struct Ent *e = &ents[i];

        if (e->kind == E_DIR)
            continue;
        if (wasparked && in_libs(e))
        {
            /* Zune's file is parked, the user's is already in place */
            parked_name(p, i);
            if (DeleteFile((STRPTR)p))
                removed++;
            continue;
        }
        if (DeleteFile((STRPTR)e->path))
            removed++;
        else if (exists(e->path, NULL))
        {
            say1(MSG_COULD_NOT_DELETE, e->path);
            kept++;
            continue;
        }
        if (e->kind == E_REPLACED && e->backup)
        {
            backup_name(b, e);
            if (move(b, e->path))
                restored++;
            else if (exists(b, NULL))
            {
                say1(MSG_COULD_NOT_PUT_BACK, e->path);
                kept++;
            }
        }
    }
    join(p, home, "Data/Manifest");
    if (!kept)
        DeleteFile((STRPTR)p);
    join(p, home, "Data/State");
    DeleteFile((STRPTR)p);
    join(p, home, "Data/Parked");
    DeleteFile((STRPTR)p);
    for (i = nents - 1; i >= 0; i--)
        if (ents[i].kind == E_DIR && !DeleteFile((STRPTR)ents[i].path)
            && IoErr() == ERROR_DIRECTORY_NOT_EMPTY
            && Strnicmp(ents[i].path, home, strlen(home)))
            say1(MSG_KEPT_DRAWER, ents[i].path);
    if (settings)
    {
        delete_tree("ENVARC:OpenMUI");
        delete_tree("ENV:OpenMUI");
        delete_tree("ENVARC:Zune");     /* an earlier edition's */
        delete_tree("ENV:Zune");
    }
    if (exists(home, NULL))
    {
        /* the drawer we were started from stays locked until we end (by
         * Workbench, or as the Shell's program drawer): removed by a
         * script afterwards. Not for the installer (QUIET), which makes
         * the drawers again at once. */
        if (!kept && !DeleteFile((STRPTR)home))
        {
            if (!quiet)
                home_later = TRUE;
            else
                say1(MSG_DRAWER_DELETABLE, home);
        }
    }
    if (kept)
        say(MSG_RECORD_KEPT);
    else
    {
        say(MSG_REMOVED);
        if (home_later)
            say(MSG_DRAWER_SOON);
    }
    if (!settings)
        say(MSG_SETTINGS_KEPT);
    (void)removed;
    (void)restored;
    return kept ? 10 : 0;
}

/* An earlier edition's settings drawer into OpenMUI's: each file or drawer
 * not there yet is moved; one whose name is taken stays where it is. The
 * old drawer goes when it is empty. */
static __attribute__((noinline)) void migrate_settings(const char *from,
    const char *to)
{
    char src[PATHLEN], dst[PATHLEN];
    LONG used, at;
    char *names = dir_names(from, &used);
    BPTR lock;

    if (!names)
    {
        DeleteFile((STRPTR)from);       /* an empty one */
        return;
    }
    if (!(lock = Lock((STRPTR)to, ACCESS_READ)))
        lock = CreateDir((STRPTR)to);
    if (lock)
    {
        int moved = 0;

        UnLock(lock);
        for (at = 0; at < used; at += strlen(names + at) + 1)
        {
            join(src, from, names + at);
            join(dst, to, names + at);
            if (exists(dst, NULL))
                say1(MSG_OLD_KEPT, src);
            else if (!move(src, dst))
                say1(MSG_COULD_NOT_MOVE, src);
            else
                moved++;
        }
        if (moved)
            say1(MSG_OLD_MOVED, from);
    }
    FreeVec(names);
    DeleteFile((STRPTR)from);
}

/* What is left in an earlier edition's class drawer (its own files are
 * gone with its record) is the user's: into the backup, written down as
 * replaced. Returns 0, or 10 when something could not be done. */
static __attribute__((noinline)) int retire_drawer(const char *path)
{
    char src[PATHLEN], bak[PATHLEN], name[40];
    LONG used, at;
    char *names = dir_names(path, &used);
    int n = 0, rc = 0;
    BPTR fh;

    if (!names)
    {
        DeleteFile((STRPTR)path);
        return 0;
    }
    join(src, home, "Data/Manifest");
    if (!(fh = Open((STRPTR)src, MODE_OLDFILE)))
    {
        say1(MSG_NO_RECORD, home);
        FreeVec(names);
        return 10;
    }
    Seek(fh, 0, OFFSET_END);
    for (at = 0; at < used; at += strlen(names + at) + 1)
    {
        const char *f = names + at;

        /* Z1-name, Z2-name, ...: the installer's are B1-..., B2-... */
        do
        {
            int v = ++n, d = 0;
            char digits[8];

            do
                digits[d++] = '0' + v % 10;
            while ((v /= 10) && d < 7);
            name[0] = 'Z';
            for (v = 1; d; )
                name[v++] = digits[--d];
            name[v++] = '-';
            strncpy(name + v, f, sizeof(name) - v - 1);
            name[sizeof(name) - 1] = 0;
            join(bak, home, "Data/Backup");
            AddPart((STRPTR)bak, (STRPTR)name, PATHLEN);
        } while (exists(bak, NULL));
        join(src, path, f);
        if (!move(src, bak))
        {
            say1(MSG_COULD_NOT_MOVE, src);
            rc = 10;
            continue;
        }
        if (FPrintf(fh, (STRPTR)"REPLACED %s\nBACKUP %s\n", (IPTR)src,
                (IPTR)name) < 0)
            rc = 10;
    }
    Close(fh);
    FreeVec(names);
    DeleteFile((STRPTR)path);
    return rc;
}

static void status(void)
{
    static char dirs[8][PATHLEN];
    char path[PATHLEN], ver[120];
    int i, n = libs_dirs(dirs, 8);
    LONG type;

    PutStr((STRPTR)msg(MSG_STATUS_LIBS));
    for (i = 0; i < n; i++)
    {
        int z;

        Printf((STRPTR)"  %s\n", (IPTR)dirs[i]);
        join(path, dirs[i], "muimaster.library");
        z = file_is_zune(path);
        if (z >= 0)
        {
            file_version(path, ver, sizeof(ver));
            Printf((STRPTR)msg(MSG_STATUS_LIBRARY), (IPTR)ver,
                (IPTR)msg(z ? MSG_STATUS_IS_OPENMUI : MSG_STATUS_NOT_OPENMUI));
        }
        join(path, dirs[i], "zunemaster.library");
        if (exists(path, NULL))
        {
            file_version(path, ver, sizeof(ver));
            Printf((STRPTR)msg(MSG_STATUS_ZUNEMASTER), (IPTR)ver);
        }
    }
    if (exists("MUI:", &type))
    {
        join(path, "MUI:Libs", "muimaster.library");
        file_version(path, ver, sizeof(ver));
        Printf((STRPTR)msg(MSG_STATUS_MUI_ASSIGNED),
            (IPTR)(ver[0] ? ver : msg(MSG_STATUS_NONE)));
    }
    else
        PutStr((STRPTR)msg(MSG_STATUS_MUI_NOT_ASSIGNED));
    if (libsdir)
        Printf((STRPTR)msg(MSG_STATUS_INSTALLED), (IPTR)home,
            (IPTR)msg(parked() ? MSG_STATUS_REAL_MUI_CHOSEN : MSG_STATUS_IN_USE));
    else
        PutStr((STRPTR)msg(MSG_STATUS_NO_RECORD));
}

static LONG to_long(const char *s)
{
    LONG v = 0;

    while (*s >= '0' && *s <= '9')
        v = v * 10 + *s++ - '0';
    return v;
}

int main(void)
{
    static const char tmpl[] = "ACTION,FILE,HOME/K,SETTINGS/S,QUIET/S";
    LONG args[5] = { 0 };
    struct RDArgs *rda = NULL;
    char action[32];
    int rc = 0;

    action[0] = 0;
    fromwb = _WBenchMsg != NULL;
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 36);
    if (fromwb)
    {
        /* ACTION from the tool types of the icon double-clicked: a project
         * ("Use real MUI", ...) whose default tool we are, or our own */
        if ((IconBase = OpenLibrary((STRPTR)"icon.library", 36)))
        {
            struct WBArg *wa = _WBenchMsg->sm_ArgList
                + _WBenchMsg->sm_NumArgs - 1;
            BPTR old = CurrentDir(wa->wa_Lock);
            struct DiskObject *dob = GetDiskObject((STRPTR)wa->wa_Name);

            if (dob)
            {
                STRPTR s = FindToolType((STRPTR *)dob->do_ToolTypes,
                    (STRPTR)"ACTION");

                if (s)
                    strncpy(action, (char *)s, sizeof(action) - 1);
                FreeDiskObject(dob);
            }
            CurrentDir(old);
            if (NameFromLock(wa->wa_Lock, (STRPTR)home, PATHLEN) == 0)
                home[0] = 0;
            CloseLibrary(IconBase);
        }
    }
    else
    {
        if (!(rda = ReadArgs((STRPTR)tmpl, args, NULL)))
        {
            PrintFault(IoErr(), (STRPTR)"OpenMUISwitch");
            rc = 10;
            goto end;
        }
        if (args[0])
            strncpy(action, (char *)args[0], sizeof(action) - 1);
        quiet = args[4] != 0;
        if (args[2])
            strncpy(home, (char *)args[2], PATHLEN - 1);
        else if (!NameFromLock(GetProgramDir(), (STRPTR)home, PATHLEN))
            home[0] = 0;
    }
    /* home: the given drawer, or ours (also when we are its Data copy),
     * or SYS:OpenMUI */
    {
        char p[PATHLEN];

        join(p, home, "Data/Manifest");
        if (!home[0] || !exists(p, NULL))
        {
            join(p, home, "Manifest");
            if (home[0] && exists(p, NULL))
                *PathPart((STRPTR)home) = 0;
            else if (!args[2])
                strcpy(home, "SYS:OpenMUI");
        }
    }

    if (!Stricmp((STRPTR)action, (STRPTR)"ISZUNE"))
    {
        int z = args[1] ? file_is_zune((char *)args[1]) : -1;

        rc = z < 0 ? 10 : z ? 0 : 5;
        goto end;
    }
    if (!Stricmp((STRPTR)action, (STRPTR)"LIBSDIR"))
    {
        static char dirs[8][PATHLEN];
        LONG want = args[1] ? to_long((char *)args[1]) : 1;
        int n = libs_dirs(dirs, 8);

        rc = 5;
        if (want >= 1 && want <= n && dirs[want - 1][0])
        {
            SetVar((STRPTR)"OpenMUI-LibsDir", (STRPTR)dirs[want - 1], -1,
                GVF_GLOBAL_ONLY);
            rc = 0;
        }
        goto end;
    }
    if (!Stricmp((STRPTR)action, (STRPTR)"MIGRATE"))
    {
        migrate_settings("ENVARC:Zune", "ENVARC:OpenMUI");
        migrate_settings("ENV:Zune", "ENV:OpenMUI");
        if (args[1])
            rc = retire_drawer((char *)args[1]);
        goto end;
    }
    if (!read_manifest())
        libsdir = NULL;
    if (!Stricmp((STRPTR)action, (STRPTR)"STATUS") || !action[0])
    {
        if (fromwb)
            say(MSG_NO_ACTION);
        else
            status();
        goto end;
    }
    if (!libsdir)
    {
        say1(MSG_NO_RECORD, home);
        rc = 10;
        goto end;
    }
    if (!Stricmp((STRPTR)action, (STRPTR)"MUI"))
        rc = do_park();
    else if (!Stricmp((STRPTR)action, (STRPTR)"ZUNE")
             || !Stricmp((STRPTR)action, (STRPTR)"OPENMUI"))
        rc = do_unpark();
    else if (!Stricmp((STRPTR)action, (STRPTR)"UNINSTALL"))
        rc = do_uninstall(args[3] != 0, fromwb);
    else
    {
        say1(MSG_UNKNOWN_ACTION, action);
        rc = 10;
    }
end:
    if (fromwb && reportlen && IntuitionBase)
        ask(report, msg(MSG_OK));
    if (home_later)
        delete_home_later();
    close_catalog();
    if (rda)
        FreeArgs(rda);
    if (IntuitionBase)
        CloseLibrary((struct Library *)IntuitionBase);
    (void)version;
    return rc;
}
