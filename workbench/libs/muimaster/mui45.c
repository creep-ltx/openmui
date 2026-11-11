/* MUI 4/5 attributes of the core classes that MUI 3 programs never use
 * (Area's frame attributes, String's kept Textinput values and 64-bit
 * integer, its multiline drawing and MUI 5's placeholder) and String's
 * Textinput methods.
 * Built as the module Zmui45.mui (modules.h, classes/zmui45.c): a MUI 3
 * program never loads it, and the library keeps only the recognition of
 * the attributes and a call (modules.c). */
#include <string.h>
#include <ctype.h>
#include <exec/types.h>
#include <intuition/classusr.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/muimaster.h>

#include "mui.h"
#include "muimaster_intern.h"
#include "support.h"
#include "modules.h"
#include "classes/string_private.h"

extern struct Library *MUIMasterBase;

/* Area's MUIA_FrameVisible and MUIA_FrameDynamic (ISG BOOL, TRUE and
 * FALSE by default, as MUI 5 answers them; MUI 3.8 does not know them).
 * FrameVisible FALSE draws the frame as a phantom: its room is kept and
 * its background painted where the frame would be (Area's MUIM_Draw
 * tests MADF2_FRAMEHIDDEN). FrameDynamic (show the frame while the pointer
 * is over the object) is kept and answered, not acted on. */
IPTR zune_area_mui45(Object *obj, ULONG attr, IPTR value, IPTR *store)
{
    struct MUI_AreaData *data = muiAreaData(obj);
    BOOL visible = attr == MUIA_FrameVisible;
    ULONG bit = visible ? MADF2_FRAMEHIDDEN : MADF2_FRAMEDYNAMIC;

    if (store)
    {
        BOOL on = !!(data->mad_Flags2 & bit);
        *store = visible ? !on : on;
        return TRUE;
    }
    if (visible)
        value = !value;
    if (value)
        data->mad_Flags2 |= bit;
    else
        data->mad_Flags2 &= ~bit;
    if (visible && (_flags(obj) & MADF_CANDRAW))
        MUI_Redraw(obj, MADF_DRAWOBJECT | MADF_DRAWFRAME);
    return TRUE;
}

/* String's 64-bit integer (MUIA_String_Integer64, MUI 4/5), in decimal
 * without a 64-bit division: 16 bits at a time */
static void format64(char *buffer, long long value)
{
    char digits[20];
    unsigned long long magnitude = value;
    unsigned int count = 0;

    if (value < 0)
    {
        *buffer++ = '-';
        magnitude = 0ULL - magnitude;
    }
    do
    {
        UWORD part[4];
        ULONG rem = 0;
        int i;

        part[0] = magnitude >> 48;
        part[1] = magnitude >> 32;
        part[2] = magnitude >> 16;
        part[3] = magnitude;
        for (i = 0; i < 4; i++)
        {
            ULONG cur = (rem << 16) | part[i];
            part[i] = cur / 10;
            rem = cur % 10;
        }
        magnitude = ((unsigned long long)part[0] << 48)
            | ((unsigned long long)part[1] << 32)
            | ((ULONG)part[2] << 16) | part[3];
        digits[count++] = '0' + rem;
    } while (magnitude);
    while (count)
        *buffer++ = digits[--count];
    *buffer = 0;
}

static long long parse64(CONST_STRPTR text)
{
    unsigned long long value = 0;
    BOOL negative = FALSE;

    while (*text == ' ' || *text == '\t')
        text++;
    if (*text == '-' || *text == '+')
        negative = *text++ == '-';
    while (*text >= '0' && *text <= '9')
    {
        unsigned digit = *text++ - '0';
        unsigned long long limit = negative ? 0x8000000000000000ULL :
            0x7fffffffffffffffULL;
        if (value > 922337203685477580ULL ||
            (value == 922337203685477580ULL && digit > (negative ? 8 : 7)))
        {
            value = limit;
            break;
        }
        value = (value << 3) + (value << 1) + digit;
    }
    return negative ? 0ULL - value : value;
}

static const struct { ULONG attr; ULONG flag; } string_flags[] = {
    { MUIA_Textinput_Changed, MSDF_CHANGED },
    { MUIA_Textinput_ResetMarkOnCursor, MSDF_RESETMARK },
    { MUIA_Textinput_IsNumeric, MSDF_ISNUMERIC },
    { MUIA_Textinput_SetMin, MSDF_SETMIN },
    { MUIA_Textinput_SetMax, MSDF_SETMAX },
    { MUIA_Textinput_SetVMin, MSDF_SETVMIN },
    { MUIA_Textinput_SetVMax, MSDF_SETVMAX },
};

