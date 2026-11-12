/*
    Importing MUI 3.8's settings (ENVARC:MUI) into OpenMUI's
    (ENVARC:OpenMUI), and an earlier edition's (ENVARC:Zune, ENV:Zune).
    The user's MUI files and the earlier edition's are only read, never
    changed.
*/

#include <string.h>
#include <stdio.h>

#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>

#include "mui38import.h"
#include "mui38prefs.h"

#define MUI38_DIR "ENVARC:MUI"

/* MUI 3.8 calls its global file "«Global».prefs" (ISO-8859-1 guillemets) */
static CONST_STRPTR const global_names[] = {
    "\253Global\273.prefs", "Global.prefs"
};

static BOOL exists(CONST_STRPTR name)
{
    BPTR lock = Lock(name, SHARED_LOCK);

    if (lock)
        UnLock(lock);
    return lock != 0;
}

static void make_dir(CONST_STRPTR name)
{
    BPTR lock = Lock(name, SHARED_LOCK);

    if (!lock)
        lock = CreateDir(name);
    if (lock)
        UnLock(lock);
}

/* Where OpenMUI looks for a picture or image class without a volume (the
 * library's IMSPEC_EXTERNAL_PREFIX and IMSPEC_EXTERNAL_PREFIX2) */
static CONST_STRPTR const image_roots[] = {
    "LIBS:OpenMUI/Images/", "MUI:Images/"
};

static int have_file(const char *name)
{
    char path[300];
    int i;

    if (strchr(name, ':'))
        return exists(name);
    for (i = 0; i < 2; i++)
    {
        snprintf(path, sizeof(path), "%s%s", image_roots[i], name);
        if (exists(path))
            return 1;
    }
    return 0;
}

BOOL mui38_global_file(char *buf, LONG size)
{
    int i;

    for (i = 0; i < 2; i++)
    {
        snprintf(buf, size, MUI38_DIR "/%s", global_names[i]);
        if (exists(buf))
            return TRUE;
    }
    return FALSE;
}

BOOL mui38_convert_file(CONST_STRPTR from, CONST_STRPTR to)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    APTR oldwin;
    BPTR fh;
    LONG len, outlen = -1;
    UBYTE *in = NULL, *out;
    BOOL ok = FALSE;

    if (!(fh = Open(from, MODE_OLDFILE)))
        return FALSE;
    /* no "please insert volume MUI" when MUI: is not assigned */
    oldwin = me->pr_WindowPtr;
    me->pr_WindowPtr = (APTR)-1;
    if (Seek(fh, 0, OFFSET_END) >= 0 && (len = Seek(fh, 0, OFFSET_BEGINNING)) > 0
        && len < 256 * 1024 && (in = AllocVec(2 * len + 64, MEMF_ANY)))
    {
        out = in + len;
        if (Read(fh, in, len) == len)
            outlen = mui38_convert(in, len, out, len + 64, have_file);
    }
    Close(fh);
    me->pr_WindowPtr = oldwin;
    if (outlen > 0 && (fh = Open(to, MODE_NEWFILE)))
    {
        ok = Write(fh, out, outlen) == outlen;
        Close(fh);
        if (!ok)
            DeleteFile(to);
    }
    FreeVec(in);
    return ok;
}

LONG mui38_import_programs(void)
{
    struct FileInfoBlock *fib;
    BPTR dir;
    LONG count = 0;
    char from[300], to[300];

    make_dir("ENVARC:OpenMUI");
    make_dir("ENV:OpenMUI");
    if (!(dir = Lock(MUI38_DIR, SHARED_LOCK)))
        return 0;
    if ((fib = AllocDosObject(DOS_FIB, NULL)))
    {
        if (Examine(dir, fib))
            while (ExNext(dir, fib))
            {
                CONST_STRPTR name = fib->fib_FileName;
                LONG n = strlen(name);

                if (fib->fib_DirEntryType >= 0 || n < 7
                    || Stricmp(name + n - 6, ".prefs")
                    || !Stricmp(name, global_names[0])
                    || !Stricmp(name, global_names[1]))
                    continue;
                /* OpenMUI's own file for the program wins */
                snprintf(to, sizeof(to), "ENVARC:OpenMUI/%s", name);
                if (exists(to))
                    continue;
                snprintf(from, sizeof(from), MUI38_DIR "/%s", name);
                if (!mui38_convert_file(from, to))
                    continue;
                count++;
                snprintf(to, sizeof(to), "ENV:OpenMUI/%s", name);
                if (!exists(to))
                    mui38_convert_file(from, to);
            }
        FreeDosObject(DOS_FIB, fib);
    }
    UnLock(dir);
    return count;
}

