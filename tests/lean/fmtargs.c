/* List's format columns: dos.library's ReadArgs() against List's own reader
 * (a model of ReadArgs' rules, below), over hand-made columns
 * and random ones built from the format's keywords, blanks, "=", quotes,
 * "*" escapes, signs, digits and other characters. Prints every column
 * where they differ (failure, or a value) and a summary line.
 * Run it on each OS the library supports (OS 3.1, OS 3.2.1).
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os tests/lean/fmtargs.c -o fmtargs
 * Run: fmtargs [COUNT [SEED]]   (random columns, default 20000, seed 68)
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/rdargs.h>
#include <proto/exec.h>
#include <proto/dos.h>

/* what list.c takes from classes/list_private.h */
#define FORMAT_TEMPLATE "DELTA=D/K/N,PREPARSE=P/K,WEIGHT=W/K/N,MINWIDTH=MIW/K," \
    "MAXWIDTH=MAW/K,COL=C/K/N,BAR/S,H=HIDDEN/S,SORTABLE/S,ORDER/K," \
    "ISOBJECT/S,ELIDE=E/K,FIXWIDTH/K/N"
enum { ARG_DELTA, ARG_PREPARSE, ARG_WEIGHT, ARG_MINWIDTH, ARG_MAXWIDTH,
    ARG_COL, ARG_BAR, ARG_HIDDEN, ARG_SORTABLE, ARG_ORDER, ARG_ISOBJECT,
    ARG_ELIDE, ARG_FIXWIDTH, ARG_CNT };
/* One column of a List format (MUIA_List_Format), read as dos.library's
 * ReadArgs() reads it with FORMAT_TEMPLATE (classes/list_private.h), without
 * ReadArgs: no RDArgs, no line buffer on the stack, no dos.library call per
 * column. The rules are ReadArgs' own, measured on OS 3.1 and OS 3.2.1
 * (tests/lean/fmtargs.c runs this and ReadArgs over the same columns):
 *
 * - items are separated by spaces and tabs; a newline or a ";" outside
 *   quotes ends the column (the rest is a comment);
 * - a keyword's value follows it, after "=" or a blank ("P=\33c",
 *   "P \33c", "P = \33c"; "P==x" is "x"); a switch may end in "="
 *   ("BAR=");
 * - keywords are the template's names and aliases, any case, whole words,
 *   a keyword with a value once (a switch may repeat);
 * - a value in double quotes may hold blanks, "=" and ";", and "*"
 *   escapes: *N a newline, *E an escape, *x the character x;
 * - /N values are decimal with an optional "-" (no "+"), at most 32 bits;
 * - anything else (an item that is no keyword, a keyword without its
 *   value, a value or "=" on its own) fails the whole column, which then
 *   keeps its defaults.
 *
 * s is the column, NUL-terminated and writable (the values are cut in it
 * and point into it). args[] as ReadArgs fills it: a /S TRUE, a /K a
 * string, a /N a pointer to its LONG in nums[]. Returns FALSE where
 * ReadArgs fails. */

/* The keywords, each followed by the argument it sets */
static const UBYTE list_format_keys[] =
    "DELTA\0" "\x00" "D\0" "\x00"
    "PREPARSE\0" "\x01" "P\0" "\x01"
    "WEIGHT\0" "\x02" "W\0" "\x02"
    "MINWIDTH\0" "\x03" "MIW\0" "\x03"
    "MAXWIDTH\0" "\x04" "MAW\0" "\x04"
    "COL\0" "\x05" "C\0" "\x05"
    "BAR\0" "\x06"
    "H\0" "\x07" "HIDDEN\0" "\x07"
    "SORTABLE\0" "\x08"
    "ORDER\0" "\x09"
    "ISOBJECT\0" "\x0a"
    "ELIDE\0" "\x0b" "E\0" "\x0b"
    "FIXWIDTH\0" "\x0c";

/* the switches (/S) and the numbers (/N) among the arguments */
#define LIST_FORMAT_SWITCHES ((1 << ARG_BAR) | (1 << ARG_HIDDEN) \
    | (1 << ARG_SORTABLE) | (1 << ARG_ISOBJECT))
#define LIST_FORMAT_NUMBERS ((1 << ARG_DELTA) | (1 << ARG_WEIGHT) \
    | (1 << ARG_COL) | (1 << ARG_FIXWIDTH))