/* String's Textinput attributes that are only kept and answered
 * (Changed, ResetMarkOnCursor, IsNumeric, SetMin/Max/VMin/VMax, MinVal,
 * MaxVal, WordWrap, Styles; Font only answered) and MUIA_String_Integer64.
 * tag without store: OM_NEW/OM_SET (Integer64 sets the contents, a tag
 * that changes nothing becomes TAG_IGNORE); tag and store: OM_NEW's
 * Integer64, its text into the buffer store; store alone: OM_GET of attr,
 * TRUE when answered. */
IPTR zune_string_mui45(Object *obj, struct MUI_StringData *data, ULONG attr,
    struct TagItem *tag, IPTR *store)
{
    IPTR value = tag ? tag->ti_Data : 0;
    unsigned int i;

    if (attr == MUIA_String_Integer64)
    {
        char buffer[22];
        long long old;

        if (!tag)
        {
            data->integer64 = parse64(data->Buffer);
            *store = (IPTR)&data->integer64;
            return TRUE;
        }
        if (!value)
            return FALSE;
        if (store)
        {
            format64((char *)store, *(const long long *)value);
            return TRUE;
        }
        old = parse64(data->Buffer);
        format64(buffer, *(const long long *)value);
        set(obj, MUIA_String_Contents, buffer);
        if (old == parse64(data->Buffer))
            tag->ti_Tag = TAG_IGNORE;
        return TRUE;
    }
    for (i = 0; i < sizeof(string_flags) / sizeof(string_flags[0]); i++)
        if (string_flags[i].attr == attr)
        {
            if (store)
                *store = !!(data->msd_Flags & string_flags[i].flag);
            else if (value)
                data->msd_Flags |= string_flags[i].flag;
            else
                data->msd_Flags &= ~string_flags[i].flag;
            return TRUE;
        }
    switch (attr)
    {
    case MUIA_Textinput_MinVal:
        if (store)
            *store = data->TiMinVal;
        else
            data->TiMinVal = (LONG)value;
        break;
    case MUIA_Textinput_MaxVal:
        if (store)
            *store = data->TiMaxVal;
        else
            data->TiMaxVal = (LONG)value;
        break;
    case MUIA_Textinput_WordWrap:
        if (store)
            *store = data->TiWordWrap;
        else
            data->TiWordWrap = (UWORD)value;
        break;
    case MUIA_Textinput_Styles:
        if (store)
            *store = data->TiStyles;
        else
            data->TiStyles = (UBYTE)value;
        break;
    case MUIA_Textinput_Font:
        if (!store)
            return FALSE;
        *store = data->TiFont;
        break;
    default:
        return FALSE;
    }
    return TRUE;
}

/* String's multiline lines end at '\n'; columns are character counts.
 * The library keeps its own Ml_LineStart/Ml_LineEnd/Ml_Rows for the keys;
 * these copies serve the drawing and the pointer below. */
static LONG Ml_LineStart(struct MUI_StringData *data, LONG pos)
{
    while (pos > 0 && data->Buffer[pos - 1] != '\n')
        pos--;
    return pos;
}

static LONG Ml_LineEnd(struct MUI_StringData *data, LONG pos)
{
    while (pos < (LONG)data->NumChars && data->Buffer[pos] != '\n')
        pos++;
    return pos;
}

static LONG Ml_LineOf(struct MUI_StringData *data, LONG pos)
{
    LONG line = 0, i;

    for (i = 0; i < pos; i++)
        if (data->Buffer[i] == '\n')
            line++;
    return line;
}

/* Start of a line; lines past the end give the last line */
static LONG Ml_LinePos(struct MUI_StringData *data, LONG line)
{
    LONG pos = 0, end;

    while (line-- > 0 && (end = Ml_LineEnd(data, pos)) < (LONG)data->NumChars)
        pos = end + 1;
    return pos;
}

static LONG Ml_Rows(Object *obj)
{
    return MAX(1, _mheight(obj) / _font(obj)->tf_YSize);
}

