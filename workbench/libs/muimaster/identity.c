/* Which MUI each program gets (lean edition, roadmap phase 8).
 *
 * Programs and classes choose their code paths by the version the
 * library base reports (TheBar sizes its virtual bar itself only below
 * MUI 4, NList and TextEditor branch on 20.2326 and 20.5640), so each
 * program should see the MUI it was written for. Exec's OpenLibrary()
 * refuses before it calls the library's Open vector when lib_Version is
 * lower than the version asked for, so the base in the library list
 * reports MUI 5 (21.227). Exec passes the version asked for to Open in
 * D0, and returns whatever Open returns (as AmiTCP's bsdsocket.library
 * does with its base per opener): so Open hands out the base for the
 * program's identity, this base itself for MUI 5 or a copy for MUI 3.8
 * (19.35) and MUI 4 (20.6000). A copy is the jump table and the Library
 * structure, 342 bytes, made the first time it is needed and shared by
 * every program with that identity.
 *
 * The identity of a program: the first match of
 *   1. the program's line in the per-program list (ENV:Zune/BehaveAs),
 *   2. the "Behave as" setting for all programs, unless Automatic,
 *   3. Automatic: the version it asks for, 0-19 MUI 3.8, 20 MUI 4,
 *      21 and higher MUI 5.
 * It is decided at the program's first open and remembered for its task
 * while it has the library open (ZUNE_ID_OPENERS records, 12 bytes each,
 * in the base), so the classes it opens from its own task and the
 * application objects it makes follow it. External classes are loaded
 * through ramlib, whose task opens this library for them: those opens
 * get the identity of the program that is loading the class (IdLoader,
 * set by support_classes.c around the load), else the rules above with
 * the version the class asks for.
 *
 * ENV:Zune/BehaveAs (written by Zune prefs):
 *   first line: Auto | MUI3.8 | MUI4 | MUI5   (the setting for all)
 *   each further line: MUI3.8|MUI4|MUI5 <program name>
 * The program name is the command's file name (the Shell's command
 * name, or the task name of a program started from Workbench), without
 * its path, compared without case. Read when the library is loaded and
 * when Zune prefs saves or uses its settings (the library's reserved
 * vector, LibReserved in zunemaster_lib.c): Open runs under Forbid and
 * must not use DOS. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <exec/execbase.h>
#include <dos/dosextens.h>
#include <dos/var.h>
#include <string.h>
#include "muimaster_intern.h"
#include "native_version.h"

extern struct Library *MUIMasterBase;

/* One entry of the parsed per-program list: identity, length, name */
struct IdProgram
{
    UBYTE identity;
    UBYTE len;
    char name[1];
};
#define IDPROG_NEXT(p) ((struct IdProgram *)((UBYTE *)(p) + 2 + (p)->len + 1))

/* A copy of the base: the jump table before it, the shared base after */
struct IdCopy
{
    struct Library lib;
    struct Library *shared;
};

static UBYTE lower(UBYTE c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 0xc0 && c <= 0xde && c != 0xd7)
        ? c + 0x20 : c;
}

/* "Auto", "MUI3.8" (also "MUI3", "3.8"), "MUI4", "MUI5": 0-3, -1 for none */
static LONG parse_identity(const char **sp)
{
    const char *s = *sp;
    LONG id = -1;

    while (*s == ' ' || *s == '\t') s++;
    if ((s[0] | 0x20) == 'a' && (s[1] | 0x20) == 'u' && (s[2] | 0x20) == 't'
        && (s[3] | 0x20) == 'o')
    {
        id = 0; s += 4;
        if ((s[0] | 0x20) == 'm') /* "Automatic" */
            while (*s > ' ') s++;
    }
    else
    {
        if ((s[0] | 0x20) == 'm' && (s[1] | 0x20) == 'u' && (s[2] | 0x20) == 'i')
            s += 3;
        if (s[0] == '3') { id = ZUNE_ID_MUI38; s++; if (s[0] == '.' && s[1] == '8') s += 2; }
        else if (s[0] == '4') { id = ZUNE_ID_MUI4; s++; }
        else if (s[0] == '5') { id = ZUNE_ID_MUI5; s++; }
    }
    if (*s > ' ')
        return -1;
    *sp = s;
    return id;
}

