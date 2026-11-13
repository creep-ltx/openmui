/* The MUI 4/5 module (modules.h): attributes of the core classes that
 * only MUI 4/5 programs use (mui45.c), MUIA_PointerType's drawn shapes
 * (pointer45.c), Text's shortening and marking (textmark45.c), RGB and
 * alpha text pens (rtgtext45.c), MUIO_ImageButton (makeobj45.c), Window's
 * case-sensitive menu shortcuts and MENUVERIFY (mui45.c). The library calls them through
 * modules.c under the names modules.h gives them. */

#define zune_area_mui45 zmod_area_mui45
#define zune_string_mui45 zmod_string_mui45
#define zune_string_draw45 zmod_string_draw45
#define zune_string_pos_at zmod_string_pos_at
#define zune_string_textinput zmod_string_textinput
#define zune_pointer_image zmod_pointer_image
#define zune_text_shortened45 zmod_text_shortened45
#define zune_text_mark zmod_text_mark
#define zune_text_copy zmod_text_copy
#define ZuneTextARGB zmod_text_argb
#define zune_image_button zmod_image_button
#define zune_window_case_shortcut zmod_window_case_shortcut
#define zune_window_menu_verify zmod_window_menu_verify
#include "../mui45.c"
#include "../pointer45.c"
#include "../textmark45.c"
#include "../rtgtext45.c"
#include "../makeobj45.c"

static const struct ZuneMui45Module mui45_module = {
    zmod_area_mui45, zmod_string_mui45, zmod_string_draw45,
    zmod_string_pos_at, zmod_string_textinput, zmod_pointer_image,
    zmod_text_shortened45, zmod_text_mark, zmod_text_copy, zmod_text_argb,
    zmod_image_button, zmod_window_case_shortcut, zmod_window_menu_verify
};

BOOPSI_DISPATCHER(IPTR, Zmui45_Dispatcher, cl, obj, msg)
{
    if (msg->MethodID == MUIM_ZuneModule_Get)
        return (IPTR)&mui45_module;
    return DoSuperMethodA(cl, obj, msg);
}
BOOPSI_DISPATCHER_END

const struct __MUIBuiltinClass _MUI_Zmui45_desc =
{
    MUIC_Zmui45,
    ROOTCLASS,
    0,
    (void *) Zmui45_Dispatcher
};
