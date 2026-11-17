/*
    Copyright (C) 2002-2026, The AROS Development Team.
    Copyright (C) 1999, David Le Corfec.
    All rights reserved.

*/

#include <stdlib.h>
#include <string.h>

#include <exec/types.h>
#include <exec/memory.h>

#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/utility.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/cybergraphics.h>

#include "mui.h"
#include "textengine.h"
#include "imspec.h"
#include "support.h"
#include "listimage.h"
#include "modules.h"

#include "muimaster_intern.h"

extern struct Library *MUIMasterBase;

/**************************************************************************
 MUI's text engine, rewritten for the 68000 (2026-11, rw-draw).

 A text is parsed into lines of chunks: runs of characters in one style,
 an image (\33I[spec]), a list image (\33O[ptr]) or an alpha picture
 (\33A[ptr]). The bounds give each chunk its size, each line the sum of
 its chunks' widths and the largest height, the text the widest line and
 the sum of the heights. Drawing walks the lines and chunks with x and y
 advanced by those sizes.

 The common case first: a text without format codes ("plain": one line,
 one chunk, left aligned) is copied with its line and chunk in one
 allocation, measured with one TextLength and drawn with one Text. Every
 other text's lines and chunks come from 192 bytes inline after the ZText,
 so a parse usually takes one allocation; lists re-parse their entries on
 every draw. MUI 4/5's colours and effects are drawn by the module Zmui45
 (texteffect45.c), which a MUI 3 program never needs.
**************************************************************************/

/* NewList and AddTail without the library calls: a list row's parse
 * builds a dozen nodes */
static inline void zt_newlist(struct MinList *list)
{
    list->mlh_Head = (struct MinNode *)&list->mlh_Tail;
    list->mlh_Tail = NULL;
    list->mlh_TailPred = (struct MinNode *)&list->mlh_Head;
}

static inline void zt_addtail(struct MinList *list, struct MinNode *node)
{
    node->mln_Succ = (struct MinNode *)&list->mlh_Tail;
    node->mln_Pred = list->mlh_TailPred;
    list->mlh_TailPred->mln_Succ = node;
    list->mlh_TailPred = node;
}

struct zune_context
{
    ZText *ztext;               /* owner of new lines and chunks */
    LONG dripen;
    LONG pen;
    ULONG style;
    LONG backpen;
    WORD object_width;
    UBYTE effects;
    UBYTE align;

    ZTextLine *line;
    CONST_STRPTR text_start;
    CONST_STRPTR text;
    CONST_STRPTR imspec;
    Object *obj;                /* Area subclass, see List_CreateImage */
    struct MUI_AlphaData *alpha_data;
};

#define ZTEXT_ARENA_SIZE 192

/* size bytes of cleared memory: from the arena while it lasts */
static APTR ztext_alloc(ZText *text, ULONG size)
{
    APTR mem;

    size = (size + 3) & ~3UL;
    if (text->arena_next + size <= text->arena_end)
    {
        mem = text->arena_next;
        text->arena_next += size;
        memset(mem, 0, size);
        return mem;
    }
    return mui_alloc(size);
}

static void ztext_free(ZText *text, APTR mem)
{
    if ((UBYTE *)mem < (UBYTE *)(text + 1) || (UBYTE *)mem >= text->arena_end)
        mui_free(mem);
}

/* Make text a plain text with its one line and chunk, the chunk's string
 * not set (set_plain_parts' callers do) */
static __attribute__((noinline)) void set_plain_parts(ZText *text,
    ZTextLine *line, ZTextChunk *chunk)
{
    zt_newlist(&text->lines);
    zt_newlist(&line->chunklist);
    zt_addtail(&text->lines, (struct MinNode *)line);
    line->align = ZTL_LEFT;
    line->lshift = 0;
    if (chunk)
    {
        zt_addtail(&line->chunklist, (struct MinNode *)chunk);
        chunk->pen = chunk->dripen = -1;
        chunk->backpen = 0xff000000;
    }
}

/**************************************************************************
 A ZText in the caller's storage for a plain content (no preparse, no
 escape, newline or tab, at most 32767 characters): FALSE otherwise.
**************************************************************************/
BOOL zune_text_init_plain(ZText *text, ZTextLine *line, ZTextChunk *chunk,
    CONST_STRPTR preparse, CONST_STRPTR content)
{
    ULONG len = 0;
    UBYTE c;

    if ((preparse && *preparse) || !content || !*content)
        return FALSE;
    while ((c = content[len]))
    {
        if ((c < 32 && (c == '\33' || c == '\n' || c == '\t'))
            || len == 32767)
            return FALSE;
        len++;
    }
    memset(text, 0, sizeof(*text));
    memset(line, 0, sizeof(*line));
    memset(chunk, 0, sizeof(*chunk));
    set_plain_parts(text, line, chunk);
    chunk->str = (char *)content;
    chunk->clen = len;
    text->plain = TRUE;
    return TRUE;
}

/************************************************************/
/* Parsing */
/************************************************************/

