/* String's instance data, as classes/string.c, the MUI 4/5 module (mui45.c:
 * the Textinput attributes and methods MUI 3 programs never use) and the
 * String module (stringx.c: clipboard, context menu, BetterString's
 * methods) read it */
#ifndef _ZUNE_STRING_PRIVATE_H
#define _ZUNE_STRING_PRIVATE_H

#include <exec/types.h>
#include <intuition/sghooks.h>

/* What only some Strings need (MUI 4/5's Textinput values and marks, a
 * secret's stars, a placeholder, BetterString's focus objects, the edit
 * menu, MUI 3.8's gadget), made the first time one is set: a plain String
 * keeps only the pointer (string.c, string_ext) */
struct ZuneStringGadget;
struct MUI_StringExt
{
    STRPTR SecBuffer;           /* MUIA_String_Secret: '*' x BufferSize-1 */
    CONST_STRPTR placeholder;   /* MUIA_String_Placeholder (MSDF_PLACEHOLDER) */
    Object *KeyUpFocus;         /* BetterString */
    Object *KeyDownFocus;
    Object *edit_menu;          /* the default context menu (stringx.c) */
    struct ZuneStringGadget *gadget; /* MUIA_Gadget_Gadget (MSDF_GADGET) */
    long long integer64;        /* what get(MUIA_String_Integer64) points to */
    /* Textinput marks are inclusive and independent of the cursor */
    LONG TiMarkStart;
    LONG TiMarkEnd;
    LONG TiMinVal;
    LONG TiMaxVal;
    LONG MlTopLine;             /* Multiline: first visible line */
    LONG MlLeftCol;             /* Multiline: first visible column */
    UWORD TiWordWrap;           /* 70 */
    UBYTE TiStyles;
};

/* The pens of the settings' string colours (MUIM_Setup) */
#define SP_INACTIVE 0           /* string_text_inactive */
#define SP_ACTIVE   1           /* string_text_active */
#define SP_MARKTEXT 2           /* string_text_marked */
#define SP_MARKBG   3           /* string_bg_marked */
#define SP_CURSOR   4           /* string_cursor */
#define SP_COUNT    5

struct MUI_StringData
{
    /* Fields mostly ripped from rom/intuition/strgadgets.c */
    STRPTR Buffer;              /* char container                   */
    ULONG BufferSize;           /* memory allocated                 */
    ULONG NumChars;             /* string length                    */
    ULONG BufferPos;            /* cursor (insert/delete) position  */
    ULONG MarkPos;              /* cursor text marking start pos    */
    LONG DispPos;               /* leftmost visible char            */
    ULONG DispCount;            /* number of visible chars          */
    ULONG msd_Flags;
    CONST_STRPTR msd_Accept;    /* MUIA_String_Accept */
    CONST_STRPTR msd_Reject;    /* MUIA_String_Reject */
    struct Hook *msd_EditHook;
    Object *msd_AttachedList;
    ULONG OldClick_Sec;
    ULONG OldClick_Micro;
    struct MUI_EventHandlerNode ehn;
    struct MUI_StringExt *ext;  /* or NULL */
    WORD Columns;
    WORD MultiClick;
    UBYTE pens[SP_COUNT];       /* SP_* */
    UBYTE pens_obtained;        /* bit SP_*: ObtainBestPen'd, released at Cleanup */
    UBYTE msd_Align;
    UBYTE msd_RedrawReason;
};

#define MSDF_ADVANCEONCR    (1<<0)
#define MSDF_LONELYEDITHOOK (1<<1)
#define MSDF_MARKING        (1<<2)
#define MSDF_KEYMARKING     (1<<3)
#define MSDF_NOINPUT        (1<<4)
#define MSDF_STAYACTIVE     (1<<5)
#define MSDF_TIMARK         (1<<6)  /* with MSDF_MARKING: Textinput mark
                                     * (only with ext) */
#define MSDF_CHANGED        (1<<7)
#define MSDF_TINOINPUT      (1<<8)
#define MSDF_MULTILINE      (1<<9)
#define MSDF_RESETMARK      (1<<10)
#define MSDF_ISNUMERIC      (1<<11)
#define MSDF_INEVENT        (1<<12) /* key input is calling methods */
#define MSDF_SETMIN         (1<<13)
#define MSDF_SETMAX         (1<<14)
#define MSDF_SETVMIN        (1<<15)
#define MSDF_SETVMAX        (1<<16)
#define MSDF_DIRECTKEYS     (1<<17) /* MUI 3.8: keys edit directly, not
                                     * through the Textinput methods */
