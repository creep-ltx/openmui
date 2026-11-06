/*
    Copyright  2026, The AROS Development Team.
    All rights reserved.

*/

/* The extra title bar gadgets as MUI 3.8 has them (Popup, Prefs, Snapshot
 * and Iconify, left of the zoom and depth gadgets) and the Popup gadget's
 * window menu. Off by default (Zune prefs, Windows page), so this code is
 * the title gadget module (modules.h, classes/zwintitle.c), loaded when a
 * window first gets one of them; classes/window.c decides whether a window
 * gets any and handles the clicks. Written from a description of MUI 3.8's
 * behaviour (studied running, not disassembled); the drawings are our own
 * (titleglyphs.h).
 *
 * Each box is 23 pixels wide and as tall as the title bar, on every
 * screen. One private gadget class for all windows, made when the first
 * window gets one (the library keeps it and frees it at expunge); a window
 * holds only its gadget objects. */

#include <exec/types.h>
#include <string.h>
#include <intuition/gadgetclass.h>
#include <intuition/imageclass.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/muimaster.h>

#include "mui.h"
#include "muimaster_intern.h"
#include "support.h"
#include "titleglyphs.h"
#include "modules.h"

extern struct Library *MUIMasterBase;

/* Prefs bit of each gadget, in title bar order (TG_POPUP...) */
static const UBYTE TitleButtons[4] = {
    MUIV_Window_Button_Popup, MUIV_Window_Button_MUI,
    MUIV_Window_Button_Snapshot, MUIV_Window_Button_Iconify
};
#define TITLE_BOX      ZUNE_TITLE_BOX
#define TITLE_LEFTMOST 4   /* in the gadget's UserData: draw the divider */

/* The window menu's items in order (ZUNE_TA_*) */
static const char *const TitleMenuLabels[] = {
    "Iconify", "Snapshot", "Unsnapshot", "Settings...", "About MUI..."
};

static void TitleGadgetRender(struct Gadget *g, struct GadgetInfo *gi,
    struct RastPort *rp)
{
    UWORD *dp = gi->gi_DrInfo->dri_Pens;
    BOOL active = (gi->gi_Window->Flags & WFLG_WINDOWACTIVE) != 0;
    BOOL pressed = (g->Flags & GFLG_SELECTED) != 0;
    ULONG which = (ULONG)g->UserData & 3;
    WORD x = gi->gi_Domain.Left + g->LeftEdge;
    WORD y = gi->gi_Domain.Top + g->TopEdge;
    WORD h = g->Height, top, bottom;
    UWORD pens[4], back = dp[BACKGROUNDPEN];

    if (g->Flags & GFLG_RELRIGHT)
        x += gi->gi_Domain.Width - 1;
    /* An inactive window: SHINE and FILL become BACKGROUND, the SHADOW
     * lines stay; a pressed gadget stays FILL inside. */
    pens[TG_SHADOW] = dp[SHADOWPEN];
    pens[TG_SHINE] = active ? dp[SHINEPEN] : back;
    pens[TG_FILL] = active ? dp[FILLPEN] : back;
    pens[TG_BACK] = back;
    SetDrMd(rp, JAM1);
    SetAPen(rp, active || pressed ? dp[FILLPEN] : back);
    RectFill(rp, x + 1, y + 1, x + TITLE_BOX - 2, y + h - 2);
    /* the top and bottom rows run through the corners, as on
     * Intuition's own gadgets */
    SetAPen(rp, dp[pressed ? SHADOWPEN : SHINEPEN]);
    RectFill(rp, x, y, x + TITLE_BOX - 1, y);
    RectFill(rp, x, y + 1, x, y + h - 2);
    SetAPen(rp, dp[pressed ? SHINEPEN : SHADOWPEN]);
    RectFill(rp, x + TITLE_BOX - 1, y + 1, x + TITLE_BOX - 1, y + h - 2);
    RectFill(rp, x, y + h - 1, x + TITLE_BOX - 1, y + h - 1);
    if ((ULONG)g->UserData & TITLE_LEFTMOST)
    {
        SetAPen(rp, dp[SHADOWPEN]);
        RectFill(rp, x - 1, y + 1, x - 1, y + h - 2);
    }
    if (which == TG_ICONIFY)
    {
        /* stretches with the title bar like the zoom gadget */
        WORD m = (h + 2) / 7;
        top = y + 1 + m;
        bottom = y + h - 2 - m;
    }
    else
    {
        /* an 8-row drawing centred in the inner rows, odd row below */
        top = y + 1 + ((h - 10) >> 1);
        bottom = top + 7;
    }
    DrawTitleGlyph(rp, pens, which * 2 + pressed,
        x + (TITLE_BOX - TG_WIDTH) / 2, top, bottom, y + 1, y + h - 2);
}

