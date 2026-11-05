/* zuneclasses [library]: list the built-in and external classes a Zune
 * library in memory holds, with their references (MUI_GetClass not yet
 * freed), objects and subclasses: what keeps it from leaving memory.
 * Built with the library's own flags (it reads its private base). */
#include <exec/execbase.h>
#include <intuition/classes.h>
#include <proto/exec.h>
#include <stdio.h>
#include "muimaster_intern.h"

extern struct ExecBase *SysBase;

struct ExternalClass        /* as in support_classes.c */
{
    struct MinNode node;
    Class *cl;
    struct Library *lib;
    ULONG refs;
    char name[1];
};

int main(int argc, char **argv)
{
    CONST_STRPTR name = argc > 1 ? argv[1] : "zunemaster.library";
    struct MUIMasterBase_intern *base;
    struct Node *n;
    struct ExternalClass *ec;

    Forbid();
    base = (struct MUIMasterBase_intern *)FindName(&SysBase->LibList, name);
    if (!base)
    {
        Permit();
        printf("%s is not in memory\n", name);
        return 5;
    }
    printf("%s open %d\n", name, base->library.lib_OpenCnt);
    for (n = (struct Node *)base->BuiltinClasses.mlh_Head; n->ln_Succ;
        n = n->ln_Succ)
    {
        Class *cl = (Class *)n;

        if (cl->cl_UserData || cl->cl_ObjectCount || cl->cl_SubclassCount)
            printf("  built-in %-22s refs %ld objects %ld subclasses %ld\n",
                cl->cl_ID ? (char *)cl->cl_ID : "?", (long)cl->cl_UserData,
                (long)cl->cl_ObjectCount, (long)cl->cl_SubclassCount);
    }
    if (base->ExternalClasses.mlh_Head)
        for (ec = (struct ExternalClass *)base->ExternalClasses.mlh_Head;
            ec->node.mln_Succ; ec = (struct ExternalClass *)ec->node.mln_Succ)
            printf("  external %-22s refs %ld objects %ld subclasses %ld\n",
                ec->name, (long)ec->refs, (long)ec->cl->cl_ObjectCount,
                (long)ec->cl->cl_SubclassCount);
    Permit();
    return 0;
}
