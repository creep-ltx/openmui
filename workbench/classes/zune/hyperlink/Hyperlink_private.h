#ifndef HYPERLINK_PRIVATE_H
#define HYPERLINK_PRIVATE_H

#include <libraries/mui.h>

struct Hyperlink_DATA
{
    STRPTR uri;                         /* our copy (AllocVec) */
    BOOL button;                        /* made with a frame: a plain button */
    BOOL hover;                         /* the pointer is over the link */
    BOOL added;                         /* event handler added */
    struct MUI_EventHandlerNode ehn;
};

#endif
