/* strbg: what a String object with its own MUIA_Background and no frame shows, what
 * MUIA_Gadget_Gadget gives for it (UP-MCC's UP-Field changes the background pens of
 * the string gadget's StringExtend), and the window's active object after opening.
 * Three Strings: plain, MUIA_Background black, and black + the gadget's background
 * pens set to 1 (black) before the window draws again. */
#include <libraries/mui.h>
#include <intuition/sghooks.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <string.h>

#ifndef MUIA_Gadget_Gadget
#define MUIA_Gadget_Gadget 0x8042ec1a /* MUI 3.8 mui.h */
#endif

struct Library *MUIMasterBase;
unsigned long __stack = 65536;

static void gad(const char *what, Object *s)
{
    struct Gadget *g = NULL;
    ULONG ok = GetAttr(MUIA_Gadget_Gadget, s, (ULONG *)&g);
    IPTR cont = 0;
    get(s, MUIA_String_Contents, &cont);
    printf("%s: get Gadget_Gadget %lu -> %p", what, (unsigned long)ok, (void *)g);
    if (g) {
        struct StringInfo *si = (struct StringInfo *)g->SpecialInfo;
        printf(" type %04x flags %04x si %p", g->GadgetType, g->Flags, (void *)si);
        if (si) {
            printf(" ext %p", (void *)si->Extension);
            if (si->Extension)
                printf(" pens %u/%u active %u/%u", si->Extension->Pens[0], si->Extension->Pens[1],
                    si->Extension->ActivePens[0], si->Extension->ActivePens[1]);
        }
    }
    printf(" contents %p \"%s\"\n", (void *)cont, cont ? (char *)cont : "(null)");
}

int main(int argc, char **argv)
{
    static const char *black = "2:00000000,00000000,00000000";
    Object *app, *win, *s1, *s2, *s3;
    IPTR act = 0;
    int hold = argc > 1 && !strcmp(argv[1], "HOLD");
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject, MUIA_Application_Base, "STRBG",
        SubWindow, win = WindowObject, MUIA_Window_Title, "strbg", MUIA_Window_LeftEdge, 10, MUIA_Window_TopEdge, 20,
            MUIA_Window_Width, 300,
            WindowContents, VGroup, MUIA_Background, "2:80808080,20202020,20202020",
                Child, s1 = StringObject, MUIA_Frame, MUIV_Frame_None, MUIA_String_Contents, "", End,
                Child, s2 = StringObject, MUIA_Frame, MUIV_Frame_None, MUIA_Background, black, MUIA_String_Contents, "", End,
                Child, s3 = StringObject, MUIA_Frame, MUIV_Frame_None, MUIA_Background, black, MUIA_String_Contents, "x", MUIA_CycleChain, 1, End,
            End, End,
        End;
    if (!app) { printf("no app\n"); return 20; }
    gad("s2 before open", s2);
    set(win, MUIA_Window_Open, TRUE);
    gad("s1", s1); gad("s2", s2); gad("s3", s3);
    get(win, MUIA_Window_ActiveObject, &act);
    printf("active object %p (s1 %p s2 %p s3 %p)\n", (void *)act, (void *)s1, (void *)s2, (void *)s3);
    {
        struct Gadget *g = NULL;
        if (GetAttr(MUIA_Gadget_Gadget, s3, (ULONG *)&g) && g && g->SpecialInfo
            && ((struct StringInfo *)g->SpecialInfo)->Extension) {
            struct StringExtend *x = ((struct StringInfo *)g->SpecialInfo)->Extension;
            x->Pens[1] = 1; x->ActivePens[1] = 1;
            MUI_Redraw(s3, MADF_DRAWOBJECT);
            printf("s3 pens set to 1\n");
        }
    }
    fflush(stdout);
    if (hold) Wait(SIGBREAKF_CTRL_C); else Delay(100);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
