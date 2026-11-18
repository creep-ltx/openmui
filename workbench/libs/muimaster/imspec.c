/*
    Copyright  2002-2026, The AROS Development Team.
    Copyright  1999, David Le Corfec.
    All rights reserved.

*/

/* This is the implementation of a MUI-like image engine
 * (see MUIA_Image_Spec for more information about MUI image specs)
 * Their external form is a string "<type>:<parameters>"
 * with type being a single char. See zune_image_spec_to_structure().
 *
 * Basically an ImageSpec can be anything which can be displayed:
 * gfx datas, drawing code, ...
 * See ImageSpecType for the known types.
 *
 * Rewritten for the 68000 (2026-11, rw-draw): the specs that hold no
 * screen resources (the patterns and the four MUI pens most objects get
 * as their background) are constants, one allocation helper for the
 * others, the configured images ("6:n", MUII_ values) resolved once in
 * zune_imspec_setup.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <exec/types.h>
#include <exec/memory.h>

#include <graphics/gfxmacros.h>

#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/dos.h>
#include <clib/alib_protos.h>

#include "mui.h"

#include "datatypescache.h"
#include "imspec.h"
#include "support.h"

#include "muimaster_intern.h"
#include "prefs.h"
#include "penspec.h"
#include "imspec_intern.h"
#include "modules.h"
#include <intuition/imageclass.h>

extern struct Library *MUIMasterBase;

static const UWORD gridpattern1[] = { 0x5555, 0xaaaa };
static const UWORD gridpattern2[] = { 0x8888, 0x2222 };

/* MUII_SHADOWBACK to MUII_MARKBACKGROUND: background pen, foreground pen,
 * the second pattern */
static const UBYTE patternPens[][3] = {
    {MPEN_SHADOW, MPEN_BACKGROUND, 0},      /* MUII_SHADOWBACK     */
    {MPEN_SHADOW, MPEN_FILL, 0},            /* MUII_SHADOWFILL     */
    {MPEN_SHADOW, MPEN_SHINE, 0},           /* MUII_SHADOWSHINE    */
    {MPEN_FILL, MPEN_BACKGROUND, 0},        /* MUII_FILLBACK       */
    {MPEN_FILL, MPEN_SHINE, 0},             /* MUII_FILLSHINE      */
    {MPEN_SHINE, MPEN_BACKGROUND, 0},       /* MUII_SHINEBACK      */
    {MPEN_FILL, MPEN_BACKGROUND, 1},        /* MUII_FILLBACK2      */
    {MPEN_HALFSHINE, MPEN_BACKGROUND, 0},   /* MUII_HSHINEBACK     */
    {MPEN_HALFSHADOW, MPEN_BACKGROUND, 0},  /* MUII_HSHADOWBACK    */
    {MPEN_HALFSHINE, MPEN_SHINE, 0},        /* MUII_HSHINESHINE    */
    {MPEN_HALFSHADOW, MPEN_SHADOW, 0},      /* MUII_HSHADOWSHADOW  */
    {MPEN_MARK, MPEN_SHINE, 0},             /* MUII_MARKSHINE      */
    {MPEN_MARK, MPEN_HALFSHINE, 0},         /* MUII_MARKHALFSHINE  */
    {MPEN_MARK, MPEN_BACKGROUND, 0},        /* MUII_MARKBACKGROUND */
};

/* Pattern specs and the four plain MUI pens (MUII_BACKGROUND, MUII_SHADOW,
 * MUII_SHINE, MUII_FILL: "0:128" and friends, the background most objects
 * get from the settings) hold no screen resources: their pens are looked
 * up in the render info they are drawn with (a PST_MUI pen,
 * zune_penspec_pen). So every object shares one constant per pattern and
 * pen; they are never set up, cleaned up or freed, and hold only the
 * fields their type reads (lean kept 18 whole specs, 900 bytes). */
