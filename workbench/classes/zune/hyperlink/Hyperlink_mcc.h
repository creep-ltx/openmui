#ifndef HYPERLINK_MCC_H
#define HYPERLINK_MCC_H

/*
    Hyperlink.mcc for Zune68: the interface of MUI 5's Hyperlink class, from
    its public autodoc (MCC_Hyperlink.doc) and the SDK header's values.
*/

#include <libraries/mui.h>

#define MUIC_Hyperlink              "Hyperlink.mcc"
#define HyperlinkObject             MUI_NewObject(MUIC_Hyperlink

#define MUIA_Hyperlink_URI          0xfed10033UL /* [ISG] STRPTR */
#define MUIA_Hyperlink_HoverTip     0xfed10036UL /* [..G] STRPTR */
#define MUIA_Hyperlink_Text         MUIA_Text_Contents
#define MUIA_Hyperlink_SetMax       MUIA_Text_SetMax

#define MUIM_Hyperlink_Follow       0xfed10034UL
#define MUIM_Hyperlink_Copy         0xfed10035UL
#define MUIM_Hyperlink_Encode       0xfed10037UL
#define MUIM_Hyperlink_Decode       0xfed10038UL

struct MUIP_Hyperlink_Follow { STACKED ULONG MethodID; };
struct MUIP_Hyperlink_Copy   { STACKED ULONG MethodID; STACKED ULONG unit; };
struct MUIP_Hyperlink_Encode { STACKED ULONG MethodID; STACKED CONST_STRPTR input; STACKED STRPTR output; };
struct MUIP_Hyperlink_Decode { STACKED ULONG MethodID; STACKED CONST_STRPTR input; STACKED STRPTR output; };

#define MUIV_Hyperlink_Encode_Count NULL

#endif