/* Parse the settings into base->IdGlobal and a new program list */
static void parse_settings(struct MUIMasterBase_intern *base, const char *text)
{
    const char *s = text, *e;
    struct IdProgram *list = NULL, *p;
    ULONG size = 1;
    LONG id;
    int pass;
    UBYTE global = 0;
    APTR old;

    /* first pass: the size; second: fill */
    for (pass = 0; pass < 2; pass++)
    {
        s = text;
        p = list;
        /* first line: the setting for all programs */
        id = parse_identity(&s);
        if (id >= 0) global = id;
        while (*s && *s != '\n') s++;
        while (*s)
        {
            ULONG len;

            s++;    /* the newline */
            id = parse_identity(&s);
            while (*s == ' ' || *s == '\t') s++;
            for (e = s; *e && *e != '\n'; e++);
            len = e - s;
            while (len && (s[len - 1] == ' ' || s[len - 1] == '\r'
                || s[len - 1] == '\t'))
                len--;
            if (id > 0 && len && len < 256)
            {
                if (pass == 0)
                    size += 2 + len + 1;
                else
                {
                    p->identity = id;
                    p->len = len;
                    memcpy(p->name, s, len);
                    p->name[len] = 0;
                    p = IDPROG_NEXT(p);
                }
            }
            s = e;
        }
        if (pass == 0)
        {
            if (size == 1)
                break;
            if (!(list = AllocVec(size, MEMF_ANY)))
                break;
        }
        else
            p->identity = 0;    /* the end */
    }
    Forbid();
    old = base->IdPrograms;
    base->IdPrograms = list;
    base->IdGlobal = global;
    Permit();
    if (old)
        FreeVec(old);
}

void zune_identity_load(void)
{
    struct MUIMasterBase_intern *base = MUIMB(MUIMasterBase);
    struct Task *me = FindTask(NULL);
    char *buf;

    if (me->tc_Node.ln_Type != NT_PROCESS)
        return;
    if (!(buf = AllocVec(2048, MEMF_ANY)))
        return;
    if (GetVar("Zune/BehaveAs", buf, 2048, GVF_GLOBAL_ONLY | GVF_BINARY_VAR) < 0)
        buf[0] = 0;
    parse_settings(base, buf);
    FreeVec(buf);
}

void zune_identity_free(void)
{
    struct MUIMasterBase_intern *base = MUIMB(MUIMasterBase);
    struct Library *c;
    int i;

    for (i = 0; i < 2; i++)
        if ((c = base->IdCopy[i]))
        {
            FreeMem((UBYTE *)c - c->lib_NegSize,
                c->lib_NegSize + sizeof(struct IdCopy));
            base->IdCopy[i] = NULL;
        }
    if (base->IdPrograms)
        FreeVec(base->IdPrograms);
    base->IdPrograms = NULL;
}

BOOL zune_is_master(struct Library *lib)
{
    struct MUIMasterBase_intern *base = MUIMB(MUIMasterBase);

    return lib && (lib == MUIMasterBase || lib == base->IdCopy[0]
        || lib == base->IdCopy[1]);
}

/* The program's name: the Shell's command name or the task name, without
 * the path */
static const char *program_name(struct Task *task, ULONG *lenp)
{
    const char *name = NULL, *s;
    ULONG len = 0, i;

    if (task->tc_Node.ln_Type == NT_PROCESS)
    {
        struct Process *pr = (struct Process *)task;
        struct CommandLineInterface *cli = BADDR(pr->pr_CLI);
        UBYTE *cmd = cli ? BADDR(cli->cli_CommandName) : NULL;

        if (cmd && cmd[0])
        {
            name = (const char *)cmd + 1;
            len = cmd[0];
        }
    }
    if (!name && (name = task->tc_Node.ln_Name))
        len = strlen(name);
    if (!name)
        return NULL;
    for (i = len, s = name + len; i--; )
        if (name[i] == ':' || name[i] == '/')
        {
            s = name + i + 1;
            break;
        }
    if (s != name + len)
    {
        len -= s - name;
        name = s;
    }
    *lenp = len;
    return len ? name : NULL;
}

/* The identity the rules give an opener (under Forbid: no DOS) */
static UBYTE decide(struct MUIMasterBase_intern *base, struct Task *task,
    ULONG version)
{
    struct IdProgram *p = base->IdPrograms;
    const char *name;
    ULONG len, i;

    if (p && task && (name = program_name(task, &len)))
        for (; p->identity; p = IDPROG_NEXT(p))
        {
            if (p->len != len)
                continue;
            for (i = 0; i < len && lower(p->name[i]) == lower(name[i]); i++);
            if (i == len)
                return p->identity;
        }
    if (base->IdGlobal)
        return base->IdGlobal;
    return version < 20 ? ZUNE_ID_MUI38 : version == 20 ? ZUNE_ID_MUI4
        : ZUNE_ID_MUI5;
}

