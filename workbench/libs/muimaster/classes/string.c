/*
    Copyright (C) 2003-2023, The AROS Development Team. All rights reserved.
*/

/* This is based on muimaster/class/text.c (first string version)
 * and on rom/intuition/str*.c. Rewritten for the 68000 in 2026 (OpenMUI's
 * rewrite round 6): the instance data is MUI 3.8's size (what only some
 * Strings need is in an extension, string_private.h), the clipboard, the
 * context menu and BetterString's Insert are in the String module
 * (stringx.c, Zstring.mui), MUI 4/5's own Textinput attributes in the MUI
 * 4/5 module (mui45.c), and a typed key's draw makes fewer text calls.
 */

#define MUIMASTER_YES_INLINE_STDARG

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stddef.h>

#include <exec/types.h>
#include <libraries/gadtools.h>
#include <intuition/sghooks.h>
#include <intuition/cghooks.h>
#include <clib/alib_protos.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/muimaster.h>
#include <proto/locale.h>

#include "mui.h"
#include "muimaster_intern.h"
#include "support.h"
#include "prefs.h"
#include "penspec.h"
#include "imspec.h"
#include "string.h"

//#define MYDEBUG 1
#include "debug.h"

extern struct Library *MUIMasterBase;

#include "classes/string_private.h"
#include "modules.h"

#define SECRET_CHAR '*'

/* The Textinput attributes (MUI 4/5's, classes/textinput.h): a range */
#define TI_FIRST MUIA_Textinput_Multiline
#define TI_LAST MUIA_Textinputscroll_HorizBar
#define IS_TI(tag) ((ULONG)((tag) - TI_FIRST) <= (ULONG)(TI_LAST - TI_FIRST))

#define MULTILINE(data) ((data)->msd_Flags & MSDF_MULTILINE)
#define ACTIVE(data) ((data)->msd_Flags & MSDF_ACTIVE)

/****** String.mui/MUIA_String_Accept ****************************************
*
*   NAME
*       MUIA_String_Accept -- (V4) [ISG], STRPTR
*
*   FUNCTION
*       A string containing the characters to be accepted by the string
*       gadget. If NULL is specified, all characters are accepted. The
*       character list is case sensitive.
*
*   NOTES
*       MUIA_String_Reject takes precedence over MUIA_String_Accept.
*
*       The specified string is not cached within the object.
*
*   SEE ALSO
*       MUIA_String_Reject
*
******************************************************************************
*
*/

/****** String.mui/MUIA_String_Reject ****************************************
*
*   NAME
*       MUIA_String_Reject -- (V4) [ISG], STRPTR
*
*   FUNCTION
*       A string containing the characters to be rejected by the string
*       gadget. If NULL is specified, no characters are rejected. The
*       character list is case sensitive.
*
*   NOTES
*       MUIA_String_Reject takes precedence over MUIA_String_Accept.
*
*       The specified string is not cached within the object.
*
*   SEE ALSO
*       MUIA_String_Accept
*
******************************************************************************
*
*/

/****** String.mui/MUIA_String_BufferPos *************************************
*
*   NAME
*       MUIA_String_BufferPos -- (V4) [ISG], LONG
*
*   FUNCTION
*       The cursor's position relative to the start of the string.
*
*   NOTES
*
******************************************************************************
*
*/

/* The extension of a String (string_private.h), made when first needed;
 * NULL without memory */
static __attribute__((noinline)) struct MUI_StringExt *string_ext(
    struct MUI_StringData *data)
{
    struct MUI_StringExt *x = data->ext;

    if (!x && (x = AllocVec(sizeof(*x), MEMF_ANY | MEMF_CLEAR)))
    {
        x->TiWordWrap = 70;
        data->ext = x;
    }
    return x;
}

/* The settings' string background, unless the program gave the String
 * its own: MUI 4 and 5 keep that, MUI 3.8 does not (probe/strbg.c) */
static __attribute__((noinline)) void string_set_back(
    register Object *obj __asm("a2"),
    register struct MUI_StringData *data __asm("a1"),
    register IPTR spec __asm("d0"))
{
    if (!(data->msd_Flags & MSDF_OWNBACK) || !ZUNE_APP_MUI45(obj))
        set(obj, MUIA_Background, spec);
}

/* Active or not: the flag, then the settings' background of the state */
static __attribute__((noinline)) void string_activity(
    register Object *obj __asm("a2"),
    register struct MUI_StringData *data __asm("a1"),
    register ULONG active __asm("d0"))
{
    struct ZunePrefsNew *prefs = muiGlobalInfo(obj)->mgi_Prefs;

    if (active)
        data->msd_Flags |= MSDF_ACTIVE;
    else
        data->msd_Flags &= ~MSDF_ACTIVE;
    string_set_back(obj, data, (IPTR)(active ? prefs->string_bg_active
        : prefs->string_bg_inactive));
}

/* A secret's stars: SecBuffer, BufferSize - 1 of them; FALSE without
 * memory (the String stays as it was) */
static BOOL Buffer_SetSecret(struct MUI_StringData *data, BOOL secret)
{
    struct MUI_StringExt *x;

    if (secret)
    {
        if (!(x = string_ext(data)))
            return FALSE;
        if (!x->SecBuffer)
        {
            if (!(x->SecBuffer = AllocVec(data->BufferSize, MEMF_ANY)))
                return FALSE;
            memset(x->SecBuffer, SECRET_CHAR, data->BufferSize - 1);
        }
        data->msd_Flags |= MSDF_SECRET;
    }
    else
        data->msd_Flags &= ~MSDF_SECRET;
    return TRUE;
}

/**************************************************************************
 Buffer_SetNewContents
 Initialize buffer with a string, replace former content if any. Returns
 TRUE if value actually changed.
**************************************************************************/
static BOOL Buffer_SetNewContents(struct MUI_StringData *data,
    CONST_STRPTR str)
{
    if (NULL == data->Buffer)
        return FALSE;

    if (NULL == str)
    {
        data->Buffer[0] = 0;
        data->NumChars = 0;
    }
    else
    {
        if (!strcmp(data->Buffer, str))
            return FALSE;

        data->NumChars = strlen(str);
        if (data->NumChars >= data->BufferSize)
            data->NumChars = data->BufferSize - 1;

        strncpy(data->Buffer, str, data->BufferSize);
        data->Buffer[data->BufferSize - 1] = 0;
    }

    // avoid to BufferPos jumps to end of string if characters are inserted
    // in string
    if (data->BufferPos > data->NumChars)
        data->BufferPos = data->NumChars;
    data->DispPos = 0;
    if (data->ext)
        data->ext->MlTopLine = data->ext->MlLeftCol = 0;
    data->msd_Flags |= MSDF_ACKED;
    return TRUE;
}

/**************************************************************************
 Multiline: lines end at '\n'. Columns are character counts.
**************************************************************************/
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

/* A page of a Multiline String: its rows but one, at least one */
static __attribute__((noinline)) LONG Ml_Page(Object *obj)
{
    return MAX(1, MAX(1, _mheight(obj) / _font(obj)->tf_YSize) - 1);
}

/* The same column delta lines away, or the nearest line's end */
static __attribute__((noinline)) LONG Ml_Vertical(
    struct MUI_StringData *data, LONG pos, LONG delta)
{
    LONG start = Ml_LineStart(data, pos), column = pos - start, end;

    for (; delta < 0 && start > 0; delta++)
        start = Ml_LineStart(data, start - 1);
    for (; delta > 0; delta--)
    {
        end = Ml_LineEnd(data, start);
        if (end >= (LONG)data->NumChars)
            break;
        start = end + 1;
    }
    return MIN(start + column, Ml_LineEnd(data, start));
}

/**************************************************************************
 Buffer_AddChar
 Add a char on cursor position
**************************************************************************/
static BOOL Buffer_AddChar(struct MUI_StringData *data, unsigned char code)
{
    STRPTR buf = data->Buffer, dst;
    ULONG pos = data->BufferPos, n = data->NumChars;

    if (buf == NULL || n + 1 >= data->BufferSize)
        return FALSE;

    /* the rest one place on, from its end (with the terminator) */
    dst = buf + n + 1;
    *dst = 0;
    while (--dst > buf + pos)
        *dst = dst[-1];
    *dst = code;

    data->BufferPos = pos + 1;
    data->NumChars = n + 1;
    return TRUE;
}

static LONG Buffer_GetWordStartIndex(struct MUI_StringData *data,
    LONG index)
{
    while (index > 0 && data->Buffer[index - 1] != ' ')
        index--;
    return index;
}

static inline __attribute__((always_inline)) LONG Buffer_GetWordEndIndex(
    struct MUI_StringData *data, LONG index)
{
    while (index < data->NumChars && data->Buffer[index] != ' ')
        index++;
    return index;
}

static LONG Buffer_GetPrevWordIndex(struct MUI_StringData *data,
    LONG index)
{
    while (index > 0)
    {
        index--;
        if ((index == 0) || ((data->Buffer[index - 1] == ' ') &&
                (data->Buffer[index] != ' ')))
            break;
    }
    return index;
}

static LONG Buffer_GetSuccWordIndex(struct MUI_StringData *data,
    LONG index)
{
    while (index < data->NumChars)
    {
        index++;
        if ((index == data->NumChars) || ((data->Buffer[index - 1] == ' ') &&
                (data->Buffer[index] != ' ')))
            break;
    }
    return index;
}

/* The marked range, ordered: start up to, not including, stop */
static BOOL Buffer_GetMarkedRange(struct MUI_StringData *data, LONG *start,
    LONG *stop)
{
    LONG markstart, markstop;

    if (TI_MARKED(data))
    {
        struct MUI_StringExt *x = data->ext;

        markstart = MIN(x->TiMarkStart, x->TiMarkEnd);
        markstop = MAX(x->TiMarkStart, x->TiMarkEnd);
        markstart = MIN(markstart, (LONG)data->NumChars);
        markstop = MIN(markstop + 1, (LONG)data->NumChars);
        if (markstart >= markstop)
            return FALSE;
    }
    else
    {
        markstart = MIN(data->MarkPos, data->NumChars);
        markstop = MIN(data->BufferPos, data->NumChars);
        if (markstart > markstop)
        {
            LONG t = markstart;
            markstart = markstop;
            markstop = t;
        }
        if (data->MultiClick == 1)
        {
            /* word select */
            markstart = Buffer_GetWordStartIndex(data, markstart);
            markstop = Buffer_GetWordEndIndex(data, markstop);
        }
        else if (data->MultiClick)
        {
            /* select all */
            markstart = 0;
            markstop = data->NumChars;
        }
        if (markstart == markstop)
            return FALSE;
    }
    if (start)
        *start = markstart;
    if (stop)
        *stop = markstop;
    return TRUE;
}