static inline BOOL List_FormatBlank(UBYTE c)
{
    return c == ' ' || c == '\t';
}

/* The argument of keyword k (len characters), -1 for none */
static LONG List_FormatKey(CONST_STRPTR k, LONG len)
{
    const UBYTE *t = list_format_keys, *name;
    LONG i;

    while (*t)
    {
        for (name = t; *t; t++)
            ;
        if (t - name == len)
        {
            /* the names are capitals: a letter in either case */
            for (i = 0; i < len && !((k[i] ^ name[i]) & 0xdf); i++)
                ;
            if (i == len)
                return t[1];
        }
        t += 2;
    }
    return -1;
}

#define LIST_ITEM_END 0
#define LIST_ITEM_WORD 1
#define LIST_ITEM_QUOTED 2
#define LIST_ITEM_EQUAL 3
#define LIST_ITEM_BAD 4

/* The next item from *sp on. A word is cut where it ends; a "=" that ends
 * it is part of it. A quoted string is decoded in place. A newline or a
 * ";" outside quotes ends the column. */
static LONG List_FormatItem(STRPTR *sp, STRPTR *item)
{
    STRPTR s = *sp, d;
    UBYTE c;

    while (List_FormatBlank(*s))
        s++;
    c = *s;
    if (!c || c == '\n' || c == ';')
        return LIST_ITEM_END;
    *item = d = s + 1;
    if (c == '=')
    {
        *sp = d;
        return LIST_ITEM_EQUAL;
    }
    if (c == '"')
    {
        for (s = d; (c = *s++) != '"'; *d++ = c)
        {
            if (!c || c == '\n')
                return LIST_ITEM_BAD;   /* no closing quote */
            if (c == '*' && *s)
            {
                c = *s++;
                if ((c & 0xdf) == 'N')
                    c = '\n';
                else if ((c & 0xdf) == 'E')
                    c = 0x1b;
            }
        }
        *d = '\0';
        *sp = s;
        return LIST_ITEM_QUOTED;
    }
    *item = s;
    while ((c = *s) && !List_FormatBlank(c) && c != '=' && c != '\n'
        && c != ';')
        s++;
    /* a blank or "=" goes with the word; the end of the line stays */
    *s++ = '\0';
    /* a blank or "=" goes with the word; past a newline or ";" the
     * column ends */
    *sp = List_FormatBlank(c) || c == '=' ? s : (STRPTR)"";
    return LIST_ITEM_WORD;
}

static BOOL List_ReadColumn(STRPTR s, ULONG *args, LONG *nums)
{
    STRPTR item, value;
    LONG arg, type;
    ULONG done = 0;

    while ((type = List_FormatItem(&s, &item)) != LIST_ITEM_END)
    {
        if (type != LIST_ITEM_WORD
            || (arg = List_FormatKey(item, strlen(item))) < 0
            )
            return FALSE;
        if ((1 << arg) & LIST_FORMAT_SWITCHES)
        {
            args[arg] = TRUE;   /* a switch may come twice */
            continue;
        }
        if (done & (1 << arg))
            return FALSE;       /* a value twice fails */
        done |= 1 << arg;
        if ((type = List_FormatItem(&s, &value)) == LIST_ITEM_EQUAL)
            type = List_FormatItem(&s, &value);
        if (type != LIST_ITEM_WORD && type != LIST_ITEM_QUOTED)
            return FALSE;
        args[arg] = (ULONG)value;
        if ((1 << arg) & LIST_FORMAT_NUMBERS)
        {
            ULONG n = 0, m;
            BOOL minus = *value == '-';

            if (minus)
                value++;
            if (!*value)
                return FALSE;
            for (; *value; value++)
            {
                if (*value < '0' || *value > '9')
                    return FALSE;
                m = n * 10 + *value - '0';
                if (n > 0x19999999 || m < n * 10)
                    return FALSE;       /* past 32 bits */
                n = m;
            }
            *nums = minus ? -(LONG)n : (LONG)n;
            args[arg] = (ULONG)nums++;
        }
    }
    return TRUE;
}


static const char *kinds = "NKNKKNSSSKSKN";     /* per argument */

