/* MUI 4/5 attributes of the core classes that MUI 3 programs never use
 * (Area's frame attributes, String's kept Textinput values and 64-bit
 * integer).
 * Built as the module Zmui45.mui (modules.h, classes/zmui45.c): a MUI 3
 * program never loads it, and the library keeps only the recognition of
 * the attributes and a call (modules.c). */
#include <exec/types.h>
#include <intuition/classusr.h>
#include <clib/alib_protos.h>
#include <proto/intuition.h>
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
