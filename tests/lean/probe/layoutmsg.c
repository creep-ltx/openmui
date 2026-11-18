/* layoutmsg: what a group's layout hook finds in its MUI_LayoutMsg at MUILM_MINMAX
 * (fields MUI does not document as input: lm_MinMax before the hook fills it,
 * lm_Layout's Width/Height and private words) and at MUILM_LAYOUT, for a plain group
 * and a Virtgroup in a Scrollgroup (the HTMLtext demo's shape: its hook reads them,
 * Compatibility round 5). Prints each call's message words.
 * Build: m68k-amigaos-gcc -noixemul -Os -s -I<build>/include layoutmsg.c -o layoutmsg -lamiga */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;
static char log[3000];
static int logn;

/* no sprintf in the hook (libnix's sprintf stopped there) */
static void put(const char *t) { while (*t && logn < (int)sizeof(log) - 1) log[logn++] = *t++; }
static void hex(ULONG v)
{
    int i;
    put(" ");
    for (i = 28; i >= 0; i -= 4)
        if (logn < (int)sizeof(log) - 1) log[logn++] = "0123456789abcdef"[(v >> i) & 15];
}

static ULONG hookfunc(struct Hook *h __asm("a0"), Object *obj __asm("a2"),
    struct MUI_LayoutMsg *lm __asm("a1"))
{
    ULONG *w = (ULONG *)lm;
    int i;

    put((char *)h->h_Data);
    put(" type");
    for (i = 0; i < 9; i++)
        hex(w[i]);
    put("\n");
    if (lm->lm_Type == MUILM_MINMAX)
    {
        lm->lm_MinMax.MinWidth = 28;
        lm->lm_MinMax.MinHeight = 24;
        lm->lm_MinMax.DefWidth = 28;
        lm->lm_MinMax.DefHeight = 24;
        lm->lm_MinMax.MaxWidth = MUI_MAXMAX;
        lm->lm_MinMax.MaxHeight = MUI_MAXMAX;
        return 0;
    }
    if (lm->lm_Type == MUILM_LAYOUT)
    {
        Object *cs = (Object *)lm->lm_Children->mlh_Head, *c;
        while ((c = NextObject(&cs)))
            MUI_Layout(c, 0, 0, 20, 10, 0);
        if (((char *)h->h_Data)[0] == 'v')
        {
            lm->lm_Layout.Width = 120;
            lm->lm_Layout.Height = 90;
        }
        return TRUE;
    }
    return MUILM_UNKNOWN;
}

static struct Hook hplain = { {NULL, NULL}, (HOOKFUNC)hookfunc, NULL, "plain" };
static struct Hook hvirt = { {NULL, NULL}, (HOOKFUNC)hookfunc, NULL, "virt" };
static struct Hook htwo = { {NULL, NULL}, (HOOKFUNC)hookfunc, NULL, "two" };
static struct Hook hframed = { {NULL, NULL}, (HOOKFUNC)hookfunc, NULL, "framed" };

int main(void)
{
    Object *app, *win;
    IPTR open = 0;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    app = ApplicationObject, MUIA_Application_Title, (IPTR)"LMSG", MUIA_Application_Base, (IPTR)"LMSG",
        SubWindow, (IPTR)(win = WindowObject, MUIA_Window_Title, (IPTR)"LMSG", MUIA_Window_ID, 0,
            MUIA_Window_UseRightBorderScroller, TRUE, MUIA_Window_UseBottomBorderScroller, TRUE,
            WindowContents, (IPTR)(VGroup,
                Child, (IPTR)(GroupObject, MUIA_Group_LayoutHook, (IPTR)&hplain,
                    Child, (IPTR)RectangleObject, End, End),
                Child, (IPTR)(GroupObject, MUIA_Group_LayoutHook, (IPTR)&htwo,
                    Child, (IPTR)(RectangleObject, MUIA_FixWidth, 50, MUIA_FixHeight, 30, End),
                    Child, (IPTR)(TextObject, MUIA_Text_Contents, (IPTR)"Some text", End), End),
                Child, (IPTR)(GroupObject, MUIA_Group_LayoutHook, (IPTR)&hframed, TextFrame,
                    MUIA_InnerLeft, 7, MUIA_InnerTop, 5, Child, (IPTR)RectangleObject, End, End),
                Child, (IPTR)(ScrollgroupObject, MUIA_Scrollgroup_UseWinBorder, TRUE,
                    MUIA_Scrollgroup_Contents, (IPTR)(VirtgroupObject, MUIA_Group_LayoutHook, (IPTR)&hvirt,
                        Child, (IPTR)RectangleObject, End, End), End),
                End), End), End;
    if (!app) { printf("no app\n"); return 20; }
    set(win, MUIA_Window_Open, TRUE);
    get(win, MUIA_Window_Open, &open);
    printf("open %ld\n%s", (long)open, log);
    fflush(stdout);
    Delay(10);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
