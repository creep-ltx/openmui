/* MUI_MakeObject's MUIO_ImageButton (an AROS Zune extension, type 112:
 * neither MUI 3.8 nor MUI 5 has it) in the module Zmui45.mui
 * (classes/zmui45.c, modules.h): only a program that asks for one loads
 * it; without the module the object is not made (NULL, as on low memory). */
#include <proto/alib.h>
#include <proto/intuition.h>
#include <intuition/classusr.h>
#include <proto/dos.h>
#include <proto/utility.h>
#include <proto/muimaster.h>
#include <string.h>

#include "muimaster_intern.h"
#include "mui.h"
#include "modules.h"
#include "debug.h"

/* Return the lowercase byte after the first underscore, or zero for a
 * NULL label, no underscore, or a trailing underscore. The label is borrowed.
 */
static int get_control_char(const char *label)
{
    /* find the control char */
    int control_char = 0;

    if (label)
    {
        const unsigned char *p = (const unsigned char *)label;
        unsigned char c;

        while ((c = *p++))
        {
            if (c == '_')
            {
                control_char = ToLower(*p);
                break;
            }
        }

    }
    return control_char;
}

/* Build an image-and-text button using a borrowed label and readable image
 * path, or a text button when the path cannot be locked. The caller owns
 * the returned tree. textFont must remain valid while the image uses it.
 * The 512-byte image-specification buffer can truncate a long path.
 */
Object *zune_image_button(CONST_STRPTR label, CONST_STRPTR imagePath,
    struct TextFont * textFont)
{
#   define BUFFERSIZE 512

    BPTR lock = Lock(imagePath, ACCESS_READ);

    D(bug("[ImageButton] Font 0x%p\n", textFont));
    if (lock != BNULL)
    {

        TEXT imageSpec[BUFFERSIZE];
        TEXT controlChar = get_control_char(label);

        imageSpec[0] = '\0';
        strlcat(imageSpec, "3:", BUFFERSIZE);
        strlcat(imageSpec, imagePath, BUFFERSIZE);

        UnLock(lock);

        return (Object *) HGroup,
            ButtonFrame,
            MUIA_Background, MUII_ButtonBack,
            MUIA_CycleChain, 1,
            MUIA_InputMode, MUIV_InputMode_RelVerify,
            MUIA_Group_Spacing, 0,
            //MUIA_Group_SameHeight, TRUE,
            controlChar ? MUIA_ControlChar : TAG_IGNORE, (IPTR) controlChar,
            
            Child, (IPTR)HVSpace,
            Child, (IPTR)ImageObject,
                MUIA_Image_Spec, (IPTR) imageSpec,
                MUIA_Image_FreeVert,    FALSE,
                MUIA_Image_FreeHoriz,   FALSE,
                MUIA_Image_FontMatch,   TRUE,
                MUIA_Font,       (IPTR) textFont,
                MUIA_Weight,            0,
            End,
            Child, (IPTR)HSpace(4),
            Child, (IPTR)TextObject,
                MUIA_Font,           (IPTR) MUIV_Font_Button,
                MUIA_Text_HiCharIdx, (IPTR) '_',
                MUIA_Text_Contents,  (IPTR) label,
                MUIA_Text_PreParse,  (IPTR) "\33c",
                MUIA_Weight,                0,
            End,
            Child, (IPTR)HVSpace,
        End;

    }
    else
    {
        return SimpleButton(label);
    }

#   undef BUFFERSIZE
}