/* String's TextM (classes/string.c): <unmarked><marked><unmarked> */
static void TextM(Object *obj, struct MUI_StringData *data,
    STRPTR text, WORD textlen, LONG markstart, LONG markend)
{
    struct RastPort *rp = _rp(obj);
    ULONG textpen = data->is_active ? data->active_text.p_pen
        : data->inactive_text.p_pen;
    LONG len;

    len = MIN(markstart, textlen);
    len = MAX(len, 0);
    if (len)
    {
        SetABPenDrMd(rp, textpen, _pens(obj)[MPEN_BACKGROUND], JAM1);
        Text(rp, text, len);
        text += len;
        textlen -= len;
    }
    len = MIN(markend - len, textlen);
    len = MAX(len, 0);
    if (len)
    {
        SetABPenDrMd(_rp(obj), data->marked_text.p_pen,
            data->marked_bg.p_pen, JAM2);
        Text(rp, text, len);
        text += len;
        textlen -= len;
    }
    if (textlen)
    {
        SetABPenDrMd(rp, textpen, _pens(obj)[MPEN_BACKGROUND], JAM1);
        Text(rp, text, textlen);
    }
    SetDrMd(rp, JAM1);
}

/* Scroll so that the cursor is visible */
static void Ml_Scroll(Object *obj, struct MUI_StringData *data)
{
    struct RastPort *rp = _rp(obj);
    LONG rows = Ml_Rows(obj), line = Ml_LineOf(data, data->BufferPos);
    LONG start = Ml_LineStart(data, data->BufferPos);
    LONG column = data->BufferPos - start, cell;
    STRPTR text = data->VisualBuffer + start;

    if (line < data->MlTopLine)
        data->MlTopLine = line;
    else if (line >= data->MlTopLine + rows)
        data->MlTopLine = line - rows + 1;

    SetFont(rp, _font(obj));
    cell = TextLength(rp, column < Ml_LineEnd(data, start) - start ?
        text + column : (STRPTR)" ", 1);
    if (column < data->MlLeftCol)
        data->MlLeftCol = column;
    while (data->MlLeftCol < column && TextLength(rp,
            text + data->MlLeftCol, column - data->MlLeftCol) + cell
        > _mwidth(obj))
        data->MlLeftCol++;
}

/* String's MUIM_Draw for a placeholder (an empty inactive String with
 * MUIA_String_Placeholder) or a Multiline String: what is ZUNE_SD_*;
 * textpen and the marked range (ZUNE_SD_MARKED) for the multiline one. */
void zune_string_draw45(Object *obj, struct MUI_StringData *data,
    ULONG what, ULONG textpen, LONG markstart, LONG markstop)
{
    struct RastPort *rp = _rp(obj);
    struct TextExtent te;
    LONG ysize = _font(obj)->tf_YSize, rows, row, pos;
    STRPTR text = data->VisualBuffer;

    if (what == ZUNE_SD_PLACEHOLDER)
    {
        ULONG style = rp->AlgoStyle;
        LONG count, x = _mleft(obj), width;

        DoMethod(obj, MUIM_DrawBackground, _mleft(obj), _mtop(obj),
            _mwidth(obj), _mheight(obj), _mleft(obj), _mtop(obj), 0);
        SetFont(rp, _font(obj));
        SetSoftStyle(rp, FSF_ITALIC, AskSoftStyle(rp));
        count = TextFit(rp, data->placeholder, strlen(data->placeholder),
            &te, NULL, 1, _mwidth(obj), _mheight(obj));
        width = TextLength(rp, data->placeholder, count);
        if (data->msd_Align == MUIV_String_Format_Center)
            x += (_mwidth(obj) - width) / 2;
        else if (data->msd_Align == MUIV_String_Format_Right)
            x += _mwidth(obj) - width;
        SetABPenDrMd(rp, _pens(obj)[MPEN_SHADOW],
            _pens(obj)[MPEN_BACKGROUND], JAM1);
        Move(rp, x, _mtop(obj) + ((_mheight(obj) - rp->Font->tf_YSize) / 2)
            + rp->Font->tf_Baseline);
        Text(rp, data->placeholder, count);
        SetSoftStyle(rp, style, AskSoftStyle(rp));
        return;
    }

    Ml_Scroll(obj, data);
    if (what & ZUNE_SD_UPDATE)
        DoMethod(obj, MUIM_DrawBackground, _mleft(obj), _mtop(obj),
            _mwidth(obj), _mheight(obj), _mleft(obj), _mtop(obj), 0);
    rows = Ml_Rows(obj);
    pos = Ml_LinePos(data, data->MlTopLine);
    for (row = 0; row < rows; row++)
    {
        LONG end = Ml_LineEnd(data, pos);
        LONG from = MIN(pos + data->MlLeftCol, end), count;
        WORD top = _mtop(obj) + row * ysize + rp->Font->tf_Baseline;

        count = TextFit(rp, text + from, end - from, &te, NULL, 1,
            _mwidth(obj), ysize);
        Move(rp, _mleft(obj), top);
        if (what & ZUNE_SD_MARKED)
            TextM(obj, data, text + from, count, markstart - from,
                markstop - from);
        else
        {
            SetABPenDrMd(rp, textpen, _pens(obj)[MPEN_BACKGROUND], JAM1);
            Text(rp, text + from, count);
            if (data->is_active && (LONG)data->BufferPos >= from
                && (LONG)data->BufferPos <= end)
            {
                SetABPenDrMd(rp, data->active_text.p_pen,
                    data->cursor.p_pen, JAM2);
                Move(rp, _mleft(obj) + TextLength(rp, text + from,
                    data->BufferPos - from), top);
                Text(rp, (LONG)data->BufferPos < end ?
                    text + data->BufferPos : (STRPTR)" ", 1);
                SetDrMd(rp, JAM1);
            }
        }
        if (end >= (LONG)data->NumChars)
            break;
        pos = end + 1;
    }
}

