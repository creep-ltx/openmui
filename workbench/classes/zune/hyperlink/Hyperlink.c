/*
    Hyperlink.mcc for Zune68: a Text that follows a URI when clicked.

    Written from MUI 5's public documentation (SDK Autodocs/MCC/MCC_Hyperlink.doc)
    and the behaviour of its Hyperlink demo, black-box. MUI 5 brings this class
    with it; MUI 3.8 has none, so only MUI 4/5 programs open this file and a
    MUI 3 program pays nothing for it.

    - Without a frame the object is a link: underlined, in a link colour, the
      hand pointer over it, MUIA_Text_SetMax TRUE by default; a click follows it.
    - With a frame it is a plain button (MUIA_InputMode RelVerify, SetMax FALSE).
    - "about:", "amigados:" and "workbench:" are handled here; every other URI
      goes to openurl.library.
*/

#define MUIMASTER_YES_INLINE_STDARG

#include <exec/types.h>
#include <exec/memory.h>
#include <devices/clipboard.h>
#include <dos/dostags.h>
#include <intuition/intuition.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/alib.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>
#include <proto/utility.h>

#include <string.h>

#include "Hyperlink_mcc.h"
#include "Hyperlink_private.h"

#ifndef MUIA_PointerType
#define MUIA_PointerType 0x8042b467
#endif
#ifndef MUIV_PointerType_Link
#define MUIV_PointerType_Link 13
#endif

/* blue and underlined, as MUI 5's default link look */
#define LINK_PREPARSE "\033u\033P[0000CC]"

static STRPTR hl_strdup(CONST_STRPTR s)
{
    STRPTR d;
    if (!s) return NULL;
    d = AllocVec(strlen(s) + 1, MEMF_ANY);
    if (d) strcpy(d, s);
    return d;
}

static LONG hl_decode(CONST_STRPTR in, STRPTR out)
{
    LONG n = 0;
    if (!in || !out) return -1;
    while (*in)
    {
        UBYTE c = *in++;
        if (c == '+') c = ' ';
        else if (c == '%' && in[0] && in[1])
        {
            UBYTE h = 0; int i;
            for (i = 0; i < 2; i++)
            {
                UBYTE x = in[i];
                h <<= 4;
                if (x >= '0' && x <= '9') h |= x - '0';
                else if (x >= 'a' && x <= 'f') h |= x - 'a' + 10;
                else if (x >= 'A' && x <= 'F') h |= x - 'A' + 10;
            }
            c = h; in += 2;
        }
        out[n++] = c;
    }
    out[n] = 0;
    return n;
}

static LONG hl_encode(CONST_STRPTR in, STRPTR out)
{
    static const char hex[] = "0123456789abcdef";
    LONG n = 0;
    if (!in) return -1;
    while (*in)
    {
        UBYTE c = *in++;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
        {
            if (out) out[n] = c;
            n++;
        }
        else
        {
            if (out) { out[n] = '%'; out[n + 1] = hex[c >> 4]; out[n + 2] = hex[c & 15]; }
            n += 3;
        }
    }
    if (out) out[n] = 0;
    return n;
}

/* openurl.library's URL_OpenA (LVO -30) */
static ULONG hl_openurl(CONST_STRPTR uri)
{
    struct Library *OpenURLBase = OpenLibrary("openurl.library", 1);
    ULONG ok = FALSE;
    if (OpenURLBase)
    {
        register ULONG d0 __asm("d0");
        register CONST_STRPTR a0 __asm("a0") = uri;
        register APTR a1 __asm("a1") = NULL;
        register struct Library *a6 __asm("a6") = OpenURLBase;
        __asm volatile ("jsr -30(%%a6)"
            : "=r"(d0), "+r"(a0), "+r"(a1)
            : "r"(a6)
            : "d1", "cc", "memory");
        ok = d0 ? TRUE : FALSE;
        CloseLibrary(OpenURLBase);
    }
    return ok;
}

