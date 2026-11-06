/* pubscr W H MODE SECS: open a public screen "TALL" and make it the default
 * public screen for SECS seconds */
#include <intuition/screens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    static UWORD pens[] = { (UWORD)~0 };
    struct Screen *s;
    int secs = argc > 4 ? atoi(argv[4]) : 120;
    s = OpenScreenTags(NULL, SA_Width, atoi(argv[1]), SA_Height, atoi(argv[2]), SA_Depth, 2,
        SA_DisplayID, strtoul(argv[3], NULL, 16), SA_Pens, (ULONG)pens, SA_Title, (ULONG)"TALL",
        SA_PubName, (ULONG)"TALL", SA_SysFont, 1, TAG_DONE);
    if (!s) return 20;
    PubScreenStatus(s, 0);
    SetDefaultPubScreen("TALL");
    Delay(secs * 50);
    SetDefaultPubScreen(NULL);
    while (!CloseScreen(s)) Delay(50);
    return 0;
}
