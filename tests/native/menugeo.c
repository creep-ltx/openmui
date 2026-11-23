/* menugeo: the menu strip of a window of another program, as Intuition
 * has it: every menu's and item's box, flags, and what its ItemFill is
 * (IntuiText: its pens, font and text; an Image: its size). Black box:
 * compare a program's menus under two muimaster.library.
 * menugeo TITLE   (the first window whose title starts with TITLE)
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os menugeo.c -o menugeo -lamiga
 */
#include <stdio.h>
#include <string.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <proto/exec.h>
#include <proto/intuition.h>

static void item(struct MenuItem *it, int depth)
{
    for (; it; it = it->NextItem)
    {
        printf("%*sitem %d,%d %dx%d flags=%04x mx=%lx cmd=%c", depth * 2, "",
            it->LeftEdge, it->TopEdge, it->Width, it->Height,
            (unsigned)it->Flags, (unsigned long)it->MutualExclude,
            it->Flags & COMMSEQ ? it->Command : '-');
        if (it->Flags & ITEMTEXT)
        {
            struct IntuiText *t = it->ItemFill;
            for (; t; t = t->NextText)
                printf(" text %d,%d pens=%d,%d mode=%d font=%s/%d \"%s\"",
                    t->LeftEdge, t->TopEdge, t->FrontPen, t->BackPen,
                    t->DrawMode, t->ITextFont ? (char *)t->ITextFont->ta_Name : "-",
                    t->ITextFont ? t->ITextFont->ta_YSize : 0,
                    t->IText ? (char *)t->IText : "");
        }
        else if (it->ItemFill)
        {
            struct Image *im = it->ItemFill;
            printf(" image %d,%d %dx%d depth=%d data=%s", im->LeftEdge,
                im->TopEdge, im->Width, im->Height, im->Depth,
                im->ImageData ? "yes" : "no");
        }
        printf(" select=%s\n", it->SelectFill ? "yes" : "no");
        if (it->SubItem)
            item(it->SubItem, depth + 1);
    }
}

int main(int argc, char **argv)
{
    struct Screen *s;
    struct Window *w, *found = NULL;
    struct Menu *m;
    ULONG lock;

    if (argc < 2) return 20;
    lock = LockIBase(0);
    for (s = IntuitionBase->FirstScreen; s && !found; s = s->NextScreen)
        for (w = s->FirstWindow; w; w = w->NextWindow)
            if (w->Title && !strncmp((char *)w->Title, argv[1], strlen(argv[1])))
            {
                found = w;
                break;
            }
    UnlockIBase(lock);
    if (!found) { printf("no window %s\n", argv[1]); return 5; }
    printf("window %dx%d idcmp=%08lx flags=%08lx screen font %s/%d bar=%d\n",
        found->Width, found->Height, (unsigned long)found->IDCMPFlags,
        (unsigned long)found->Flags,
        (char *)found->WScreen->Font->ta_Name, found->WScreen->Font->ta_YSize,
        found->WScreen->BarHeight);
    for (m = found->MenuStrip; m; m = m->NextMenu)
    {
        printf("menu %d,%d %dx%d flags=%04x \"%s\" jazz=%d,%d\n", m->LeftEdge,
            m->TopEdge, m->Width, m->Height, (unsigned)m->Flags,
            m->MenuName ? (char *)m->MenuName : "", m->JazzX, m->JazzY);
        item(m->FirstItem, 1);
    }
    return 0;
}
