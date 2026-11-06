/*
    Copyright  2002-2026, The AROS Development Team.
    All rights reserved.

*/

/* A window's side of drag and drop (MUIM_Window_DragObject and the events
 * while an object is dragged), split out of classes/window.c. In the
 * native build it is part of the drag and drop module (modules.h,
 * classes/zdragdrop.c), which a drag needs anyway for its image; other
 * builds include it in classes/window.c. The window's state is a struct
 * ZuneDrag (dragwin.h). */

#include <exec/types.h>
#include <intuition/intuition.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/layers.h>
#include <proto/muimaster.h>

#include "mui.h"
#include "support.h"
#include "classes/area.h"
#include "dragndrop.h"
#include "dragwin.h"

extern struct Library *MUIMasterBase;

/* Ask the live source obj for a drag image using touchx/touchy/flags,
 * create a drag context and bitmap node, attach/prepare it for screen
 * scr, then mark the source dragging and keep the borrowed source and
 * image pointers in d. Return 1 on success or 0 after cleaning up. */
IPTR zune_drag_begin(struct ZuneDrag *d, Object *obj, LONG touchx,
    LONG touchy, ULONG flags, struct Screen *scr)
{
    struct DragNDrop *dnd;
    struct MUI_DragImage *di;
    struct BitMapNode *bmn;

    if (!(dnd = CreateDragNDropA(NULL)))
        return 0;

    if (!(di = (struct MUI_DragImage *)DoMethod(obj, MUIM_CreateDragImage,
        touchx, touchy, flags)))
    {
        DeleteDragNDrop(dnd);
        return 0;
    }
    if (!di->bm)
    {
        DoMethod(obj, MUIM_DeleteDragImage, (IPTR) di);
        DeleteDragNDrop(dnd);
        return 0;
    }

    if (!(bmn = CreateBitMapNodeA(TAGLIST(
        GUI_BitMap, (IPTR)di->bm,
        GUI_LeftOffset, di->touchx,
        GUI_TopOffset, di->touchy,
        GUI_Width, di->width,
        GUI_Height, di->height,
        GUI_SourceAlpha, !!(di->flags & MUIF_DRAGIMAGE_SOURCEALPHA)))))
    {
        DoMethod(obj, MUIM_DeleteDragImage, (IPTR) di);
        DeleteDragNDrop(dnd);
        return 0;
    }

    AttachBitMapNode(dnd, bmn);

    if (!PrepareDragNDrop(dnd, scr))
    {
        DoMethod(obj, MUIM_DeleteDragImage, (IPTR) di);
        DeleteDragNDrop(dnd);
        return 0;
    }

    muiAreaData(obj)->mad_Flags |= MADF_DRAGGING;

    d->object = obj;
    d->dnd = dnd;
    d->image = di;
    return 1;
}

/* Process a live dequeued IntuiMessage of window oWin while a drag is on.
 * Track application-window targets, issue synchronous drag
 * query/begin/report/drop/finish callbacks and draw the drag image. Keep
 * source, destination, application lists and context alive through
 * callbacks. Return FALSE after replying when the drag continues; TRUE
 * after releasing the drag's resources when it is finished, leaving the
 * message for ordinary event processing (the window then stops listening
 * to mouse moves). The Lock report is not implemented. */
BOOL zune_drag_event(Object *oWin, struct ZuneDrag *d,
    struct IntuiMessage *imsg)
{
    struct Window *iWin;
    BOOL finish_drag = FALSE;

    iWin = imsg->IDCMPWindow;