/**************************************************************************
 Close the chunk the context has gathered (text from text_start to text,
 or an image, list image or alpha picture) and add it to the line.
**************************************************************************/
static void zune_text_chunk_new(struct zune_context *zc)
{
    ZTextChunk *ztc;

    /* No char has been processed so we needn't allocate anything */
    if (zc->text == zc->text_start)
        return;
    /* A text chunk carries its string in the same allocation */
    if (!zc->imspec && !zc->obj && !zc->alpha_data)
    {
        LONG len = zc->text - zc->text_start;

        if (!(ztc = ztext_alloc(zc->ztext, sizeof(ZTextChunk) + len + 1)))
            return;
        ztc->str = (char *)(ztc + 1);
        ztc->clen = len;
        memcpy(ztc->str, zc->text_start, len);
        ztc->str[len] = 0;
        ztc->backpen = zc->backpen;
    }
    else if (!(ztc = ztext_alloc(zc->ztext, sizeof(ZTextChunk))))
        return;
    ztc->style = zc->style;
    ztc->effects = zc->effects;
    ztc->dripen = zc->dripen;
    ztc->pen = zc->pen;
    if (zc->imspec)
    {
        ztc->spec = zc->imspec;
        zc->imspec = NULL;
    }
    else if (zc->obj)
    {
        ztc->obj = zc->obj;
        ztc->clen = zc->object_width;
        zc->object_width = 0;
        zc->obj = NULL;
    }
    else if (zc->alpha_data)
    {
        ztc->alpha_data = zc->alpha_data;
        zc->alpha_data = NULL;
    }
    zt_addtail(&zc->line->chunklist, (struct MinNode *)ztc);
}

/* "\33P[...]" and "\33p[...]": the text pen or the background as a pen
 * number or RRGGBB (MUI 4/5 also AARRGGBB, \33P[AA------] the alpha) */
static void parse_pen(struct zune_context *zc, char c, char *s, char *end)
{
    ULONG length = end - s - 1;
    LONG pen = 0;
    BOOL alpha_only = c == 'P' && length == 8 && !strncmp(s + 3, "------", 6);
    BOOL rgb = !alpha_only && (length == 6 || length == 8);
    ULONG alpha = 255;
    IPTR value;

    *end = 0;
    if (alpha_only)
    {
        char hex[3] = {s[1], s[2], 0};

        if (HexToIPTR(hex, &value) >= 0)
            alpha = value;
    }
    else if (rgb)
    {
        if (HexToIPTR(s + 1, &value) >= 0)
        {
            pen = value & 0xffffff;
            if (length == 8)
                alpha = value >> 24;
        }
    }
    else if (StrToLong(s + 1, &pen) < 0)
        pen = 0;
    *end = ']';
    if (c == 'P')
    {
        if (!alpha_only)
        {
            zc->pen = pen;
            zc->dripen = -1;
            zc->effects = (zc->effects & ~ZTEXT_RGBPEN) | (rgb ? ZTEXT_RGBPEN : 0);
        }
        zc->backpen = (zc->backpen & 0xffffff) | (alpha << 24);
        zc->effects = (zc->effects & ~ZTEXT_ALPHA) | (alpha != 255 ? ZTEXT_ALPHA : 0);
    }
    else
    {
        zc->backpen = (zc->backpen & 0xff000000) | (pen & 0xffffff);
        zc->effects = (zc->effects & ~ZTEXT_RGBBACK) | (rgb ? ZTEXT_RGBBACK : 0);
    }
}

/* One letter's style or effect bit: set (lower case) or cleared */
static const struct { char c; UBYTE effect; UBYTE bits; } zt_codes[] = {
    {'U', 0, FSF_UNDERLINED}, {'u', 0, FSF_UNDERLINED}, {'b', 0, FSF_BOLD},
    {'i', 0, FSF_ITALIC},
    {'t', 0, ZTEXT_STYLE_OUTLINE}, {'T', 0, ZTEXT_STYLE_OUTLINE},
    {'s', 0, ZTEXT_STYLE_SHADOW}, {'S', 0, ZTEXT_STYLE_SHADOW},
    {'g', 0, ZTEXT_STYLE_GHOST}, {'G', 0, ZTEXT_STYLE_GHOST},
    {'B', 1, ZTEXT_JAM2}, {'F', 1, ZTEXT_JAM2},
    {'V', 1, ZTEXT_INVERSE}, {'v', 1, ZTEXT_INVERSE},
    {'h', 1, ZTEXT_HIDEIMAGES}, {'H', 1, ZTEXT_HIDEIMAGES},
};
/* which of those set their bit (the others clear it) */
#define ZT_SETS "ubitsgBVh"

/**************************************************************************
 An escape code: s points at the letter after the ESC. Returns where the
 text goes on. Only alignments at the beginning of a line affect it
 (tested in MUI).
**************************************************************************/
static CONST_STRPTR parse_escape_code(ZTextLine *ztl,
    struct zune_context *zc, CONST_STRPTR s)
{
    UBYTE c = *s;
    char *t;
    int i;

    zune_text_chunk_new(zc);
    s++;                        /* s now points after the command */
    zc->text_start = zc->text = s;

