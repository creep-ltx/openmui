#ifndef ZUNE_REQUESTERS_H
#define ZUNE_REQUESTERS_H
APTR ZuneAllocAslRequest(ULONG type, struct TagItem *tags);
BOOL ZuneAslRequest(APTR request, struct TagItem *tags);
void ZuneFreeAslRequest(APTR request);
/* MUI_RequestA() with an application (in the request module, modules.h) */
LONG ZuneRequestWindow(APTR app, APTR win, LONGBITS flags,
    CONST_STRPTR title, CONST_STRPTR gadgets, CONST_STRPTR format,
    APTR params);
#endif
