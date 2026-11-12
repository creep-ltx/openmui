#ifndef _COLORADJUST_PRIVATE_H_
#define _COLORADJUST_PRIVATE_H_

#ifndef _MUIMASTER_SUPPORT_H
#include "support.h"
#endif

/*** Instance data **********************************************************/
struct Coloradjust_DATA
{
    struct Library *colorwheelbase;
    struct Library *gradientsliderbase;
    struct Hook     sliderhook, wheelhook, gradhook;
    Object         *rslider, *gslider, *bslider, *colfield, *wheel, *grad;
    ULONG           rgb[3];
    UWORD           gradpenarray[3];
    LONG            gradpen;
    BOOL            truecolor;

    EXEC_INTERFACE_DECLARE(struct ColorWheelIFace * icolorwheel);
    /* after the fields tests/check_coloradjust.py reads at fixed offsets */
    Object         *popup, *palette;   /* the popup under the gradient slider, its palette */
    struct Hook     palhook, openhook;
    ULONG           palpens;           /* colour fields in the palette (made at Setup) */
};

#endif /* _COLORADJUST_PRIVATE_H_ */