    switch (c)
    {
    case 'c':
        zc->align = ztl->align = ZTL_CENTER;
        return s;
    case 'r':
        zc->align = ztl->align = ZTL_RIGHT;
        return s;
    case 'l':
        zc->align = ztl->align = ZTL_LEFT;
        return s;
    case 'n':
        zc->style = FS_NORMAL;
        return s;
    case 'I':                  /* image spec */
    case 'O':                  /* pointer from List_CreateImage */
    case 'A':                  /* pointer to MUI_AlphaData */
    case 'p':
    case 'P':
        if (*s != '[' || !(t = strchr(s + 1, ']')))
            return s;
        if (c == 'p' || c == 'P')
        {
            parse_pen(zc, c, (char *)s, t);
            zc->text = zc->text_start = t + 1;
            return t + 1;
        }
        *t = 0;
        if (c == 'I')
        {
            char *state = strstr(s + 1, ",STATE=");

            if (state)
                *state = 0;
            zc->imspec = StrDup(s + 1);
            if (state)
            {
                if (atoi(state + 7))
                    zc->effects |= ZTEXT_SELECTED;
                *state = ',';
            }
            *t = ']';
            zc->text = t;
            if (zc->imspec)
                zune_text_chunk_new(zc);
            zc->effects &= ~ZTEXT_SELECTED;
        }
        else
        {
            IPTR tmp;

            /* the hex-encoded pointer, "$" optional; a NULL one draws
             * nothing (MUI's autodocs for CreateImage; MUI 4 for alpha) */
            if (HexToIPTR(s[1] == '$' ? s + 2 : s + 1, &tmp) != -1)
            {
                if (tmp == 0)
                {
                    *t = ']';
                    zc->text = zc->text_start = t + 1;
                    return t + 1;
                }
                if (c == 'O')
                {
                    char *comma = strchr(s + 1, ',');

                    zc->obj = ((struct ListImage *)tmp)->obj;
                    if (comma)
                        zc->object_width = MAX(0, MIN(32767, atoi(comma + 1)));
                }
                else
                    zc->alpha_data = (struct MUI_AlphaData *)tmp;
            }
            *t = ']';
            zc->text = t;
            zune_text_chunk_new(zc);
        }
        zc->text = zc->text_start = t + 1;
        return t + 1;
    case '-':                  /* the rest of the line is not parsed */
        while (*s && *s != '\n')
            s++;
        zc->text = s;
        return s;
    }
    if (c >= '0' && c <= '9')   /* pen */
    {
        zc->dripen = c - '0';
        zc->effects &= ~ZTEXT_RGBPEN;
        return s;
    }
    for (i = 0; i < sizeof(zt_codes) / sizeof(zt_codes[0]); i++)
        if (zt_codes[i].c == c)
        {
            UBYTE bits = zt_codes[i].bits;

            if (zt_codes[i].effect)
                zc->effects = strchr(ZT_SETS, c) ? zc->effects | bits
                    : zc->effects & ~bits;
            else
                zc->style = strchr(ZT_SETS, c) ? zc->style | bits
                    : zc->style & ~bits;
            break;
        }
    return s;
}

/**************************************************************************
 Parse a line into chunks: an escape code ends the current chunk and
 changes the context for the next. *s_ptr moves to the '\n' or the end.
 The string is the parser's own copy: image specs are cut in it.
**************************************************************************/
static ZTextLine *zune_text_parse_line(CONST_STRPTR *s_ptr,
    struct zune_context *zc, int *argtype, int arg)
{
    CONST_STRPTR s = *s_ptr;
    ZTextLine *ztl;
    UBYTE c;

    if (!(ztl = ztext_alloc(zc->ztext, sizeof(ZTextLine))))
        return NULL;
    zt_newlist(&ztl->chunklist);
    ztl->align = zc->align;
    zc->text_start = zc->text = s;
    zc->line = ztl;

    while ((c = *s))
    {
        if (c < 32)
        {
            if (c == '\n')
                break;
            if (c == '\33')
            {
                if (!*++s)
                    break;
                s = parse_escape_code(ztl, zc, s);
                continue;
            }
            if (c == '\t')
            {
                zune_text_chunk_new(zc);
                zc->text_start = "    ";
                zc->text = zc->text_start + 4;
                zune_text_chunk_new(zc);
                zc->text_start = zc->text = ++s;
                continue;
            }
        }
        if (*argtype != ZTEXT_ARG_NONE
            && (*argtype == ZTEXT_ARG_HICHAR ? (arg & 0xff) == c
                || ((arg >> 8) & 0xff) == c : arg == c))
        {
            ULONG styleback = zc->style;

            /* HiChar: that letter as a chunk of its own, underlined as
             * MUI 3.9 does (ZTEXT_STYLE_HICHAR); HiCharIdx: the next
             * letter, in the underlined soft style (tests/native/
             * ulprobe.c) */
            zune_text_chunk_new(zc);
            if (*argtype == ZTEXT_ARG_HICHAR)
            {
                zc->style |= ZTEXT_STYLE_HICHAR;
                zc->text_start = s;
                zc->text = ++s;
                zune_text_chunk_new(zc);
                zc->text_start = s;
                zc->style = styleback;
                *argtype = ZTEXT_ARG_NONE;
                continue;
            }
            zc->style |= FSF_UNDERLINED;
            zc->text_start = ++s;
            zc->text = ++s;
            zune_text_chunk_new(zc);
            zc->text_start = s;
            zc->style = styleback;
            *argtype = ZTEXT_ARG_NONE;
        }
        zc->text = ++s;
    }
    zune_text_chunk_new(zc);
    *s_ptr = s;
    return ztl;
}

