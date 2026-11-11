#ifndef BUSY_MCC_H
#define BUSY_MCC_H

/*
    Copyright © 1995-2003, The AROS Development Team. All rights reserved.
    $Id$
*/

#include <libraries/mui.h>

/*** Name *******************************************************************/
#define MUIC_Busy                     	"Busy.mcc"

/*** Identifier base ********************************************************/
#define MUIB_Busy                       0x80020000

/*** Attributes *************************************************************/
#define MUIA_Busy_ShowHideIH	    	(MUIB_Busy | 0xa9)
#define MUIA_Busy_Speed     	    	(MUIB_Busy | 0x49)

/* Zune68k: the settings as attributes (the settings page's sample uses them);
   without them the bar reads MUI 3.8's settings (Busy.mcp). */
#define MUIA_Busy_Style     	    	(MUIB_Busy | 0xb0)  /* ISG 0 Old, 1 Queue, 2 KnightRider, 3 StarTrek */
#define MUIA_Busy_Size     	    	(MUIB_Busy | 0xb1)  /* ISG 4..20 */
#define MUIA_Busy_Number     	    	(MUIB_Busy | 0xb2)  /* ISG 4..32 */
#define MUIA_Busy_Fade     	    	(MUIB_Busy | 0xb3)  /* ISG 0..16 */
#define MUIA_Busy_Frame     	    	(MUIB_Busy | 0xb4)  /* ISG 0..4 */
#define MUIA_Busy_Color1     	    	(MUIB_Busy | 0xb5)  /* IS. STRPTR pen spec */
#define MUIA_Busy_Color2     	    	(MUIB_Busy | 0xb6)  /* IS. STRPTR pen spec */

#define MUIV_Busy_Speed_Off 	    	0
#define MUIV_Busy_Speed_User	    	-1

/*** Methods ****************************************************************/
#define MUIM_Busy_Move    	        (MUIB_Busy | 0x01)

struct MUIP_Busy_Move
{
    STACKED ULONG MethodID;
};

/*** Macros *****************************************************************/
#define BusyObject MUIOBJMACRO_START(MUIC_Busy)
#define BusyBar    BusyObject, MUIA_Busy_Speed, MUIV_Busy_Speed_User, End

#endif /* BUSY_MCC_H */
