/*
    Copyright (C) 2002-2026, The AROS Development Team. All rights reserved.
*/

#include <proto/intuition.h>
#include <intuition/classusr.h>
#include <clib/alib_protos.h>
#include <proto/muimaster.h>

#include "muimaster_intern.h"
#include "mui.h"

/*****************************************************************************

    NAME */
        AROS_LH2(VOID, MUI_RejectIDCMP,

/*  SYNOPSIS */
        AROS_LHA(Object *, obj  , A0),
        AROS_LHA(ULONG   , flags, D0),

/*  LOCATION */
        struct Library *, MUIMasterBase, 16, MUIMaster)

/*  FUNCTION
        Remove IDCMP event bits from an Area-derived object's legacy input
        handler. If the object is set up and has a window, unregister the
        handler before changing its mask, then register it again only if
        event bits remain. Otherwise, update the stored mask.

    INPUTS
        obj - Non-NULL Area-derived object.
        flags - IDCMP event bits to remove. Other bits remain set.

    RESULT
        None.

    NOTES
        This is the counterpart of MUI_RequestIDCMP(). It does not remove
        separately registered event-handler nodes. Call in the object's
        owning GUI task.

    SEE ALSO
        MUI_RequestIDCMP()

*****************************************************************************/
{
    AROS_LIBFUNC_INIT
    /* The node is in the Area's live block; without one nothing was
     * requested. */
    struct MUI_AreaLive *live = muiAreaData(obj)->mad_Live;

    if (!live)
        return;

    if (muiRenderInfo(obj) && (_flags(obj) & MADF_SETUP) && _win(obj))
    {
        if (live->hiehn.ehn_Events)
        {
            DoMethod(_win(obj), MUIM_Window_RemEventHandler,
                (IPTR)&live->hiehn);
        }
        live->hiehn.ehn_Events &= ~flags;
        if (live->hiehn.ehn_Events)
        {
            DoMethod(_win(obj), MUIM_Window_AddEventHandler,
                (IPTR)&live->hiehn);
        }
    }
    else
    {
        live->hiehn.ehn_Events &= ~flags;
    }

    AROS_LIBFUNC_EXIT

} /* MUIA_RejectIDCMP */