ZText *zune_text_new(CONST_STRPTR preparse, CONST_STRPTR content,
    int argtype, TEXT argbyte)
{
    ZText *text;
    STRPTR dup_content;
    char local_content[128];
    CONST_STRPTR buf;
    struct zune_context zc;
    ULONG preparse_len, content_len = 0, arena_size = ZTEXT_ARENA_SIZE;
    BOOL plain;
    int arg;

    if (!content)
        content = "";
    preparse_len = preparse ? strlen(preparse) : 0;
    plain = !preparse_len && argtype == ZTEXT_ARG_NONE;
    if (plain)
    {
        const UBYTE *p;

        for (p = (const UBYTE *)content; *p; p++, content_len++)
            if (*p < 32 && (*p == '\33' || *p == '\n' || *p == '\t'))
                plain = FALSE;
    }
    else
        content_len = strlen(content);
    if (content_len > 32767)
        plain = FALSE;
    if (plain)
        arena_size = ((sizeof(ZTextLine) + 3) & ~3UL) +
            (content_len ? (sizeof(ZTextChunk) + content_len + 4) & ~3UL : 0);

    if (!(text = AllocVec(sizeof(ZText) + arena_size, MEMF_ANY)))
        return NULL;
    text->width = text->height = 0;
    text->styles = text->nontext = text->truncate = 0;
    text->plain = plain && content_len;
    text->arena_next = (UBYTE *)(text + 1);
    text->arena_end = text->arena_next + arena_size;

    if (plain)
    {
        ZTextLine *line = ztext_alloc(text, sizeof(*line));
        ZTextChunk *chunk = NULL;

        if (content_len)
        {
            chunk = ztext_alloc(text, sizeof(*chunk) + content_len + 1);
            chunk->str = (char *)(chunk + 1);
            chunk->clen = content_len;
            memcpy(chunk->str, content, content_len + 1);
        }
        set_plain_parts(text, line, chunk);
        return text;
    }
    zt_newlist(&text->lines);

    /* The parser cuts its own copy; short ones on the stack */
    if (preparse_len + content_len < sizeof(local_content))
        dup_content = (STRPTR)local_content;
    else if (!(dup_content = mui_alloc(preparse_len + content_len + 1)))
    {
        mui_free(text);
        return NULL;
    }
    if (preparse_len)
        memcpy(dup_content, preparse, preparse_len);
    strcpy(&dup_content[preparse_len], content);
    buf = dup_content;

    memset(&zc, 0, sizeof(zc));
    zc.ztext = text;
    zc.pen = zc.dripen = -1;
    zc.backpen = 0xff000000;
    zc.align = ZTL_LEFT;
    /* HiChar: lower and upper case in arg */
    arg = argtype == ZTEXT_ARG_HICHAR
        ? ToLower(argbyte) | (ToUpper(argbyte) << 8) : argbyte;

    for (;;)
    {
        struct ZTextLine *ztl = zune_text_parse_line(&buf, &zc, &argtype, arg);

        if (!ztl)
            break;
        zt_addtail(&text->lines, (struct MinNode *)ztl);
        if (*buf != '\n')
            break;
        buf++;
    }
    if (dup_content != (STRPTR)local_content)
        mui_free(dup_content);
    return text;
}

/**************************************************************************
 A parse that is kept (a Text's, from its setup until its cleanup) used a
 part of its 192-byte arena: a button title with a shortcut ("_Ok": a line
 and two chunks) about 120 bytes, a centred word 80. Copy the header and
 the used arena into a block of their size and move every pointer into the
 old block (the list links, the inline chunk strings) by the distance;
 lines and chunks outside the arena stay where they are.
**************************************************************************/
#define ZT_MOVED(p) (((UBYTE *)(p) >= lo && (UBYTE *)(p) < hi) \
    ? (APTR)((UBYTE *)(p) + d) : (APTR)(p))