/* BetterString's signed length is relative to the cursor, which stays
 * in place. There is no special select-all value: -1 selects one character
 * to the left. Clamp before adding so LONG_MIN/LONG_MAX cannot wrap.
 */
static void Buffer_Select(struct MUI_StringData *data, LONG length)
{
    ULONG cursor = MIN(data->BufferPos, data->NumChars);
    ULONG extent;

    data->BufferPos = cursor;
    if (length < 0)
    {
        extent = 0UL - (ULONG)length;
        data->MarkPos = cursor - MIN(extent, cursor);
    }
    else
    {
        extent = MIN((ULONG)length, data->NumChars - cursor);
        data->MarkPos = cursor + extent;
    }
    data->MultiClick = 0;
    data->msd_Flags &= ~(MSDF_MARKING | MSDF_KEYMARKING | MSDF_TIMARK);
    if (data->MarkPos != cursor) data->msd_Flags |= MSDF_MARKING;
    data->msd_RedrawReason = MOVE_CURSOR;
}

static BOOL Buffer_KillMarked(struct MUI_StringData *data)
{
    LONG markstart, markstop;
    BOOL marked;

    if (!(data->msd_Flags & MSDF_MARKING))
        return FALSE;
    marked = Buffer_GetMarkedRange(data, &markstart, &markstop);
    data->msd_Flags &= ~(MSDF_MARKING | MSDF_TIMARK);
    if (!marked)
        return FALSE;
    memmove(&data->Buffer[markstart], &data->Buffer[markstop],
        data->NumChars - markstop + 1);
    data->NumChars -= markstop - markstart;
    data->BufferPos = markstart;
    return TRUE;
}

/* Insert up to len characters at pos; returns the number inserted */
static LONG Buffer_InsertText(struct MUI_StringData *data, ULONG pos,
    CONST_STRPTR text, LONG len)
{
    LONG count = 0;

    if (!text)
        return 0;
    while ((len < 0 || count < len) && text[count])
        count++;
    count = MIN(count, (LONG)(data->BufferSize - 1 - data->NumChars));
    if (count <= 0)
        return 0;
    pos = MIN(pos, data->NumChars);
    memmove(&data->Buffer[pos + count], &data->Buffer[pos],
        data->NumChars - pos + 1);
    memcpy(&data->Buffer[pos], text, count);
    data->NumChars += count;
    return count;
}

/* Remove the characters from start up to, not including, stop */
static BOOL Buffer_Delete(struct MUI_StringData *data, ULONG start,
    ULONG stop)
{
    stop = MIN(stop, data->NumChars);
    if (start >= stop)
        return FALSE;
    memmove(&data->Buffer[start], &data->Buffer[stop],
        data->NumChars - stop + 1);
    data->NumChars -= stop - start;
    if (data->BufferPos > stop)
        data->BufferPos -= stop - start;
    else if (data->BufferPos > start)
        data->BufferPos = start;
    return TRUE;
}

/* The MUI 4/5 module's attributes (mui45.c), with the extension made
 * first; out of line, so that the call's five arguments stay off the
 * callers' frames */
static __attribute__((noinline)) IPTR String_Mui45(Object *obj,
    struct MUI_StringData *data, ULONG attr, struct TagItem *tag,
    IPTR *store)
{
    if (!string_ext(data))
        return FALSE;
    return zune_string_mui45(obj, data, attr, tag, store);
}

/* A Textinput attribute that is a String attribute by another name (OM_NEW,
 * OM_SET, OM_GET), or the Textinput attribute itself */
static const ULONG ti_aliases[][2] =
{
    { MUIA_Textinput_Contents, MUIA_String_Contents },
    { MUIA_Textinput_AcceptChars, MUIA_String_Accept },
    { MUIA_Textinput_RejectChars, MUIA_String_Reject },
    { MUIA_Textinput_AttachedList, MUIA_String_AttachedList },
    { MUIA_Textinput_Secret, MUIA_String_Secret },
    { MUIA_Textinput_Integer, MUIA_String_Integer },
    { MUIA_Textinput_AdvanceOnCR, MUIA_String_AdvanceOnCR },
    { MUIA_Textinput_MaxLen, MUIA_String_MaxLen },
    { MUIA_Textinput_Format, MUIA_String_Format },
    { MUIA_Textinput_RemainActive, MUIA_String_StayActive },
};

static __attribute__((noinline)) ULONG ti_alias(ULONG tag)
{
    unsigned i;

    for (i = 0; i < sizeof(ti_aliases) / sizeof(ti_aliases[0]); i++)
        if (ti_aliases[i][0] == tag)
            return ti_aliases[i][1];
    return tag;
}

/* MUIA_String_Placeholder: the extension's, and its flag */
static __attribute__((noinline)) BOOL string_placeholder(
    struct MUI_StringData *data, CONST_STRPTR text)
{
    struct MUI_StringExt *x = data->ext;

    if ((x ? x->placeholder : NULL) == text)
        return FALSE;
    if (!(x = string_ext(data)))
        return FALSE;
    x->placeholder = text;
    data->msd_Flags &= ~MSDF_PLACEHOLDER;
    if (text)
        data->msd_Flags |= MSDF_PLACEHOLDER;
    return TRUE;
}

/* The attributes OM_NEW and OM_SET take alike: a field or a flag */
static __attribute__((noinline)) void string_set_plain(
    struct MUI_StringData *data, struct TagItem *tag)
{
    static const struct { ULONG attr; ULONG what; } plain[] =
    {
        /* what: a flag, or (below 256) the field's offset */
        { MUIA_String_Accept, offsetof(struct MUI_StringData, msd_Accept) },
        { MUIA_String_Reject, offsetof(struct MUI_StringData, msd_Reject) },
        { MUIA_String_AttachedList,
            offsetof(struct MUI_StringData, msd_AttachedList) },
        { MUIA_String_EditHook,
            offsetof(struct MUI_StringData, msd_EditHook) },
        { MUIA_String_LonelyEditHook, MSDF_LONELYEDITHOOK },
        { MUIA_String_AdvanceOnCR, MSDF_ADVANCEONCR },
        { MUIA_String_NoInput, MSDF_NOINPUT },
        { MUIA_String_StayActive, MSDF_STAYACTIVE },
    };
    unsigned i;

    for (i = 0; plain[i].attr != tag->ti_Tag; i++)
        if (i == sizeof(plain) / sizeof(plain[0]) - 1)
        {
            /* BetterString's focus objects */
            if (string_ext(data))
                (&data->ext->KeyUpFocus)[tag->ti_Tag
                    == MUIA_String_KeyDownFocus] = (Object *)tag->ti_Data;
            return;
        }
    if (plain[i].what < 256)
        *(IPTR *)((UBYTE *)data + plain[i].what) = tag->ti_Data;
    else
        _handle_bool_tag(data->msd_Flags, tag->ti_Data, plain[i].what);
}

/**************************************************************************
 OM_NEW
**************************************************************************/
IPTR String__OM_NEW(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct MUI_StringData *data;
    struct TagItem *tags, *tag;
    CONST_STRPTR str = NULL;
    char integerbuf[22];
    BOOL fixed = GetTagData(MUIA_Textinput_Font, MUIV_Textinput_Font_Normal,
        msg->ops_AttrList) == MUIV_Textinput_Font_Fixed;

    obj = (Object *) DoSuperNewTags(cl, obj, NULL,
        MUIA_PointerType, MUIV_PointerType_Text,
        MUIA_ContextMenu, 1,
        fixed ? MUIA_Font : TAG_IGNORE, MUIV_Font_Fixed,
        TAG_MORE, (IPTR) msg->ops_AttrList);
    if (!obj)
        return FALSE;

    data = INST_DATA(cl, obj);
    data->BufferSize = 80;
    data->Columns = -1;
    if (fixed)
        data->msd_Flags = MSDF_FIXEDFONT;

    /* parse initial taglist */
    for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));)
    {
        ULONG t = tag->ti_Tag;

        if (IS_TI(t))
        {
            /* A Textinput attribute: another name of a String one, or
             * MUI 4/5's own (the module); the cursor is BufferPos here */
            if (t == MUIA_Textinput_CursorPos)
                t = MUIA_String_BufferPos;
            else if ((t = ti_alias(t)) == tag->ti_Tag)
            {
                String_Mui45(obj, data, t, tag, NULL);
                continue;
            }
        }
        switch (t)
        {
        case MUIA_Background:
            data->msd_Flags |= MSDF_OWNBACK;
            break;

        case MUIA_String_Accept:
        case MUIA_String_Reject:
        case MUIA_String_AttachedList:
        case MUIA_String_EditHook:
        case MUIA_String_LonelyEditHook:
        case MUIA_String_AdvanceOnCR:
        case MUIA_String_NoInput:
        case MUIA_String_StayActive:
        case MUIA_String_KeyUpFocus:   /* BetterString */
        case MUIA_String_KeyDownFocus:
            string_set_plain(data, tag);
            break;

        case MUIA_String_Placeholder:
        case MUIA_String_InactiveContents:
            string_placeholder(data, (CONST_STRPTR)tag->ti_Data);
            break;

        case MUIA_String_BufferPos:
            data->BufferPos = (ULONG) tag->ti_Data;
            break;

        case MUIA_String_Secret:
            /* (asked for: the stars are made with the buffer) */
            _handle_bool_tag(data->msd_Flags, (BOOL) tag->ti_Data,
                MSDF_SECRET);
            break;

        case MUIA_String_Contents:
            str = (CONST_STRPTR) tag->ti_Data;
            break;

        case MUIA_String_Format:
            data->msd_Align = (UBYTE) tag->ti_Data;
            break;

        case MUIA_String_Integer64:    /* MUI 4/5: in the module */
            if (tag->ti_Data && String_Mui45(obj, data, t,
                tag, (IPTR *)integerbuf))
                str = integerbuf;
            break;

        case MUIA_String_Integer:
            snprintf(integerbuf, 19, "%ld", tag->ti_Data);
            str = integerbuf;
            break;

        case MUIA_String_MaxLen:
            data->BufferSize = tag->ti_Data;
            if (data->BufferSize < 1)
                data->BufferSize = 1;
            break;

        case MUIA_String_Columns:      /* BetterString */
            data->Columns = (WORD) tag->ti_Data;
            break;

        }
    }

    if ((data->Buffer = AllocVec(data->BufferSize, MEMF_ANY))
        && (!(data->msd_Flags & MSDF_SECRET)
            || Buffer_SetSecret(data, TRUE)))
    {
        data->Buffer[0] = 0;    /* Buffer_SetNewContents compares with it */
        Buffer_SetNewContents(data, str);
        if (data->BufferPos > data->NumChars) /* MUIA_String_BufferPos */
            data->BufferPos = data->NumChars;
        if (data->NumChars)
            data->msd_Flags |= MSDF_CHANGED;
        /* Marks given at creation refer to the initial contents */
        if (data->msd_Flags & MSDF_TIMARK)
            data->msd_Flags |= MSDF_MARKING;
    }
    else
    {
        CoerceMethod(cl, obj, OM_DISPOSE);
        return 0;
    }

    data->ehn.ehn_Events = IDCMP_MOUSEBUTTONS;
    data->ehn.ehn_Flags = MUI_EHF_GUIMODE;
    data->ehn.ehn_Object = obj;
    data->ehn.ehn_Class = cl;

    CurrentTime(&data->OldClick_Sec, &data->OldClick_Micro);

    return (IPTR) obj;
}

