/* instsizes: the instance data size of every class of the installed
 * muimaster.library (black box: MUI_GetClass(name)->cl_InstSize, and
 * cl_InstOffset + cl_InstSize, the bytes of an object of that class
 * without rootclass's header). Compare the outputs of two libraries.
 * Build: m68k-amigaos-gcc -m68000 -noixemul -Os instsizes.c -o instsizes -lamiga
 */
#include <stdio.h>
#include <intuition/classes.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
struct Library *MUIMasterBase;
static const char *const names[] = {
    "Notify.mui", "Family.mui", "Menustrip.mui", "Menu.mui", "Menuitem.mui",
    "Application.mui", "Window.mui", "Aboutmui.mui", "Area.mui", "Rectangle.mui",
    "Balance.mui", "Image.mui", "Bitmap.mui", "Bodychunk.mui", "Text.mui",
    "Gadget.mui", "String.mui", "Boopsi.mui", "Prop.mui", "Gauge.mui",
    "Scale.mui", "Colorfield.mui", "List.mui", "Floattext.mui", "Volumelist.mui",
    "Scrmodelist.mui", "Dirlist.mui", "Numeric.mui", "Knob.mui", "Levelmeter.mui",
    "Numericbutton.mui", "Slider.mui", "Framedisplay.mui", "Popframe.mui",
    "Imagedisplay.mui", "Popimage.mui", "Pendisplay.mui", "Poppen.mui",
    "Group.mui", "Mccprefs.mui", "Register.mui", "Penadjust.mui",
    "Settingsgroup.mui", "Settings.mui", "Frameadjust.mui", "Imageadjust.mui",
    "Virtgroup.mui", "Scrollgroup.mui", "Scrollbar.mui", "Listview.mui",
    "Radio.mui", "Cycle.mui", "Coloradjust.mui", "Palette.mui", "Popstring.mui",
    "Popobject.mui", "Poplist.mui", "Popscreen.mui", "Popasl.mui",
    "Semaphore.mui", "Applist.mui", "Dataspace.mui", "Configdata.mui", NULL };
int main(void)
{
    int i;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    for (i = 0; names[i]; i++)
    {
        struct IClass *cl = MUI_GetClass((char *)names[i]);
        if (cl)
        {
            printf("%-18s %4u %4u\n", names[i], (unsigned)cl->cl_InstSize,
                (unsigned)(cl->cl_InstOffset + cl->cl_InstSize));
            MUI_FreeClass(cl);
        }
        else
            printf("%-18s    -    -\n", names[i]);
    }
    CloseLibrary(MUIMasterBase);
    return 0;
}
