/* MUI 5's text shortening (MUIA_Text_Shorten, a List format's ELIDE) and
 * marking (MUIA_Text_Marking), both V20: in the module Zmui45.mui
 * (classes/zmui45.c, modules.h). The library draws a text that fits itself
 * (textengine.c's zune_text_draw_shortened) and calls these for the rest;
 * MUI 3.8 has neither, so a MUI 3 program never loads them. */
#include <string.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/graphics.h>

#include "mui.h"
#include "textengine.h"
#include "support.h"
#include "muimaster_intern.h"
#include "modules.h"

static inline void zt_addtail(struct MinList *list, struct MinNode *node)
{
    node->mln_Succ = (struct MinNode *)&list->mlh_Tail;
    node->mln_Pred = list->mlh_TailPred;
    list->mlh_TailPred->mln_Succ = node;
    list->mlh_TailPred = node;
}

/* Elide parsed chunks rather than cutting escape sequences in the source.
 * Text keeps its original contents, styles, images and per-line alignment. */
static LONG elide_chunk(struct RastPort *rp, const ZTextChunk *source,
    ZTextChunk *dest, LONG available, BOOL tail)
{
    struct TextExtent extent;
    LONG length;
    if (available <= 0) return 0;
    *dest = *source;
    if (source->cwidth <= available) return source->cwidth;
    if (!source->str || !source->clen || source->image || source->obj)
        return 0;
    SetSoftStyle(rp, source->style & ~ZTEXT_STYLE_HICHAR, AskSoftStyle(rp));
    length = TextFit(rp, source->str + (tail ? source->clen - 1 : 0),
        source->clen, &extent, NULL, tail ? -1 : 1, available, 32767);
    if (length <= 0) return 0;
    dest->str += tail ? source->clen - length : 0;
    dest->clen = length;
    dest->cwidth = TextLength(rp, dest->str, length);
    return dest->cwidth;
}

static void text_shortened(ZText *text, Object *obj, WORD left,
    WORD right, WORD top, LONG mode,
    void (*visit)(ZText *, Object *, WORD, WORD, WORD, APTR), APTR state)
{
    ZTextLine *source;
    LONG available = right - left + 1;
    if (!text || available <= 0) return;
    if (mode == MUIV_Text_Shorten_Hide && text->width > available) return;
    for (source = (APTR)text->lines.mlh_Head; source->node.mln_Succ;
        source = (APTR)source->node.mln_Succ)
    {
        ZText view = *text;
        view.plain = FALSE;
        ZTextLine line = *source;
        ZTextChunk *chunk, *chunks = NULL;
        struct RastPort rp = *_rp(obj);
        LONG count = 0, used = 0, suffix, i, budget, width;
        NewList((struct List *)&view.lines);
        zt_addtail(&view.lines, (struct MinNode *)&line.node);
        view.height = line.lheight;
        if (source->lwidth <= available)
            visit(&view, obj, left, right, top, state);
        else if (mode != MUIV_Text_Shorten_Hide)
        {
            for (chunk = (APTR)source->chunklist.mlh_Head; chunk->node.mln_Succ;
                chunk = (APTR)chunk->node.mln_Succ) count++;
            chunks = AllocVec((2 * count + 1) * sizeof(*chunks), MEMF_CLEAR);
            if (!chunks)
            {
                view.truncate = TRUE;
                visit(&view, obj, left, right, top, state);
            }
            else
            {
                SetFont(&rp, _font(obj)); SetSoftStyle(&rp, 0, AskSoftStyle(&rp));
                LONG dots = TextLength(&rp, "...", 3);
                LONG remaining = MAX(0, available - dots);
                LONG prefix = mode == MUIV_Text_Shorten_ElideLeft ? 0 :
                    mode == MUIV_Text_Shorten_ElideCenter ? remaining / 2 : remaining;
                NewList((struct List *)&line.chunklist);
                line.lwidth = 0; line.centreskip = 0; line.lshift = 0;
                budget = prefix;
                for (chunk = (APTR)source->chunklist.mlh_Head;
                    chunk->node.mln_Succ && budget > 0; chunk = (APTR)chunk->node.mln_Succ)
                {
                    width = elide_chunk(&rp, chunk, &chunks[used], budget, FALSE);
                    if (!width) break;
                    zt_addtail(&line.chunklist, (struct MinNode *)&chunks[used++].node);
                    budget -= width; line.lwidth += width;
                    if (width < chunk->cwidth) break;
                }
                chunks[used].str = "..."; chunks[used].clen = 3;
                chunks[used].cwidth = dots; chunks[used].cheight = rp.TxHeight;
                chunks[used].dripen = TEXTPEN; chunks[used].pen = -1;
                zt_addtail(&line.chunklist, (struct MinNode *)&chunks[used++].node);
                line.lwidth += dots;
                budget = mode == MUIV_Text_Shorten_ElideRight ? 0 :
                    remaining - (line.lwidth - dots);
                suffix = used;
                for (chunk = (APTR)source->chunklist.mlh_TailPred;
                    chunk->node.mln_Pred && budget > 0; chunk = (APTR)chunk->node.mln_Pred)
                {
                    width = elide_chunk(&rp, chunk, &chunks[used], budget, TRUE);
                    if (!width) break;
                    used++; budget -= width; line.lwidth += width;
                    if (width < chunk->cwidth) break;
                }
                for (i = used - 1; i >= suffix; i--)
                    zt_addtail(&line.chunklist, (struct MinNode *)&chunks[i].node);
                view.width = line.lwidth; view.truncate = TRUE;
                visit(&view, obj, left, right, top, state);
                FreeVec(chunks);
            }
        }
        top += source->lheight;
    }
}