struct Result
{
    BOOL ok;
    LONG num[ARG_CNT];
    BOOL set[ARG_CNT];
    char str[ARG_CNT][130];
};

static struct RDArgs *rdargs;

/* as list.c did: the column (at most 126 characters) and a newline */
static void by_readargs(const char *col, struct Result *r)
{
    char line[128];
    ULONG args[ARG_CNT];
    LONG len = strlen(col), i;

    memset(r, 0, sizeof(*r));
    if (len > (LONG)sizeof(line) - 2)
        len = sizeof(line) - 2;
    memcpy(line, col, len);
    line[len++] = '\n';
    line[len] = '\0';
    rdargs->RDA_Source.CS_Buffer = line;
    rdargs->RDA_Source.CS_Length = len;
    rdargs->RDA_Source.CS_CurChr = 0;
    rdargs->RDA_DAList = 0;
    rdargs->RDA_Buffer = NULL;
    rdargs->RDA_BufSiz = 0;
    rdargs->RDA_ExtHelp = NULL;
    rdargs->RDA_Flags = RDAF_NOPROMPT;
    memset(args, 0, sizeof args);
    if (!ReadArgs(FORMAT_TEMPLATE, (LONG *)args, rdargs))
        return;
    r->ok = TRUE;
    for (i = 0; i < ARG_CNT; i++)
    {
        r->set[i] = args[i] != 0;
        if (!args[i])
            continue;
        if (kinds[i] == 'N')
            r->num[i] = *(LONG *)args[i];
        else if (kinds[i] == 'K')
            strncpy(r->str[i], (char *)args[i], 129);
    }
    FreeArgs(rdargs);
}

static void by_list(const char *col, struct Result *r)
{
    char buf[130];
    ULONG args[ARG_CNT];
    LONG nums[ARG_CNT], len = strlen(col), i;

    memset(r, 0, sizeof(*r));
    if (len > 126)
        len = 126;
    memcpy(buf, col, len);
    buf[len] = '\0';
    memset(args, 0, sizeof args);
    if (!List_ReadColumn(buf, args, nums))
        return;
    r->ok = TRUE;
    for (i = 0; i < ARG_CNT; i++)
    {
        r->set[i] = args[i] != 0;
        if (!args[i])
            continue;
        if (kinds[i] == 'N')
            r->num[i] = *(LONG *)args[i];
        else if (kinds[i] == 'K')
            strncpy(r->str[i], (char *)args[i], 129);
    }
}

static void show(const char *col)
{
    const unsigned char *p;
    putchar('[');
    for (p = (const unsigned char *)col; *p; p++)
        if (*p < 32 || *p > 126 || *p == '\\')
            printf("\\%02x", *p);
        else
            putchar(*p);
    putchar(']');
}

static LONG checked, differ, accepted;

static void check(const char *col)
{
    static struct Result a, b;
    LONG i;
    BOOL same;

    by_readargs(col, &a);
    by_list(col, &b);
    checked++;
    same = a.ok == b.ok;
    if (same && a.ok)
    {
        accepted++;
        for (i = 0; i < ARG_CNT; i++)
            if (a.set[i] != b.set[i] || a.num[i] != b.num[i]
                || strcmp(a.str[i], b.str[i]))
                same = FALSE;
    }
    if (same)
        return;
    if (++differ > 60)
        return;
    show(col);
    printf(" ReadArgs %s, list %s", a.ok ? "ok" : "fails", b.ok ? "ok" : "fails");
    if (a.ok && b.ok)
        for (i = 0; i < ARG_CNT; i++)
            if (a.set[i] != b.set[i] || a.num[i] != b.num[i]
                || strcmp(a.str[i], b.str[i]))
            {
                printf(" arg%ld:", (long)i);
                if (kinds[i] == 'N')
                    printf("%ld/%ld", (long)a.num[i], (long)b.num[i]);
                else if (kinds[i] == 'K')
                    show(a.str[i]), putchar('/'), show(b.str[i]);
                else
                    printf("%d/%d", a.set[i], b.set[i]);
            }
    putchar('\n');
}