static ULONG hl_run(CONST_STRPTR command)
{
    BPTR in = Open("NIL:", MODE_OLDFILE), out = Open("NIL:", MODE_NEWFILE);
    LONG rc;
    if (!in || !out)
    {
        if (in) Close(in);
        if (out) Close(out);
        return FALSE;
    }
    rc = SystemTags((STRPTR)command, SYS_Input, in, SYS_Output, out,
        SYS_Asynch, TRUE, NP_StackSize, 16384, TAG_DONE);
    if (rc == -1) { Close(in); Close(out); return FALSE; }
    return TRUE;
}

/* workbench.library 44+ OpenWorkbenchObjectA (LVO -96), else nothing */
static ULONG hl_workbench(CONST_STRPTR name)
{
    struct Library *WorkbenchBase = OpenLibrary("workbench.library", 44);
    ULONG ok = FALSE;
    if (WorkbenchBase)
    {
        register ULONG d0 __asm("d0");
        register CONST_STRPTR a0 __asm("a0") = name;
        register APTR a1 __asm("a1") = NULL;
        register struct Library *a6 __asm("a6") = WorkbenchBase;
        __asm volatile ("jsr -96(%%a6)"
            : "=r"(d0), "+r"(a0), "+r"(a1)
            : "r"(a6)
            : "d1", "cc", "memory");
        ok = d0 ? TRUE : FALSE;
        CloseLibrary(WorkbenchBase);
    }
    return ok;
}

static ULONG hl_about(Object *obj, CONST_STRPTR what)
{
    Object *app = _app(obj);
    STRPTR title = NULL, version = NULL, copyright = NULL;
    char body[256];
    if (!app) return FALSE;
    if (what && *what)
    {
        if (!Stricmp((STRPTR)what, "AmigaOS"))
        {
            struct Library *ex = (struct Library *)SysBase;
            body[0] = 0;
            strcpy(body, "AmigaOS\nexec.library ");
            {
                char num[24]; LONG v = ex->lib_Version, r = ex->lib_Revision, i = 0;
                char t[12]; int k = 0;
                do { t[k++] = '0' + v % 10; v /= 10; } while (v);
                while (k) num[i++] = t[--k];
                num[i++] = '.';
                do { t[k++] = '0' + r % 10; r /= 10; } while (r);
                while (k) num[i++] = t[--k];
                num[i] = 0;
                strcat(body, num);
            }
        }
        else
        {
            strcpy(body, "Hyperlink.mcc 21.1\nZune68");
        }
    }
    else
    {
        get(app, MUIA_Application_Title, &title);
        get(app, MUIA_Application_Version, &version);
        get(app, MUIA_Application_Copyright, &copyright);
        body[0] = 0;
        if (version && !strncmp(version, "$VER: ", 6)) version += 6;
        strncat(body, version ? (const char *)version : title ? (const char *)title : "", 120);
        if (copyright) { strcat(body, "\n"); strncat(body, copyright, 120); }
    }
    MUI_Request(app, _win(obj), 0, NULL, "OK", "%s", (IPTR)body);
    return TRUE;
}

static ULONG hl_follow(Object *obj, CONST_STRPTR uri)
{
    STRPTR buf;
    ULONG ok = FALSE;
    if (!uri || !*uri) return FALSE;
    if (!Strnicmp((STRPTR)uri, "about:", 6))
    {
        if (!(buf = AllocVec(strlen(uri) + 1, MEMF_ANY))) return FALSE;
        hl_decode(uri + 6, buf);
        ok = hl_about(obj, buf);
    }
    else if (!Strnicmp((STRPTR)uri, "amigados:", 9))
    {
        if (!(buf = AllocVec(strlen(uri) + 1, MEMF_ANY))) return FALSE;
        hl_decode(uri + 9, buf);
        ok = hl_run(buf);
    }
    else if (!Strnicmp((STRPTR)uri, "workbench:", 10))
    {
        if (!(buf = AllocVec(strlen(uri) + 1, MEMF_ANY))) return FALSE;
        hl_decode(uri + 10, buf);
        ok = hl_workbench(buf);
    }
    else
        return hl_openurl(uri);
    FreeVec(buf);
    return ok;
}

