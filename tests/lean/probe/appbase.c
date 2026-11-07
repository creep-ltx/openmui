/* appbase: two applications made with MUIA_Application_Base "lowBase";
 * prints what MUIA_Application_Base gives back for each (MUI 3.8: the
 * ARexx port's name, upper case, with ".1", ".2"). */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>

struct Library *MUIMasterBase;

int main(void)
{
    Object *a1, *a2;
    char *b1 = NULL, *b2 = NULL;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    a1 = ApplicationObject, MUIA_Application_Base, (IPTR)"lowBase", End;
    a2 = ApplicationObject, MUIA_Application_Base, (IPTR)"lowBase", End;
    if (a1) get(a1, MUIA_Application_Base, &b1);
    if (a2) get(a2, MUIA_Application_Base, &b2);
    printf("base 1 '%s' (port %s), base 2 '%s'\n", b1 ? b1 : "-",
        b1 && FindPort((STRPTR)b1) ? "found" : "not found", b2 ? b2 : "-");
    if (a2) MUI_DisposeObject(a2);
    if (a1) MUI_DisposeObject(a1);
    CloseLibrary(MUIMasterBase);
    return 0;
}
