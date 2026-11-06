/*
    Copyright  2026, The AROS Development Team.
    All rights reserved.

*/

#ifndef _DRAGWIN_H
#define _DRAGWIN_H

/* A window's drag and drop state (dragwin.c); in the window's instance
 * data, where its fields were before (wd_DragObject ... wd_DragImage) */
struct DragNDrop;
struct MUI_DragImage;
struct ZuneDrag
{
    Object *object;             /* the object which is being dragged */
    struct Window *dropwin;     /* the destination window */
    Object *drop;               /* the destination object */
    struct DragNDrop *dnd;
    struct MUI_DragImage *image;
};

struct Screen;
struct IntuiMessage;
IPTR zune_drag_begin(struct ZuneDrag *d, Object *obj, LONG touchx,
    LONG touchy, ULONG flags, struct Screen *scr);
BOOL zune_drag_event(Object *oWin, struct ZuneDrag *d,
    struct IntuiMessage *imsg);

#endif