#define NPATTERN_SPECS (MUII_MARKBACKGROUND - MUII_SHADOWBACK + 1)
struct shared_pattern { ImageSpecType type; LONG pattern; };
struct shared_color { ImageSpecType type; struct MUI_PenSpec_intern penspec; };
#define SP(n) {IST_PATTERN, n}
#define SC(pen) {IST_COLOR, {PST_MUI, NULL, 0, FALSE, {pen}}}
static const struct
{
    struct shared_color color[4];
    struct shared_pattern pattern[NPATTERN_SPECS];
} shared = {
    {SC(MPEN_BACKGROUND), SC(MPEN_SHADOW), SC(MPEN_SHINE), SC(MPEN_FILL)},
    {SP(0), SP(1), SP(2), SP(3), SP(4), SP(5), SP(6), SP(7), SP(8), SP(9),
     SP(10), SP(11), SP(12), SP(13)},
};
#define SHARED(p) ((struct MUI_ImageSpec_intern *)(p))

static BOOL is_shared_spec(const struct MUI_ImageSpec_intern *spec)
{
    return (const void *)spec >= (const void *)&shared
        && (const void *)spec < (const void *)(&shared + 1);
}

BOOL zune_imspec_is_solid(struct MUI_ImageSpec_intern *spec)
{
    return spec && spec->type == IST_COLOR;
}

/* StrToLong for the short numbers in image specs, without the
 * dos.library call: leading blanks, an optional sign, decimal digits.
 * def when there are no digits, as StrToLong leaves the value. */
static LONG spec_parse_long(CONST_STRPTR s, LONG def)
{
    LONG v = 0;
    BOOL neg = FALSE, digits = FALSE;

    while (*s == ' ' || *s == '\t')
        s++;
    if (*s == '-' || *s == '+')
        neg = *s++ == '-';
    while (*s >= '0' && *s <= '9')
    {
        v = v * 10 + (*s++ - '0');
        digits = TRUE;
    }
    return digits ? (neg ? -v : v) : def;
}

/* MUII_BACKGROUND to MUII_MARKBACKGROUND: the shared constants */
static struct MUI_ImageSpec_intern *get_pattern_imspec(LONG in)
{
    if (in >= MUII_BACKGROUND && in <= MUII_FILL)
        return SHARED(&shared.color[in - MUII_BACKGROUND]);
    if (in >= MUII_SHADOWBACK && in <= MUII_MARKBACKGROUND)
        return SHARED(&shared.pattern[in - MUII_SHADOWBACK]);
    return NULL;
}

/* A cleared spec of type, with a copy of name in its first string field
 * when name is given; NULL when either allocation fails */
static struct MUI_ImageSpec_intern *spec_new(ImageSpecType type,
    CONST_STRPTR name)
{
    struct MUI_ImageSpec_intern *spec;

    if ((spec = mui_alloc_struct(struct MUI_ImageSpec_intern)))
    {
        spec->type = type;
        /* the filename is the first field of each type that has one */
        if (name && !(spec->u.bitmap.filename = StrDup(name)))
        {
            mui_free(spec);
            return NULL;
        }
    }
    return spec;
}

static struct MUI_ImageSpec_intern *get_pen_imspec(CONST_STRPTR str)
{
    struct MUI_ImageSpec_intern *spec = spec_new(IST_COLOR, NULL);

    if (spec && !zune_pen_string_to_intern(str, &spec->u.penspec))
    {
        mui_free(spec);
        return NULL;
    }
    return spec;
}

static struct MUI_ImageSpec_intern *get_gradient_imspec(CONST_STRPTR str,
    ImageSpecType type)
{
    /* the type is set after the parse, as lean did */
    struct MUI_ImageSpec_intern *spec = spec_new(IST_PATTERN, NULL);

    if (spec && !zune_gradient_string_to_intern(str, spec))
    {
        mui_free(spec);
        return NULL;
    }
    if (spec)
        spec->type = type;
    return spec;
}

/* "4:name": a brush; a name ending in 0 has its selected state in the
 * same name ending in 1 */
static struct MUI_ImageSpec_intern *get_brush_imspec(CONST_STRPTR filename)
{
    struct MUI_ImageSpec_intern *spec = spec_new(IST_BRUSH, filename);
    size_t last_idx;
    char *tmpstr;

    if (spec && (last_idx = strlen(filename))
        && filename[--last_idx] == '0')
    {
        if (!(tmpstr = StrDup(filename)))
        {
            FreeVec((APTR)spec->u.brush.filename[0]);
            mui_free(spec);
            return NULL;
        }
        tmpstr[last_idx] = '1';
        spec->u.brush.filename[1] = tmpstr;
    }
    return spec;
}