static void TitleGadgetRefresh(struct Gadget *g, struct GadgetInfo *gi)
{
    struct RastPort *rp = ObtainGIRPort(gi);
    if (rp)
    {
        TitleGadgetRender(g, gi, rp);
        ReleaseGIRPort(rp);
    }
}

static BOOPSI_DISPATCHER(IPTR, TitleGadget_Dispatcher, cl, obj, msg)
{
    struct Gadget *g = (struct Gadget *)obj;
    struct gpInput *gpi = (struct gpInput *)msg;

    switch (msg->MethodID)
    {
    case GM_RENDER:
        TitleGadgetRender(g, ((struct gpRender *)msg)->gpr_GInfo,
            ((struct gpRender *)msg)->gpr_RPort);
        return 0;
    case GM_GOACTIVE:
        {
            struct Window *win = gpi->gpi_GInfo->gi_Window;
            /* a sleeping window's gadgets look normal and do nothing */
            if (!gpi->gpi_IEvent || !win->UserData ||
                zune_window_sleeping((Object *)win->UserData))
                return GMR_NOREUSE;
            g->Flags |= GFLG_SELECTED;
            TitleGadgetRefresh(g, gpi->gpi_GInfo);
            return GMR_MEACTIVE;
        }
    case GM_HANDLEINPUT:
        {
            struct InputEvent *ie = gpi->gpi_IEvent;
            BOOL in = gpi->gpi_Mouse.X >= 0 && gpi->gpi_Mouse.X < g->Width &&
                gpi->gpi_Mouse.Y >= 0 && gpi->gpi_Mouse.Y < g->Height;

            if (ie->ie_Class != IECLASS_RAWMOUSE)
                return GMR_MEACTIVE;
            if (ie->ie_Code == SELECTUP)
            {
                *gpi->gpi_Termination = 0;
                return in ? GMR_NOREUSE | GMR_VERIFY : GMR_NOREUSE;
            }
            if (ie->ie_Code == MENUDOWN)
                return GMR_NOREUSE;
            if (in != ((g->Flags & GFLG_SELECTED) != 0))
            {
                g->Flags ^= GFLG_SELECTED;
                TitleGadgetRefresh(g, gpi->gpi_GInfo);
            }
            return GMR_MEACTIVE;
        }
    case GM_GOINACTIVE:
        g->Flags &= ~GFLG_SELECTED;
        TitleGadgetRefresh(g, ((struct gpGoInactive *)msg)->gpgi_GInfo);
        return 0;
    }
    return DoSuperMethodA(cl, obj, msg);
}
BOOPSI_DISPATCHER_END

/* The gadgets of the buttons in BUTTONS (MUIV_Window_Button_*), right to
 * left from RIGHT pixels left of the window's right edge, added to WIN and
 * stored in GADGETS[TG_*]. *CLP is the library's gadget class, made here
 * the first time. */
void zune_wintitle_create(struct Window *win, ULONG buttons, LONG right,
    struct Gadget **gadgets, Class **clp)
{
    struct Gadget *g, *leftmost = NULL;
    LONG height = win->BorderTop;
    int i;

    if (!*clp)
    {
        struct MUIMasterBase_intern *base = MUIMB(MUIMasterBase);
        ObtainSemaphore(&base->ZuneSemaphore);
        if (!*clp)
        {
            Class *cl = MakeClass(NULL, GADGETCLASS, NULL, 0, 0);
            if (cl)
            {
                cl->cl_Dispatcher.h_Entry = (HOOKFUNC)TitleGadget_Dispatcher;
                *clp = cl;
            }
        }
        ReleaseSemaphore(&base->ZuneSemaphore);
        if (!*clp)
            return;
    }

    /* Right to left; switched off ones leave no gap. MUI 3.8 does not
     * widen a narrow window for them. */
    for (i = 4; i--; )
    {
        if (!(buttons & TitleButtons[i]))
            continue;
        g = (struct Gadget *)NewObject(*clp, NULL,
            GA_RelRight, -(right + TITLE_BOX), GA_Top, 0,
            GA_Width, TITLE_BOX, GA_Height, height, GA_TopBorder, TRUE,
            GA_RelVerify, TRUE, GA_UserData, i, TAG_DONE);
        if (!g)
            continue;
        right += TITLE_BOX;
        gadgets[i] = g;
        leftmost = g;
        /* Before the system drag gadget, so this region stays clickable */
        AddGadget(win, g, 0);
    }
    if (leftmost)
    {
        leftmost->UserData = (APTR)((ULONG)leftmost->UserData | TITLE_LEFTMOST);
        /* Draws the gadgets and cuts the title where they begin */
        RefreshWindowFrame(win);
    }
}

/* One window menu item: a FILL bar 2 pixels in from the side lines when
 * highlighted, the label centred one row below the item's top. */
