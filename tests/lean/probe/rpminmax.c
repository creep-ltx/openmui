/* rpminmax: a custom class that prints its render info's rastport (_rp)
 * and that rastport's font in MUIM_Setup and MUIM_AskMinMax, before the
 * window has opened. IBrowse 2.5.9's toolbar buttons measure their labels
 * by their font in AskMinMax; also prints what get(obj, MUIA_Font) gives. */
#include <libraries/mui.h>
#include <graphics/rastport.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>

struct Library *MUIMasterBase;
static int argc_dump = 1;

static void show(const char *where, Object *obj)
{
    struct RastPort *rp = _rp(obj);
    struct TextFont *f = rp ? rp->Font : NULL;
    printf("%s: _rp %s, its font %s/%d, _font %s/%d, window %s\n", where,
        rp ? "set" : "NULL",
        f && f->tf_Message.mn_Node.ln_Name ? f->tf_Message.mn_Node.ln_Name : "-", f ? f->tf_YSize : 0,
        _font(obj) ? _font(obj)->tf_Message.mn_Node.ln_Name : "-", _font(obj) ? _font(obj)->tf_YSize : 0,
        _window(obj) ? "open" : "none");
    if (rp && f)
        printf("  TextLength(\"Back\") = %d\n", (int)TextLength(rp, "Back", 4));
    printf("  _screen %08lx, its font %08lx, _dri %08lx, dri_Font %08lx, _pens %08lx\n",
        (unsigned long)_screen(obj), _screen(obj) ? (unsigned long)_screen(obj)->RastPort.Font : 0UL,
        (unsigned long)_dri(obj), _dri(obj) ? (unsigned long)_dri(obj)->dri_Font : 0UL,
        (unsigned long)_pens(obj));
    if (argc_dump && muiRenderInfo(obj))
    {
        /* which words of the render info (past its public part) and of
         * the area data hold the object's font pointer */
        ULONG *m = (ULONG *)muiRenderInfo(obj), *a = (ULONG *)muiAreaData(obj);
        int i;
        for (i = 0; i < 64; i++)
            if (m[i] == (ULONG)_font(obj)) printf("  render info word %d (offset %d) = _font\n", i, i * 4);
        for (i = 0; i < 40; i++)
            if (a[i] == (ULONG)_font(obj)) printf("  area data word %d (offset %d) = _font\n", i, i * 4);
        printf("  render info offsets 28-60:");
        for (i = 7; i < 16; i++)
        {
            struct TextFont *f = (struct TextFont *)m[i];
            if (f && (ULONG)f > 0x1000 && (ULONG)f < 0x10000000 && f->tf_YSize < 100)
                printf(" %s/%d", f->tf_Message.mn_Node.ln_Name ? f->tf_Message.mn_Node.ln_Name : "?", f->tf_YSize);
            else
                printf(" %08lx", (unsigned long)m[i]);
        }
        printf("\n");
    }
    {
        IPTR v = 0xdeadbeef;
        ULONG ok = GetAttr(MUIA_Font, obj, &v);
        printf("  get MUIA_Font: %s %08lx (_font %08lx)\n", ok ? "ok" : "not known",
            (unsigned long)v, (unsigned long)_font(obj));
    }
}

static IPTR dispatch(struct IClass *cl __asm("a0"), Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    IPTR r;
    switch (msg->MethodID)
    {
    case OM_NEW:
        r = DoSuperMethodA(cl, obj, msg);
        if (r)
            printf("OM_NEW: _font %08lx\n", (unsigned long)_font((Object *)r));
        return r;
    case MUIM_ConnectParent:
        printf("ConnectParent before: _font %08lx\n", (unsigned long)_font(obj));
        r = DoSuperMethodA(cl, obj, msg);
        printf("ConnectParent after: _font %08lx\n", (unsigned long)_font(obj));
        return r;
    case MUIM_Setup:
        printf("Setup before: _font %08lx\n", (unsigned long)_font(obj));
        r = DoSuperMethodA(cl, obj, msg);
        if (r) show("Setup", obj);
        return r;
    case MUIM_AskMinMax:
        r = DoSuperMethodA(cl, obj, msg);
        show("AskMinMax", obj);
        ((struct MUIP_AskMinMax *)msg)->MinMaxInfo->MinWidth += 20;
        ((struct MUIP_AskMinMax *)msg)->MinMaxInfo->DefWidth += 20;
        ((struct MUIP_AskMinMax *)msg)->MinMaxInfo->MaxWidth += 20;
        ((struct MUIP_AskMinMax *)msg)->MinMaxInfo->MinHeight += 10;
        ((struct MUIP_AskMinMax *)msg)->MinMaxInfo->DefHeight += 10;
        ((struct MUIP_AskMinMax *)msg)->MinMaxInfo->MaxHeight += 10;
        return r;
    }
    return DoSuperMethodA(cl, obj, msg);
}

int main(void)
{
    struct MUI_CustomClass *mcc;
    Object *app, *win;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    mcc = MUI_CreateCustomClass(NULL, MUIC_Area, NULL, 0, (APTR)dispatch);
    if (!mcc) return 20;
    app = ApplicationObject, MUIA_Application_Title, (IPTR)"RPMinMax", MUIA_Application_Base, (IPTR)"RPMINMAX",
        SubWindow, (IPTR)(win = WindowObject, MUIA_Window_Title, (IPTR)"RPMinMax",
            WindowContents, (IPTR)(VGroup,
                Child, (IPTR)NewObject(mcc->mcc_Class, NULL, TAG_DONE),
                End), End), End;
    if (!app) return 20;
    set(win, MUIA_Window_Open, TRUE);
    Delay(50);
    MUI_DisposeObject(app);
    MUI_DeleteCustomClass(mcc);
    CloseLibrary(MUIMasterBase);
    return 0;
}