/* "3:name": a BOOPSI image class when the name has ".image" in it, else
 * a brush */
static struct MUI_ImageSpec_intern *get_boopsi_imspec(CONST_STRPTR filename)
{
    if (!strstr(filename, ".image"))
        return get_brush_imspec(filename);
    return spec_new(IST_BOOPSI, filename);
}

/**************************************************************************
 Create a image spec from a string or a magic value (MUII_BACKGROUND to
 MUII_MARKBACKGROUND). Called by zune_imspec_setup(), which resolves the
 configured images ("6:n" and the values below MUII_Count) first.
**************************************************************************/
static struct MUI_ImageSpec_intern *zune_image_spec_to_structure(IPTR in)
{
    CONST_STRPTR s;

    if (in >= MUII_BACKGROUND && in <= MUII_MARKBACKGROUND)
        return get_pattern_imspec(in);
    if (!(s = (CONST_STRPTR)in) || !s[0] || !s[1])
        return NULL;
    switch (*s)
    {
    case '0':              /* builtin pattern */
        return get_pattern_imspec(spec_parse_long(s + 2, -1));
    case '1':              /* builtin standard image, obsoleted by 6: */
        return zune_imspec_create_vector(spec_parse_long(s + 2, -1));
    case '2':              /* a penspec */
        return get_pen_imspec(s + 2);
    case 'r':              /* a penspec, WookieChat is doing this on MorphOS */
        return get_pen_imspec(s + 1);
    case '3':              /* BOOPSI image class name */
        return get_boopsi_imspec(s + 2);
    case '4':              /* external MUI brush name */
        return get_brush_imspec(s + 2);
    case '5':              /* external bitmap loaded with datatypes */
        return spec_new(IST_BITMAP, s + 2);
    case '7':              /* scaled gradient */
        return get_gradient_imspec(s + 2, IST_SCALED_GRADIENT);
    case '8':              /* tiled gradient */
        return get_gradient_imspec(s + 2, IST_TILED_GRADIENT);
    }
    /* '6' with a number out of range, and anything else */
    return NULL;
}

static __attribute__((noinline)) void zune_imspec_free(
    struct MUI_ImageSpec_intern *spec)
{
    if (!spec || is_shared_spec(spec))
        return;
    switch (spec->type)
    {
    case IST_BRUSH:
        if (spec->u.brush.filename[1])
            FreeVec((APTR)spec->u.brush.filename[1]);
        /* fall through: the first name is where the others have theirs */
    case IST_BOOPSI:
    case IST_BITMAP:
        if (spec->u.bitmap.filename)
            FreeVec((APTR)spec->u.bitmap.filename);
        break;
    default:
        break;
    }
    mui_free(spec);
}

/* A brush picture: by its name, or for a name without a device in the
 * images drawers */
static struct dt_node *load_brush(CONST_STRPTR name, struct Screen *scr)
{
    static CONST_STRPTR const prefixes[] = {
        IMSPEC_EXTERNAL_PREFIX,
#ifdef IMSPEC_EXTERNAL_PREFIX2
        IMSPEC_EXTERNAL_PREFIX2,
#endif
    };
    struct dt_node *dt = dt_load_picture(name, scr);
    int p, size;
    STRPTR fullpath;

    if (dt || strchr(name, ':'))
        return dt;
    for (p = 0; !dt && p < sizeof(prefixes) / sizeof(prefixes[0]); p++)
    {
        size = strlen(prefixes[p]) + strlen(name) + 1;
        if ((fullpath = (STRPTR)AllocVec(size, MEMF_ANY)))
        {
            strcpy(fullpath, prefixes[p]);
            strcat(fullpath, name);
            fullpath[size - 1] = 0;
            dt = dt_load_picture(fullpath, scr);
            FreeVec(fullpath);
        }
    }
    return dt;
}

struct MUI_ImageSpec_intern *zune_imspec_setup(IPTR s,
    struct MUI_RenderInfo *mri)
{
    struct MUI_ImageSpec_intern *spec;
    LONG img = -1;