/**************************************************************************
 OM_DISPOSE
**************************************************************************/
/* the frees, out of its frame */
static __attribute__((noinline)) void string_free(
    register struct MUI_StringData *data __asm("a1"))
{
    struct MUI_StringExt *x = data->ext;

    FreeVec(data->Buffer);
    if (x)
    {
        if (x->edit_menu) MUI_DisposeObject(x->edit_menu);
        FreeVec(x->SecBuffer);
        FreeVec(x->gadget);
        FreeVec(x);
    }
}

IPTR String__OM_DISPOSE(struct IClass *cl, Object *obj, Msg msg)
{
    string_free(INST_DATA(cl, obj));
    return DoSuperMethodA(cl, obj, msg);
}

/**************************************************************************
 OM_SET
**************************************************************************/
/* MUIA_String_Integer: the number as the contents (a set of its own, as a
 * subclass may want it); FALSE when the value did not change. Out of
 * OM_SET's frame. */
static __attribute__((noinline)) BOOL string_set_integer(Object *obj,
    IPTR value)
{
    char buf[20];
    IPTR oldval, newval;
    ULONG have_oldval;

    have_oldval = get(obj, MUIA_String_Integer, &oldval);
    snprintf(buf, 19, "%ld", value);
    set(obj, MUIA_String_Contents, buf);
    return !(get(obj, MUIA_String_Integer, &newval)
        && have_oldval && oldval == newval);
}

IPTR String__OM_SET(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct MUI_StringData *data = INST_DATA(cl, obj);
    struct TagItem *tags = msg->ops_AttrList;
    struct TagItem *tag;
    ULONG alias = TAG_IGNORE;
    IPTR result;
    BOOL update = FALSE, setpos = FALSE;

    while ((tag = NextTagItem(&tags)) != NULL)
        if (tag->ti_Tag == MUIA_String_BufferPos
            || tag->ti_Tag == MUIA_Textinput_CursorPos)
            setpos = TRUE;
    tags = msg->ops_AttrList;

    while ((tag = NextTagItem(&tags)) != NULL)
    {
        ULONG t = tag->ti_Tag;

        if (IS_TI(t) && (t = ti_alias(t)) == tag->ti_Tag)
        {
            /* MUI 4/5's own Textinput attributes, in the module: 2 when
             * the String is to be redrawn */
            if (String_Mui45(obj, data, t, tag, NULL) == 2)
                update = TRUE;
            continue;
        }
        switch (t)
        {
        case MUIA_Text_Contents: /* MUI5 */
        case MUIA_String_Contents:
        {
            /* As in MUI 21.227, the cursor keeps its distance from the
             * end unless the same call sets its position. */
            LONG keep = (LONG)data->NumChars - (LONG)data->BufferPos;

            if (Buffer_SetNewContents(data, (STRPTR) tag->ti_Data))
            {
                if (!setpos)
                    data->BufferPos = MAX(0, (LONG)data->NumChars - keep);
                data->msd_RedrawReason = NEW_CONTENTS;
                data->msd_Flags &= ~(MSDF_MARKING | MSDF_KEYMARKING |
                    MSDF_TIMARK);
                data->msd_Flags |= MSDF_CHANGED;
                MUI_Redraw(obj, MADF_DRAWOBJECT);
                /* Notify both names of the contents */
                alias = tag->ti_Tag == MUIA_Textinput_Contents ?
                    MUIA_String_Contents : MUIA_Textinput_Contents;
            }
            else
            {
                /* Attribute value not changed, don't fire notification */
                tag->ti_Tag = TAG_IGNORE;
            }
            break;
        }

        case MUIA_String_Accept:
        case MUIA_String_Reject:
        case MUIA_String_AttachedList:
        case MUIA_String_EditHook:
        case MUIA_String_LonelyEditHook:
        case MUIA_String_AdvanceOnCR:
        case MUIA_String_NoInput:
        case MUIA_String_StayActive:
        case MUIA_String_KeyUpFocus:   /* BetterString */
        case MUIA_String_KeyDownFocus:
            string_set_plain(data, tag);
            break;

        case MUIA_String_Secret:
            if ((BOOL)!!tag->ti_Data != !!(data->msd_Flags & MSDF_SECRET))
            {
                Buffer_SetSecret(data, !!tag->ti_Data);
                data->msd_RedrawReason = NEW_CONTENTS;
                MUI_Redraw(obj, MADF_DRAWUPDATE);
            }
            break;

        case MUIA_String_Integer64:     /* MUI 4/5: in the module */
            String_Mui45(obj, data, t, tag, NULL);
            break;

        case MUIA_String_Placeholder:
        case MUIA_String_InactiveContents:
            if (!string_placeholder(data, (CONST_STRPTR)tag->ti_Data))
                tag->ti_Tag = TAG_IGNORE;
            else
                MUI_Redraw(obj, MADF_DRAWOBJECT);
            break;

        case MUIA_String_Integer:
            if (!string_set_integer(obj, tag->ti_Data))
                tag->ti_Tag = TAG_IGNORE; /* Attribute value not changed */
            break;

        case MUIA_String_BufferPos:
            /* Inside the text, as Textinput_CursorPos: Delete and
             * Backspace move NumChars - BufferPos bytes */
            data->BufferPos = MIN((ULONG)tag->ti_Data, data->NumChars);
            data->msd_Flags &= ~(MSDF_MARKING | MSDF_TIMARK);
            data->msd_RedrawReason = MOVE_CURSOR;
            MUI_Redraw(obj, MADF_DRAWUPDATE);
            break;

        case MUIA_String_DisplayPos:
            /* Scrolling the view must not move the editing cursor. Drawing
             * may bring it back into view when the gadget is active. */
            data->DispPos = MIN((ULONG)tag->ti_Data, data->NumChars);
            data->msd_RedrawReason = MOVE_CURSOR;
            MUI_Redraw(obj, MADF_DRAWUPDATE);
            break;

        /* MUI's Newstring wrapper forwards its selection anchor to String. */
        case 0x81cd0025: /* MUIA_Newstring_StartPos */
            Buffer_Select(data, (LONG)MIN(tag->ti_Data, data->NumChars) -
                (LONG)MIN(data->BufferPos, data->NumChars));
            MUI_Redraw(obj, MADF_DRAWUPDATE);
            break;

        case MUIA_String_SelectSize:   /* BetterString */
            Buffer_Select(data, (LONG)tag->ti_Data);
            MUI_Redraw(obj, MADF_DRAWUPDATE);
            break;
        }
    }

    if (update)
        MUI_Redraw(obj, MADF_DRAWUPDATE);
    result = DoSuperMethodA(cl, obj, (Msg) msg);
    if (alias != TAG_IGNORE)
        SetSuperAttrs(cl, obj, MUIA_NoNotify,
            GetTagData(MUIA_NoNotify, FALSE, msg->ops_AttrList),
            alias, (IPTR)data->Buffer, TAG_DONE);
    return result;
}


/**************************************************************************
 OM_GET
**************************************************************************/
/* OM_GET of the module's attributes: three arguments, so that the call's
 * five stay off OM_GET's frame */
static __attribute__((noinline)) IPTR String_Get45(Object *obj,
    struct MUI_StringData *data, struct opGet *msg)
{
    return String_Mui45(obj, data, msg->opg_AttrID, NULL, msg->opg_Storage);
}

/* MUIA_String_Integer: the contents (as the subclass gives them) as a
 * number, 0 when not one */
static __attribute__((noinline)) IPTR string_get_integer(Object *obj)
{
    STRPTR buf = NULL;
    LONG val = 0;

    get(obj, MUIA_String_Contents, &buf);
    if (NULL != buf)
        StrToLong(buf, &val);
    return val;
}

/* BetterString's MUIA_String_SelectSize: the marked length, negative
 * when the mark is before the cursor */
static __attribute__((noinline)) IPTR string_select_size(
    struct MUI_StringData *data)
{
    LONG markstart, markstop, size = 0;

    if ((data->msd_Flags & MSDF_MARKING)
        && Buffer_GetMarkedRange(data, &markstart, &markstop))
    {
        size = markstop - markstart;
        if (data->MarkPos < data->BufferPos)
            size = -size;
    }
    return size;
}

IPTR String__OM_GET(struct IClass *cl, Object *obj, struct opGet *msg)
{
    struct MUI_StringData *data = INST_DATA(cl, obj);
    IPTR *store = msg->opg_Storage;
    ULONG id = msg->opg_AttrID;
    IPTR value;

    if (IS_TI(id) && (id = ti_alias(id)) == msg->opg_AttrID)
        /* MUI 4/5's own: in the module, which answers the rest of the
         * range with 0 */
        return String_Get45(obj, data, msg);

    switch (id)
    {
    case MUIA_Text_Contents: /* MUI5 */
    case MUIA_String_Contents:
        value = (IPTR) data->Buffer;
        break;

    case MUIA_String_Integer64:     /* MUI 4/5 */
    case MUIA_Gadget_Gadget:        /* MUI 3.8's */
        return String_Get45(obj, data, msg);

    case MUIA_String_EditHook:
        value = (IPTR)data->msd_EditHook;
        break;
    case MUIA_String_LonelyEditHook:
        value = !!(data->msd_Flags & MSDF_LONELYEDITHOOK);
        break;
    case MUIA_String_Secret:
        value = !!(data->msd_Flags & MSDF_SECRET);
        break;
    case MUIA_String_Acknowledge:
        value = data->msd_Flags & MSDF_ACKED ? (IPTR) data->Buffer : 0;
        break;
    case MUIA_String_Accept:
        value = (IPTR) data->msd_Accept;
        break;
    case MUIA_String_Reject:
        value = (IPTR) data->msd_Reject;
        break;
    case MUIA_String_AttachedList:
        value = (IPTR) data->msd_AttachedList;
        break;
    case MUIA_String_Format:
        value = data->msd_Align;
        break;
    case MUIA_String_Placeholder:
    case MUIA_String_InactiveContents:
        value = data->ext ? (IPTR)data->ext->placeholder : 0;
        break;

    case MUIA_String_Integer:
        value = string_get_integer(obj);
        break;

    case MUIA_String_MaxLen:
        value = data->BufferSize;
        break;
    case MUIA_String_AdvanceOnCR:
        value = !!(data->msd_Flags & MSDF_ADVANCEONCR);
        break;
    case MUIA_String_BufferPos:
        value = data->BufferPos;
        break;
    case MUIA_String_DisplayPos:
        value = data->DispPos;
        break;
    case MUIA_String_NoInput:  /* BetterString */
        value = !!(data->msd_Flags & MSDF_NOINPUT);
        break;
    case MUIA_String_StayActive:       /* BetterString */
        value = !!(data->msd_Flags & MSDF_STAYACTIVE);
        break;
    case MUIA_String_KeyUpFocus:       /* BetterString */
    case MUIA_String_KeyDownFocus:
        value = data->ext
            ? (IPTR)(&data->ext->KeyUpFocus)[id == MUIA_String_KeyDownFocus]
            : 0;
        break;
    case 0x81cd0025: /* MUIA_Newstring_StartPos */
        value = data->msd_Flags & MSDF_MARKING ? data->MarkPos
            : data->BufferPos;
        break;
    case MUIA_String_SelectSize:       /* BetterString */
        value = string_select_size(data);
        break;
    default:
        return DoSuperMethodA(cl, obj, (Msg) msg);
    }
    *store = value;
    return TRUE;
}

