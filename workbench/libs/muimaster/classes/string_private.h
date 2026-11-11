/* String's instance data, as classes/string.c and the MUI 4/5 module
 * (mui45.c: the Textinput attributes MUI 3 programs never use) read it */
#ifndef _ZUNE_STRING_PRIVATE_H
#define _ZUNE_STRING_PRIVATE_H

#include <exec/types.h>
#include "penspec.h"

struct MUI_StringData
{
    ULONG msd_Flags;
    CONST_STRPTR msd_Accept;    /* MUIA_String_Accept */
    CONST_STRPTR msd_Reject;    /* MUIA_String_Reject */
    LONG msd_Align;
    struct Hook *msd_EditHook;
    Object *msd_AttachedList;
    LONG msd_RedrawReason;

    ULONG msd_useSecret;
    /* Fields mostly ripped from rom/intuition/strgadgets.c */
    STRPTR Buffer;              /* char container                   */
    STRPTR SecBuffer;           /* Buffer for secret string         */
    STRPTR VisualBuffer;        /* Pointer to either Buffer or SecBuffer */
    ULONG BufferSize;           /* memory allocated                 */
    ULONG NumChars;             /* string length                    */
    ULONG BufferPos;            /* cursor (insert/delete) position  */
    ULONG MarkPos;              /* cursor text marking start pos    */
    LONG DispPos;               /* leftmost visible char            */
    ULONG DispCount;            /* number of visible chars          */

    ULONG OldClick_Sec;
    ULONG OldClick_Micro;
    ULONG NewClick_Sec;
    ULONG NewClick_Micro;
    WORD MultiClick;

    WORD Columns;

    struct MUI_EventHandlerNode ehn;
    struct MUI_PenSpec_intern inactive_text;
    struct MUI_PenSpec_intern active_text;
    struct MUI_PenSpec_intern marked_text;
    struct MUI_PenSpec_intern marked_bg;
    struct MUI_PenSpec_intern cursor;

    BOOL is_active;
    STRPTR AckBuffer;           /* pointer to buffer of MUIA_String_Acknowledge */
    Object *KeyUpFocus;
    Object *KeyDownFocus;
    CONST_STRPTR placeholder;
    long long integer64;
    Object *edit_menu;
    /* Textinput marks are inclusive and independent of the cursor */
    LONG TiMarkStart;
    LONG TiMarkEnd;
    ULONG TiPending;            /* TIP_* collected during input events */
    LONG TiMinVal;
    LONG TiMaxVal;
    UWORD TiWordWrap;
    UBYTE TiStyles;
    UBYTE TiFont;
    LONG MlTopLine;             /* Multiline: first visible line */
    LONG MlLeftCol;             /* Multiline: first visible column */
};

#define MSDF_ADVANCEONCR    (1<<0)
#define MSDF_LONELYEDITHOOK (1<<1)
#define MSDF_MARKING            (1<<2)
#define MSDF_KEYMARKING            (1<<3)
#define MSDF_NOINPUT            (1<<4)
#define MSDF_STAYACTIVE     (1<<5)
#define MSDF_TIMARK         (1<<6)  /* with MSDF_MARKING: Textinput mark */
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

#define TIP_UPDATE 1
#define TIP_EDITED 2

#define TI_MARKED(data) (((data)->msd_Flags & (MSDF_MARKING | MSDF_TIMARK)) \
    == (MSDF_MARKING | MSDF_TIMARK))

#endif
