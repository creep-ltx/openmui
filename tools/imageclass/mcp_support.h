/* Shared by Busy.mcp and Listtree.mcp (MUI 3.8's settings pages of the two
 * outside classes): number sliders with their own texts, and the exchange
 * with the settings' dataspace. Settings that equal their default are not
 * stored, as everywhere in Zune prefs; MUI 3.8's own files store them all
 * and are read the same way (numbers are 4 bytes, specs are strings). */
#ifndef ZUNE68_MCP_SUPPORT_H
#define ZUNE68_MCP_SUPPORT_H

struct NumInfo
{
    LONG special;           /* a value shown as text */
    const char *text;
    const char *fmt;        /* the others, printf style */
};

struct NumSliderData
{
    char buf[32];
};

static IPTR xget(Object *o, ULONG a) { IPTR v = 0; GetAttr(a, o, &v); return v; }

static struct MUI_CustomClass *NumSliderClass;

DISPATCHER(NumSliderDisp)
{
    if (msg->MethodID == MUIM_Numeric_Stringify)
    {
        const struct NumInfo *ni = (const struct NumInfo *)xget(obj, MUIA_UserData);

        if (ni)
        {
            struct NumSliderData *d = INST_DATA(cl, obj);
            LONG v = ((struct MUIP_Numeric_Stringify *)msg)->value;

            if (v == ni->special)
                return (IPTR)ni->text;
            snprintf(d->buf, sizeof(d->buf), ni->fmt, (long)v);
            return (IPTR)d->buf;
        }
    }
    return DoSuperMethodA(cl, obj, msg);
}

static BOOL MakeNumSliderClass(void)
{
    if (!NumSliderClass)
        NumSliderClass = MUI_CreateCustomClass(NULL, MUIC_Slider, NULL,
            sizeof(struct NumSliderData), ENTRY(NumSliderDisp));
    return NumSliderClass != NULL;
}

static void FreeNumSliderClass(void)
{
    if (NumSliderClass)
        MUI_DeleteCustomClass(NumSliderClass);
    NumSliderClass = NULL;
}

static Object *NumSlider(const struct NumInfo *ni, LONG min, LONG max)
{
    return NewObject(NumSliderClass->mcc_Class, NULL, MUIA_Numeric_Min, min,
        MUIA_Numeric_Max, max, MUIA_UserData, (IPTR)ni, MUIA_CycleChain, TRUE, TAG_DONE);
}

static Object *PrefLabel(const char *text)
{
    return Label2((STRPTR)text);
}

/* the settings of the page, one row each */
struct Item
{
    ULONG id;           /* the item id in the settings */
    ULONG tag;          /* the gadget's attribute */
    LONG def;           /* the default (a number, or a string) */
    BOOL string;
};

static void ItemToGadget(Object *configdata, Object *gadget, const struct Item *it)
{
    IPTR p = DoMethod(configdata, MUIM_Dataspace_Find, it->id);

    if (it->string)
        nnset(gadget, it->tag, p ? p : (IPTR)it->def);
    else
        nnset(gadget, it->tag, p ? *(LONG *)p : it->def);
}

static void ItemFromGadget(Object *configdata, Object *gadget, const struct Item *it)
{
    IPTR v = xget(gadget, it->tag);

    if (it->string)
    {
        if (v && stricmp((char *)v, (char *)it->def))
            DoMethod(configdata, MUIM_Dataspace_Add, v, strlen((char *)v) + 1, it->id);
        else
            DoMethod(configdata, MUIM_Dataspace_Remove, it->id);
    }
    else if ((LONG)v != it->def)
    {
        LONG n = v;

        DoMethod(configdata, MUIM_Dataspace_Add, (IPTR)&n, sizeof(n), it->id);
    }
    else
        DoMethod(configdata, MUIM_Dataspace_Remove, it->id);
}

#endif