    if (!mri)
        return NULL;
    /* A configured image ("6:n" or the MUII_ value n) names a spec in the
     * preferences: that spec is set up instead. */
    if (s < MUII_Count)
        img = s;
    else if (s >= MUII_BACKGROUND && s < MUII_LASTPAT)
        img = -1;
    else if (((CONST_STRPTR)s)[0] == '6' && ((CONST_STRPTR)s)[1])
        img = spec_parse_long((CONST_STRPTR)s + 2, -1);
    if (img >= (LONG)MUII_WindowBack && img < (LONG)MUII_Count)
    {
        struct ZunePrefsNew *prefs =
            muiGlobalInfo(mri->mri_WindowObject)->mgi_Prefs;
        CONST_STRPTR desc = prefs->imagespecs[img];

        /* potential for deadloop if Zune prefs images contain a 6: */
        if (!desc || desc[0] == '6')
            return NULL;
        s = (IPTR)desc;
    }
    if (!(spec = zune_image_spec_to_structure(s)))
        return NULL;
    switch (spec->type)
    {
    case IST_COLOR:
        if (!is_shared_spec(spec))
            zune_penspec_setup(&spec->u.penspec, mri);
        break;
    case IST_BOOPSI:
        zune_boopsi_image_open(spec->u.boopsi.filename, mri,
            &spec->u.boopsi.base, &spec->u.boopsi.obj, spec->u.boopsi.size);
        break;
    case IST_BRUSH:
        spec->u.brush.dt[0] = load_brush(spec->u.brush.filename[0],
            mri->mri_Screen);
        spec->u.brush.dt[1] = spec->u.brush.filename[1]
            ? load_brush(spec->u.brush.filename[1], mri->mri_Screen)
            : spec->u.brush.dt[0];
        break;
    case IST_BITMAP:
        if (spec->u.bitmap.filename)
            spec->u.bitmap.dt = dt_load_picture(spec->u.bitmap.filename,
                mri->mri_Screen);
        break;
    case IST_SCALED_GRADIENT:
    case IST_TILED_GRADIENT:
        zune_gradientspec_setup(spec, mri);
        break;
    default:
        break;
    }
    return spec;
}

void zune_imspec_cleanup(struct MUI_ImageSpec_intern *spec)
{
    if (!spec)
        return;
    switch (spec->type)
    {
    case IST_COLOR:
        if (!is_shared_spec(spec))
            zune_penspec_cleanup(&spec->u.penspec);
        break;
    case IST_BOOPSI:
        zune_boopsi_image_close(spec->u.boopsi.base, spec->u.boopsi.obj);
        spec->u.boopsi.base = NULL;
        spec->u.boopsi.obj = NULL;
        break;
    case IST_BRUSH:
        {
            int i;

            for (i = 0; i < 2; i++)
            {
                if (spec->u.brush.filename[i])
                    dt_dispose_picture(spec->u.brush.dt[i]);
                spec->u.brush.dt[i] = NULL;
            }
        }
        break;
    case IST_BITMAP:
        if (spec->u.bitmap.dt)
        {
            dt_dispose_picture(spec->u.bitmap.dt);
            spec->u.bitmap.dt = NULL;
        }
        break;
    case IST_SCALED_GRADIENT:
    case IST_TILED_GRADIENT:
        zune_gradientspec_cleanup(spec);
        break;
    default:
        break;
    }
    zune_imspec_free(spec);
}

BOOL zune_imspec_is_position_free(const struct MUI_ImageSpec_intern *spec)
{
    return spec && (spec->type == IST_COLOR || spec->type == IST_PATTERN);
}

BOOL zune_imspec_fill_pen(const struct MUI_ImageSpec_intern *spec,
    struct MUI_RenderInfo *mri, LONG *pen)
{
    if (!spec || spec->type != IST_COLOR)
        return FALSE;
    *pen = zune_penspec_pen(&spec->u.penspec, mri);
    return TRUE;
}

BOOL zune_imspec_is_mui_pen(const struct MUI_ImageSpec_intern *spec,
    LONG muipen)
{
    return spec && spec->type == IST_COLOR
        && spec->u.penspec.p_type == PST_MUI
        && spec->u.penspec.p_mui == muipen;
}

