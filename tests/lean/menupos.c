/* menupos WINDOW MENU ITEM [SUB]: where a window's menu item is on its screen.
 * WINDOW is the window's address in hex (as "wasabi windows --raw" shows it). Prints
 * "titlex titley itemx itemy [subx suby]" in screen pixels, or "none". Names compare without
 * case, leading/trailing blanks and a trailing "..." or "»". The real-program suite
 * (scripts/suite-lib.sh) picks MUI menus with the right mouse button at these places, because
 * "wasabi ui menu" sends a message that MUI's windows do not act on.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -o menupos menupos.c */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <intuition/intuitionbase.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

struct IntuitionBase *IntuitionBase;

static void norm(const char *s, char *d, int n)
{
    int l;
    while (*s == ' ') s++;
    for (l = 0; s[l] && l < n - 1; l++) d[l] = toupper((unsigned char)s[l]);
    d[l] = 0;
    for (;;) {
        while (l > 0 && d[l - 1] == ' ') d[--l] = 0;
        if (l >= 3 && !strcmp(d + l - 3, "...")) { l -= 3; d[l] = 0; continue; }
        if (l >= 1 && (unsigned char)d[l - 1] == 0xbb) { d[--l] = 0; continue; }
        break;
    }
}

static int same(const char *a, const char *b)
{
    char x[80], y[80];
    norm(a, x, sizeof x); norm(b, y, sizeof y);
    return !strcmp(x, y);
}

static struct MenuItem *find(struct MenuItem *it, const char *name)
{
    for (; it; it = it->NextItem)
        if ((it->Flags & ITEMTEXT) && it->ItemFill && ((struct IntuiText *)it->ItemFill)->IText &&
            same((char *)((struct IntuiText *)it->ItemFill)->IText, name))
            return it;
    return NULL;
}

int main(int argc, char **argv)
{
    struct Window *w, *ww;
    struct Screen *s;
    struct Menu *m;
    struct MenuItem *it, *sub = NULL;
    ULONG lock;
    int found = 0, bh, mx, my, ix, iy, sx = 0, sy = 0;

    if (argc < 4) { printf("usage: menupos WINDOW MENU ITEM [SUB]\n"); return 10; }
    if (!(IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 36))) return 20;
    w = (struct Window *)strtoul(argv[1], NULL, 16);
    lock = LockIBase(0);
    /* only a window that is open now */
    for (s = IntuitionBase->FirstScreen; s && !found; s = s->NextScreen)
        for (ww = s->FirstWindow; ww; ww = ww->NextWindow)
            if (ww == w) { found = 1; break; }
    if (found && w->MenuStrip) {
        s = w->WScreen;
        bh = s->BarHeight;
        for (m = w->MenuStrip; m; m = m->NextMenu)
            if (m->MenuName && same(m->MenuName, argv[2])) break;
        it = m ? find(m->FirstItem, argv[3]) : NULL;
        if (it && argc > 4) sub = find(it->SubItem, argv[4]);
        if (it && (argc <= 4 || sub)) {
            mx = m->LeftEdge + m->Width / 2; my = bh / 2;
            ix = m->LeftEdge + it->LeftEdge + it->Width / 2;
            iy = bh + 1 + it->TopEdge + it->Height / 2;
            if (sub) {
                sx = m->LeftEdge + it->LeftEdge + sub->LeftEdge + sub->Width / 2;
                sy = bh + 1 + it->TopEdge + sub->TopEdge + sub->Height / 2;
            }
            UnlockIBase(lock);
            if (sub) printf("%d %d %d %d %d %d\n", mx, my, ix, iy, sx, sy);
            else printf("%d %d %d %d\n", mx, my, ix, iy);
            CloseLibrary((struct Library *)IntuitionBase);
            return 0;
        }
    }
    UnlockIBase(lock);
    printf("none\n");
    CloseLibrary((struct Library *)IntuitionBase);
    return 5;
}
