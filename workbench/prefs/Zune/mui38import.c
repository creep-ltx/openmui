/*
    Importing MUI 3.8's settings (ENVARC:MUI) into Zune's (ENVARC:zune).
    The user's MUI files are only read, never changed.
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

/* Where Zune looks for a picture or image class without a volume (the
 * library's IMSPEC_EXTERNAL_PREFIX and IMSPEC_EXTERNAL_PREFIX2) */
static CONST_STRPTR const image_roots[] = {
    "LIBS:Zune/Images/", "MUI:Images/"
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

    make_dir("ENVARC:zune");
    make_dir("ENV:zune");
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
                /* Zune's own file for the program wins */
                snprintf(to, sizeof(to), "ENVARC:zune/%s", name);
                if (exists(to))
                    continue;
                snprintf(from, sizeof(from), MUI38_DIR "/%s", name);
                if (!mui38_convert_file(from, to))
                    continue;
                count++;
                snprintf(to, sizeof(to), "ENV:zune/%s", name);
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

    return !exists("ENV:zune/global.prefs")
        && !exists("ENVARC:zune/global.prefs")
        && mui38_global_file(buf, sizeof(buf));
}