/* the URI as FTXT on clipboard.device unit "unit" */
static ULONG hl_copy(CONST_STRPTR text, ULONG unit)
{
    struct MsgPort *port;
    struct IOClipReq *io;
    ULONG len, pad, ok = FALSE;
    ULONG head[5];
    if (!text) return FALSE;
    len = strlen(text); pad = len & 1;
    if (!(port = CreateMsgPort())) return FALSE;
    if ((io = (struct IOClipReq *)CreateIORequest(port, sizeof(*io))))
    {
        if (!OpenDevice("clipboard.device", unit, (struct IORequest *)io, 0))
        {
            head[0] = MAKE_ID('F','O','R','M'); head[1] = 12 + len + pad;
            head[2] = MAKE_ID('F','T','X','T'); head[3] = MAKE_ID('C','H','R','S');
            head[4] = len;
            io->io_Offset = 0; io->io_Error = 0; io->io_ClipID = 0;
            io->io_Command = CMD_WRITE; io->io_Data = (STRPTR)head; io->io_Length = 20;
            DoIO((struct IORequest *)io);
            io->io_Data = (STRPTR)text; io->io_Length = len;
            DoIO((struct IORequest *)io);
            if (pad) { io->io_Data = (STRPTR)"\0"; io->io_Length = 1; DoIO((struct IORequest *)io); }
            ok = io->io_Error == 0;
            io->io_Command = CMD_UPDATE; io->io_Length = 0;
            DoIO((struct IORequest *)io);
            CloseDevice((struct IORequest *)io);
        }
        DeleteIORequest((struct IORequest *)io);
    }
    DeleteMsgPort(port);
    return ok;
}

/*** Methods ****************************************************************/
IPTR Hyperlink__OM_NEW(Class *cl, Object *obj, struct opSet *msg)
{
    struct TagItem *tags = msg->ops_AttrList;
    CONST_STRPTR uri = (CONST_STRPTR)GetTagData(MUIA_Hyperlink_URI, 0, tags);
    BOOL button = GetTagData(MUIA_Frame, MUIV_Frame_None, tags) != MUIV_Frame_None;
    struct Hyperlink_DATA *data;

    obj = (Object *)DoSuperNewTags(cl, obj, NULL,
        MUIA_Text_SetMax, !button,
        MUIA_InputMode, MUIV_InputMode_RelVerify,
        button ? TAG_IGNORE : MUIA_Text_PreParse, (IPTR)LINK_PREPARSE,
        button ? TAG_IGNORE : MUIA_PointerType, MUIV_PointerType_Link,
        FindTagItem(MUIA_Text_Contents, tags) ? TAG_IGNORE : MUIA_Text_Contents, (IPTR)uri,
        TAG_MORE, (IPTR)tags);
    if (!obj) return 0;
    data = INST_DATA(cl, obj);
    data->button = button;
    data->uri = hl_strdup(uri);
    data->ehn.ehn_Events = IDCMP_MOUSEMOVE;
    data->ehn.ehn_Priority = 0;
    data->ehn.ehn_Flags = 0;
    data->ehn.ehn_Object = obj;
    data->ehn.ehn_Class = cl;
    return (IPTR)obj;
}

IPTR Hyperlink__OM_DISPOSE(Class *cl, Object *obj, Msg msg)
{
    struct Hyperlink_DATA *data = INST_DATA(cl, obj);
    if (data->uri) FreeVec(data->uri);
    return DoSuperMethodA(cl, obj, msg);
}

