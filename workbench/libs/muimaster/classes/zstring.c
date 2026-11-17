/* The String module (modules.h): String's clipboard, its default context
 * menu and BetterString's Insert (stringx.c). The library calls them
 * through modules.c under the names modules.h gives them. */

#define zune_string_clip zmod_string_clip
#define zune_string_method zmod_string_method
#define clipboard_write_text zmod_clipboard_write_text
#include "../stringx.c"

static const struct ZuneStringModule string_module = {
    zmod_string_clip, zmod_string_method, zmod_clipboard_write_text
};

BOOPSI_DISPATCHER(IPTR, Zstring_Dispatcher, cl, obj, msg)
{
    if (msg->MethodID == MUIM_ZuneModule_Get)
        return (IPTR)&string_module;
    return DoSuperMethodA(cl, obj, msg);
}
BOOPSI_DISPATCHER_END

const struct __MUIBuiltinClass _MUI_Zstring_desc =
{
    MUIC_Zstring,
    ROOTCLASS,
    0,
    (void *) Zstring_Dispatcher
};