/**************************************************************************
 MUIM_Setup
**************************************************************************/
/* The settings' string pens, in SP_* order */
static const UWORD string_pen_specs[SP_COUNT] =
{
    offsetof(struct ZunePrefsNew, string_text_inactive),
    offsetof(struct ZunePrefsNew, string_text_active),
    offsetof(struct ZunePrefsNew, string_text_marked),
    offsetof(struct ZunePrefsNew, string_bg_marked),
    offsetof(struct ZunePrefsNew, string_cursor),
};

/* Each pen found for the window (zune_penspec_setup); the ones obtained
 * from the colour map are given back at Cleanup */
static __attribute__((noinline)) void string_pens_setup(Object *obj,
    struct MUI_StringData *data)
{
    struct MUI_PenSpec_intern psi;
    UBYTE *prefs = (UBYTE *)muiGlobalInfo(obj)->mgi_Prefs;
    int i;

    data->pens_obtained = 0;
    for (i = 0; i < SP_COUNT; i++)
    {
        memset(&psi, 0, sizeof(psi));
        zune_pen_spec_to_intern(*(const struct MUI_PenSpec **)
            (prefs + string_pen_specs[i]), &psi);
        zune_penspec_setup(&psi, muiRenderInfo(obj));
        data->pens[i] = psi.p_pen;
        if (psi.p_is_allocated)
            data->pens_obtained |= 1 << i;
    }
}

IPTR String__MUIM_Setup(struct IClass *cl, Object *obj,
    struct MUIP_Setup *msg)
{
    struct MUI_StringData *data = INST_DATA(cl, obj);

    if (0 == DoSuperMethodA(cl, obj, (Msg) msg))
        return FALSE;

    /* A program that gets MUI 3.8 has no Textinput methods to catch: its
     * keys edit directly (String_Key) */
    data->msd_Flags &= ~MSDF_DIRECTKEYS;
    if (ZUNE_APP_MUI38(obj))
        data->msd_Flags |= MSDF_DIRECTKEYS;
    string_activity(obj, data, FALSE);
    string_pens_setup(obj, data);

    DoMethod(_win(obj), MUIM_Window_AddEventHandler, (IPTR) & data->ehn);
    return TRUE;
}

/**************************************************************************
 MUIM_Cleanup
**************************************************************************/
IPTR String__MUIM_Cleanup(struct IClass *cl, Object *obj,
    struct MUIP_Cleanup *msg)
{
    struct MUI_StringData *data = INST_DATA(cl, obj);
    ULONG obtained = data->pens_obtained;
    UBYTE *pen = data->pens;

    DoMethod(_win(obj), MUIM_Window_RemEventHandler, (IPTR) & data->ehn);
    for (; obtained; obtained >>= 1, pen++)
        if (obtained & 1)
            ReleasePen(muiRenderInfo(obj)->mri_Colormap, *pen);
    data->pens_obtained = 0;

    return DoSuperMethodA(cl, obj, (Msg) msg);
}

/**************************************************************************
 MUIM_AskMinMax
**************************************************************************/
IPTR String__MUIM_AskMinMax(struct IClass *cl, Object *obj,
    struct MUIP_AskMinMax *msg)
{
    struct MUI_StringData *data = INST_DATA(cl, obj);

    DoSuperMethodA(cl, obj, (Msg) msg);

    if (data->Columns >= 0)
    {
        msg->MinMaxInfo->MinWidth += _font(obj)->tf_XSize * data->Columns;
        msg->MinMaxInfo->DefWidth += _font(obj)->tf_XSize * data->Columns;
        msg->MinMaxInfo->MaxWidth += _font(obj)->tf_XSize * data->Columns;
    }
    else
    {
        /* MUI 3.8/3.9 use fixed content widths, independent of font */
        msg->MinMaxInfo->MinWidth += 20;
        msg->MinMaxInfo->DefWidth += 100;
        msg->MinMaxInfo->MaxWidth = MUI_MAXMAX;
    }

    msg->MinMaxInfo->MinHeight += _font(obj)->tf_YSize;
    msg->MinMaxInfo->DefHeight += _font(obj)->tf_YSize;
    msg->MinMaxInfo->MaxHeight += _font(obj)->tf_YSize;
    /* Textinputscroll subclasses add rows on top of this */
    if (MULTILINE(data))
        msg->MinMaxInfo->MaxHeight = MUI_MAXMAX;
    return TRUE;
}


/* The first character that may be shown so that no room is left on the
 * right while characters are scrolled out at the left (measured on the
 * buffer, also for a secret) */
static __attribute__((noinline)) WORD MaxDispPos(Object *obj,
    struct MUI_StringData *data)
{
    struct TextExtent te;
    WORD numchars = data->NumChars;

    if (data->BufferPos == data->NumChars)
        numchars++;             /* Take cursor into account */
    return numchars - TextFit(_rp(obj), &data->Buffer[numchars - 1],
        numchars, &te, NULL, -1, _mwidth(obj), _mheight(obj));
}

/* DispPos and DispCount for the cursor and the box (Intuition's rule (R):
 * no room on the right while characters are scrolled out at the left).
 * The cursor at the end is measured as a space after the text. visual:
 * what the String shows (STRING_VISUAL) */
static __attribute__((noinline)) void UpdateDisp(Object *obj,
    struct MUI_StringData *data,
    STRPTR visual)
{
    struct RastPort *rp = _rp(obj);
    struct TextExtent te;
    ULONG pos = data->BufferPos, n = data->NumChars;

    if (pos == n)
        visual[n] = ' ';

    if (pos < data->DispPos)
    {
        /* 1) Cursor to the left of the visible area */
        WORD max_disppos = MaxDispPos(obj, data);

        data->DispPos = MIN(pos, max_disppos);
    }
    else if ((UWORD)TextLength(rp, visual + data->DispPos,
            pos - data->DispPos + 1) > _mwidth(obj))
    {
        /* 2) More than fits: the cursor at the right */
        data->DispPos = pos - TextFit(rp, visual + pos, n, &te, NULL, -1,
            _mwidth(obj), _mheight(obj)) + 1;
    }
    else if (data->DispPos > 0 || n >= 0x7ffe)
    {
        /* 3) Cursor inside: rule (R). With DispPos 0 nothing is scrolled
         * out (the maximum, a character count, is never below 0) */
        WORD max_disppos = MaxDispPos(obj, data);

        if (data->DispPos > max_disppos)
            data->DispPos = max_disppos;
    }

    /* It might be necessary with special handling for centre aligned gads */
    data->DispCount = TextFit(rp, visual + data->DispPos, n - data->DispPos,
        &te, NULL, 1, _mwidth(obj), _mheight(obj));

    /* 0-terminate string (in case we put a SPACE at the end) */
    visual[n] = '\0';
}

/* The left (right FALSE) or right of the shown text */
static __attribute__((noinline)) UWORD string_text_edge(Object *obj,
    struct MUI_StringData *data, BOOL right)
{
    STRPTR dispstr = STRING_VISUAL(data) + data->DispPos;
    UWORD dispstrlen = MIN(data->DispCount, data->NumChars - data->DispPos);
    struct RastPort *rp = _rp(obj);
    WORD textwidth = 0;

    if (data->msd_Align == MUIV_String_Format_Left)
        return right ? _mleft(obj) + TextLength(rp, dispstr, dispstrlen)
            : _mleft(obj);
    if (data->msd_Align == MUIV_String_Format_Center
        || (data->msd_Align == MUIV_String_Format_Right && !right))
    {
        textwidth = TextLength(rp, dispstr, dispstrlen);
        if (data->BufferPos == data->NumChars)
            textwidth += TextLength(rp, " ", 1);
    }
    if (data->msd_Align == MUIV_String_Format_Center)
        return right ? _mright(obj) - ((_mwidth(obj) - textwidth) / 2)
            : _mleft(obj) + ((_mwidth(obj) - textwidth) / 2);
    if (data->msd_Align == MUIV_String_Format_Right)
        return right ? _mright(obj)
            : _mleft(obj) + (_mwidth(obj) - 1 - textwidth);
    return 0;
}

/* <unmarked><marked><unmarked> */
static __attribute__((noinline)) VOID TextM(Object *obj,
    struct MUI_StringData *data, STRPTR text, WORD textlen, LONG markstart,
    LONG markend)
{
    struct RastPort *rp = _rp(obj);
    ULONG textpen = data->pens[ACTIVE(data) ? SP_ACTIVE : SP_INACTIVE];
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
        SetABPenDrMd(rp, data->pens[SP_MARKTEXT], data->pens[SP_MARKBG],
            JAM2);
        Text(rp, text, len);
        text += len;
        textlen -= len;
    }

    if (textlen)
    {
        SetABPenDrMd(rp, textpen, _pens(obj)[MPEN_BACKGROUND], JAM1);
        Text(rp, text, textlen);
    }
    /* Leave JAM1 behind: classes that draw text without setting the
     * mode (TheBar's labels) follow in the same rastport. */
    SetDrMd(rp, JAM1);
}

/* The rare draws in the MUI 4/5 module, out of the draw's frame: a MUI
 * 3.8 program's string gadget (MUIA_Gadget_Gadget) filled with the
 * background pen the program put into it, or with the object's background
 * on an update; a Multiline String, with its marked range; MUI 5's
 * placeholder */
static __attribute__((noinline)) void string_draw45(Object *obj,
    struct MUI_StringData *data, ULONG what, ULONG textpen)
{
    LONG markstart = 0, markstop = 0;

