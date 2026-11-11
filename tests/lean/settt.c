/* settt ICONPATH TOOLTYPE...: replace an icon's tool types (path without .info) */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/icon.h>
#include <workbench/workbench.h>
struct Library *IconBase;
int main(int argc, char **argv)
{
    struct DiskObject *d;
    STRPTR *old, tt[16];
    int i, rc = 5;
    if (argc < 2 || !(IconBase = OpenLibrary("icon.library", 36))) return 20;
    if ((d = GetDiskObject(argv[1]))) {
        for (i = 2; i < argc && i < 17; i++) tt[i - 2] = argv[i];
        tt[i - 2] = NULL;
        old = d->do_ToolTypes;
        d->do_ToolTypes = tt;
        rc = PutDiskObject(argv[1], d) ? 0 : 10;
        d->do_ToolTypes = old;
        FreeDiskObject(d);
    }
    Printf("settt %s: %ld\n", argv[1], (LONG)rc);
    CloseLibrary(IconBase);
    return rc;
}