/* min, default and max sizes of an image that may take any size */
static BOOL minmax_free(struct MUI_MinMax *minmax, BOOL result)
{
    minmax->MinWidth = minmax->MinHeight = 3;
    minmax->DefWidth = minmax->DefHeight = 8;
    minmax->MaxWidth = minmax->MaxHeight = MUI_MAXMAX;
    return result;
}

/* a fixed size */
static void minmax_fixed(struct MUI_MinMax *minmax, WORD w, WORD h)
{
    minmax->MinWidth = minmax->DefWidth = minmax->MaxWidth = w;
    minmax->MinHeight = minmax->DefHeight = minmax->MaxHeight = h;
}

/* ".mim" brushes hold both states side by side */
static BOOL is_mim(CONST_STRPTR name)
{
    long len = strlen(name);

    return len > 4 && !strcmp(&name[len - 4], ".mim");
}

BOOL zune_imspec_askminmax(struct MUI_ImageSpec_intern *spec,
    struct MUI_MinMax *minmax)
{
    if (!spec || !minmax)
        return FALSE;
    switch (spec->type)
    {
    case IST_PATTERN:
    case IST_SCALED_GRADIENT:
    case IST_TILED_GRADIENT:
    case IST_COLOR:
        return minmax_free(minmax, TRUE);
    case IST_VECTOR:
        return zune_imspec_vector_get_minmax(spec, minmax);
    case IST_BOOPSI:
        /* the class's sizes (imspec_boopsi.c): a fixed image at its
         * minimum, as MUI 5 shows them; larger when the object may grow
         * (drawn with IA_Width/IA_Height set) */
        if (!spec->u.boopsi.obj)
            return minmax_free(minmax, FALSE);
        minmax->MinWidth = minmax->DefWidth = spec->u.boopsi.size[0];
        minmax->MinHeight = minmax->DefHeight = spec->u.boopsi.size[1];
        minmax->MaxWidth = spec->u.boopsi.size[2];
        minmax->MaxHeight = spec->u.boopsi.size[3];
        break;
    case IST_BRUSH:
        if (!spec->u.brush.dt[0])
            return minmax_free(minmax, FALSE);
        minmax_fixed(minmax, dt_width(spec->u.brush.dt[0])
            >> is_mim(spec->u.brush.filename[0]),
            dt_height(spec->u.brush.dt[0]));
        break;
    case IST_BITMAP:
        minmax_free(minmax, TRUE);
        minmax->DefWidth = dt_width(spec->u.bitmap.dt);
        minmax->DefHeight = dt_height(spec->u.bitmap.dt);
        if (!spec->u.bitmap.dt)
            return FALSE;
        break;
    default:
        break;
    }
    return TRUE;
}

void zune_imspec_show(struct MUI_ImageSpec_intern *spec, Object *obj)
{
    /* the gradients are made for the object they are shown in */
    if (spec && obj && (spec->type == IST_SCALED_GRADIENT
        || spec->type == IST_TILED_GRADIENT))
        spec->u.gradient.obj = obj;
}

void zune_imspec_hide(struct MUI_ImageSpec_intern *spec)
{
}

/* Patterns start at (ox, oy) when origin is set: MUI 3.9 aligns an
 * object's pattern background to the object's top left corner
 * (tests/native/patternphase.c). Without it they stay aligned to the
 * RastPort as before. */