/* The record of a task; a record of a task that closed the library from
 * another task (and has ended) does not match a new task at its address:
 * the stack differs */
static struct ZuneOpener *find_opener(struct MUIMasterBase_intern *base,
    struct Task *task)
{
    struct ZuneOpener *o;

    for (o = base->IdOpeners; o < base->IdOpeners + ZUNE_ID_OPENERS; o++)
        if (o->task == task && o->splower == task->tc_SPLower)
            return o;
    return NULL;
}

/* A free record, or that of an ended task at the address of this one */
static struct ZuneOpener *free_opener(struct MUIMasterBase_intern *base,
    struct Task *task)
{
    struct ZuneOpener *o;

    for (o = base->IdOpeners; o < base->IdOpeners + ZUNE_ID_OPENERS; o++)
        if (!o->task || o->task == task)
            return o;
    return NULL;
}

/* The base for an identity: this one for MUI 5, else its copy */
static struct Library *identity_base(struct MUIMasterBase_intern *base,
    UBYTE id)
{
    struct Library *lib = &base->library, *c;
    UWORD neg = lib->lib_NegSize;
    UBYTE *mem;

    if (id == ZUNE_ID_MUI5)
        return lib;
    if ((c = base->IdCopy[id - 1]))
        return c;
    /* AllocMem does not break the Forbid Open runs in */
    if (!(mem = AllocMem(neg + sizeof(struct IdCopy), MEMF_ANY)))
        return lib;
    c = (struct Library *)(mem + neg);
    CopyMem((UBYTE *)lib - neg, mem, neg + sizeof(struct Library));
    ((struct IdCopy *)c)->shared = lib;
    c->lib_OpenCnt = 0;
    c->lib_Flags = LIBF_CHANGED;
    c->lib_PosSize = sizeof(struct IdCopy);
    if (id == ZUNE_ID_MUI38)
    {
        c->lib_Version = ZUNE68_MUI38_VERSION;
        c->lib_Revision = ZUNE68_MUI38_REVISION;
    }
    else
    {
        c->lib_Version = ZUNE68_MUI4_VERSION;
        c->lib_Revision = ZUNE68_MUI4_REVISION;
    }
    /* the jump table is code: the 68020+ caches must see it */
    if (SysBase->LibNode.lib_Version >= 37)
        CacheClearU();
    base->IdCopy[id - 1] = c;
    return c;
}

/* LibOpen: the base this opener gets (under Forbid; the shared count is
 * the caller's) */
struct Library *zune_identity_open(ULONG version)
{
    struct MUIMasterBase_intern *base = MUIMB(MUIMasterBase);
    struct Task *me = FindTask(NULL);
    struct ZuneOpener *o;
    struct Library *lib;
    UBYTE id;

    if ((o = find_opener(base, me)))
    {
        o->count++;
        id = o->identity;
    }
    else if (me->tc_Node.ln_Name && !strcmp(me->tc_Node.ln_Name, "ramlib"))
    {
        /* a class library's init, loaded for a program: no record */
        id = base->IdLoader ? base->IdLoader : decide(base, NULL, version);
    }
    else
    {
        id = decide(base, me, version);
        if ((o = free_opener(base, me)))
        {
            o->task = me;
            o->splower = me->tc_SPLower;
            o->count = 1;
            o->identity = id;
        }
    }
    lib = identity_base(base, id);
    if (lib != &base->library)
        lib->lib_OpenCnt++;
    return lib;
}

/* LibClose through any of the bases (under Forbid) */
void zune_identity_close(struct Library *lib)
{
    struct MUIMasterBase_intern *base = MUIMB(MUIMasterBase);
    struct ZuneOpener *o;

    if (lib != MUIMasterBase && lib->lib_OpenCnt)
        lib->lib_OpenCnt--;
    if ((o = find_opener(base, FindTask(NULL))) && !--o->count)
        o->task = NULL;
}

/* The identity of the calling task: its record, else what the settings
 * give a program with no record of its own (a subtask) */
UBYTE zune_task_identity(void)
{
    struct MUIMasterBase_intern *base = MUIMB(MUIMasterBase);
    struct Task *me = FindTask(NULL);
    struct ZuneOpener *o;
    UBYTE id;

    Forbid();
    if ((o = find_opener(base, me)))
        id = o->identity;
    else if (base->IdLoader && me->tc_Node.ln_Name
        && !strcmp(me->tc_Node.ln_Name, "ramlib"))
        id = base->IdLoader;    /* a class's init making objects */
    else
        id = base->IdGlobal ? base->IdGlobal : ZUNE_ID_MUI38;
    Permit();
    return id;
}
