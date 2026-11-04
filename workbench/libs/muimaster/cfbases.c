/* The library bases of a class file (classfile.h), copied from
 * muimaster.library's. Kept apart from headers that declare the bases with
 * their own types. */
#include "classfile.h"

#define ZCF_DEFINE(n) void *n;
ZUNE_CLASSFILE_BASES(ZCF_DEFINE)

/* libnix's soft-float and math functions name these */
void *__UtilityBase;

const struct ZuneClassFileIface *zune_cf_iface;

void zune_cf_setbases(const struct ZuneClassFileIface *iface)
{
    unsigned i = 0;

#define ZCF_COPY(n) n = *iface->bases[i++];
    ZUNE_CLASSFILE_BASES(ZCF_COPY)
    __UtilityBase = UtilityBase;
}

/* lazylib.h in a class file: open the library through muimaster.library,
 * then take over the bases again. */
struct Library *zune_lazylib_open(unsigned which);

void *zune_cf_lazy(unsigned which)
{
    void *base = zune_lazylib_open(which);

    if (zune_cf_iface)
        zune_cf_setbases(zune_cf_iface);
    return base;
}