ZText *zune_text_compact(ZText *text)
{
    UBYTE *lo, *hi;
    ULONG used;
    LONG d;
    ZText *copy;
    struct MinNode *ln, *cn;
    struct MinList *chunks;

    if (!text || text->arena_end - text->arena_next < 16)
        return text;
    lo = (UBYTE *)text;
    hi = text->arena_end;
    used = text->arena_next - lo;
    if (!(copy = AllocVec(used, MEMF_ANY)))
        return text;
    CopyMem(text, copy, used);
    d = (UBYTE *)copy - lo;
    copy->lines.mlh_Head = ZT_MOVED(copy->lines.mlh_Head);
    copy->lines.mlh_TailPred = ZT_MOVED(copy->lines.mlh_TailPred);
    for (ln = copy->lines.mlh_Head; ln->mln_Succ; ln = ln->mln_Succ)
    {
        ln->mln_Succ = ZT_MOVED(ln->mln_Succ);
        ln->mln_Pred = ZT_MOVED(ln->mln_Pred);
        chunks = &((ZTextLine *)ln)->chunklist;
        chunks->mlh_Head = ZT_MOVED(chunks->mlh_Head);
        chunks->mlh_TailPred = ZT_MOVED(chunks->mlh_TailPred);
        for (cn = chunks->mlh_Head; cn->mln_Succ; cn = cn->mln_Succ)
        {
            cn->mln_Succ = ZT_MOVED(cn->mln_Succ);
            cn->mln_Pred = ZT_MOVED(cn->mln_Pred);
            ((ZTextChunk *)cn)->str = ZT_MOVED(((ZTextChunk *)cn)->str);
        }
    }
    copy->arena_next = copy->arena_end = (UBYTE *)copy + used;
    FreeVec(text);
    return copy;
}
#undef ZT_MOVED

/**************************************************************************
 If a ZText holds nothing but the plain string s (one line, one text
 chunk), return that chunk, so that zune_text_set_plain() can give it a
 new plain string without parsing; otherwise NULL.
**************************************************************************/
ZTextChunk *zune_text_plain_chunk(ZText *text, CONST_STRPTR s)
{
    ZTextLine *line;
    ZTextChunk *chunk;

    if (!text || !s || !*s)
        return NULL;
    line = (ZTextLine *)text->lines.mlh_Head;
    if (!line->node.mln_Succ || line->node.mln_Succ->mln_Succ)
        return NULL;
    chunk = (ZTextChunk *)line->chunklist.mlh_Head;
    if (!chunk->node.mln_Succ || chunk->node.mln_Succ->mln_Succ
        || !chunk->str || chunk->spec || chunk->obj
        || strcmp(chunk->str, s))
        return NULL;
    return chunk;
}

/**************************************************************************
 Completely frees a ZText. The lists are not unlinked node by node: the
 nodes are only freed (those outside the arena) and the ZText with them.
**************************************************************************/
void zune_text_destroy(ZText *text)
{
    struct MinNode *ln, *lnext, *cn, *cnext;

    for (ln = text->lines.mlh_Head; (lnext = ln->mln_Succ); ln = lnext)
    {
        for (cn = ((ZTextLine *)ln)->chunklist.mlh_Head;
            (cnext = cn->mln_Succ); cn = cnext)
        {
            ZTextChunk *ztc = (ZTextChunk *)cn;

            /* String chunks hold their text inline (zune_text_chunk_new) */
            if (ztc->spec)
            {
                FreeVec((APTR)ztc->spec);
                if (ztc->image)
                    zune_imspec_cleanup(ztc->image);
            }
            ztext_free(text, ztc);
        }
        ztext_free(text, ln);
    }
    mui_free(text);
}

/************************************************************/
/* Bounds */
/************************************************************/

/* The extent of a chunk in its bold or italic soft style, on the copy of
 * the RastPort (made now if *rpp is not the copy yet: the style must not
 * touch the object's own) */
static __attribute__((noinline)) struct RastPort *style_extent(
    struct RastPort *rp, struct RastPort *copy, ZTextChunk *chunk,
    struct TextExtent *te)
{
    if (rp != copy)
    {
        *copy = *rp;
        rp = copy;
    }
    SetSoftStyle(rp, chunk->style & ~ZTEXT_STYLE_HICHAR, AskSoftStyle(rp));
    TextExtent(rp, chunk->str, chunk->clen, te);
    SetSoftStyle(rp, FS_NORMAL, AskSoftStyle(rp));
    return rp;
}

#define TE_OVERHANG(te) ((te).te_Extent.MaxX - (te).te_Extent.MinX + 1 \
    - (te).te_Width)