    if ((what & ZUNE_SD_MULTILINE) && (data->msd_Flags & MSDF_MARKING)
        && Buffer_GetMarkedRange(data, &markstart, &markstop))
        what |= ZUNE_SD_MARKED;
    zune_string_draw45(obj, data, what, textpen, markstart, markstop);
}

/**************************************************************************
 MUIM_Draw
**************************************************************************/
IPTR String__MUIM_Draw(struct IClass *cl, Object *obj,
    struct MUIP_Draw *msg)
{
    struct MUI_StringData *data = INST_DATA(cl, obj);
    struct RastPort *rp;
    STRPTR visual, dispstr;
    UWORD text_left, textleft_save, text_top, dispstrlen, skip;
    ULONG textpen, flags;
    LONG markstart, markstop;

    DoSuperMethodA(cl, obj, (Msg) msg);

    if (!((flags = msg->flags) & (MADF_DRAWUPDATE | MADF_DRAWOBJECT)))
        return 0;

    /* MUI 5's placeholder (MUIA_String_Placeholder, V21) of an empty
     * inactive String: drawn by the MUI 4/5 module */
    if ((data->msd_Flags & (MSDF_ACTIVE | MSDF_PLACEHOLDER))
        == MSDF_PLACEHOLDER && !data->Buffer[0])
    {
        string_draw45(obj, data, ZUNE_SD_PLACEHOLDER, 0);
        data->msd_RedrawReason = NO_REASON;
        return 0;
    }

    visual = STRING_VISUAL(data);
    if (data->msd_Flags & MSDF_SECRET)
        visual[data->NumChars] = '\0';
    rp = _rp(obj);
    SetFont(rp, _font(obj));
    textpen = data->pens[ACTIVE(data) ? SP_ACTIVE : SP_INACTIVE];

    /* the contents may have been changed in the buffer (an edit hook, a
     * program through the gadget) */
    data->NumChars = strlen(data->Buffer);
    if (data->BufferPos > data->NumChars)
        data->BufferPos = data->NumChars;

    /* a Multiline String (MUIA_Textinput_Multiline, MUI 4/5): drawn by the
     * MUI 4/5 module, which scrolls it so that the cursor is visible */
    if (MULTILINE(data))
    {
        string_draw45(obj, data, ZUNE_SD_MULTILINE
            | ((flags & MADF_DRAWUPDATE) ? ZUNE_SD_UPDATE : 0), textpen);
        goto done;
    }

    UpdateDisp(obj, data, visual);

    text_top = _mtop(obj) + ((_mheight(obj) - rp->Font->tf_YSize) >> 1)
        + rp->Font->tf_Baseline;
    dispstr = visual + data->DispPos;
    dispstrlen = MIN(data->DispCount, data->NumChars - data->DispPos);
    textleft_save = text_left = data->msd_Align == MUIV_String_Format_Left
        ? _mleft(obj) : string_text_edge(obj, data, FALSE);
    skip = 0;   /* characters of dispstr left as they are drawn */

    // little flicker improvement, don't redraw first part of string
    // when adding a char
    if ((flags & MADF_DRAWUPDATE) && !(data->msd_Flags & MSDF_GADGET) &&
        data->msd_RedrawReason == DO_ADDCHAR &&
        data->msd_Align == MUIV_String_Format_Left && data->DispPos == 0)
    {
        skip = data->BufferPos - 1;
        text_left += TextLength(rp, dispstr, skip);
        DoMethod(obj, MUIM_DrawBackground, text_left, _mtop(obj),
            _mwidth(obj) - text_left + _mleft(obj), _mheight(obj),
            text_left, _mtop(obj), 0);
    }
    /* a full draw too when a program may have changed the gadget's
     * background pen: Area has filled with the object's background */
    else if (data->msd_Flags & MSDF_GADGET)
        string_draw45(obj, data, ZUNE_SD_GADGET
            | ((flags & MADF_DRAWUPDATE) ? ZUNE_SD_UPDATE : 0), 0);
    else if (flags & MADF_DRAWUPDATE)
    {
        DoMethod(obj, MUIM_DrawBackground, _mleft(obj), _mtop(obj),
            _mwidth(obj), _mheight(obj), _mleft(obj), _mtop(obj), 0);
    }

    SetABPenDrMd(rp, textpen, _pens(obj)[MPEN_BACKGROUND], JAM1);
    Move(rp, text_left, text_top);

    if ((data->msd_Flags & MSDF_MARKING)
        && Buffer_GetMarkedRange(data, &markstart, &markstop))
    {
        /* (the marks as lean measured them, from the shown start) */
        TextM(obj, data, dispstr + skip, dispstrlen - skip,
            markstart - data->DispPos, markstop - data->DispPos);
    }
    else
    {
        Text(rp, dispstr + skip, dispstrlen - skip);

        if (ACTIVE(data))    // active, draw cursor
        {
            UWORD cursoroffset = data->BufferPos - data->DispPos;

            /* Where the shown text ends, Text() has left the pen: it moves
             * it by just what TextLength() measures */
            if (cursoroffset == dispstrlen)
                text_left = rp->cp_x;
            else
                text_left = textleft_save
                    + TextLength(rp, dispstr, cursoroffset);
            SetABPenDrMd(rp, data->pens[SP_ACTIVE], data->pens[SP_CURSOR],
                JAM2);
            Move(rp, text_left, text_top);
            Text(rp, ((data->BufferPos < data->NumChars)
                    ? dispstr + cursoroffset : (STRPTR) " "), 1);
            SetDrMd(rp, JAM1);
        }
    }

done:
    data->msd_RedrawReason = NO_REASON;
    if (data->msd_Flags & MSDF_SECRET)
        visual[data->NumChars] = SECRET_CHAR;

    return TRUE;
}

/**************************************************************************
 Cursor movement of the editing operations. Returns whether the object
 needs redrawing.
**************************************************************************/
static int String_CursorKey(struct MUI_StringData *data, LONG muikey,
    LONG target)
{
    ULONG flags = data->msd_Flags;
    BOOL kills = (flags & (MSDF_MARKING | MSDF_KEYMARKING)) == MSDF_MARKING;
    ULONG pos = data->BufferPos;
    int update = 1;

    /* A Textinput mark goes away and the cursor moves from where it is */
    if (TI_MARKED(data))
    {
        flags &= ~(MSDF_MARKING | MSDF_TIMARK);
        kills = FALSE;
    }
    else
        update = kills;
    if (kills)
        flags &= ~MSDF_MARKING;

    switch (muikey)
    {
    case MUIKEY_LEFT:
        if (kills)
        {
            pos = MIN(pos, data->MarkPos);
            if (pos > 0)
                pos--;
        }
        else if (pos > 0)
        {
            update = 1;
            pos--;
            data->msd_RedrawReason = DO_CURSOR_LEFT;
        }
        break;
    case MUIKEY_RIGHT:
        if (kills)
            pos = MAX(pos, data->MarkPos);
        else if (pos < data->NumChars)
        {
            update = 1;
            pos++;
            data->msd_RedrawReason = DO_CURSOR_RIGHT;
        }
        break;
    case MUIKEY_WORDLEFT:
        if (pos > 0)
        {
            pos = Buffer_GetPrevWordIndex(data, pos);
            update = 1;
            data->msd_RedrawReason = DO_CURSOR_LEFT;
        }
        break;
    case MUIKEY_WORDRIGHT:
        if (pos < data->NumChars)
        {
            pos = Buffer_GetSuccWordIndex(data, pos);
            update = 1;
            data->msd_RedrawReason = DO_CURSOR_RIGHT;
        }
        break;
    case MUIKEY_LINESTART:
        pos = MULTILINE(data) ? Ml_LineStart(data, pos) : 0;
        update = 1;
        break;
    case MUIKEY_LINEEND:
        pos = MULTILINE(data) ? Ml_LineEnd(data, pos) : data->NumChars;
        update = 1;
        break;
    default:   /* MUIKEY_NONE: to target, for multiline movement */
        pos = target;
        update = 1;
        break;
    }
    data->BufferPos = pos;
    data->msd_Flags = flags;
    return update;
}

/* Return in the String: the contents are acknowledged (MUIA_String_
 * Acknowledge and MUIA_Textinput_Acknowledge; MUIA_String_Integer when
 * they are a number). MUIM_Textinput_Acknowledge in a program that gets
 * MUI 4/5 (mui45.c); called directly for MUI 3.8 */
static void String_Acknowledge(struct IClass *cl, Object *obj,
    struct MUI_StringData *data)
{
    LONG val = 0;

    data->msd_Flags |= MSDF_ACKED;
    set(obj, MUIA_String_Acknowledge, data->Buffer);
    SetSuperAttrs(cl, obj, MUIA_Textinput_Acknowledge, (IPTR)data->Buffer,
        TAG_DONE);
    if (StrToLong(data->Buffer, &val) >= 0)
        superset(cl, obj, MUIA_String_Integer, val);
}

static const struct ZuneStringOps string_ops;

/* The clipboard's operations in the String module, out of String_Edit's
 * frame */
static __attribute__((noinline)) ULONG String_Clip(struct IClass *cl,
    Object *obj, ULONG oparg)
{
    return zune_string_clip(cl, obj, oparg & 0xff, oparg >> 8, &string_ops);
}