static const char *fixed[] = {
    "", " ", "BAR", "bar", "Bar", "BAR ", " BAR", "BAR\t", "BA", "BARR",
    "P=\33c", "P=\33r", "P=\33c\33b", "p=\33c", "PREPARSE=\33c", "P \33c",
    "P = \33c", "P= \33c", "P =\33c", "P", "P=", "P= ", "P=\"\"", "P=\"a b\"",
    "P=\"a=b\"", "P=a=b", "P=a b", "P=\"*N\"", "P=\"*E[1m\"", "P=\"**\"",
    "P=\"*\"\"", "P=\"x*\"", "P=\"abc", "P=\"a\"b", "\"BAR\"", "=BAR", "BAR=",
    "BAR=1", "BAR BAR", "P=x P=y", "W=10 W=20", "D=8", "D=-1", "D=+5", "D=5x",
    "D=x5", "D=", "D=\"7\"", "D= 7", "D=007", "D=-", "D=+", "D=--1",
    "D=2147483647", "D=2147483648", "D=99999999999", "D=-2147483648",
    "W=-1", "W=0", "W=100", "W=50 BAR", "MIW=10", "MIW=10px", "MAW=-1",
    "MAW=50", "MIW=50 MAW=80", "COL=0", "COL=1", "C=2", "COL=-1", "COL=300",
    "H", "HIDDEN", "hidden", "SORTABLE", "ORDER=DESC", "ORDER=asc",
    "ISOBJECT", "ELIDE=left", "E=right", "FIXWIDTH=20", "FIXWIDTH=x",
    "BAR P=\33c W=-1", "P=\33c BAR", "BAR,", "BAR;x", "BAR ;x", ";", "x",
    "BAR x", "\tBAR\t", "BAR\n", "P=\33\n", "P=a\nb", "\33c", "P=\33c\33l",
    "DELTA=8 PREPARSE=\33r WEIGHT=50 MINWIDTH=10 MAXWIDTH=90 COL=1 BAR",
    "P=\"\33c\" BAR", "P=\"*e[2m\"", "P=*N", "P=\"*n\"", "P=\"*x\"",
    "P==x", "P = = x", "D==3", "P=\x9b" "1m", "P=\xa0x", "\xa0" "BAR",
    "BAR\xa0", "BAR\x01", "BAR\x1b", "P=\x7f", "BAR\x0b", "BAR\x0c",
    "BAR\x0d", "BAR \x0d", "P=a\x0d", NULL
};

static unsigned long seed = 68;
static LONG rnd(LONG n)
{
    seed = seed * 1103515245UL + 12345UL;
    return (LONG)((seed >> 16) % (unsigned long)n);
}

static const char *pieces[] = {
    "DELTA", "D", "PREPARSE", "P", "WEIGHT", "W", "MINWIDTH", "MIW",
    "MAXWIDTH", "MAW", "COL", "C", "BAR", "H", "HIDDEN", "SORTABLE", "ORDER",
    "ISOBJECT", "ELIDE", "E", "FIXWIDTH", "bar", "p", "w", "col", "miw",
    "=", "=", "=", " ", " ", " ", "\t", "\"", "\"", "*", "*N", "*E", "**",
    "*\"", "0", "1", "7", "12", "100", "-", "+", "-1", "x", "px", "\33c",
    "\33r", "\33b", "abc", "DESC", "left", ";", "\x9b", "\xe4", "\x0d",
    "\x0b", "\x01", "%", "#",
};

int main(int argc, char **argv)
{
    LONG count = argc > 1 ? atol(argv[1]) : 20000, i, j, n;
    char col[200];

    if (argc > 2)
        seed = atol(argv[2]);
    if (!(rdargs = AllocDosObject(DOS_RDARGS, NULL)))
        return 20;
    for (i = 0; fixed[i]; i++)
        check(fixed[i]);
    printf("fixed: %ld columns, %ld differ\n", (long)checked, (long)differ);
    for (i = 0; i < count; i++)
    {
        col[0] = '\0';
        n = 1 + rnd(8);
        for (j = 0; j < n; j++)
        {
            const char *p = pieces[rnd(sizeof(pieces) / sizeof(pieces[0]))];
            if (strlen(col) + strlen(p) < sizeof(col) - 1)
                strcat(col, p);
        }
        check(col);
    }
    FreeDosObject(DOS_RDARGS, rdargs);
    printf("all: %ld columns, %ld accepted by ReadArgs or both, %ld differ\n",
        (long)checked, (long)accepted, (long)differ);
    return 0;
}
