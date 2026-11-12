/* zclasses: what keeps OpenMUI's muimaster.library from being expunged: the base's open count and
   flags, every built-in class with a reference (cl_UserData), objects or subclasses, every loaded
   external class. Test tool (stability round 2). The list offsets are offsetof() in
   MUIMasterBase_intern from a build of muimaster_intern.h (2026-11-12): ExternalClasses 546,
   BuiltinClasses 1094; an OpenMUI whose base layout changed needs them again.
   Build: m68k-amigaos-gcc -Os -m68000 -noixemul -s -o zclasses zclasses.c */
#include <proto/exec.h>
#include <proto/dos.h>
#include <exec/execbase.h>
#include <intuition/classes.h>
#include <string.h>
#include <stdio.h>
#define O_EC 546
#define O_BC 1094
struct ec { struct MinNode node; Class *cl; struct Library *lib; ULONG refs; UBYTE identity; char name[1]; };
int main(void)
{
    struct Library *mb;
    struct MinNode *n;
    static char out[8000]; int len = 0, total = 0;
    Forbid();
    mb = (struct Library *)FindName(&SysBase->LibList, "muimaster.library");
    if (!mb || !mb->lib_IdString || !strstr((char *)mb->lib_IdString, "OpenMUI")) { Permit(); PutStr("no OpenMUI in memory\n"); return 0; }
    len += sprintf(out + len, "muimaster.library open %u flags %x\n", mb->lib_OpenCnt, mb->lib_Flags);
    for (n = ((struct MinList *)((UBYTE *)mb + O_BC))->mlh_Head; n->mln_Succ && len < 7600; n = n->mln_Succ) {
        Class *cl = (Class *)n;
        total++;
        if (cl->cl_UserData || cl->cl_ObjectCount || cl->cl_SubclassCount)
            len += sprintf(out + len, "builtin %-24s refs %lu objects %lu subclasses %lu\n",
                cl->cl_ID ? (char *)cl->cl_ID : "?", cl->cl_UserData, cl->cl_ObjectCount, cl->cl_SubclassCount);
    }
    len += sprintf(out + len, "%d built-in classes\n", total);
    if (((struct MinList *)((UBYTE *)mb + O_EC))->mlh_Head)
        for (n = ((struct MinList *)((UBYTE *)mb + O_EC))->mlh_Head; n->mln_Succ && len < 7600; n = n->mln_Succ) {
            struct ec *e = (struct ec *)n;
            len += sprintf(out + len, "external %-24s refs %lu objects %lu subclasses %lu lib open %u\n", e->name, e->refs,
                e->cl ? e->cl->cl_ObjectCount : 0, e->cl ? e->cl->cl_SubclassCount : 0, e->lib ? e->lib->lib_OpenCnt : 0);
        }
    Permit();
    PutStr(out);
    return 0;
}
