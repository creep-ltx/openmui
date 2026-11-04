#include <proto/exec.h>
#include <dos/dos.h>
int main(void)
{
    struct Library *b = OpenLibrary("muimaster.library", 11);
    if (!b) return 20;
    Wait(SIGBREAKF_CTRL_C);
    CloseLibrary(b);
    return 0;
}