/**************************************************************************
 The editing: one SE_* operation (string_private.h) on the buffer, the
 cursor and the mark; arg is the character for SE_CHAR. Answers TIP_EDITED
 (the contents changed; MSDF_CHANGED is set) and TIP_UPDATE (a redraw is
 due); the caller notifies and redraws. Every key of a program that gets
 MUI 3.8 comes here straight from MUIM_HandleEvent; the Textinput methods
 (mui45.c) come here for what they share with the keys; the clipboard's
 operations are the String module's (stringx.c). Not inlined: its locals
 stay off HandleEvent's frame, which every key and click passes.
**************************************************************************/
static __attribute__((noinline)) ULONG String_Edit(struct IClass *cl,
    Object *obj, struct MUI_StringData *data, ULONG op, LONG arg)
{
    BOOL writable = !(data->msd_Flags & MSDF_NOINPUT);
    ULONG oldchars = data->NumChars, oldpos = data->BufferPos;
    BOOL edited = FALSE, moved = FALSE;

    switch (op)
    {
    case SE_CHAR:
        /* Control characters are keys, not text; a newline is text in a
         * Multiline String */
        if (!writable || arg == 127 || (arg < 32
            && !(MULTILINE(data) && (arg == '\r' || arg == '\n'))))
            return 0;
        if (((data->msd_Flags & MSDF_ISNUMERIC) && !isdigit(arg)
                && arg != '-')
            || (data->msd_Accept != NULL
                && strchr(data->msd_Accept, arg) == NULL)
            || (data->msd_Reject != NULL && strchr(data->msd_Reject, arg)))
        {
            DisplayBeep(NULL);
            return 0;
        }
        data->msd_RedrawReason = DO_UNKNOWN;
        if (arg < 32)   /* a newline in a Multiline String */
        {
            Buffer_KillMarked(data);
            if (Buffer_AddChar(data, '\n'))
                data->msd_RedrawReason = NEW_CONTENTS;
        }
        else
        {
            /* ASCII 32-126 is printable in every locale; the locale says
             * for the rest */
            if (arg >= 0x7f)
            {
                struct Locale *locale = OpenLocale(NULL);
                BOOL printable = IsPrint(locale, arg);

                CloseLocale(locale);
                if (!printable)
                    break;
            }
            Buffer_KillMarked(data);
            if (Buffer_AddChar(data, arg))
                data->msd_RedrawReason = DO_ADDCHAR;
        }
        edited = data->NumChars != oldchars
            || data->msd_RedrawReason == NEW_CONTENTS;
        break;

    case SE_BS:
    case SE_BSSOL:
        if (!writable)
            return 0;
        if (!Buffer_KillMarked(data) && data->BufferPos > 0)
        {
            if (op == SE_BSSOL)
            {
                Buffer_Delete(data, MULTILINE(data) ?
                    Ml_LineStart(data, data->BufferPos) : 0, data->BufferPos);
                data->msd_RedrawReason = NEW_CONTENTS;
            }
            else
            {
                Buffer_Delete(data, data->BufferPos - 1, data->BufferPos);
                data->msd_RedrawReason = DO_BACKSPACE;
            }
        }
        goto changed;

    case SE_DEL:
    case SE_DELEOL:
        if (!writable)
            return 0;
        if (!Buffer_KillMarked(data))
        {
            if (op == SE_DELEOL)
            {
                Buffer_Delete(data, data->BufferPos, MULTILINE(data) ?
                    Ml_LineEnd(data, data->BufferPos) : data->NumChars);
                data->msd_RedrawReason = NEW_CONTENTS;
            }
            else
            {
                Buffer_Delete(data, data->BufferPos, data->BufferPos + 1);
                data->msd_RedrawReason = DO_DELETE;
            }
        }
    changed:
        edited = data->NumChars != oldchars;
        moved = data->BufferPos != oldpos;
        break;

    case SE_COPY:
    case SE_CUT:
    case SE_PASTE:
        /* in the String module: 1 edited, 2 moved */
        {
            ULONG r = String_Clip(cl, obj, op | (arg ? 0x100 : 0));

            edited = r & 1;
            moved = r & 2;
        }
        break;

    case MUIKEY_UP:
    case MUIKEY_DOWN:
    case MUIKEY_PAGEUP:
    case MUIKEY_PAGEDOWN:
        if (!MULTILINE(data))
            return 0;
        arg = op == MUIKEY_UP || op == MUIKEY_DOWN ? 1 : Ml_Page(obj);
        if (op == MUIKEY_UP || op == MUIKEY_PAGEUP)
            arg = -arg;
        moved = String_CursorKey(data, MUIKEY_NONE,
            Ml_Vertical(data, data->BufferPos, arg));
        break;

    case MUIKEY_TOP:
        moved = String_CursorKey(data, MUIKEY_NONE, 0);
        break;
    case MUIKEY_BOTTOM:
        moved = String_CursorKey(data, MUIKEY_NONE, data->NumChars);
        break;

    case MUIKEY_LEFT:
    case MUIKEY_RIGHT:
    case MUIKEY_WORDLEFT:
    case MUIKEY_WORDRIGHT:
    case MUIKEY_LINESTART:
    case MUIKEY_LINEEND:
        moved = String_CursorKey(data, op, 0);
        break;

    default:
        return 0;
    }

    if (edited)
    {
        if (data->msd_RedrawReason != DO_ADDCHAR || op != SE_CHAR)
            data->msd_RedrawReason = NEW_CONTENTS;
        data->msd_Flags |= MSDF_CHANGED;
        return TIP_EDITED | TIP_UPDATE;
    }
    if (moved)
    {
        if (data->msd_RedrawReason == NO_REASON)
            data->msd_RedrawReason = MOVE_CURSOR;
        return TIP_UPDATE;
    }
    return 0;
}

/* What the modules call (string_private.h); the word end out of line
 * here only (inlined into Buffer_GetMarkedRange, whose frame would grow by
 * the call) */
static LONG String_WordEnd(struct MUI_StringData *data, LONG index)
{
    return Buffer_GetWordEndIndex(data, index);
}

static const struct ZuneStringOps string_ops =
{
    String_Edit, Buffer_KillMarked, Buffer_Delete, Buffer_InsertText,
    Buffer_GetPrevWordIndex, Buffer_GetSuccWordIndex,
    String_WordEnd, Buffer_GetMarkedRange, clipboard_write_text,
    String_Acknowledge, string_ext, Buffer_AddChar, Buffer_Select
};

/* The Textinput methods (MUI 4/5): in the module; a program that gets
 * MUI 3.8 never sends one, its keys edit directly. Out of line: the
 * dispatcher's frame, on every String method, does not grow by the
 * fourth argument */
static __attribute__((noinline)) IPTR String_Textinput45(struct IClass *cl,
    Object *obj, Msg msg)
{
    return zune_string_textinput(cl, obj, msg, &string_ops);
}

/* The String module's methods: BetterString's Insert; the default context
 * menu (a menu the program gave is Area's) */
static __attribute__((noinline)) IPTR String_Module(struct IClass *cl,
    Object *obj, Msg msg)
{
    IPTR menu = 1;

    if (msg->MethodID != MUIM_String_Insert)
    {
        get(obj, MUIA_ContextMenu, &menu);
        if (menu != 1)
            return DoSuperMethodA(cl, obj, msg);
    }
    return zune_string_method(cl, obj, msg, &string_ops);
}

/* In a program that gets MUI 4/5 a key is a Textinput method, so that a
 * subclass sees it (mui45.c answers it through string_ops.edit) */
static const ULONG ti_methods[SE_COUNT - MUIKEY_UP] =
{
    MUIM_Textinput_DoUp, MUIM_Textinput_DoDown, MUIM_Textinput_DoPageUp,
    MUIM_Textinput_DoPageDown, MUIM_Textinput_DoTop,
    MUIM_Textinput_DoBottom, MUIM_Textinput_DoLeft,
    MUIM_Textinput_DoRight, MUIM_Textinput_DoPrevWord,
    MUIM_Textinput_DoNextWord, MUIM_Textinput_DoLineStart,
    MUIM_Textinput_DoLineEnd, MUIM_Textinput_HandleChar,
    MUIM_Textinput_DoBS, MUIM_Textinput_DoBSSOL, MUIM_Textinput_DoDel,
    MUIM_Textinput_DoDelEOL, MUIM_Textinput_DoCopy, MUIM_Textinput_DoCut,
    MUIM_Textinput_DoPaste, MUIM_Textinput_DoUndo
};

/* A key's editing: directly for MUI 3.8, else as the Textinput method,
 * collecting what the method reports (TIP_*) instead of redrawing */
static ULONG String_Key(struct IClass *cl, Object *obj,
    struct MUI_StringData *data, ULONG op, LONG arg)
{
    ULONG pending;

    if (data->msd_Flags & MSDF_DIRECTKEYS)
        return String_Edit(cl, obj, data, op, arg);
    data->msd_Flags = (data->msd_Flags & ~MSDF_TIPENDING) | MSDF_INEVENT;
    DoMethod(obj, ti_methods[op - MUIKEY_UP], arg, 0);
    pending = (data->msd_Flags & MSDF_TIPENDING) >> MSDF_TIP_SHIFT;
    data->msd_Flags &= ~(MSDF_INEVENT | MSDF_TIPENDING);
    return pending;
}

/* The operation of a typed character, as MUI 5 maps the keys; -1: not
 * one String handles */
static LONG String_KeyOp(UBYTE code, UWORD qual)
{
    BOOL shift = (qual & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) != 0;

    if (qual & IEQUALIFIER_RCOMMAND)
    {
        switch (ToLower(code))
        {
        case 'c': return SE_COPY;
        case 'x': return SE_CUT;
        case 'v': return SE_PASTE;
        case 'z': return SE_NONE;   /* MUI 5's Undo */
        }
        return -1;
    }
    if (code >= 32 && (code < 128 || code >= 160))
        return code == 127 ? (shift ? SE_DELEOL : SE_DEL) : SE_CHAR;
    switch (code)
    {
    case '\b': return shift ? SE_BSSOL : SE_BS;
    case 21: /* Ctrl-U */ return SE_BSSOL;
    case 11: /* Ctrl-K */ return SE_DELEOL;
    case 24: /* Ctrl-X */ return SE_CUT;
    case 1: /* Ctrl-A */ return MUIKEY_LINESTART;
    case 26: /* Ctrl-Z */ return MUIKEY_LINEEND;
    case '\r': return SE_CHAR;
    }
    return -1;
}

/* Whether a key release is of a key the String takes when pressed: a
 * character it edits with, or a cursor key (the window has converted the
 * press code for this event already: ConvertKey's cache answers) */
static __attribute__((noinline)) BOOL string_own_release(
    struct IntuiMessage *imsg)
{
    UWORD code = imsg->Code & ~IECODE_UP_PREFIX;
    UBYTE c;

    if (code >= 0x4C && code <= 0x4F)
        return TRUE;
    imsg->Code = code;
    c = ConvertKey(imsg);
    imsg->Code = code | IECODE_UP_PREFIX;
    return c && c != '\r' && String_KeyOp(c, imsg->Qualifier) >= 0;
}

/* The String's event handler with other events */
static __attribute__((noinline)) void String_SetEvents(Object *obj,
    struct MUI_StringData *data, ULONG events)
{
    DoMethod(_win(obj), MUIM_Window_RemEventHandler, (IPTR) &data->ehn);
    data->ehn.ehn_Events = events;
    DoMethod(_win(obj), MUIM_Window_AddEventHandler, (IPTR) &data->ehn);
}

/* An edit hook (MUIA_String_EditHook) sees the key or click as with
 * Intuition's SGWork: previous is the buffer before, oldpos/oldflags/
 * oldmark the state before. Frees previous; returns whether the contents
 * changed. Not inlined: its Intuition structures (about 250 bytes) stay off
 * HandleEvent's frame, which every key passes. */