static void imspec_draw(struct MUI_ImageSpec_intern *spec,
    struct RastPort *rp, struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG xoffset, LONG yoffset, LONG state,
    LONG dx, LONG dy, WORD mode, LONG abs_l, LONG abs_t, LONG abs_r,
    LONG abs_b, BOOL origin, LONG ox, LONG oy)
{
    LONG right = left + width - 1;
    LONG bottom = top + height - 1;

    /* nothing to draw in an empty box (OS 3.1's RectFill with its corners
     * crossed writes outside the rastport: framepix's weight-0 objects
     * beside a Space hung the machine, Compatibility round 5) */
    if (!spec || width <= 0 || height <= 0)
        return;
    /* a picture that did not load is the background pen */
    if ((spec->type == IST_BITMAP && !spec->u.bitmap.dt)
        || (spec->type == IST_BRUSH && !spec->u.brush.dt[0])
        || (spec->type == IST_BOOPSI && !spec->u.boopsi.obj))
        spec = SHARED(&shared.color[0]);

    switch (spec->type)
    {
    case IST_PATTERN:
        {
            const UBYTE *pp = patternPens[spec->u.pattern];
            const UWORD *pattern = pp[2] ? gridpattern2 : gridpattern1;
            UWORD rows[2];
            UBYTE oldDrMd = rp->DrawMode, oldBPen = rp->BgPen;

            if (origin)
            {
                /* Rotate so pattern column 0, row 0 lands on the origin */
                ULONG sx = (ULONG)(ox - dx) & 15;
                int i;

                for (i = 0; i < 2; i++)
                {
                    UWORD w = pattern[(i - (oy - dy)) & 1];

                    rows[i] = sx ? (UWORD)((w >> sx) | (w << (16 - sx))) : w;
                }
                pattern = rows;
            }
            /* Restore the draw mode and background pen afterwards */
            SetDrMd(rp, JAM2);
            SetAPen(rp, mri->mri_Pens[pp[1]]);
            SetBPen(rp, mri->mri_Pens[pp[0]]);
            SetAfPt(rp, (UWORD *)pattern, 1);
            RectFill(rp, left - dx, top - dy, right - dx, bottom - dy);
            SetAfPt(rp, NULL, 0);
            SetBPen(rp, oldBPen);
            SetDrMd(rp, oldDrMd);
        }
        break;
    case IST_VECTOR:
        if (spec->u.vect.draw)
            spec->u.vect.draw(mri, left - dx, top - dy, width, height,
                state);
        break;
    case IST_COLOR:
        zune_penspec_drawdirect(&spec->u.penspec, rp, mri, left - dx,
            top - dy, right - dx, bottom - dy);
        break;
    case IST_BOOPSI:
        {
            struct Image *im = (struct Image *)spec->u.boopsi.obj;

            /* the class draws at the size it has */
            if (im->Width != width || im->Height != height)
                SetAttrs(spec->u.boopsi.obj, IA_Width, width,
                    IA_Height, height, TAG_DONE);
            DrawImageState(rp, im, left - dx, top - dy,
                state == IDS_SELECTED ? IDS_SELECTED : IDS_NORMAL,
                mri->mri_DrawInfo);
        }
        break;
    case IST_BRUSH:
        if (state < 0 || state > 1)
            state = 0;
        if (spec->u.brush.dt[state])
        {
            if (is_mim(spec->u.brush.filename[0]))
                dt_put_mim_on_rastport(spec->u.brush.dt[0],
                    mri->mri_RastPort, left - dx, top - dy, state);
            else
                dt_put_on_rastport(spec->u.brush.dt[state],
                    mri->mri_RastPort, left - dx, top - dy);
        }
        break;
    case IST_BITMAP:
        dt_put_on_rastport_tiled(spec->u.bitmap.dt, rp, left - dx, top - dy,
            right - dx, bottom - dy, xoffset - left, yoffset - top);
        break;
    case IST_SCALED_GRADIENT:
        /* drawn whole: no offset (drawfast takes them all the same) */
        if (mode == 0)
            xoffset = yoffset = 0;
        /* fall through */
    case IST_TILED_GRADIENT:
        if (mode == 0)
            zune_gradient_draw(spec, mri, left, top, right, bottom, xoffset,
                yoffset);
        else
            zune_gradient_drawfast(spec, rp, mri, 1, abs_l, abs_t, abs_r,
                abs_b, left, top, right, bottom, xoffset, yoffset);
        break;
    default:
        break;
    }
}

void zune_imspec_drawbuffered(struct MUI_ImageSpec_intern *spec,
    struct RastPort *rp, struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG xoffset, LONG yoffset, LONG state,
    LONG dx, LONG dy, WORD mode, LONG abs_l, LONG abs_t, LONG abs_r,
    LONG abs_b)
{
    imspec_draw(spec, rp, mri, left, top, width, height, xoffset, yoffset,
        state, dx, dy, mode, abs_l, abs_t, abs_r, abs_b, FALSE, 0, 0);
}

