#ifndef _BUSY_PRIVATE_H_
#define _BUSY_PRIVATE_H_

#include <exec/types.h>
#include <utility/date.h>
#include <libraries/mui.h>

/*** Instance data **********************************************************/
struct Busy_DATA
{
    struct MUI_InputHandlerNode ihn;
    UWORD   	    	    	flags;
    UWORD   	    	    	speed;
    UWORD   	    	    	pos;
    UBYTE                   style;      /* 0 Old, 1 Queue, 2 KnightRider, 3 StarTrek */
    UBYTE                   size;       /* stripe width of Old */
    UBYTE                   number;     /* segments of the other styles */
    UBYTE                   fade;       /* segments that fade behind the head */
    UBYTE                   frame;      /* pixels around each segment */
    BYTE                    nopens;     /* pens are obtained */
    LONG                    pen[2];
    struct MUI_PenSpec      spec[2];
};

/* MUI 3.8's own ids of the settings (item ids in the settings file). */
#define BUSYCFG_Color1  0x80020001
#define BUSYCFG_Color2  0x80020002
#define BUSYCFG_Style   0x80020003
#define BUSYCFG_Size    0x80020004
#define BUSYCFG_Number  0x80020008
#define BUSYCFG_Fade    0x80020009
#define BUSYCFG_Frame   0x8002000a
#define BUSYCFG_Speed   0x8002000b

#define BUSY_SPEED_OFF  251
#define BUSY_SPEED_DEF  50

#define FLG_SHOWHIDEIH	1
#define FLG_SHOWN   	2
#define FLG_SETUP   	4
#define FLG_USERSPEED   8
#define FLG_STYLESET    16

#endif /* _BUSY_PRIVATE_H_ */