static __attribute__((noinline)) BOOL String_CallEditHook(Object *obj,
    struct MUI_StringData *data, struct IntuiMessage *imsg,
    STRPTR previous, LONG oldpos, ULONG oldflags, ULONG oldmark,
    BOOL hook_key, BOOL acknowledge, ULONG *hook_actions)
{
    BOOL edited;
    struct InputEvent event = {0};
    struct StringExtend extension = {0};
    struct StringInfo info = {0};
    struct Gadget gadget = {0};
    struct GadgetInfo gi = {0};
    struct SGWork work = {0};
    ULONG command = hook_key ? SGH_KEY : SGH_CLICK;
    BOOL handled;

    event.ie_Class = hook_key ? IECLASS_RAWKEY : IECLASS_RAWMOUSE;
    event.ie_Code = imsg->Code;
    event.ie_Qualifier = imsg->Qualifier;
    info.Buffer = previous;
    info.BufferPos = oldpos;
    info.NumChars = strlen(previous);
    info.MaxChars = data->BufferSize;
    info.Extension = &extension;
    extension.WorkBuffer = data->Buffer;
    extension.EditHook = data->msd_EditHook;
    gadget.SpecialInfo = &info;
    gadget.LeftEdge = _mleft(obj); gadget.TopEdge = _mtop(obj);
    gadget.Width = _mwidth(obj); gadget.Height = _mheight(obj);
    gi.gi_Window = _window(obj); gi.gi_Screen = _screen(obj);
    gi.gi_RastPort = _rp(obj);
    work.Gadget = &gadget; work.StringInfo = &info;
    work.GadgetInfo = &gi; work.IEvent = &event;
    work.WorkBuffer = data->Buffer; work.PrevBuffer = previous;
    work.BufferPos = data->BufferPos; work.NumChars = data->NumChars;
    work.Code = hook_key ? ConvertKey(imsg) : 0;
    work.Actions = SGA_USE | SGA_REDISPLAY;
    work.EditOp = EO_NOOP;
    if (acknowledge) work.EditOp = EO_ENTER;
    else if (data->NumChars > info.NumChars) work.EditOp = EO_INSERTCHAR;
    else if (data->NumChars < info.NumChars)
    {
        work.EditOp = EO_DELFORWARD;
        if (work.Code == 8) work.Code = 127;
    }
    else if (data->BufferPos != oldpos) work.EditOp = EO_MOVECURSOR;
    else if (strcmp((char *)previous, (char *)data->Buffer)) work.EditOp = EO_REPLACECHAR;
    handled = CallHookPkt(data->msd_EditHook, &work, &command);
    if (handled && (work.Actions & SGA_USE))
    {
        LONG length = MAX(0, MIN((LONG)data->BufferSize - 1, work.NumChars));
        data->Buffer[length] = 0;
        data->NumChars = strlen((char *)data->Buffer);
        data->BufferPos = MAX(0, MIN((LONG)data->NumChars, work.BufferPos));
    }
    else if (handled)
    {
        /* the edit undone; the activity and the acknowledged state are
         * not the edit's */
        const ULONG keep = MSDF_ACTIVE | MSDF_ACKED | MSDF_TIPENDING;

        memcpy(data->Buffer, previous, data->BufferSize);
        data->NumChars = info.NumChars;
        data->BufferPos = oldpos;
        data->msd_Flags = (oldflags & ~keep) | (data->msd_Flags & keep);
        data->MarkPos = oldmark;
    }
    if (handled && hook_key) *hook_actions = work.Actions;
    if (handled && (work.Actions & SGA_BEEP)) DisplayBeep(_screen(obj));
    edited = strcmp((char *)previous, (char *)data->Buffer) != 0;
    FreeVec(previous);
    return edited;
}

/* A new cursor position from the pointer's x in the shown text (the
 * buffer's characters, also for a secret) */
static __attribute__((noinline)) ULONG string_pos_at(Object *obj,
    struct MUI_StringData *data, WORD x, WORD y, UWORD text_left)
{
    struct TextExtent te;

    if (MULTILINE(data))
        return zune_string_pos_at(obj, data, x, y);
    return data->DispPos + TextFit(_rp(obj), data->Buffer + data->DispPos,
        data->NumChars - data->DispPos, &te, NULL, 1, x - text_left,
        _rp(obj)->Font->tf_YSize);
}

/* SELECTDOWN in the String: activated, the cursor to the pointer, a double
 * click marks the word, a third everything. Returns whether to redraw. */
static __attribute__((noinline)) int string_click(Object *obj,
    struct MUI_StringData *data, WORD x, WORD y)
{
    ULONG secs, micros, newpos;
    UWORD text_left, text_right;
    int update = 0;

    CurrentTime(&secs, &micros);
    if (DoubleClick(data->OldClick_Sec, data->OldClick_Micro, secs, micros))
        data->MultiClick++;
    else
        data->MultiClick = 0;
    data->OldClick_Sec = secs;
    data->OldClick_Micro = micros;
    if (TI_MARKED(data))
        update = 1;
    data->msd_Flags &= ~MSDF_TIMARK;

    if (!ACTIVE(data))
    {
        data->msd_RedrawReason = WENT_ACTIVE;
        string_activity(obj, data, TRUE);

        /* An edit ended by a hook leaves the window's active object
         * unchanged, so setting it again would not restore key input. */
        if (XGET(_win(obj), MUIA_Window_ActiveObject) == (IPTR)obj)
            DoMethod(obj, MUIM_GoActive);
        else
            set(_win(obj), MUIA_Window_ActiveObject, obj);
    }

    if (!(data->ehn.ehn_Events & IDCMP_MOUSEMOVE))
        String_SetEvents(obj, data, data->ehn.ehn_Events | IDCMP_MOUSEMOVE);

    text_left = string_text_edge(obj, data, FALSE);
    text_right = string_text_edge(obj, data, TRUE);

    if (MULTILINE(data) || (x >= text_left && x <= text_right))
        newpos = string_pos_at(obj, data, x, y, text_left);
    else if (x < text_left)
        /* Click on empty space at left: the first visible */
        newpos = data->DispPos;
    else
        /* Click on empty space at right: the last visible */
        newpos = data->DispPos + data->DispCount;
    if (data->BufferPos != newpos)
    {
        data->BufferPos = newpos;
        update = 1;
    }

    data->MarkPos = data->BufferPos;

    if (data->MultiClick == 0)
    {
        if (data->msd_Flags & MSDF_MARKING)
        {
            data->msd_Flags &= ~MSDF_MARKING;
            update = 1;
        }
    }
    else if (Buffer_GetMarkedRange(data, NULL, NULL))
    {
        data->msd_Flags |= MSDF_MARKING;
        update = 1;
    }
    return update;
}

/* A move with the button held: marking. Returns whether to redraw. */
static __attribute__((noinline)) int string_drag(Object *obj,
    struct MUI_StringData *data, WORD x, WORD y)
{
    UWORD text_left, text_right;

    data->msd_Flags |= MSDF_MARKING;
    text_left = string_text_edge(obj, data, FALSE);
    text_right = string_text_edge(obj, data, TRUE);

    if (MULTILINE(data) || (x >= text_left && x <= text_right))
    {
        ULONG newpos = string_pos_at(obj, data, x, y, text_left);

        if (data->BufferPos != newpos)
        {
            LONG old_markstart = 0, old_markstop = 0;
            LONG markstart = 0, markstop = 0;
            BOOL was_marked, is_marked;

            was_marked = Buffer_GetMarkedRange(data, &old_markstart,
                &old_markstop);
            data->BufferPos = newpos;
            is_marked = Buffer_GetMarkedRange(data, &markstart, &markstop);
            return was_marked != is_marked || old_markstart != markstart
                || old_markstop != markstop;
        }
    }
    else if (x < text_left && data->BufferPos > 0)
    {
        data->BufferPos--;
        data->msd_RedrawReason = DO_CURSOR_LEFT;
        return 1;
    }
    else if (x > text_right && data->BufferPos < data->NumChars)
    {
        data->BufferPos++;
        data->msd_RedrawReason = DO_CURSOR_RIGHT;
        return 1;
    }
    return 0;
}

/* The up and down keys: BetterString's focus objects, else the attached
 * list moves */
static __attribute__((noinline)) void string_updown(Object *obj,
    struct MUI_StringData *data, LONG muikey)
{
    static const BYTE active[] = {
        MUIV_List_Active_Up, MUIV_List_Active_Down,
        MUIV_List_Active_PageUp, MUIV_List_Active_PageDown,
        MUIV_List_Active_Top, MUIV_List_Active_Bottom
    };
    struct MUI_StringExt *x = data->ext;
    Object *focus = !x ? NULL : muikey == MUIKEY_UP ? x->KeyUpFocus
        : muikey == MUIKEY_DOWN ? x->KeyDownFocus : NULL;

    if (focus && muiRenderInfo(obj) && _win(obj))
        set(_win(obj), MUIA_Window_ActiveObject, focus);
    else if (data->msd_AttachedList)
        set(data->msd_AttachedList, MUIA_List_Active,
            (LONG)active[muikey - MUIKEY_UP]);
}

/* Return: the window's next or no active object, then the acknowledge
 * (MUIA_String_Acknowledge; a MUI 4/5 subclass sees the method) */
static __attribute__((noinline)) void string_return(struct IClass *cl,
    Object *obj, struct MUI_StringData *data)
{
    if (!(data->msd_Flags & MSDF_STAYACTIVE))
        set(_win(obj), MUIA_Window_ActiveObject,
            (data->msd_Flags & MSDF_ADVANCEONCR)
            ? MUIV_Window_ActiveObject_Next : MUIV_Window_ActiveObject_None);

    data->msd_Flags &= ~MSDF_CHANGED;
    if (data->msd_Flags & MSDF_DIRECTKEYS)
        String_Acknowledge(cl, obj, data);
    else
    {
        UBYTE *buf = NULL;

        get(obj, MUIA_String_Contents, &buf);
        DoMethod(obj, MUIM_Textinput_Acknowledge, (IPTR)buf);
    }
}

/**************************************************************************
 MUIM_HandleEvent
**************************************************************************/
IPTR String__MUIM_HandleEvent(struct IClass *cl, Object *obj,
    struct MUIP_HandleEvent *msg)
{
    struct MUI_StringData *data =
        (struct MUI_StringData *)INST_DATA(cl, obj);
    struct IntuiMessage *imsg = msg->imsg;
    IPTR retval = 0;
    ULONG hook_actions = 0, pending = 0;
    LONG muikey = msg->muikey;
    BOOL acknowledge = FALSE, hook_key = FALSE;
    STRPTR previous = NULL;
    LONG oldpos = data->BufferPos;
    ULONG oldflags = data->msd_Flags, oldmark = data->MarkPos;
    ULONG class = imsg ? imsg->Class : 0;

