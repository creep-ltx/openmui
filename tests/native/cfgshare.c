/* cfgshare: the shared global settings (classes/configdata.c, cfg_global).
 * OpenMUI only: it writes ENV:OpenMUI/global.prefs and application files
 * and reads settings back through MUIM_GetConfigItem of applications made
 * before and after each change. Every line ends in ok or FAIL; the files
 * it wrote are deleted at the end (an ENV:OpenMUI/global.prefs that was
 * there is kept as global.prefs.cfgshare and put back).
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os cfgshare.c -o cfgshare -lmui -lamiga
 */
#include <stdio.h>
#include <string.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#ifndef MUIM_GetConfigItem
#define MUIM_GetConfigItem 0x80423edb
#endif
#ifndef MUIM_Application_SetConfigItem
#define MUIM_Application_SetConfigItem 0x80424a80
#endif
#define CFG_CURSOR 0x400    /* MUICFG_String_Cursor, a string, default m7 */
#define CFG_HSPACE 0x07     /* MUICFG_Group_HSpacing, a ULONG, default 6 */
#define CFG_WINPOS 0x509
struct Library *MUIMasterBase;
static int fails;

static UBYTE buf[512];
static int len;
static void put32(ULONG v) { buf[len++] = v >> 24; buf[len++] = v >> 16; buf[len++] = v >> 8; buf[len++] = v; }
static void rec(ULONG id, const void *d, int n) { put32(id); put32(n); memcpy(buf + len, d, n); len += n; }
static void recul(ULONG id, ULONG v) { UBYTE b[4] = { v >> 24, v >> 16, v >> 8, v }; rec(id, b, 4); }
static void begin(void) { len = 0; memcpy(buf, "FORM\0\0\0\0PREFPRHD\0\0\0\6\0\0\0\0\0\0MUIC\0\0\0\0", 34); len = 34; }
static void save(const char *name)
{
    BPTR f;
    int n = len - 34, total;
    buf[30] = n >> 24; buf[31] = n >> 16; buf[32] = n >> 8; buf[33] = n;
    if (len & 1) buf[len++] = 0;
    total = len - 8;
    buf[4] = total >> 24; buf[5] = total >> 16; buf[6] = total >> 8; buf[7] = total;
    if ((f = Open(name, MODE_NEWFILE))) { Write(f, buf, len); Close(f); }
    else { printf("cannot write %s FAIL\n", name); fails++; }
}
static Object *app(const char *base)
{
    return ApplicationObject, MUIA_Application_Title, "cfgshare",
        MUIA_Application_Base, base, End;
}
static void check(const char *what, Object *a, const char *cursor, ULONG hspace)
{
    ULONG s = 0, h = 0;
    DoMethod(a, MUIM_GetConfigItem, CFG_CURSOR, (ULONG)&s);
    DoMethod(a, MUIM_GetConfigItem, CFG_HSPACE, (ULONG)&h);
    if (s && !strcmp((char *)s, cursor) && h == hspace)
        printf("%-44s %s %lu ok\n", what, (char *)s, h);
    else
    {
        printf("%-44s %s %lu, expected %s %lu FAIL\n", what,
            s ? (char *)s : "(none)", h, cursor, hspace);
        fails++;
    }
}
int main(void)
{
    Object *a, *b, *c, *d, *e, *f;
    BPTR l;
    BOOL had_drawer, had_global;
    static const UBYTE winpos[24] = { 0,0,0,24, 'T','E','S','T', 0,10,0,10,0,100,0,50, 0,10,0,10,0,100,0,50 };

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    if ((had_drawer = (l = Lock("ENV:OpenMUI", SHARED_LOCK)) != 0)) UnLock(l);
    else if ((l = CreateDir("ENV:OpenMUI"))) UnLock(l);
    had_global = Rename("ENV:OpenMUI/global.prefs", "ENV:OpenMUI/global.prefs.cfgshare");

    begin(); rec(CFG_CURSOR, "m3", 3); recul(CFG_HSPACE, 9); save("ENV:OpenMUI/global.prefs");
    a = app("CFGA"); b = app("CFGB");
    if (!a || !b) { printf("no application FAIL\n"); return 20; }
    check("first application, global m3 9", a, "m3", 9);
    check("second application, the same file", b, "m3", 9);

    Delay(60);
    begin(); rec(CFG_CURSOR, "m4", 3); recul(CFG_HSPACE, 11); recul(0x08, 5); save("ENV:OpenMUI/global.prefs");
    c = app("CFGC");
    check("made after the file changed", c, "m4", 11);
    check("made before it: its settings stay", a, "m3", 9);

    DoMethod(a, MUIM_Application_SetConfigItem, CFG_CURSOR, (ULONG)"m1");
    check("SetConfigItem on the first", a, "m1", 9);
    check("the second is untouched", b, "m3", 9);
    MUI_DisposeObject(b);
    check("the first after the second is gone", a, "m1", 9);

    /* (an application's ARexx base, and so its file, ends in .1) */
    begin(); rec(CFG_WINPOS, winpos, 24); save("ENV:OpenMUI/CFGD.1.prefs");
    d = app("CFGD");
    check("own file with window positions only", d, "m4", 11);
    begin(); recul(CFG_HSPACE, 13); save("ENV:OpenMUI/CFGE.1.prefs");
    e = app("CFGE");
    check("own file with a setting", e, "m4", 13);
    DoMethod(e, MUIM_Application_SetConfigItem, CFG_CURSOR, (ULONG)"m2");
    check("SetConfigItem on that one", e, "m2", 13);
    check("the shared ones untouched", d, "m4", 11);

    DeleteFile("ENV:OpenMUI/global.prefs");
    f = app("CFGF");
    check("made after the file was deleted: defaults", f, "m7", 6);
    check("made before: its settings stay", c, "m4", 11);

    MUI_DisposeObject(a); MUI_DisposeObject(c); MUI_DisposeObject(d);
    MUI_DisposeObject(e); MUI_DisposeObject(f);

    begin(); rec(CFG_CURSOR, "m5", 3); save("ENV:OpenMUI/global.prefs");
    a = app("CFGA");
    check("a new file after all were gone", a, "m5", 6);
    MUI_DisposeObject(a);

    DeleteFile("ENV:OpenMUI/global.prefs");
    DeleteFile("ENV:OpenMUI/CFGD.1.prefs");
    DeleteFile("ENV:OpenMUI/CFGE.1.prefs");
    if (had_global) Rename("ENV:OpenMUI/global.prefs.cfgshare", "ENV:OpenMUI/global.prefs");
    if (!had_drawer) DeleteFile("ENV:OpenMUI");
    CloseLibrary(MUIMasterBase);
    printf(fails ? "cfgshare: %d FAILED\n" : "cfgshare: all ok\n", fails);
    return fails ? 5 : 0;
}