void zune_imspec_draw(struct MUI_ImageSpec_intern *spec,
    struct MUI_RenderInfo *mri, LONG left, LONG top, LONG width,
    LONG height, LONG xoffset, LONG yoffset, LONG state)
{
    /* A plain pen, the usual window background, needs none of the
     * argument shuffling of the general path below */
    if (spec && spec->type == IST_COLOR)
    {
        zune_penspec_drawdirect(&spec->u.penspec, mri->mri_RastPort, mri,
            left, top, left + width - 1, top + height - 1);
        return;
    }
    zune_imspec_drawbuffered(spec, mri->mri_RastPort, mri, left, top, width,
        height, xoffset, yoffset, state, 0, 0, 0, left, top, left + width,
        top + height);
}

void zune_imspec_draw_at(struct MUI_ImageSpec_intern *spec,
    struct MUI_RenderInfo *mri, LONG left, LONG top, LONG width,
    LONG height, LONG xoffset, LONG yoffset, LONG state, LONG ox, LONG oy)
{
    imspec_draw(spec, mri->mri_RastPort, mri, left, top, width, height,
        xoffset, yoffset, state, 0, 0, 0, left, top, left + width,
        top + height, TRUE, ox, oy);
}

/**************************************************************************
 Duplicates an image spec. In 'in' may be one of the MUII_#? identifiers
 (but it will always return a string).
 The returned string must be freed with zune_image_spec_free() because
 in the future it might be that the MUII_#? stuff is not converted to
 a string
**************************************************************************/
/* The built-in images and patterns as strings: nearly every object gets
 * one as its background, so they are shared instead of copied (205
 * copies in MUI-Demo). Read-only; zune_image_spec_free() leaves them. */
static const char builtin_spec_window[][5] = {
    "6:0", "6:1", "6:2", "6:3", "6:4", "6:5", "6:6", "6:7", "6:8", "6:9",
    "6:10", "6:11", "6:12", "6:13", "6:14", "6:15", "6:16", "6:17", "6:18",
    "6:19", "6:20", "6:21", "6:22", "6:23", "6:24", "6:25", "6:26", "6:27",
    "6:28", "6:29", "6:30", "6:31", "6:32", "6:33", "6:34", "6:35", "6:36",
    "6:37", "6:38", "6:39", "6:40", "6:41", "6:42"
};
static const char builtin_spec_pattern[][6] = {
    "0:128", "0:129", "0:130", "0:131", "0:132", "0:133", "0:134", "0:135",
    "0:136", "0:137", "0:138", "0:139", "0:140", "0:141", "0:142", "0:143",
    "0:144", "0:145"
};

STRPTR zune_image_spec_duplicate(IPTR in)
{
    char *spec;
    char spec_buf[20];

    if (in < sizeof(builtin_spec_window) / sizeof(builtin_spec_window[0]))
        return (STRPTR)builtin_spec_window[in];
    if (in >= MUII_BACKGROUND && in - MUII_BACKGROUND
        < sizeof(builtin_spec_pattern) / sizeof(builtin_spec_pattern[0]))
        return (STRPTR)builtin_spec_pattern[in - MUII_BACKGROUND];

    if (in >= MUII_WindowBack && in < MUII_BACKGROUND)
    {
        sprintf(spec_buf, "6:%lu", in);
        spec = spec_buf;
    }
    else
    {
        if (in >= MUII_BACKGROUND && in < MUII_LASTPAT)
        {
            sprintf(spec_buf, "0:%lu", in);
            spec = spec_buf;
        }
        else
            spec = (char *)in;
    }

    return StrDup(spec);
}

/**************************************************************************
 Use this function to free the zune_image_spec_duplicate() result
**************************************************************************/
void zune_image_spec_free(CONST_STRPTR spec)
{
    if (spec
        && !(spec >= (CONST_STRPTR)builtin_spec_window
            && spec < (CONST_STRPTR)builtin_spec_window
                + sizeof(builtin_spec_window))
        && !(spec >= (CONST_STRPTR)builtin_spec_pattern
            && spec < (CONST_STRPTR)builtin_spec_pattern
                + sizeof(builtin_spec_pattern)))
        FreeVec((APTR) spec);
}
