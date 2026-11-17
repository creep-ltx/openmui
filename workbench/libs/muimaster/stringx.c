/* String's rarely used parts (modules.h): the clipboard (copy, cut and
 * paste of the keys and the Textinput methods; Text's copy), the default
 * context menu, BetterString's MUIM_String_Insert. Built as the module
 * Zstring.mui (classes/zstring.c): a program loads it the first time its
 * user copies, cuts, pastes or opens a String's menu, or it sends Insert.
 * classes/string.c calls it under the names modules.h gives, with the
 * library's editing helpers in ops (classes/string_private.h). */
#include <string.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <libraries/gadtools.h>
#include <libraries/iffparse.h>
#include <datatypes/textclass.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/iffparse.h>
#include <proto/locale.h>
#include <proto/muimaster.h>

#include "mui.h"
#include "muimaster_intern.h"
#include "support.h"
#include "modules.h"
#include "classes/string.h"
#include "classes/string_private.h"

extern struct Library *MUIMasterBase;

void clipboard_write_text(STRPTR text, LONG textlen)
{
    struct IFFHandle *iff;

    if (ZUNE_IFFPARSE() && (iff = AllocIFF()))
    {
        if ((iff->iff_Stream = (IPTR) OpenClipboard(0)))
        {
            InitIFFasClip(iff);
            if (!OpenIFF(iff, IFFF_WRITE))
            {
                if (!PushChunk(iff, ID_FTXT, ID_FORM, IFFSIZE_UNKNOWN))
                {
                    if (!PushChunk(iff, ID_FTXT, ID_CHRS, IFFSIZE_UNKNOWN))
                    {
                        WriteChunkBytes(iff, text, textlen);
                        PopChunk(iff);
                    }
                    PopChunk(iff);
                }
                CloseIFF(iff);
            }
            CloseClipboard((struct ClipboardHandle *)iff->iff_Stream);
        }
        FreeIFF(iff);
    }
}

static STRPTR clipboard_read_text(void)
{
    struct IFFHandle *iff;
    struct ContextNode *cn;
    STRPTR retval = 0;

    if (ZUNE_IFFPARSE() && (iff = AllocIFF()))
    {
        if ((iff->iff_Stream = (IPTR) OpenClipboard(0)))
        {
            InitIFFasClip(iff);
            if (!OpenIFF(iff, IFFF_READ))
            {
                if (!(StopChunk(iff, ID_FTXT, ID_CHRS)))
                {
                    if (!ParseIFF(iff, IFFPARSE_SCAN))
                    {
                        cn = CurrentChunk(iff);
                        if ((cn->cn_Type == ID_FTXT)
                            && (cn->cn_ID == ID_CHRS) && (cn->cn_Size > 0))
                        {
                            if ((retval = mui_alloc(cn->cn_Size + 1)))
                            {
                                ReadChunkBytes(iff, retval, cn->cn_Size);
                                retval[cn->cn_Size] = '\0';
                            }
                        }
                    }
                }
                CloseIFF(iff);
            }
            CloseClipboard((struct ClipboardHandle *)iff->iff_Stream);
        }
        FreeIFF(iff);
    }
    return retval;
}

/* The marked range, or everything, to the clipboard (not a secret) */
static void string_copy(struct MUI_StringData *data,
    const struct ZuneStringOps *ops)
{
    LONG start = 0, stop = data->NumChars;

    if (data->msd_Flags & MSDF_SECRET)
        return;
    if (data->msd_Flags & MSDF_MARKING)
        ops->marked_range(data, &start, &stop);
    if (stop > start)
        clipboard_write_text(&data->Buffer[start], stop - start);
}

/* SE_COPY, SE_CUT and SE_PASTE of String_Edit (classes/string.c): 1 when
 * the contents changed, 2 when only the mark did */
ULONG zune_string_clip(struct IClass *cl, Object *obj, ULONG op, LONG arg,
    const struct ZuneStringOps *ops)
{
    struct MUI_StringData *data = INST_DATA(cl, obj);
    BOOL writable = !(data->msd_Flags & MSDF_NOINPUT);
    ULONG r = 0;

    switch (op)
    {
    case SE_COPY:
        string_copy(data, ops);
        if (data->msd_Flags & MSDF_MARKING)
            r = 2;
        data->msd_Flags &= ~(MSDF_MARKING | MSDF_TIMARK);
        break;

    case SE_CUT:
        /* With nothing marked, Cut clears the contents, as in MUI 5 */
        if (arg || ((data->msd_Flags & MSDF_MARKING)
                && ops->marked_range(data, NULL, NULL)))
            string_copy(data, ops);
        if (!writable)
        {
            data->msd_Flags &= ~(MSDF_MARKING | MSDF_TIMARK);
            r = 2;
        }
        else if (ops->kill_marked(data)
            || ops->buffer_delete(data, 0, data->NumChars))
            r = 1;
        break;

    case SE_PASTE:
        if (writable)
        {
            STRPTR text;
            struct Locale *locale = OpenLocale(NULL);

            r = ops->kill_marked(data);
            if ((text = clipboard_read_text()))
            {
                STRPTR text2 = text;
                UBYTE c;

                while ((c = *text2++))
                {
                    if (!IsPrint(locale, c) || !ops->add_char(data, c))
                        break;
                    r = 1;
                }
                FreeVec(text);
            }
            CloseLocale(locale);
        }
        break;
    }
    return r;
}