#define MSDF_OWNBACK        (1<<18) /* MUIA_Background given at creation */
#define MSDF_ACTIVE         (1<<19) /* the String takes the keys */
#define MSDF_SECRET         (1<<20) /* MUIA_String_Secret (ext->SecBuffer) */
#define MSDF_ACKED          (1<<21) /* MUIA_String_Acknowledge is Buffer */
#define MSDF_PLACEHOLDER    (1<<22) /* ext->placeholder is set */
#define MSDF_GADGET         (1<<23) /* ext->gadget is set */
#define MSDF_FIXEDFONT      (1<<24) /* MUIA_Textinput_Font_Fixed */
#define MSDF_TIP_SHIFT      25      /* TIP_* collected during input events */
#define MSDF_TIPENDING      (3<<25)

/* What the String shows: the stars of a secret, else the text */
#define STRING_VISUAL(data) (((data)->msd_Flags & MSDF_SECRET) \
    ? (data)->ext->SecBuffer : (data)->Buffer)

/* MUI 3.8's String is an Intuition string gadget: get(MUIA_Gadget_Gadget)
 * gives it (GTYP_CUSTOMGADGET, a StringInfo with a StringExtend, pens 1/0:
 * tests/lean/probe/strbg.c), and a program can recolour the field
 * through the StringExtend's background pens (UP-MCC's UP-Field does).
 * For a program that gets MUI 3.8 the MUI 4/5 module (mui45.c) makes such
 * a gadget at the first get, and the field is filled with a background pen
 * the program changed there. MUI 5 gives no gadget, nor does Zune for a
 * program that gets MUI 4/5. */
#ifndef MUIA_Gadget_Gadget
#define MUIA_Gadget_Gadget 0x8042ec1a /* MUI 3.8's Gadget class, V11 ..g */
#endif
struct ZuneStringGadget
{
    struct Gadget g;
    struct StringInfo si;
    struct StringExtend sx;
};
#define ZSG_BACKPEN 0           /* the background pen Zune puts in */

/* zune_string_mui45's store for a Textinput attribute at OM_NEW */
#define ZUNE_STRING_NEW ((IPTR *)1)

#define TIP_UPDATE 1
#define TIP_EDITED 2

#define TI_MARKED(data) (((data)->msd_Flags & (MSDF_MARKING | MSDF_TIMARK)) \
    == (MSDF_MARKING | MSDF_TIMARK))

/* Why the next MADF_DRAWUPDATE draws (msd_RedrawReason) */
enum
{
    NO_REASON = 0,
    WENT_ACTIVE = 1,
    WENT_INACTIVE,
    DO_CURSOR_LEFT = 3,
    DO_CURSOR_RIGHT,
    DO_DELETE = 5,
    DO_BACKSPACE,
    DO_ADDCHAR = 7,
    NEW_CONTENTS,
    MOVE_CURSOR,
    DO_UNKNOWN,
};

/* The editing operations of String_Edit (classes/string.c), what a key
 * does: MUIKEY_UP .. MUIKEY_LINEEND (2 .. 13) move the cursor, then */
#define SE_CHAR    14   /* arg: the character */
#define SE_BS      15
#define SE_BSSOL   16   /* to the start of the line (Shift-Backspace, Ctrl-U) */
#define SE_DEL     17
#define SE_DELEOL  18   /* to the end of the line (Shift-Del, Ctrl-K) */
#define SE_COPY    19
#define SE_CUT     20   /* arg: copy even with nothing marked (DoCopyCut) */
#define SE_PASTE   21
#define SE_NONE    22   /* a key String eats and does nothing with */
#define SE_COUNT   23

/* The library's editing, as the modules (mui45.c, stringx.c) call it:
 * edit does an SE_* operation and answers TIP_*; the rest are the buffer
 * helpers of classes/string.c */
struct IClass;
struct ZuneStringOps
{
    ULONG (*edit)(struct IClass *, Object *, struct MUI_StringData *,
        ULONG op, LONG arg);
    BOOL (*kill_marked)(struct MUI_StringData *);
    BOOL (*buffer_delete)(struct MUI_StringData *, ULONG, ULONG);
    LONG (*buffer_insert)(struct MUI_StringData *, ULONG, CONST_STRPTR,
        LONG);
    LONG (*prev_word)(struct MUI_StringData *, LONG);
    LONG (*succ_word)(struct MUI_StringData *, LONG);
    LONG (*word_end)(struct MUI_StringData *, LONG);
    BOOL (*marked_range)(struct MUI_StringData *, LONG *, LONG *);
    void (*clip_write)(STRPTR, LONG);
    void (*acknowledge)(struct IClass *, Object *, struct MUI_StringData *);
    struct MUI_StringExt *(*ext)(struct MUI_StringData *);
    BOOL (*add_char)(struct MUI_StringData *, unsigned char);
    void (*select)(struct MUI_StringData *, LONG);
};

#endif