void zune_text_get_bounds(ZText *text, Object *obj)
{
    struct RastPort copy, *rp = &copy;
    struct TextFont *font;
    struct TextExtent te;
    ZTextLine *line_node;
    ZTextChunk *chunk_node;
    WORD base_width;

    if (!text || !obj)
        return;

    text->width = 0;
    text->height = 0;
    text->styles = 0;
    text->nontext = 0;

    font = _font(obj);
    /* Measuring needs a RastPort with the object's font in the plain
     * style. TextLength and TextExtent only read it, so the object's own
     * RastPort serves as it is when its font and style are right; a
     * copy (100 bytes) is made only to change them. Bold and italic
     * chunks below set the style on it, so they take the copy too. */
    if ((_flags(obj) & MADF_SETUP) && muiRenderInfo(obj)
        && muiRenderInfo(obj)->mri_RastPort)
    {
        if (_rp(obj)->Font == font && !_rp(obj)->AlgoStyle)
            rp = _rp(obj);
        else
        {
            copy = *_rp(obj);
            if (copy.Font != font)
                SetFont(&copy, font);
            /* Measure with the plain style, as a fresh RastPort would */
            if (copy.AlgoStyle)
                SetSoftStyle(&copy, FS_NORMAL, AskSoftStyle(&copy));
        }
    }
    else
    {
        zune_init_rastport(&copy);
        SetFont(&copy, font);
    }

    for (line_node = (ZTextLine *)text->lines.mlh_Head;
        line_node->node.mln_Succ;
        line_node = (ZTextLine *)line_node->node.mln_Succ)
    {
        line_node->lheight = font->tf_YSize;
        line_node->lwidth = 0;
        line_node->centreskip = 0;
        line_node->lshift = 0;

        for (chunk_node = (ZTextChunk *)line_node->chunklist.mlh_Head;
            chunk_node->node.mln_Succ;
            chunk_node = (ZTextChunk *)chunk_node->node.mln_Succ)
        {
            if (chunk_node->str)
            {
                LONG extra = (chunk_node->style & ZTEXT_STYLE_OUTLINE) ? 2 :
                    (chunk_node->style & (ZTEXT_STYLE_SHADOW
                    | ZTEXT_STYLE_GHOST)) ? 1 : 0;

                chunk_node->cheight = font->tf_YSize + extra;
                chunk_node->cwidth = TextLength(rp, chunk_node->str,
                    chunk_node->clen) + extra;
                text->styles |= chunk_node->style;
            }
            else
            {
                text->nontext = 1;
                if (chunk_node->spec)
                {
                    struct MUI_MinMax minmax;

                    chunk_node->image = zune_imspec_setup(
                        (IPTR)chunk_node->spec, muiRenderInfo(obj));
                    if (!chunk_node->image)
                        return;
                    zune_imspec_askminmax(chunk_node->image, &minmax);
                    chunk_node->cwidth = minmax.DefWidth;
                    chunk_node->cheight = minmax.DefHeight;
                }
                else if (chunk_node->obj)
                {
                    struct MUI_MinMax MinMax;
                    struct MUIP_AskMinMax amm = { MUIM_AskMinMax, &MinMax };

                    /* Area points the message at the object's own minmax
                     * and __area_finish_minmax updates it in place. */
                    DoMethodA(chunk_node->obj, (Msg)&amm);
                    __area_finish_minmax(chunk_node->obj, amm.MinMaxInfo);
                    chunk_node->cwidth = chunk_node->clen ? chunk_node->clen
                        : amm.MinMaxInfo->MinWidth;
                    chunk_node->cheight = amm.MinMaxInfo->DefHeight;
                }
                else if (chunk_node->alpha_data)
                {
                    chunk_node->cwidth = chunk_node->alpha_data->width;
                    chunk_node->cheight = chunk_node->alpha_data->height;
                }
            }
            line_node->lheight = MAX(line_node->lheight, chunk_node->cheight);
            line_node->lwidth += chunk_node->cwidth;
        }

        /* MUI 3.9 adds the overhang of a bold or italic line end, the
         * part of its TextExtent beyond the advance (tests/native/
         * textstyles.c) */
        base_width = line_node->lwidth;
        chunk_node = (ZTextChunk *)line_node->chunklist.mlh_TailPred;
        while (chunk_node->node.mln_Pred && !chunk_node->str)
            chunk_node = (ZTextChunk *)chunk_node->node.mln_Pred;
        if (chunk_node->node.mln_Pred && chunk_node->str
            && (chunk_node->style & (FSF_BOLD | FSF_ITALIC)))
        {
            rp = style_extent(rp, &copy, chunk_node, &te);
            line_node->centreskip = TE_OVERHANG(te);
            line_node->lwidth += line_node->centreskip;
            /* a line in one style is centred on its advance alone, a
             * line ending in another style with the overhang
             * (tests/native/centrestyle.c) */
            if (chunk_node->node.mln_Pred->mln_Pred)
                line_node->centreskip = 0;
        }
        /* MUI 3.8 centres a line with bold or italic parts on its advance
         * plus every such part's overhang plus one (demopix: MUI-Demo's
         * title, three bold letters, sits two pixels left of MUI 3.9's
         * place); the sizes stay as above */
        if ((text->styles & (FSF_BOLD | FSF_ITALIC))
            && (_flags(obj) & MADF_SETUP) && ZUNE_APP_MUI38(obj))
        {
            LONG extra = 0;
            BOOL styled = FALSE;

            for (chunk_node = (ZTextChunk *)line_node->chunklist.mlh_Head;
                chunk_node->node.mln_Succ;
                chunk_node = (ZTextChunk *)chunk_node->node.mln_Succ)
            {
                if (!chunk_node->str
                    || !(chunk_node->style & (FSF_BOLD | FSF_ITALIC)))
                    continue;
                rp = style_extent(rp, &copy, chunk_node, &te);
                extra += TE_OVERHANG(te);
                styled = TRUE;
            }
            if (styled)
                line_node->centreskip = line_node->lwidth - base_width
                    - (extra + 1);
            /* and moves a line starting in italics right by the part of
             * its first letters left of the start (demopix: "Hello User !"
             * in the Virtual demo) */
            chunk_node = (ZTextChunk *)line_node->chunklist.mlh_Head;
            if (chunk_node->node.mln_Succ && chunk_node->str
                && (chunk_node->style & FSF_ITALIC))
            {
                rp = style_extent(rp, &copy, chunk_node, &te);
                if (te.te_Extent.MinX < 0)
                    line_node->lshift = -te.te_Extent.MinX;
            }
        }

        text->height += line_node->lheight;
        text->width = MAX(text->width, line_node->lwidth);
    }
    /* ... and one pixel to a text using bold or italic anywhere */
    if (text->styles & (FSF_BOLD | FSF_ITALIC))
        text->width++;
}