/* MUIM_ContextMenuBuild of a String with the default menu (the program
 * gave none): Edit with Cut, Copy, Paste, Select all, made once */
static IPTR string_menu_build(Object *obj, struct MUI_StringData *data,
    const struct ZuneStringOps *ops)
{
    struct MUI_StringExt *x;
    BOOL selected, writable, secret;

    if (!(_flags(obj) & MADF_SETUP)) return 1;
    if (!(x = ops->ext(data))) return 0;
    if (!x->edit_menu)
    {
        struct NewMenu items[] = {
            {NM_TITLE, "Edit", NULL, 0, 0, NULL},
            {NM_ITEM, "Cut", "X", 0, 0, (APTR)1},
            {NM_ITEM, "Copy", "C", 0, 0, (APTR)2},
            {NM_ITEM, "Paste", "V", 0, 0, (APTR)3},
            {NM_ITEM, "Select all", NULL, 0, 0, (APTR)4},
            {NM_END, NULL, NULL, 0, 0, NULL}
        };
        IPTR params[] = {(IPTR)items, 0};
        x->edit_menu = MUI_MakeObjectA(MUIO_MenustripNM, params);
    }
    if (x->edit_menu)
    {
        selected = (data->msd_Flags & MSDF_MARKING)
            && ops->marked_range(data, NULL, NULL);
        writable = !(data->msd_Flags & MSDF_NOINPUT);
        secret = !!(data->msd_Flags & MSDF_SECRET);
        DoMethod(x->edit_menu, MUIM_SetUData, 1, MUIA_Menuitem_Enabled,
            selected && writable && !secret);
        DoMethod(x->edit_menu, MUIM_SetUData, 2, MUIA_Menuitem_Enabled,
            selected && !secret);
        DoMethod(x->edit_menu, MUIM_SetUData, 3, MUIA_Menuitem_Enabled,
            writable);
    }
    return (IPTR)x->edit_menu;
}

static IPTR string_menu_choice(struct IClass *cl, Object *obj,
    struct MUI_StringData *data, struct MUIP_ContextMenuChoice *msg,
    const struct ZuneStringOps *ops)
{
    IPTR action = 0;

    get(msg->item, MUIA_UserData, &action);
    if (action == 4)
    {
        data->BufferPos = 0;
        ops->select(data, data->NumChars);
    }
    else if (action >= 1 && action <= 3)
    {
        static const UBYTE edit_ops[] = { SE_CUT, SE_COPY, SE_PASTE };

        if (action != 3 && (data->msd_Flags & MSDF_SECRET)) return 0;
        if (ops->edit(cl, obj, data, edit_ops[action - 1], 0) & TIP_EDITED)
        {
            data->msd_Flags &= ~MSDF_ACKED;
            SetSuperAttrs(cl, obj, MUIA_String_Contents, (IPTR)data->Buffer,
                TAG_DONE);
        }
    }
    MUI_Redraw(obj, MADF_DRAWUPDATE);
    return 0;
}

/* BetterString's MUIM_String_Insert */
static IPTR string_insert(Object *obj, struct MUI_StringData *data,
    struct MUIP_String_Insert *msg, const struct ZuneStringOps *ops)
{
    LONG pos;
    ULONG old_bufferpos;
    ULONG num_inserted = 0;

    switch ((ULONG) msg->pos)
    {
    case MUIV_String_Insert_StartOfString:
        pos = 0;
        break;
    case MUIV_String_Insert_EndOfString:
        pos = data->NumChars;
        break;
    case MUIV_String_Insert_BufferPos:
        pos = data->BufferPos;
        break;
    default:
        pos = msg->pos;
        break;
    }

    if ((pos < 0) || (pos > data->NumChars))
        return 0;

    old_bufferpos = data->BufferPos;
    data->BufferPos = pos;

    while (msg->text[num_inserted]
        && ops->add_char(data, msg->text[num_inserted]))
        num_inserted++;

    if (num_inserted)
    {
        if (old_bufferpos >= pos)
            data->BufferPos = old_bufferpos + num_inserted;
        else
            data->BufferPos = old_bufferpos;
        MUI_Redraw(obj, MADF_DRAWUPDATE);
    }
    return 0;
}

/* The String module's methods (classes/string.c's String_Module) */
IPTR zune_string_method(struct IClass *cl, Object *obj, Msg msg,
    const struct ZuneStringOps *ops)
{
    struct MUI_StringData *data = INST_DATA(cl, obj);

    switch (msg->MethodID)
    {
    case MUIM_ContextMenuBuild:
        return string_menu_build(obj, data, ops);
    case MUIM_ContextMenuChoice:
        return string_menu_choice(cl, obj, data, (APTR)msg, ops);
    case MUIM_String_Insert:
        return string_insert(obj, data, (APTR)msg, ops);
    }
    return 0;
}
