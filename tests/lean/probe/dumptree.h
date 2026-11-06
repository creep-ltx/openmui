/* dumptree.h: print every object of a window with its class, min/def/max
 * and current box, frame additions; works on MUI 3.8 and Zune (public
 * MUI_AreaData fields only). */
#include <intuition/classes.h>
#include <stdio.h>
#include <proto/muimaster.h>

static const char *dt_names[] = { "Text.mui", "String.mui", "Listview.mui", "List.mui", "Floattext.mui",
    "Volumelist.mui", "Dirlist.mui", "Scrollbar.mui", "Prop.mui", "Register.mui", "Cycle.mui",
    "Image.mui", "Rectangle.mui", "Balance.mui", "Virtgroup.mui", "Scrollgroup.mui", "Slider.mui",
    "Numeric.mui", "Gauge.mui", "Scale.mui", "Radio.mui", "Popstring.mui", "Popobject.mui",
    "Popasl.mui", "Poplist.mui", "Bodychunk.mui", "Bitmap.mui", "Colorfield.mui", "Coloradjust.mui",
    "Palette.mui", "Group.mui", "Area.mui", NULL };
static struct IClass *dt_cl[40];
static const char *dt_cls(Object *o, int *priv)
{
    struct IClass *c = OCLASS(o);
    int i;
    *priv = 0;
    if (!dt_cl[0])
        for (i = 0; dt_names[i]; i++) dt_cl[i] = (struct IClass *)MUI_GetClass((char *)dt_names[i]);
    for (; c; c = c->cl_Super, (*priv)++)
    {
        if (c->cl_ID) return (const char *)c->cl_ID;
        for (i = 0; dt_names[i]; i++) if (dt_cl[i] == c) return dt_names[i];
    }
    return "?";
}

static void dumptree(Object *o, int depth)
{
    struct List *l = NULL;
    int priv;
    const char *cn = dt_cls(o, &priv);
    printf("%*s%s%s min %dx%d def %dx%d max %dx%d box %d,%d %dx%d add %d,%d sub %d,%d %s/%d\n",
        depth * 2, "", cn, priv ? "*" : "",
        _minwidth(o), _minheight(o), _defwidth(o), _defheight(o),
        _maxwidth(o) > 9999 ? 9999 : _maxwidth(o), _maxheight(o) > 9999 ? 9999 : _maxheight(o),
        _left(o), _top(o), _width(o), _height(o),
        _addleft(o), _addtop(o), _subwidth(o), _subheight(o),
        _font(o) && _font(o)->tf_Message.mn_Node.ln_Name ? _font(o)->tf_Message.mn_Node.ln_Name : "-", _font(o) ? _font(o)->tf_YSize : 0);
    if (get(o, MUIA_Group_ChildList, &l) && l)
    {
        APTR st = l->lh_Head;
        Object *c;
        while ((c = NextObject(&st)))
            dumptree(c, depth + 1);
    }
}

/* a screen of W x H (mode from argv), or NULL for the Workbench */
static struct TextAttr dt_ta;
static struct Screen *dt_screen(int w, int h, ULONG mode, const char *font, int size)
{
    static UWORD pens[] = { (UWORD)~0 };
    dt_ta.ta_Name = (STRPTR)font; dt_ta.ta_YSize = size;
    return OpenScreenTags(NULL, SA_Width, w, SA_Height, h, SA_Depth, 3,
        SA_DisplayID, mode, SA_Pens, (IPTR)pens, SA_Title, (IPTR)"dt",
        font ? SA_Font : SA_SysFont, font ? (IPTR)&dt_ta : 1, TAG_DONE);
}