static void TitleMenuItem(struct RastPort *rp, UWORD *pens,
    struct TextFont *font, const char *s, WORD y, WORD w, WORD bw, BOOL hl)
{
    WORD len = strlen(s), l = TextLength(rp, s, len);
    SetAPen(rp, pens[hl ? FILLPEN : SHINEPEN]);
    RectFill(rp, bw + 2, y, w - 3 - bw, y + font->tf_YSize);
    SetAPen(rp, pens[TEXTPEN]);
    Move(rp, bw + (w - 2 * bw - l) / 2, y + 1 + font->tf_Baseline);
    Text(rp, s, len);
}

/* The window menu: an own drawn menu under the title bar of PW, centred
 * on the Popup gadget; Snapshot and Unsnapshot only for a window with an
 * ID. Return the chosen ZUNE_TA_* or -1. */
int zune_wintitle_menu(struct Window *pw, struct DrawInfo *dri,
    struct Gadget *popup, BOOL has_id)
{
    struct Screen *scr = pw->WScreen;
    struct Window *mw;
    UWORD *pens = dri->dri_Pens;
    struct TextFont *font = dri->dri_Font;
    struct RastPort *rp, measure;
    UBYTE items[5];
    WORD n = 0, i, hit = -1, choice = -1, fh = font->tf_YSize + 1;
    WORD tw = 0, w, h, bw, left, top;
    BOOL down = FALSE, done = FALSE, deactivated = FALSE;

    for (i = 0; i < 5; i++)
    {
        if ((i == ZUNE_TA_SNAPSHOT || i == ZUNE_TA_UNSNAPSHOT) && !has_id)
            continue;
        items[n++] = i;
    }
    InitRastPort(&measure);
    SetFont(&measure, font);
    for (i = 0; i < n; i++)
    {
        const char *s = TitleMenuLabels[items[i]];
        WORD l = TextLength(&measure, s, strlen(s));
        if (l > tw)
            tw = l;
    }
    /* Twice as wide side lines where pixels are twice as tall */
    bw = dri->dri_Resolution.Y > dri->dri_Resolution.X ? 2 : 1;
    w = tw + 28 + 2 * bw;
    h = 4 + n * fh;
    left = pw->LeftEdge + pw->Width - 1 + popup->LeftEdge + TITLE_BOX / 2
        - w / 2;
    if (left + w > scr->Width)
        left = scr->Width - w;
    if (left < 0)
        left = 0;
    top = pw->TopEdge + pw->BorderTop;
    if (top + h > scr->Height)
        top = scr->Height - h;
    mw = OpenWindowTags(NULL, WA_CustomScreen, (IPTR)scr,
        WA_Left, left, WA_Top, top, WA_Width, w, WA_Height, h,
        WA_Borderless, TRUE, WA_Activate, TRUE, WA_RMBTrap, TRUE,
        WA_ReportMouse, TRUE, WA_IDCMP, IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS |
        IDCMP_RAWKEY | IDCMP_INACTIVEWINDOW, TAG_DONE);
    if (!mw)
        return -1;
    rp = mw->RPort;
    SetFont(rp, font);
    SetDrMd(rp, JAM1);
    SetAPen(rp, pens[SHADOWPEN]);
    RectFill(rp, 0, 0, w - 1, h - 1);
    SetAPen(rp, pens[SHINEPEN]);
    RectFill(rp, bw, 1, w - 1 - bw, h - 2);
    for (i = 0; i < n; i++)
        TitleMenuItem(rp, pens, font, TitleMenuLabels[items[i]],
            2 + i * fh, w, bw, FALSE);
    while (!done)
    {
        struct IntuiMessage *im;
        WaitPort(mw->UserPort);
        while ((im = (struct IntuiMessage *)GetMsg(mw->UserPort)))
        {
            ULONG class = im->Class;
            UWORD code = im->Code;
            WORD mx = im->MouseX, my = im->MouseY, now = -1;
            ReplyMsg((struct Message *)im);
            if (class == IDCMP_INACTIVEWINDOW)
            {
                done = deactivated = TRUE;
                continue;
            }
            if (class == IDCMP_RAWKEY)
            {
                if (code == 0x45)   /* Esc */
                    done = TRUE;
                continue;
            }
            if (mx >= bw && mx < w - bw && my >= 2 && my < h - 2)
                now = (my - 2) / fh;
            if (now != hit)
            {
                if (hit >= 0)
                    TitleMenuItem(rp, pens, font, TitleMenuLabels[items[hit]],
                        2 + hit * fh, w, bw, FALSE);
                hit = now;
                if (hit >= 0)
                    TitleMenuItem(rp, pens, font, TitleMenuLabels[items[hit]],
                        2 + hit * fh, w, bw, TRUE);
            }
            if (class == IDCMP_MOUSEBUTTONS)
            {
                if (code == SELECTDOWN)
                    down = TRUE;
                else if (code == SELECTUP && down && hit >= 0)
                {
                    choice = items[hit];
                    done = TRUE;
                }
            }
        }
    }
    CloseWindow(mw);
    if (!deactivated)
        ActivateWindow(pw);
    return choice;
}
