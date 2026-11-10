/* rexxport: when does an application's ARexx port appear, and under which
 * name?  Each case makes an application, prints what MUIA_Application_Base
 * reads back and whether a public port of that name exists right after
 * OM_NEW returned (before any window or MUIM_Application_Run/Input).
 *   1 Base "lowBase", no Commands, no RexxHook
 *   2 the same with MUIA_Application_UseRexx FALSE
 *   3 no MUIA_Application_Base
 *   4 Base "preBase" while a foreign public port "PREBASE.1" exists
 *   5 Base "single" with MUIA_Application_SingleTask TRUE
 *   6 Base "lowBase" again while case 1's application lives (".2")
 * and whether the port is gone after MUI_DisposeObject.
 * "rexxport NAME [s]": only one application of base NAME (single task
 * with a second argument). */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <string.h>

struct Library *MUIMasterBase;

static int has_port(const char *name)
{
    int r;
    Forbid();
    r = FindPort((STRPTR)name) != NULL;
    Permit();
    return r;
}

static int count_ports(const char *name)
{
    struct Node *n;
    int c = 0;
    Forbid();
    for (n = SysBase->PortList.lh_Head; n->ln_Succ; n = n->ln_Succ)
        if (n->ln_Name && !strcmp(n->ln_Name, name)) c++;
    Permit();
    return c;
}

static Object *report(const char *what, Object *app)
{
    char *b = NULL;
    if (!app) { printf("%s: no application\n", what); return NULL; }
    get(app, MUIA_Application_Base, &b);
    printf("%s: base '%s', %d port(s) of that name\n", what, b ? b : "-",
        b ? count_ports(b) : 0);
    return app;
}

static void gone(const char *what, Object *app)
{
    char name[64] = "";
    char *b = NULL;
    if (!app) return;
    get(app, MUIA_Application_Base, &b);
    if (b) strncpy(name, b, 63);
    MUI_DisposeObject(app);
    printf("%s disposed: port %s %s\n", what, name, has_port(name) ? "still there" : "gone");
}

int main(int argc, char **argv)
{
    Object *a1, *a2, *a3, *a4, *a5, *a6;
    struct MsgPort *pre;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    if (argc > 1)   /* "rexxport NAME [s]": one application, single task with s */
    {
        a1 = report(argv[1], ApplicationObject, MUIA_Application_Base, (IPTR)argv[1],
            MUIA_Application_SingleTask, argc > 2, End);
        gone(argv[1], a1);
        CloseLibrary(MUIMasterBase);
        return 0;
    }

    a1 = report("1 lowBase", ApplicationObject, MUIA_Application_Base, (IPTR)"lowBase", End);
    a2 = report("2 UseRexx FALSE", ApplicationObject, MUIA_Application_Base, (IPTR)"noRexx",
        MUIA_Application_UseRexx, FALSE, End);
    a3 = report("3 no base", ApplicationObject, End);
    pre = CreateMsgPort();
    if (pre) { pre->mp_Node.ln_Name = "PREBASE.1"; pre->mp_Node.ln_Pri = 0; AddPort(pre); }
    a4 = report("4 foreign PREBASE.1", ApplicationObject, MUIA_Application_Base, (IPTR)"preBase", End);
    a5 = report("5 single", ApplicationObject, MUIA_Application_Base, (IPTR)"single",
        MUIA_Application_SingleTask, TRUE, End);
    a6 = report("6 lowBase again", ApplicationObject, MUIA_Application_Base, (IPTR)"lowBase", End);

    gone("6", a6); gone("5", a5); gone("4", a4); gone("3", a3); gone("2", a2); gone("1", a1);
    if (pre) { RemPort(pre); DeleteMsgPort(pre); }
    CloseLibrary(MUIMasterBase);
    return 0;
}
