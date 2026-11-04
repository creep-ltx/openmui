#include <proto/exec.h>
#include <exec/execbase.h>
#include <stdio.h>
extern struct ExecBase *SysBase;
int main(void)
{
    struct Node *n;
    Forbid();
    for (n = SysBase->LibList.lh_Head; n->ln_Succ; n = n->ln_Succ) {
        struct Library *l = (struct Library *)n;
        printf("%-28s %3d.%-3d open %2d size %ld\n", n->ln_Name, l->lib_Version, l->lib_Revision, l->lib_OpenCnt, (long)(l->lib_NegSize + l->lib_PosSize));
    }
    Permit();
    return 0;
}
