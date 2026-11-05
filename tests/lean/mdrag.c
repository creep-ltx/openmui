/* mdrag DX DY STEPS HOLDTICKS: press the left button where the pointer is,
 * move by DX,DY in STEPS relative steps with the button held (qualifier
 * set, as a real mouse does), wait HOLDTICKS, release. Test tool only. */
#include <exec/io.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdlib.h>

static struct IOStdReq *io;

static void send(UWORD code, UWORD qual, WORD dx, WORD dy)
{
    struct InputEvent ie = {0};

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

int main(int argc, char **argv)
{
    struct MsgPort *port;
    LONG dx, dy, steps, hold, i, px = 0, py = 0;

    if (argc < 5) return 20;
    dx = atol(argv[1]); dy = atol(argv[2]);
    steps = atol(argv[3]); hold = atol(argv[4]);
    if (steps < 1) steps = 1;
    if (!(port = CreateMsgPort())) return 20;
    if (!(io = (struct IOStdReq *)CreateIORequest(port, sizeof(*io))))
        return 20;
    if (OpenDevice("input.device", 0, (struct IORequest *)io, 0))
        return 20;
    send(IECODE_LBUTTON, IEQUALIFIER_LEFTBUTTON, 0, 0);
    Delay(5);
    for (i = 1; i <= steps; i++)
    {
        LONG x = dx * i / steps, y = dy * i / steps;
        send(IECODE_NOBUTTON, IEQUALIFIER_LEFTBUTTON, x - px, y - py);
        px = x; py = y;
        Delay(2);
    }
    Delay(hold);
    send(IECODE_LBUTTON | IECODE_UP_PREFIX, 0, 0, 0);
    CloseDevice((struct IORequest *)io);
    DeleteIORequest((struct IORequest *)io);
    DeleteMsgPort(port);
    return 0;
}
