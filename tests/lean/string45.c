/* string45: String's MUI 4/5 attributes that only MUI 4/5 programs use
 * (MUIA_String_Integer64, the Textinput values String keeps and answers),
 * in the Zmui45 module since branch smaller2: prints what a String
 * answers after init and set. The output must be the same on every
 * library build. Built by hand:
 *   m68k-amigaos-gcc -m68000 -noixemul -Os -s -o string45 tests/lean/string45.c
 */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>
#include <clib/alib_protos.h>
#include <stdio.h>

#ifndef IPTR
#define IPTR ULONG
#endif
#define INTEGER64 0x80424820

/* Textinput.mcc's tag base, TAG_USER | (1307 << 16) + 0x712 */
#define TI(x) (0x851B0712UL + (x))
static const struct { const char *name; ULONG id; ULONG value; } attrs[] = {
    { "WordWrap", TI(118), 55 }, { "IsNumeric", TI(119), 1 },
    { "MinVal", TI(120), 3 }, { "MaxVal", TI(121), 99 },
    { "Changed", TI(124), 1 }, { "SetMin", TI(137), 1 },
    { "SetMax", TI(138), 1 }, { "SetVMax", TI(139), 1 },
    { "Styles", TI(140), 2 }, { "SetVMin", TI(143), 1 },
    { "Font", TI(152), 1 }, { "ResetMarkOnCursor", TI(157), 1 },
};

struct Library *MUIMasterBase;

static void show64(Object *s, const char *what)
{
    long long *p = NULL;
    STRPTR c = NULL;
    ULONG ok = GetAttr(INTEGER64, s, (IPTR *)&p);

    GetAttr(MUIA_String_Contents, s, (IPTR *)&c);
    if (ok && p)
    {
        unsigned long long v = *p;
        printf("%-12s contents \"%s\" int64 %08lx%08lx\n", what,
            c ? (char *)c : "", (ULONG)(v >> 32), (ULONG)v);
    }
    else
        printf("%-12s contents \"%s\" int64 not answered\n", what,
            c ? (char *)c : "");
}

int main(void)
{
    static const long long values[] = {
        0, -1, 1234567890123LL, -9223372036854775807LL - 1,
        9223372036854775807LL, 4294967296LL
    };
    long long init = -42;
    Object *s;
    unsigned int i;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 0)))
        return 20;
    s = MUI_NewObject(MUIC_String, MUIA_String_MaxLen, 40,
        INTEGER64, (IPTR)&init, TAG_DONE);
    if (!s)
    {
        printf("no String\n");
        CloseLibrary(MUIMasterBase);
        return 20;
    }
    show64(s, "init -42");
    for (i = 0; i < sizeof(values) / sizeof(values[0]); i++)
    {
        char name[16];
        sprintf(name, "set #%u", i);
        SetAttrs(s, INTEGER64, (IPTR)&values[i], TAG_DONE);
        show64(s, name);
    }
    for (i = 0; i < sizeof(attrs) / sizeof(attrs[0]); i++)
    {
        IPTR before = 0xdead, after = 0xdead;
        ULONG k1 = GetAttr(attrs[i].id, s, &before), k2;

        SetAttrs(s, attrs[i].id, attrs[i].value, TAG_DONE);
        k2 = GetAttr(attrs[i].id, s, &after);
        printf("%-18s get %lu value %lu, set %lu: get %lu value %lu\n",
            attrs[i].name, k1, (ULONG)before, attrs[i].value, k2,
            (ULONG)after);
    }
    SetAttrs(s, MUIA_String_Contents, (IPTR)"  -77abc", TAG_DONE);
    show64(s, "text -77abc");
    MUI_DisposeObject(s);
    printf("done\n");
    CloseLibrary(MUIMasterBase);
    return 0;
}