/**************************************************************************
 Point the only chunk of a plain ZText (zune_text_plain_chunk) at s, a
 plain string of len bytes the caller keeps alive, and measure it: the
 same result as zune_text_get_bounds() for such a text, without the
 loops and the RastPort copy. The object must be set up.
**************************************************************************/
void zune_text_set_plain(ZText *text, ZTextChunk *chunk, STRPTR s,
    LONG len, Object *obj)
{
    ZTextLine *line = (ZTextLine *)text->lines.mlh_Head;
    struct TextFont *font = _font(obj);
    struct RastPort *rp = _rp(obj);
    struct RastPort copy;

    chunk->str = s;
    chunk->clen = len;
    /* a styled line has an overhang or extra pixels to measure */
    if (chunk->style & (FSF_BOLD | FSF_ITALIC | ZTEXT_STYLE_OUTLINE
        | ZTEXT_STYLE_SHADOW | ZTEXT_STYLE_GHOST))
    {
        zune_text_get_bounds(text, obj);
        return;
    }
    /* TextLength only reads the RastPort; a copy is needed only when
     * its font or soft style is not the object's plain one */
    if (rp->Font != font || rp->AlgoStyle)
    {
        copy = *rp;
        if (copy.Font != font)
            SetFont(&copy, font);
        if (copy.AlgoStyle)
            SetSoftStyle(&copy, FS_NORMAL, AskSoftStyle(&copy));
        rp = &copy;
    }
    chunk->cheight = line->lheight = text->height = font->tf_YSize;
    chunk->cwidth = line->lwidth = text->width = TextLength(rp, s, len);
    line->centreskip = 0;
    text->styles = chunk->style;
    text->nontext = 0;
}

/************************************************************/
/* Drawing                                                  */
/************************************************************/

/* MUI 3.9 underlines a shortcut one pixel short of its advance, MUI 3.8
 * the whole advance (demopix), two rows below the baseline but within the
 * font (tests/native/ulprobe.c) */
static void draw_hichar(struct RastPort *rp, Object *obj, LONG x, WORD cwidth)
{
    struct TextFont *font = _font(obj);
    WORD y = rp->cp_y - font->tf_Baseline
        + MIN(font->tf_Baseline + 2, font->tf_YSize - 1);

    RectFill(rp, x, y, x + MAX(cwidth - (ZUNE_APP_MUI38(obj) ? 1 : 2), 0), y);
}

/* The characters of a chunk at x that fit left of right + 1, for a text
 * that is truncated (Gauge, MUIA_Text_Shorten's fitting part) */
#define CHUNK_FIT(text, rp, chunk, x, right) ((text)->truncate \
    ? chunk_fit(text, rp, chunk, x, right) : (chunk)->clen)
static LONG chunk_fit(ZText *text, struct RastPort *rp, ZTextChunk *chunk,
    LONG x, LONG right)
{
    struct TextExtent te;

    if (x + chunk->cwidth <= right + 1)
        return chunk->clen;
    return right + 1 > x ? TextFit(rp, chunk->str, chunk->clen, &te, NULL, 1,
        right + 1 - x, 32767) : 0;
}