/* A Multiline String's buffer position at a point; outside the rows it is
 * in the line beyond */
LONG zune_string_pos_at(Object *obj, struct MUI_StringData *data, LONG x,
    LONG y)
{
    struct RastPort *rp = _rp(obj);
    struct TextExtent te;
    LONG ysize = _font(obj)->tf_YSize, row, start, end, from;

    row = y < _mtop(obj) ? -1 : (y - _mtop(obj)) / ysize;
    start = Ml_LinePos(data, MAX(0, data->MlTopLine + row));
    end = Ml_LineEnd(data, start);
    from = MIN(start + data->MlLeftCol, end);
    if (x <= _mleft(obj))
        return from;
    SetFont(rp, _font(obj));
    return from + TextFit(rp, data->VisualBuffer + from, end - from, &te,
        NULL, 1, x - _mleft(obj), ysize);
}

/* String's Textinput methods (MUI 4/5): a program sends them, or
 * classes/string.c sends a key as one in a program that gets MUI 4/5, so
 * that a subclass sees it (MSDF_INEVENT: the event handler notifies and
 * redraws afterwards from TiPending; else the method does). What a method
 * shares with a key is the library's String_Edit (ops->edit, an SE_*
 * operation); the rest edits through the ops' buffer helpers. */
#define TI_MULTILINE(data) ((data)->msd_Flags & MSDF_MULTILINE)

/* The methods that are one editing operation; writable: nothing without
 * MUIA_Textinput_Editable */
static const struct { ULONG method; UBYTE op; UBYTE writable; } ti_edits[] =
{
    { MUIM_Textinput_HandleChar, SE_CHAR, 0 },
    { MUIM_Textinput_DoBS, SE_BS, 1 }, { MUIM_Textinput_DoBSSOL, SE_BSSOL, 1 },
    { MUIM_Textinput_DoDel, SE_DEL, 1 },
    { MUIM_Textinput_DoDelEOL, SE_DELEOL, 1 },
    { MUIM_Textinput_DoCopy, SE_COPY, 0 }, { MUIM_Textinput_DoCut, SE_CUT, 0 },
    { MUIM_Textinput_DoCopyCut, SE_CUT, 0 },
    { MUIM_Textinput_DoPaste, SE_PASTE, 1 },
    { MUIM_Textinput_DoLeft, MUIKEY_LEFT, 0 },
    { MUIM_Textinput_DoRight, MUIKEY_RIGHT, 0 },
    { MUIM_Textinput_DoPrevWord, MUIKEY_WORDLEFT, 0 },
    { MUIM_Textinput_DoNextWord, MUIKEY_WORDRIGHT, 0 },
    { MUIM_Textinput_DoLineStart, MUIKEY_LINESTART, 0 },
    { MUIM_Textinput_DoLineEnd, MUIKEY_LINEEND, 0 },
    { MUIM_Textinput_DoTop, MUIKEY_TOP, 0 },
    { MUIM_Textinput_DoBottom, MUIKEY_BOTTOM, 0 },
    { MUIM_Textinput_DoUp, MUIKEY_UP, 0 }, { MUIM_Textinput_DoDown, MUIKEY_DOWN, 0 },
    { MUIM_Textinput_DoPageUp, MUIKEY_PAGEUP, 0 },
    { MUIM_Textinput_DoPageDown, MUIKEY_PAGEDOWN, 0 },
};

