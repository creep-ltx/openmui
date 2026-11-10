/* bmlogo: a Bitmap object as AmTelnet makes its logo (tags logged with
 * tagspy): a 50x37 5-plane chip bitmap of its own (InitBitMap), 32
 * SourceColors, Precision 0, an ImageButton frame, inner spacing 0,
 * FixWidth/FixHeight; beside it the same with Transparent 0.  Each colour
 * index is a vertical band; the window stays open for a screenshot until
 * closed (or Ctrl-C). */
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <string.h>

struct Library *MUIMasterBase;

#define W 50
#define H 37
#define D 5

static ULONG colors[32 * 3];
static struct BitMap bm;

int main(void)
{
    Object *app, *win;
    ULONG sigs = 0;
    int i, x, y, bpr = ((W + 15) >> 4) << 1;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    InitBitMap(&bm, D, W, H);
    for (i = 0; i < D; i++)
    {
        if (!(bm.Planes[i] = AllocMem(bpr * H, MEMF_CHIP | MEMF_CLEAR))) return 20;
    }
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++)
        {
            int c = (x * 32 / W) ^ (y >= H / 2 ? 31 : 0);
            for (i = 0; i < D; i++)
                if (c & (1 << i))
                    bm.Planes[i][y * bpr + (x >> 3)] |= 0x80 >> (x & 7);
        }
    for (i = 0; i < 32; i++)   /* white to black, then blue */
    {
        ULONG v = i < 16 ? (255 - i * 17) : 0;
        colors[i * 3] = colors[i * 3 + 1] = v * 0x01010101;
        colors[i * 3 + 2] = (i < 16 ? v : (i - 16) * 17) * 0x01010101;
    }

    app = ApplicationObject, MUIA_Application_Base, (IPTR)"BMLOGO",
        SubWindow, win = WindowObject, MUIA_Window_Title, (IPTR)"bmlogo",
            WindowContents, HGroup,
                Child, BitmapObject, MUIA_Frame, MUIV_Frame_ImageButton,
                    MUIA_InnerLeft, 0, MUIA_InnerRight, 0, MUIA_InnerTop, 0, MUIA_InnerBottom, 0,
                    MUIA_Bitmap_Bitmap, (IPTR)&bm, MUIA_Bitmap_Width, W, MUIA_Bitmap_Height, H,
                    MUIA_Bitmap_Precision, 0, MUIA_Bitmap_SourceColors, (IPTR)colors,
                    MUIA_FixWidth, W, MUIA_FixHeight, H, End,
                Child, BitmapObject,
                    MUIA_Bitmap_Bitmap, (IPTR)&bm, MUIA_Bitmap_Width, W, MUIA_Bitmap_Height, H,
                    MUIA_Bitmap_Transparent, 0, MUIA_Bitmap_Precision, 0,
                    MUIA_Bitmap_SourceColors, (IPTR)colors,
                    MUIA_FixWidth, W, MUIA_FixHeight, H, End,
            End,
        End,
    End;
    if (app)
    {
        DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (IPTR)app, 2,
            MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
        set(win, MUIA_Window_Open, TRUE);
        while (DoMethod(app, MUIM_Application_NewInput, (IPTR)&sigs) != MUIV_Application_ReturnID_Quit)
            if (sigs && ((sigs = Wait(sigs | SIGBREAKF_CTRL_C)) & SIGBREAKF_CTRL_C))
                break;
        MUI_DisposeObject(app);
    }
    for (i = 0; i < D; i++)
        if (bm.Planes[i]) FreeMem(bm.Planes[i], bpr * H);
    CloseLibrary(MUIMasterBase);
    return 0;
}