static void draw_view(ZText *text, Object *obj, WORD left, WORD right,
    WORD top, APTR state)
{
    zune_text_draw(text, obj, left, right, top);
}

void zune_text_shortened45(ZText *text, Object *obj, WORD left,
    WORD right, WORD top, LONG mode)
{
    text_shortened(text, obj, left, right, top, mode, draw_view, NULL);
}

struct MarkWalk
{
    ZText *source;
    LONG first, last, hit, mousex, mousey;
    BOOL draw;
};

/* Views made by elision point into the original parsed chunks. Map those
 * pointers back to character offsets; generated ellipses are not text. */
static LONG text_offset(ZText *text, const char *str)
{
    ZTextLine *line;
    ZTextChunk *chunk;
    LONG offset = 0;
    for (line = (APTR)text->lines.mlh_Head; line->node.mln_Succ;
        line = (APTR)line->node.mln_Succ)
    {
        for (chunk = (APTR)line->chunklist.mlh_Head; chunk->node.mln_Succ;
            chunk = (APTR)chunk->node.mln_Succ)
            if (chunk->str)
            {
                if ((IPTR)str >= (IPTR)chunk->str &&
                    (IPTR)str < (IPTR)chunk->str + chunk->clen)
                    return offset + (str - chunk->str);
                offset += chunk->clen;
            }
        offset++;
    }
    return -1;
}

static void mark_view(ZText *text, Object *obj, WORD left, WORD right,
    WORD top, APTR state)
{
    struct MarkWalk *walk = state;
    struct RastPort rp = *_rp(obj);
    ZTextLine *line;
    ZTextChunk *chunk;
    SetFont(&rp, _font(obj));
    SetDrMd(&rp, COMPLEMENT);
    for (line = (APTR)text->lines.mlh_Head; line->node.mln_Succ;
        line = (APTR)line->node.mln_Succ)
    {
        LONG x = left;
        BOOL first_chunk = TRUE;
        if (line->align == ZTL_CENTER)
            x = MAX(left, (left + right + 1 -
                (line->lwidth - line->centreskip)) / 2);
        else if (line->align == ZTL_RIGHT)
            x = MAX(left, right - (line->lwidth - line->centreskip) + 1);
        for (chunk = (APTR)line->chunklist.mlh_Head; chunk->node.mln_Succ;
            chunk = (APTR)chunk->node.mln_Succ)
        {
            LONG offset = chunk->str ? text_offset(walk->source, chunk->str) : -1;
            if (offset >= 0)
            {
                LONG i, cx = x;
                if (first_chunk && !walk->draw && walk->mousey >= top &&
                    walk->mousey < top + line->lheight) walk->hit = offset;
                first_chunk = FALSE;
                SetSoftStyle(&rp, chunk->style & ~ZTEXT_STYLE_HICHAR,
                    AskSoftStyle(&rp));
                for (i = 0; i < chunk->clen; i++)
                {
                    LONG next = x + TextLength(&rp, chunk->str, i + 1);
                    if (next > right + 1) break;
                    if (walk->draw && offset + i >= walk->first &&
                        offset + i < walk->last && next > cx)
                    {
                        LONG y1 = MAX(top, _mtop(obj));
                        LONG y2 = MIN(top + line->lheight - 1, _mbottom(obj));
                        if (y1 <= y2)
                            RectFill(&rp, cx, y1, next - 1, y2);
                    }
                    if (!walk->draw)
                    {
                        if (walk->mousey >= top + line->lheight)
                            walk->hit = offset + i + 1;
                        else if (walk->mousey >= top)
                        {
                            if (walk->mousex >= (cx + next) / 2)
                                walk->hit = offset + i + 1;
                        }
                    }
                    cx = next;
                }
            }
            x += chunk->cwidth;
        }
        top += line->lheight;
    }
}

LONG zune_text_mark(ZText *text, Object *obj, LONG mode, LONG first,
    LONG last, LONG mousex, LONG mousey, BOOL draw)
{
    struct MarkWalk walk = {text, first, last, 0, mousex, mousey, draw};
    WORD top;
    if (!text) return 0;
    top = _mtop(obj) + (_mheight(obj) - text->height) / 2;
    if (mode >= MUIV_Text_Shorten_Hide)
        text_shortened(text, obj, _mleft(obj), _mright(obj), top,
            mode, mark_view, &walk);
    else
        mark_view(text, obj, _mleft(obj), _mright(obj), top, &walk);
    return walk.hit;
}

/* Return a plain copy of a marked range, with line breaks but without
 * preparse escapes, inline objects or synthetic elision characters. */
STRPTR zune_text_copy(ZText *text, LONG first, LONG last)
{
    ZTextLine *line;
    ZTextChunk *chunk;
    LONG offset = 0, used = 0, i;
    STRPTR result;
    if (!text || first < 0 || last <= first) return NULL;
    result = AllocVec(last - first + 1, MEMF_ANY);
    if (!result) return NULL;
    for (line = (APTR)text->lines.mlh_Head; line->node.mln_Succ;
        line = (APTR)line->node.mln_Succ)
    {
        for (chunk = (APTR)line->chunklist.mlh_Head; chunk->node.mln_Succ;
            chunk = (APTR)chunk->node.mln_Succ)
            if (chunk->str)
                for (i = 0; i < chunk->clen; i++, offset++)
                    if (offset >= first && offset < last)
                        result[used++] = chunk->str[i];
        if (line->node.mln_Succ->mln_Succ)
        {
            if (offset >= first && offset < last) result[used++] = '\n';
            offset++;
        }
    }
    result[used] = 0;
    return result;
}