/* Read a file into a new buffer of at most max bytes, NUL-terminated */
static __attribute__((noinline)) STRPTR Ti_ReadFile(CONST_STRPTR name, ULONG max)
{
    BPTR file = Open(name, MODE_OLDFILE);
    STRPTR text;
    LONG length;

    if (!file)
        return NULL;
    text = AllocVec(max, MEMF_ANY);
    length = text ? Read(file, text, max - 1) : -1;
    Close(file);
    if (length < 0)
    {
        FreeVec(text);
        return NULL;
    }
    text[length] = 0;
    return text;
}

/* Find the decimal number at or just before the cursor and add delta */
static __attribute__((noinline)) BOOL Ti_AddToNumber(struct MUI_StringData *data,
    const struct ZuneStringOps *ops, LONG delta)
{
    STRPTR buf = data->Buffer;
    LONG pos = MIN(data->BufferPos, data->NumChars), start, stop, value = 0;
    char number[22];
    LONG length;

    if (!isdigit(buf[pos]) && pos > 0 && isdigit(buf[pos - 1]))
        pos--;
    if (!isdigit(buf[pos]))
        return FALSE;
    for (start = pos; start > 0 && isdigit(buf[start - 1]); start--);
    for (stop = pos; stop < data->NumChars && isdigit(buf[stop]); stop++);
    if (start > 0 && buf[start - 1] == '-')
        start--;
    StrToLong(&buf[start], &value);
    format64(number, (long long)value + delta);
    length = strlen(number);
    if (data->NumChars - (stop - start) + length >= data->BufferSize)
        return FALSE;
    pos = data->BufferPos;
    ops->buffer_delete(data, start, stop);
    ops->buffer_insert(data, start, number, length);
    data->BufferPos = MIN(pos, (LONG)data->NumChars);
    return TRUE;
}