BOOL mui38_first_run(void)
{
    char buf[300];

    return !exists("ENV:OpenMUI/global.prefs")
        && !exists("ENVARC:OpenMUI/global.prefs")
        && mui38_global_file(buf, sizeof(buf));
}

/* Copy one file; FALSE (and nothing left) if it cannot be done whole */
static BOOL copy_file(CONST_STRPTR from, CONST_STRPTR to)
{
    BPTR in, out;
    LONG n;
    BOOL ok = FALSE;
    APTR buf = AllocVec(4096, MEMF_ANY);

    if (!buf)
        return FALSE;
    if ((in = Open(from, MODE_OLDFILE)))
    {
        if ((out = Open(to, MODE_NEWFILE)))
        {
            ok = TRUE;
            while ((n = Read(in, buf, 4096)) > 0)
                if (Write(out, buf, n) != n)
                {
                    ok = FALSE;
                    break;
                }
            if (n < 0)
                ok = FALSE;
            Close(out);
            if (!ok)
                DeleteFile(to);
        }
        Close(in);
    }
    FreeVec(buf);
    return ok;
}

/* No drawer, or one with nothing in it */
static BOOL dir_empty(CONST_STRPTR name, struct FileInfoBlock *fib)
{
    BPTR dir = Lock(name, SHARED_LOCK);
    BOOL empty = TRUE;

    if (dir)
    {
        if (Examine(dir, fib) && ExNext(dir, fib))
            empty = FALSE;
        UnLock(dir);
    }
    return empty;
}

/* Every file of an earlier edition's drawer that the new one lacks */
static LONG copy_drawer(CONST_STRPTR from, CONST_STRPTR to,
    struct FileInfoBlock *fib)
{
    char src[300], dst[300];
    BPTR dir;
    LONG count = 0;

    if (!(dir = Lock(from, SHARED_LOCK)))
        return 0;
    make_dir(to);
    if (Examine(dir, fib))
        while (ExNext(dir, fib))
        {
            if (fib->fib_DirEntryType >= 0)
                continue;
            snprintf(dst, sizeof(dst), "%s/%s", to, fib->fib_FileName);
            if (exists(dst))
                continue;
            snprintf(src, sizeof(src), "%s/%s", from, fib->fib_FileName);
            if (copy_file(src, dst))
                count++;
        }
    UnLock(dir);
    return count;
}

/* The new drawer has nothing yet, or no global settings while the old
 * one has them (only window positions that programs saved in between) */
LONG openmui_import_old(void)
{
    static CONST_STRPTR const drawers[][4] = {
        { "ENVARC:Zune", "ENVARC:OpenMUI",
          "ENVARC:Zune/global.prefs", "ENVARC:OpenMUI/global.prefs" },
        { "ENV:Zune", "ENV:OpenMUI",
          "ENV:Zune/global.prefs", "ENV:OpenMUI/global.prefs" },
    };
    struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
    LONG count = 0;
    int i;

    if (!fib)
        return 0;
    for (i = 0; i < 2; i++)
        if (!dir_empty(drawers[i][0], fib)
            && (dir_empty(drawers[i][1], fib)
                || (exists(drawers[i][2]) && !exists(drawers[i][3]))))
            count += copy_drawer(drawers[i][0], drawers[i][1], fib);
    FreeDosObject(DOS_FIB, fib);
    return count;
}
