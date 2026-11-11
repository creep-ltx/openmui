/* mpress: mouse input for timing and drag tests, sent through
 * input.device as a real mouse does (button qualifiers held while moving).
 *   mpress pop MAXTICKS DX DY [pixel]  (hold: the same, held MAXTICKS frames;
 *                            pixel: watch the pixel only, not the layers:
 *                            a popup window adds its layer before it draws)
 *                            press the left button where the pointer is,
 *                            count video frames until the pixel DX,DY from
 *                            the pointer changes or a layer opens on the
 *                            front screen (a cycle popup: MUI 3.8 draws
 *                            its popup with no layer of its own), release
 *                            then or after MAXTICKS frames; prints
 *                            "popup after N frames" or "none in N frames"
 *   mpress drag B QUAL DX DY STEPS HOLD
 *                            B l or m (left / middle button), QUAL extra
 *                            qualifier bits in hex (0 none; 8 control,
 *                            10 left alt), move by DX,DY in STEPS steps
 *                            two ticks apart, hold HOLD ticks, release
 * Test tool only. Build: m68k-amigaos-gcc -m68000 -noixemul -Os mpress.c
 *                        -o mpress -lamiga */
#include <exec/io.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <intuition/intuitionbase.h>
#include <graphics/clip.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <stdio.h>
#include <stdlib.h>

extern struct IntuitionBase *IntuitionBase;
static struct IOStdReq *io;

static void send(UWORD code, UWORD qual, WORD dx, WORD dy)
{
    struct InputEvent ie = {0};
    struct DateStamp ds;

    /* a time stamp, as real events have: Intuition copies it into the
     * IntuiMessage, and a zero time can look like no time passing */
    DateStamp(&ds);
    ie.ie_TimeStamp.tv_secs = (ds.ds_Days * 1440 + ds.ds_Minute) * 60
        + ds.ds_Tick / TICKS_PER_SECOND;
    ie.ie_TimeStamp.tv_micro = (ds.ds_Tick % TICKS_PER_SECOND) * 20000;
    ie.ie_Class = IECLASS_RAWMOUSE;
    ie.ie_Code = code;
    ie.ie_Qualifier = qual | IEQUALIFIER_RELATIVEMOUSE;
    ie.ie_X = dx;
    ie.ie_Y = dy;
    io->io_Command = IND_WRITEEVENT;
    io->io_Data = &ie;
    io->io_Length = sizeof(ie);
    DoIO((struct IORequest *)io);
}

static LONG px, py;
static int argc_layers = 1;   /* "pixel" as a fifth argument: the pixel only */

static int windows(void)
{
    struct Screen *scr = IntuitionBase->FirstScreen;
    struct Layer *l;
    int n = 0;

    /* layers, not windows: a popup may be a layer of its own */
    Forbid();
    for (l = IntuitionBase->FirstScreen->LayerInfo.top_layer; l; l = l->back)
        n++;
    Permit();
    return (argc_layers ? n * 256 : 0) + ReadPixel(&scr->RastPort, px, py);
}

int main(int argc, char **argv)
{
    struct MsgPort *port;

    if (argc < 3) return 20;
    if (!(port = CreateMsgPort())) return 20;
    if (!(io = (struct IOStdReq *)CreateIORequest(port, sizeof(*io))))
        return 20;
    if (OpenDevice("input.device", 0, (struct IORequest *)io, 0))
        return 20;
    if (argv[1][0] == 'p' || argv[1][0] == 'h')
    {
        LONG max = atol(argv[2]), n, at = -1;
        int before;

        if (argc < 5) return 20;
        if (argc > 5) argc_layers = 0;
        px = IntuitionBase->FirstScreen->MouseX + atol(argv[3]);
        py = IntuitionBase->FirstScreen->MouseY + atol(argv[4]);
        before = windows();

        WaitTOF();
        send(IECODE_LBUTTON, IEQUALIFIER_LEFTBUTTON, 0, 0);
        for (n = 0; n < max; n++)
        {
            if (at < 0 && windows() != before)
            {
                at = n;
                if (argv[1][0] == 'p') break;
            }
            WaitTOF();
        }
        if (at >= 0)
            printf("popup after %ld frames\n", (long)at);
        else
            printf("none in %ld frames\n", (long)n);
        send(IECODE_LBUTTON | IECODE_UP_PREFIX, 0, 0, 0);
    }
    else if (argc >= 8)
    {
        BOOL mid = argv[2][0] == 'm';
        UWORD code = mid ? IECODE_MBUTTON : IECODE_LBUTTON;
        UWORD bq = mid ? IEQUALIFIER_MIDBUTTON : IEQUALIFIER_LEFTBUTTON;
        UWORD q = (UWORD)strtol(argv[3], NULL, 16);
        LONG dx = atol(argv[4]), dy = atol(argv[5]);
        LONG steps = atol(argv[6]), hold = atol(argv[7]), i, px = 0, py = 0;

        if (steps < 1) steps = 1;
        send(code, bq | q, 0, 0);
        Delay(5);
        for (i = 1; i <= steps; i++)
        {
            LONG x = dx * i / steps, y = dy * i / steps;
            send(IECODE_NOBUTTON, bq | q, x - px, y - py);
            px = x; py = y;
            Delay(2);
        }
        Delay(hold);
        send(code | IECODE_UP_PREFIX, q, 0, 0);
    }
    CloseDevice((struct IORequest *)io);
    DeleteIORequest((struct IORequest *)io);
    DeleteMsgPort(port);
    return 0;
}