IPTR zune_string_textinput(struct IClass *cl, Object *obj, Msg msg,
    const struct ZuneStringOps *ops)
{
    struct MUI_StringData *data = INST_DATA(cl, obj);
    BOOL writable = !(data->msd_Flags & MSDF_NOINPUT);
    BOOL edited = FALSE, moved = FALSE;
    IPTR retval = TRUE;
    ULONG pending = 0;
    LONG start, stop;
    unsigned int i;

    for (i = 0; i < sizeof(ti_edits) / sizeof(ti_edits[0]); i++)
        if (ti_edits[i].method == msg->MethodID)
        {
            if (ti_edits[i].writable && !writable)
                return 0;
            pending = ops->edit(cl, obj, data, ti_edits[i].op,
                msg->MethodID == MUIM_Textinput_DoCopyCut ? 1 :
                (LONG)((struct MUIP_Textinput_HandleChar *)msg)->ch);
            if (msg->MethodID == MUIM_Textinput_HandleChar)
                retval = !!(pending & TIP_EDITED);
            goto done;
        }

    switch (msg->MethodID)
    {
    case MUIM_Textinput_DoBSWord:
    case MUIM_Textinput_DoDelWord:
        if (!writable)
            return 0;
        if (!(edited = ops->kill_marked(data)))
        {
            stop = data->BufferPos;
            if (msg->MethodID == MUIM_Textinput_DoBSWord)
                edited = ops->buffer_delete(data,
                    ops->prev_word(data, stop), stop);
            else
                edited = ops->buffer_delete(data, stop,
                    ops->succ_word(data, stop));
        }
        break;

    case MUIM_Textinput_DoDelLine:
    case MUIM_Textinput_DoCutLine:
        if (!writable)
            return 0;
        start = 0;
        stop = data->NumChars;
        if (TI_MULTILINE(data))
        {
            /* The current line with its newline */
            start = Ml_LineStart(data, data->BufferPos);
            stop = Ml_LineEnd(data, data->BufferPos);
            if (stop < (LONG)data->NumChars)
                stop++;
            else if (start > 0)
                start--;
        }
        if (msg->MethodID == MUIM_Textinput_DoCutLine && stop > start
            && !data->msd_useSecret)
            ops->clip_write(data->Buffer + start, stop - start);
        data->msd_Flags &= ~(MSDF_MARKING | MSDF_TIMARK);
        edited = ops->buffer_delete(data, start, stop);
        break;

    case MUIM_Textinput_DoMarkStart:
        ops->freeze_mark(data);
        data->msd_Flags &= ~MSDF_TIMARK;
        data->MarkPos = data->BufferPos;
        data->MultiClick = 0;
        data->msd_Flags |= MSDF_MARKING | MSDF_KEYMARKING;
        moved = TRUE;
        break;

    case MUIM_Textinput_DoMarkAll:
        data->TiMarkStart = 0;
        data->TiMarkEnd = data->NumChars;
        data->BufferPos = data->NumChars;
        data->msd_Flags &= ~MSDF_KEYMARKING;
        data->msd_Flags |= MSDF_MARKING | MSDF_TIMARK;
        moved = TRUE;
        break;

    case MUIM_Textinput_DoToggleCase:
    case MUIM_Textinput_DoToggleCaseEOW:
        if (!writable)
            return 0;
        stop = msg->MethodID == MUIM_Textinput_DoToggleCase ?
            MIN(data->BufferPos + 1, data->NumChars) :
            ops->word_end(data, data->BufferPos);
        for (start = data->BufferPos; start < stop; start++)
        {
            UBYTE c = data->Buffer[start];
            data->Buffer[start] = ToUpper(c) != c ? ToUpper(c) : ToLower(c);
            edited = TRUE;
        }
        data->BufferPos = MAX((LONG)data->BufferPos, stop);
        break;

    case MUIM_Textinput_DoIncrementDec:
    case MUIM_Textinput_DoDecrementDec:
        if (!writable)
            return 0;
        edited = Ti_AddToNumber(data, ops,
            msg->MethodID == MUIM_Textinput_DoIncrementDec ? 1 : -1);
        break;

    case MUIM_Textinput_InsertText:
    case MUIM_Textinput_AppendText:
        {
            struct MUIP_Textinput_InsertText *m = (APTR)msg;
            BOOL append = msg->MethodID == MUIM_Textinput_AppendText;
            LONG count = ops->buffer_insert(data,
                append ? data->NumChars : data->BufferPos, m->text, m->len);

            if (!append)
                data->BufferPos += count;
            edited = count > 0;
        }
        break;

    case MUIM_Textinput_SaveToFile:
        {
            struct MUIP_Textinput_SaveToFile *m = (APTR)msg;
            BPTR file = Open(m->filename, MODE_NEWFILE);

            if (!file)
                return FALSE;
            retval = Write(file, data->Buffer, data->NumChars)
                == data->NumChars;
            Close(file);
            return retval;
        }

    case MUIM_Textinput_LoadFromFile:
    case MUIM_Textinput_InsertFromFile:
        {
            struct MUIP_Textinput_LoadFromFile *m = (APTR)msg;
            STRPTR text = Ti_ReadFile(m->filename, data->BufferSize);

            if (!text)
                return FALSE;
            if (msg->MethodID == MUIM_Textinput_LoadFromFile)
            {
                data->msd_Flags &= ~(MSDF_MARKING | MSDF_TIMARK);
                ops->buffer_delete(data, 0, data->NumChars);
                data->BufferPos = 0;
            }
            data->BufferPos += ops->buffer_insert(data, data->BufferPos,
                text, -1);
            FreeVec(text);
            edited = TRUE;
        }
        break;

    case MUIM_Textinput_Acknowledge:
        ops->acknowledge(cl, obj, data);
        return TRUE;

    case MUIM_Textinput_DoNextGadget:
        if (muiRenderInfo(obj) && _win(obj))
            set(_win(obj), MUIA_Window_ActiveObject,
                MUIV_Window_ActiveObject_Next);
        return TRUE;

    default:
        /* Undo, file requesters, popups and URLs are not available in a
         * String */
        return 0;
    }

    if (edited)
    {
        data->msd_RedrawReason = NEW_CONTENTS;
        data->msd_Flags |= MSDF_CHANGED;
        pending = TIP_EDITED | TIP_UPDATE;
    }
    else if (moved)
    {
        if (data->msd_RedrawReason == NO_REASON)
            data->msd_RedrawReason = MOVE_CURSOR;
        pending = TIP_UPDATE;
    }

done:
    if (data->msd_Flags & MSDF_INEVENT)
    {
        data->TiPending |= pending;
        return retval;
    }
    if (pending & TIP_EDITED)
    {
        data->AckBuffer = NULL;
        SetSuperAttrs(cl, obj, MUIA_String_Contents, (IPTR)data->Buffer,
            MUIA_Textinput_Contents, (IPTR)data->Buffer, TAG_DONE);
    }
    if (pending)
        MUI_Redraw(obj, MADF_DRAWUPDATE);
    return retval;
}