IPTR Hyperlink__OM_SET(Class *cl, Object *obj, struct opSet *msg)
{
    struct Hyperlink_DATA *data = INST_DATA(cl, obj);
    struct TagItem *tags = msg->ops_AttrList, *tag;
    IPTR rc;
    BOOL follow = FALSE;

    while ((tag = NextTagItem(&tags)))
    {
        switch (tag->ti_Tag)
        {
        case MUIA_Hyperlink_URI:
            if (data->uri) FreeVec(data->uri);
            data->uri = hl_strdup((CONST_STRPTR)tag->ti_Data);
            break;
        case MUIA_Pressed:
            /* a click released over the object (RelVerify) follows the link */
            if (!tag->ti_Data) follow = TRUE;
            break;
        }
    }
    rc = DoSuperMethodA(cl, obj, (Msg)msg);
    if (follow) DoMethod(obj, MUIM_Hyperlink_Follow);
    return rc;
}

IPTR Hyperlink__OM_GET(Class *cl, Object *obj, struct opGet *msg)
{
    struct Hyperlink_DATA *data = INST_DATA(cl, obj);
    switch (msg->opg_AttrID)
    {
    case MUIA_Hyperlink_URI:
        *msg->opg_Storage = (IPTR)data->uri;
        return TRUE;
    case MUIA_Hyperlink_HoverTip:
        *msg->opg_Storage = data->hover ? (IPTR)data->uri : 0;
        return TRUE;
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

IPTR Hyperlink__MUIM_Setup(Class *cl, Object *obj, struct MUIP_Setup *msg)
{
    struct Hyperlink_DATA *data = INST_DATA(cl, obj);
    if (!DoSuperMethodA(cl, obj, (Msg)msg)) return FALSE;
    DoMethod(_win(obj), MUIM_Window_AddEventHandler, (IPTR)&data->ehn);
    data->added = TRUE;
    return TRUE;
}

IPTR Hyperlink__MUIM_Cleanup(Class *cl, Object *obj, struct MUIP_Cleanup *msg)
{
    struct Hyperlink_DATA *data = INST_DATA(cl, obj);
    if (data->added)
    {
        DoMethod(_win(obj), MUIM_Window_RemEventHandler, (IPTR)&data->ehn);
        data->added = FALSE;
    }
    if (data->hover)
    {
        data->hover = FALSE;
        SetAttrs(obj, MUIA_Hyperlink_HoverTip, 0, TAG_DONE);
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

IPTR Hyperlink__MUIM_HandleEvent(Class *cl, Object *obj, struct MUIP_HandleEvent *msg)
{
    struct Hyperlink_DATA *data = INST_DATA(cl, obj);
    if (msg->imsg && msg->imsg->Class == IDCMP_MOUSEMOVE)
    {
        WORD x = msg->imsg->MouseX, y = msg->imsg->MouseY;
        BOOL in = x >= _left(obj) && x <= _right(obj) && y >= _top(obj) && y <= _bottom(obj);
        if (in != data->hover)
        {
            data->hover = in;
            /* notify only: the attribute is [..G], set it for the listeners */
            SetAttrs(obj, MUIA_Hyperlink_HoverTip, in ? (IPTR)data->uri : 0, TAG_DONE);
        }
    }
    return 0;
}

IPTR Hyperlink__MUIM_Hyperlink_Follow(Class *cl, Object *obj, struct MUIP_Hyperlink_Follow *msg)
{
    struct Hyperlink_DATA *data = INST_DATA(cl, obj);
    return hl_follow(obj, data->uri);
}

IPTR Hyperlink__MUIM_Hyperlink_Copy(Class *cl, Object *obj, struct MUIP_Hyperlink_Copy *msg)
{
    struct Hyperlink_DATA *data = INST_DATA(cl, obj);
    return hl_copy(data->uri, msg->unit);
}

IPTR Hyperlink__MUIM_Hyperlink_Encode(Class *cl, Object *obj, struct MUIP_Hyperlink_Encode *msg)
{
    return (IPTR)hl_encode(msg->input, msg->output);
}

IPTR Hyperlink__MUIM_Hyperlink_Decode(Class *cl, Object *obj, struct MUIP_Hyperlink_Decode *msg)
{
    return (IPTR)hl_decode(msg->input, msg->output);
}
