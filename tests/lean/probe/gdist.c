/* gdist [N [SEED]] or gdist FILE: N random groups (2-6 children of random minimum, default, maximum
 * and weight; horizontal or vertical; spacing 0 or 4; a random window size), each opened
 * once; prints the case and the sizes the children got, for tests/lean/probe/gdist.py to
 * check a model of MUI 3.8's spare-pixel rule against (Compatibility round 5). Output to stdout.
 * A line: "H|V spacing length: min/def/max/weight ... = size ...". With FILE
 * (a name with a colon), its lines "H|V spacing length min/def/max/weight ..." are the cases;
 * "C2 spacing width ..." a ColGroup of 2 columns (horizontal values; every child 10 high).
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os -I<build>/include gdist.c -o gdist -lamiga */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

struct Library *MUIMasterBase;
ULONG __stack = 65536;

#define TAG_MIN (0x4d440000 | 1)
#define TAG_DEF (0x4d440000 | 2)
#define TAG_MAX (0x4d440000 | 3)

struct Data { LONG min, def, max; };
static int vertical;
static ULONG seed = 1;
static LONG rnd(LONG n) { seed = seed * 1103515245UL + 12345UL; return (LONG)((seed >> 16) % (ULONG)n); }

static ULONG Dispatch(struct IClass *cl __asm("a0"), Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    struct Data *d;
    if (msg->MethodID == OM_NEW)
    {
        if ((obj = (Object *)DoSuperMethodA(cl, obj, msg)))
        {
            struct TagItem *t = ((struct opSet *)msg)->ops_AttrList;
            d = INST_DATA(cl, obj);
            for (; t && t->ti_Tag != TAG_DONE; t++)
                if (t->ti_Tag == TAG_MIN) d->min = t->ti_Data;
                else if (t->ti_Tag == TAG_DEF) d->def = t->ti_Data;
                else if (t->ti_Tag == TAG_MAX) d->max = t->ti_Data;
        }
        return (ULONG)obj;
    }
    if (msg->MethodID == MUIM_AskMinMax)
    {
        struct MUI_MinMax *mm;
        DoSuperMethodA(cl, obj, msg);
        mm = ((struct MUIP_AskMinMax *)msg)->MinMaxInfo;
        d = INST_DATA(cl, obj);
        if (vertical)
        {
            mm->MinHeight += d->min; mm->DefHeight += d->def; mm->MaxHeight += d->max;
            mm->MinWidth += 10; mm->DefWidth += 10; mm->MaxWidth += 10;
        }
        else
        {
            mm->MinWidth += d->min; mm->DefWidth += d->def; mm->MaxWidth += d->max;
            mm->MinHeight += 10; mm->DefHeight += 10; mm->MaxHeight += 10;
        }
        return 0;
    }
    return DoSuperMethodA(cl, obj, msg);
}

int main(int argc, char **argv)
{
    static const UWORD weights[] = { 0, 1, 2, 3, 30, 50, 70, 100, 100, 100, 100, 150, 200, 255 };
    struct MUI_CustomClass *mcc;
    int n = argc > 1 ? atoi(argv[1]) : 100, c, k;
    FILE *cf = argc > 1 && strchr(argv[1], ':') ? fopen(argv[1], "r") : NULL;
    char line[256];
    if (cf) n = 0x7fff;
    if (argc > 2) seed = atoi(argv[2]);
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    mcc = MUI_CreateCustomClass(NULL, MUIC_Area, NULL, sizeof(struct Data), (APTR)Dispatch);
    if (!mcc) return 20;
    for (c = 0; c < n; c++)
    {
        Object *app, *win, *g, *kid[8];
        struct TagItem gt[12];
        LONG mn[8], df[8], mx[8], w[8], len, sp = rnd(2) ? 4 : 0, cols = 0;
        int nk = 2 + rnd(5), j = 0;
        IPTR open = 0;
        char *p = line;
        vertical = rnd(3) == 0;
        len = vertical ? 60 + rnd(180) : 100 + rnd(520);
        if (cf)
        {
            if (!fgets(line, sizeof(line), cf)) break;
            vertical = line[0] == 'V';
            cols = line[0] == 'C' ? line[1] - '0' : 0;
            sp = strtol(line + (cols ? 2 : 1), &p, 10);
            len = strtol(p, &p, 10);
            for (nk = 0; nk < 8; nk++)
            {
                mn[nk] = strtol(p, &p, 10);
                if (*p != '/') break;
                df[nk] = strtol(p + 1, &p, 10);
                mx[nk] = strtol(p + 1, &p, 10);
                w[nk] = strtol(p + 1, &p, 10);
            }
        }
        for (k = 0; k < nk; k++)
        {
            LONG r = rnd(10);
            if (!cf)
            {
                mn[k] = rnd(3) ? rnd(60) : 0;
                df[k] = mn[k] + (rnd(2) ? rnd(40) : 0);
                mx[k] = r < 2 ? mn[k] : r < 4 ? df[k] + rnd(80) : 9999;
                if (df[k] > mx[k]) df[k] = mx[k];
                w[k] = weights[rnd(sizeof(weights) / sizeof(weights[0]))];
            }
            kid[k] = NewObject(mcc->mcc_Class, NULL, TAG_MIN, mn[k], TAG_DEF, df[k], TAG_MAX, mx[k],
                vertical ? MUIA_VertWeight : MUIA_HorizWeight, w[k], TAG_DONE);
            gt[j].ti_Tag = MUIA_Group_Child; gt[j++].ti_Data = (IPTR)kid[k];
        }
        gt[j].ti_Tag = cols ? MUIA_Group_Columns : MUIA_Group_Horiz;
        gt[j++].ti_Data = cols ? cols : !vertical;
        gt[j].ti_Tag = MUIA_Group_Spacing; gt[j++].ti_Data = sp;
        gt[j].ti_Tag = TAG_DONE;
        g = MUI_NewObjectA(MUIC_Group, gt);
        app = ApplicationObject, SubWindow, (IPTR)(win = WindowObject,
            vertical ? MUIA_Window_Height : MUIA_Window_Width, len,
            WindowContents, (IPTR)g, End), End;
        if (!app) return 20;
        set(win, MUIA_Window_Open, TRUE);
        get(win, MUIA_Window_Open, &open);
        if (open)
        {
            if (cols) printf("C%ld %ld %d:", (long)cols, (long)sp, _mwidth(g));
            else printf("%c %ld %d:", vertical ? 'V' : 'H', (long)sp, vertical ? _mheight(g) : _mwidth(g));
            for (k = 0; k < nk; k++)
                printf(" %ld/%ld/%ld/%ld", (long)mn[k], (long)df[k], (long)mx[k], (long)w[k]);
            printf(" =");
            for (k = 0; k < nk; k++)
                printf(" %d", vertical ? _height(kid[k]) : _width(kid[k]));
            printf("\n");
            fflush(stdout);
        }
        set(win, MUIA_Window_Open, FALSE);
        MUI_DisposeObject(app);
    }
    MUI_DeleteCustomClass(mcc);
    CloseLibrary(MUIMasterBase);
    printf("rc=0\n");
    return 0;
}