    if (class == IDCMP_RAWKEY)
    {
        UWORD code = imsg->Code;

        hook_key = !(code & IECODE_UP_PREFIX) && ACTIVE(data);
        /* Convert raw keys to MUI keys for marking: the cursor keys, a
         * word with Ctrl, the line with Shift */
        if (muikey == MUIKEY_NONE && (code == 0x4F || code == 0x4E))
        {
            UWORD qual = imsg->Qualifier;

            muikey = (code == 0x4F ? MUIKEY_LEFT : MUIKEY_RIGHT)
                + ((qual & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) ? 4
                : (qual & IEQUALIFIER_CONTROL) ? 2 : 0);
        }
    }
    if (data->msd_EditHook && (hook_key || (class == IDCMP_MOUSEBUTTONS
        && imsg->Code == SELECTDOWN
        && _isinobject(obj, imsg->MouseX, imsg->MouseY))))
    {
        /* LonelyEditHook still receives the ordinary proposed edit, as
         * with Intuition SGWork. It does not suppress character editing
         * itself. */
        previous = AllocVec(data->BufferSize, MEMF_ANY);
        if (!previous) return MUI_EventHandlerRC_Eat;
        memcpy(previous, data->Buffer, data->BufferSize);
    }

    if (muikey != MUIKEY_NONE && ACTIVE(data))
    {
        retval = MUI_EventHandlerRC_Eat;

        /* The cursor keys; the vertical ones within a Multiline String */
        if (muikey >= MUIKEY_UP && muikey <= MUIKEY_LINEEND
            && (muikey >= MUIKEY_LEFT || MULTILINE(data)))
            pending = String_Key(cl, obj, data, muikey, 0);
        else if (muikey >= MUIKEY_UP && muikey <= MUIKEY_BOTTOM)
            string_updown(obj, data, muikey);
        else if (muikey == MUIKEY_PRESS)
            /* Return is a newline in a Multiline String */
            acknowledge = !MULTILINE(data);
        else
            retval = 0;
    }

    switch (class)
    {
    case IDCMP_RAWKEY:
        if (ACTIVE(data))
        {
            UBYTE code = ConvertKey(imsg);

            if (code)
            {
                LONG op = String_KeyOp(code, imsg->Qualifier);

                if (op >= 0)
                {
                    LONG arg = code;

                    /* Return is a newline in a Multiline String; else
                     * the acknowledge follows from MUIKEY_PRESS (a MUI
                     * 4/5 subclass still sees the HandleChar) */
                    if (code != '\r')
                        retval = MUI_EventHandlerRC_Eat;
                    else if (MULTILINE(data))
                    {
                        arg = '\n';
                        retval = MUI_EventHandlerRC_Eat;
                    }
                    pending |= String_Key(cl, obj, data, op, arg);
                }
            }
            else if ((imsg->Code & IECODE_UP_PREFIX)
                && (data->msd_Flags & MSDF_DIRECTKEYS)
                && string_own_release(imsg))
                /* As MUI 3.8's String (an Intuition gadget, which takes
                 * the keys typed into it): the release of a key the
                 * String took is not offered to other objects
                 * (tests/native/keyoffers.c string) */
                retval = MUI_EventHandlerRC_Eat;
            else if ((imsg->Code & ~IECODE_UP_PREFIX) == 0x64
                || (imsg->Code & ~IECODE_UP_PREFIX) == 0x65)
            {
                /* Alt: marking with the cursor keys */
                if (imsg->Qualifier & (IEQUALIFIER_LALT | IEQUALIFIER_RALT))
                {
                    data->MarkPos = data->BufferPos;
                    data->msd_Flags = (data->msd_Flags & ~MSDF_TIMARK)
                        | MSDF_MARKING | MSDF_KEYMARKING;
                }
                else
                    data->msd_Flags &= ~MSDF_KEYMARKING;
            }
        }
        break;

    case IDCMP_MOUSEBUTTONS:       /* set cursor and activate it */
        if (imsg->Code == SELECTDOWN)
        {
            if (_isinobject(obj, imsg->MouseX, imsg->MouseY))
            {
                retval = MUI_EventHandlerRC_Eat;
                if (string_click(obj, data, imsg->MouseX, imsg->MouseY))
                    pending |= TIP_UPDATE;
            }
            else if ((data->msd_Flags & (MSDF_ACTIVE | MSDF_STAYACTIVE))
                == MSDF_ACTIVE)
                /* a click elsewhere */
                string_activity(obj, data, FALSE);
        }
        else if (imsg->Code == SELECTUP
            && (data->ehn.ehn_Events & IDCMP_MOUSEMOVE))
            String_SetEvents(obj, data,
                data->ehn.ehn_Events & ~IDCMP_MOUSEMOVE);
        break;

    case IDCMP_MOUSEMOVE:
        if (ACTIVE(data)
            && string_drag(obj, data, imsg->MouseX, imsg->MouseY))
            pending |= TIP_UPDATE;
        break;
    }

    if (previous)
    {
        if (String_CallEditHook(obj, data, imsg, previous, oldpos,
                oldflags, oldmark, hook_key, acknowledge, &hook_actions))
            pending |= TIP_EDITED;
        else
            pending &= ~TIP_EDITED;
        pending |= TIP_UPDATE;
        retval = MUI_EventHandlerRC_Eat;
    }

    /* Trigger notification (a program that gets MUI 3.8 knows no
     * MUIA_Textinput_Contents) */
    if (pending & TIP_EDITED)
    {
        data->msd_Flags = (data->msd_Flags & ~MSDF_ACKED) | MSDF_CHANGED;
        SetSuperAttrs(cl, obj, MUIA_String_Contents, (IPTR)data->Buffer,
            (data->msd_Flags & MSDF_DIRECTKEYS) ? TAG_DONE
            : MUIA_Textinput_Contents, (IPTR)data->Buffer, TAG_DONE);
    }

    if (pending)
        MUI_Redraw(obj, MADF_DRAWUPDATE);
    if ((hook_actions & SGA_END) && !acknowledge)
    {
        /* Ending an edit need not clear Window_ActiveObject. The hook can
         * explicitly request a cycle-chain move, as in MUI 21.227. */
        if (hook_actions & (SGA_NEXTACTIVE | SGA_PREVACTIVE))
            set(_win(obj), MUIA_Window_ActiveObject,
                (hook_actions & SGA_PREVACTIVE) ? MUIV_Window_ActiveObject_Prev : MUIV_Window_ActiveObject_Next);
        else DoMethod(obj, MUIM_GoInactive);
        return (hook_actions & SGA_REUSE) ? 0 : MUI_EventHandlerRC_Eat;
    }
    if (acknowledge)
        string_return(cl, obj, data);

    return retval;
}


/**************************************************************************
 MUIM_Export : to export an object's "contents" to a dataspace object.
**************************************************************************/
IPTR String__MUIM_Export(struct IClass *cl, Object *obj,
    struct MUIP_Export *msg)
{
    struct MUI_StringData *data = INST_DATA(cl, obj);
    ULONG id;

    if ((id = muiNotifyData(obj)->mnd_ObjectID))
    {
        if (data->Buffer != NULL)
            DoMethod(msg->dataspace, MUIM_Dataspace_Add,
                (IPTR) data->Buffer, data->NumChars + 1, (IPTR) id);
        else
            DoMethod(msg->dataspace, MUIM_Dataspace_Remove, (IPTR) id);
    }

    return 0;
}


/**************************************************************************
 MUIM_Import : to import an object's "contents" from a dataspace object.
**************************************************************************/
IPTR String__MUIM_Import(struct IClass *cl, Object *obj,
    struct MUIP_Import *msg)
{
    ULONG id;
    STRPTR s;

    if ((id = muiNotifyData(obj)->mnd_ObjectID))
    {
        if ((s = (STRPTR) DoMethod(msg->dataspace, MUIM_Dataspace_Find,
                    (IPTR) id)))
        {
            set(obj, MUIA_String_Contents, s);
        }
    }

    return 0;
}

/**************************************************************************
 MUIM_GoActive, MUIM_GoInactive
**************************************************************************/
static IPTR String_GoActive(struct IClass *cl, Object *obj, Msg msg)
{
    struct MUI_StringData *data = INST_DATA(cl, obj);
    BOOL active = msg->MethodID == MUIM_GoActive;

    String_SetEvents(obj, data, active ? IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY
        : IDCMP_MOUSEBUTTONS);
    if (active)
    {
        data->msd_Flags &= ~MSDF_KEYMARKING;
        data->msd_RedrawReason = WENT_ACTIVE;
    }
    else
    {
        data->msd_RedrawReason = WENT_INACTIVE;
        data->MultiClick = 0;
    }
    string_activity(obj, data, active);
    return 0;
}

/**************************************************************************
 MUIM_String_ClearSelected (BetterString)
**************************************************************************/
IPTR String__MUIM_ClearSelected(struct IClass *cl, Object *obj,
    struct MUIP_String_ClearSelected *msg)
{
    if (Buffer_KillMarked(INST_DATA(cl, obj)))
        MUI_Redraw(obj, MADF_DRAWUPDATE);
    return 0;
}

BOOPSI_DISPATCHER(IPTR, String_Dispatcher, cl, obj, msg)
{
    IPTR (*method)(struct IClass *, Object *, APTR);

    switch (msg->MethodID)
    {
    case MUIM_HandleEvent: method = (APTR)String__MUIM_HandleEvent; break;
    case MUIM_Draw: method = (APTR)String__MUIM_Draw; break;
    case OM_NEW: method = (APTR)String__OM_NEW; break;
    case OM_DISPOSE: method = (APTR)String__OM_DISPOSE; break;
    case OM_SET: method = (APTR)String__OM_SET; break;
    case OM_GET: method = (APTR)String__OM_GET; break;

    case MUIM_Setup: method = (APTR)String__MUIM_Setup; break;
    case MUIM_Cleanup: method = (APTR)String__MUIM_Cleanup; break;
    case MUIM_AskMinMax: method = (APTR)String__MUIM_AskMinMax; break;
    case MUIM_Export: method = (APTR)String__MUIM_Export; break;
    case MUIM_Import: method = (APTR)String__MUIM_Import; break;
    case MUIM_GoActive:
    case MUIM_GoInactive: method = (APTR)String_GoActive; break;

    /* BetterString */
    case MUIM_ContextMenuBuild:
    case MUIM_ContextMenuChoice:
    case MUIM_String_Insert: method = (APTR)String_Module; break;
    case MUIM_String_ClearSelected: method = (APTR)String__MUIM_ClearSelected; break;
    case MUIM_String_FileNameStart:
        // TODO: Return the partial filename start position, not the buffer pointer.
        return (IPTR)((struct MUI_StringData *)INST_DATA(cl, obj))->Buffer;
    default:
        if (msg->MethodID >= MUIM_Textinput_ExternalEdit
            && msg->MethodID <= MUIM_Textinput_DoCopyCut)
        {
            method = (APTR)String_Textinput45;
            break;
        }
        return DoSuperMethodA(cl, obj, msg);
    }

    return method(cl, obj, msg);
}
BOOPSI_DISPATCHER_END

/*
 * Class descriptor.
 */
const struct __MUIBuiltinClass _MUI_String_desc =
{
    MUIC_String,
    MUIC_Area,
    sizeof(struct MUI_StringData),
    (void *)String_Dispatcher
};