    if (imsg->Class == IDCMP_MOUSEMOVE)
    {
        struct Layer *layer;

        LockLayerInfo(&iWin->WScreen->LayerInfo);
        layer = WhichLayer(&iWin->WScreen->LayerInfo,
            iWin->LeftEdge + imsg->MouseX, iWin->TopEdge + imsg->MouseY);
        UnlockLayerInfo(&iWin->WScreen->LayerInfo);

        if (d->drop)
        {
            struct Window *wnd;
            WORD mousex =
                imsg->MouseX + iWin->LeftEdge - d->dropwin->LeftEdge;
            WORD mousey =
                imsg->MouseY + iWin->TopEdge - d->dropwin->TopEdge;

            wnd = _window(d->drop);
            if (mousex < _left(d->drop)
                || mousex > _right(d->drop)
                || mousey < _top(d->drop)
                || mousey > _bottom(d->drop)
                || layer != wnd->WLayer)
            {
                /* We have left the object */
                UndrawDragNDrop(d->dnd);
                DoMethod(d->drop, MUIM_DragFinish, (IPTR) d->object);
                d->drop = NULL;
            }
        }

        if (!d->drop)
        {
            Object *dest_wnd = NULL;

            /* Find out if app has an open window at this position */
            if (layer)
            {
                Object *cstate;
                Object *child;
                struct MinList *ChildList = 0;

                get(_app(oWin), MUIA_Application_WindowList, &(ChildList));
                cstate = ChildList ? (Object *)ChildList->mlh_Head : NULL;
                while ((child = NextObject(&cstate)))
                {
                    struct Window *wnd = NULL;
                    get(child, MUIA_Window_Window, &wnd);
                    if (!wnd)
                        continue;

                    if (wnd->WLayer == layer)
                    {
                        d->dropwin = wnd;
                        dest_wnd = child;
                        break;
                    }
                }
            }

            if (dest_wnd)
            {
                Object *root = NULL;
                get(dest_wnd, MUIA_Window_RootObject, &root);

                if (root)
                {
                    if ((d->drop = (Object *) DoMethod
                            (root, MUIM_DragQueryExtended,
                                (IPTR) d->object,
                                imsg->MouseX + iWin->LeftEdge -
                                d->dropwin->LeftEdge,
                                imsg->MouseY + iWin->TopEdge -
                                d->dropwin->TopEdge)))
                    {
                        UndrawDragNDrop(d->dnd);
                        DoMethod(d->drop, MUIM_DragBegin, (IPTR) d->object);
                    }
                }
            }
        }

        if (d->drop)
        {
            LONG update = 0;
            LONG i;
            for (i = 0; i < 2; i++)
            {
                LONG res = DoMethod(d->drop, MUIM_DragReport,
                    (IPTR) d->object,
                    imsg->MouseX + iWin->LeftEdge - d->dropwin->LeftEdge,
                    imsg->MouseY + iWin->TopEdge - d->dropwin->TopEdge,
                    update);
                switch (res)
                {
                case MUIV_DragReport_Abort:
                    UndrawDragNDrop(d->dnd);
                    DoMethod(d->drop, MUIM_DragFinish, (IPTR) d->object);
                    d->drop = NULL;
                    i = 1;
                    break;

                case MUIV_DragReport_Continue:
                    break;
                case MUIV_DragReport_Lock:
                    break;      /* NYI */
                case MUIV_DragReport_Refresh:
                    UndrawDragNDrop(d->dnd);
                    update = 1;
                    break;
                }
            }
        }
        DrawDragNDrop(d->dnd, imsg->MouseX + iWin->LeftEdge,
            imsg->MouseY + iWin->TopEdge);
    }

    if (imsg->Class == IDCMP_MOUSEBUTTONS)
    {
        if ((imsg->Code == MENUDOWN) || (imsg->Code == SELECTUP))
        {
            UndrawDragNDrop(d->dnd);
            if (imsg->Code == SELECTUP && d->drop)
            {
                DoMethod(d->drop, MUIM_DragDrop, (IPTR) d->object,
                    imsg->MouseX + iWin->LeftEdge - d->dropwin->LeftEdge,
                    imsg->MouseY + iWin->TopEdge - d->dropwin->TopEdge);
                DoMethod(d->drop, MUIM_DragFinish, (IPTR) d->object);
                d->drop = NULL;
            }
            else if (imsg->Code == SELECTUP)
            {
                DoMethod(d->object, MUIM_UnknownDropDestination, imsg);
            }
            finish_drag = TRUE;
        }
    }

    if (imsg->Class == IDCMP_CLOSEWINDOW)
        finish_drag = TRUE;

    if (finish_drag)
    {
        if (d->drop)
        {
            UndrawDragNDrop(d->dnd);
            DoMethod(d->drop, MUIM_DragFinish, (IPTR) d->object);
            d->drop = NULL;
        }
        DeleteDragNDrop(d->dnd);
        DoMethod(d->object, MUIM_DeleteDragImage, (IPTR) d->image);
        muiAreaData(d->object)->mad_Flags &= ~MADF_DRAGGING;
        d->image = NULL;
        d->object = NULL;
        d->dropwin = NULL;
        d->dnd = NULL;
    }
    if (!finish_drag)
        ReplyMsg((struct Message *)imsg);

    return finish_drag;
}
