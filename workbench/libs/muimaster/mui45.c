/* MUI 4/5 attributes of the core classes that MUI 3 programs never use.
 * Built as the module Zmui45.mui (modules.h, classes/zmui45.c): a MUI 3
 * program never loads it, and the library keeps only the recognition of
 * the attributes and a call (modules.c). */
#include <exec/types.h>
#include <intuition/classusr.h>
#include <clib/alib_protos.h>
#include <proto/muimaster.h>

#include "mui.h"
#include "muimaster_intern.h"
#include "support.h"
#include "modules.h"

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