void zune_text_draw(ZText *text, Object *obj, WORD left, WORD right,
    WORD top)
{
    struct RastPort *rp;
    ULONG pensave, style = FS_NORMAL;
    ZTextLine *line_node;
    ZTextChunk *chunk_node;
    LONG len;

    if (!text || !obj)
        return;

    rp = _rp(obj);
    if (rp->Font != _font(obj))
        SetFont(rp, _font(obj));
    /* SetSoftStyle and the AskSoftStyle it needs are not free; most
     * RastPorts already draw plain text */
    if (rp->AlgoStyle != style)
        SetSoftStyle(rp, style, AskSoftStyle(rp));

    if (text->plain)
    {
        chunk_node = (APTR)((ZTextLine *)text->lines.mlh_Head)
            ->chunklist.mlh_Head;
        if ((len = CHUNK_FIT(text, rp, chunk_node, left, right)) > 0)
        {
            if (rp->DrawMode != JAM1)
                SetDrMd(rp, JAM1);
            Move(rp, left, _font(obj)->tf_Baseline + top);
            Text(rp, chunk_node->str, len);
        }
        return;
    }

    for (line_node = (ZTextLine *)text->lines.mlh_Head;
        line_node->node.mln_Succ;
        line_node = (ZTextLine *)line_node->node.mln_Succ)
    {
        LONG x, w = line_node->lwidth - line_node->centreskip;

        /* right-aligned the same way as centred: the overhang may stick
         * out (tests/native/textset.c) */
        x = line_node->align == ZTL_CENTER ? (left + right + 1 - w) / 2 :
            line_node->align == ZTL_RIGHT ? right - w + 1 : left;
        /* like MUI, never truncates the beginning of a line */
        if (x < left)
            x = left;
        x += line_node->lshift;

        for (chunk_node = (ZTextChunk *)line_node->chunklist.mlh_Head;
            chunk_node->node.mln_Succ;
            x += chunk_node->cwidth,
            chunk_node = (ZTextChunk *)chunk_node->node.mln_Succ)
        {
            WORD ctop = top + (line_node->lheight - chunk_node->cheight) / 2;

            if (chunk_node->str)
            {
                if ((chunk_node->style & (FSF_UNDERLINED | FSF_BOLD
                    | FSF_ITALIC)) != style)
                {
                    style = chunk_node->style & (FSF_UNDERLINED | FSF_BOLD
                        | FSF_ITALIC);
                    SetSoftStyle(rp, style, 0xff);
                }
                Move(rp, x, _font(obj)->tf_Baseline
                    + (chunk_node->cheight ? ctop : top));
                /* MUI 4/5's colours and effects: Zmui45 (texteffect45.c);
                 * hidden images and a selected state are no effects on
                 * text */
                if ((chunk_node->effects & ~(ZTEXT_HIDEIMAGES
                    | ZTEXT_SELECTED)) || (chunk_node->style
                    & (ZTEXT_STYLE_OUTLINE | ZTEXT_STYLE_SHADOW
                    | ZTEXT_STYLE_GHOST)))
                {
                    zune_text_effect45(text, chunk_node, obj, rp, x, right);
                    continue;
                }
                if (chunk_node->dripen != -1)
                    SetAPen(rp, _dri(obj)->dri_Pens[chunk_node->dripen]);
                else if (chunk_node->pen != -1)
                    SetAPen(rp, chunk_node->pen);
                if (rp->DrawMode != JAM1)
                    SetDrMd(rp, JAM1);
                if ((len = CHUNK_FIT(text, rp, chunk_node, x, right)) > 0)
                {
                    Text(rp, chunk_node->str, len);
                    if (chunk_node->style & ZTEXT_STYLE_HICHAR)
                        draw_hichar(rp, obj, x, chunk_node->cwidth);
                }
                continue;
            }
            /* Show/Hide stuff should be put in new api calls */
            if (chunk_node->effects & ZTEXT_HIDEIMAGES)
                continue;
            if (chunk_node->image)
            {
                zune_imspec_show(chunk_node->image, obj);
                pensave = GetAPen(rp);
                zune_imspec_draw(chunk_node->image, muiRenderInfo(obj), x,
                    ctop, chunk_node->cwidth, chunk_node->cheight, 0, 0,
                    !!(chunk_node->effects & ZTEXT_SELECTED));
                SetAPen(rp, pensave);
                zune_imspec_hide(chunk_node->image);
            }
            else if (chunk_node->obj)
            {
                _left(chunk_node->obj) = x;
                _top(chunk_node->obj) = ctop;
                _width(chunk_node->obj) = chunk_node->cwidth;
                _height(chunk_node->obj) = chunk_node->cheight;
                DoShowMethod(chunk_node->obj);
                pensave = GetAPen(rp);
                MUI_Redraw(chunk_node->obj,
                    MADF_DRAWOBJECT | MADF_DRAWFRAME);
                SetAPen(rp, pensave);
                DoHideMethod(chunk_node->obj);
            }
#ifndef ZUNE_NO_CYBERGRAPHICS
            else if (chunk_node->alpha_data)
            {
                struct MUI_AlphaData *alpha_data = chunk_node->alpha_data;

                WritePixelArrayAlpha(alpha_data->data, 0, 0,
                    alpha_data->width * 4, rp, x, ctop, alpha_data->width,
                    alpha_data->height, 0xFFFFFFFF);
            }
#endif
        }
        top += line_node->lheight;
    }
}

/* MUIA_Text_Shorten's elision and MUIA_Text_Marking (MUI 5, V20): in the
 * module Zmui45 (textmark45.c, modules.h), which MUI 3 programs never load.
 * A line that fits is drawn here, as the module would draw it: a single
 * line text whose width fits, not as the plain fast path (the module's
 * view is not plain), so an ELIDE column or a shortened Text whose text
 * fits never calls the module. */
static void __attribute__((noinline)) draw_fitting(ZText *text,
    Object *obj, WORD left, WORD right, WORD top)
{
    UBYTE plain = text->plain;

    text->plain = FALSE;
    zune_text_draw(text, obj, left, right, top);
    text->plain = plain;
}

void zune_text_draw_shortened(ZText *text, Object *obj, WORD left,
    WORD right, WORD top, LONG mode)
{
    ZTextLine *line;

    if (text && right >= left
        && (line = (APTR)text->lines.mlh_Head)->node.mln_Succ
        && !line->node.mln_Succ->mln_Succ
        && line->lwidth <= right - left + 1)
        draw_fitting(text, obj, left, right, top);
    else
        zune_text_shortened45(text, obj, left, right, top, mode);
}
