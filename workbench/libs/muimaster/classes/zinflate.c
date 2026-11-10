/*
    zinflate.c: a small inflate (RFC 1950/1951) for Pixmap.mui's
    MUIV_Pixmap_Compression_Z (MUI 5's "Z" images, zlib streams), written for
    Zune68 from the RFCs. Decoder only, the whole output size known in
    advance; the Adler-32 check is not verified. Included by pixmap.c, so
    only the Pixmap class file (a MUI 4/5 class) carries it.
*/

struct zinf_huff
{
    UWORD count[16];
    UWORD sym[288];
};

struct zinf
{
    const UBYTE *s, *se;
    UBYTE *d, *d0, *de;
    ULONG bits;
    WORD nb;
    WORD err;
    struct zinf_huff lit, dist;
    UBYTE lens[320];
};

static ULONG zinf_bits(struct zinf *z, int n)
{
    ULONG v = z->bits;
    while (z->nb < n)
    {
        if (z->s >= z->se) { z->err = 1; return 0; }
        v |= (ULONG)*z->s++ << z->nb;
        z->nb += 8;
    }
    z->bits = v >> n;
    z->nb -= n;
    return v & ((1UL << n) - 1);
}

static void zinf_build(struct zinf_huff *h, const UBYTE *len, int n)
{
    UWORD offs[16];
    int i;
    for (i = 0; i < 16; i++) h->count[i] = 0;
    for (i = 0; i < n; i++) h->count[len[i]]++;
    h->count[0] = 0;
    offs[1] = 0;
    for (i = 1; i < 15; i++) offs[i + 1] = offs[i] + h->count[i];
    for (i = 0; i < n; i++)
        if (len[i]) h->sym[offs[len[i]]++] = i;
}

static int zinf_decode(struct zinf *z, const struct zinf_huff *h)
{
    int code = 0, first = 0, index = 0, len, count;
    for (len = 1; len < 16; len++)
    {
        code |= zinf_bits(z, 1);
        if (z->err) return -1;
        count = h->count[len];
        if (code - count < first) return h->sym[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    z->err = 1;
    return -1;
}

static const UWORD zinf_lbase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const UBYTE zinf_lext[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const UWORD zinf_dbase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const UBYTE zinf_dext[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

static int zinf_codes(struct zinf *z)
{
    for (;;)
    {
        int sym = zinf_decode(z, &z->lit);
        if (sym < 0) return 0;
        if (sym < 256)
        {
            if (z->d >= z->de) return 0;
            *z->d++ = sym;
        }
        else if (sym == 256)
            return 1;
        else
        {
            ULONG len, dist;
            sym -= 257;
            if (sym >= 29) return 0;
            len = zinf_lbase[sym] + zinf_bits(z, zinf_lext[sym]);
            sym = zinf_decode(z, &z->dist);
            if (sym < 0 || sym >= 30) return 0;
            dist = zinf_dbase[sym] + zinf_bits(z, zinf_dext[sym]);
            if (z->err || dist > (ULONG)(z->d - z->d0) || len > (ULONG)(z->de - z->d)) return 0;
            while (len--) { *z->d = z->d[-(LONG)dist]; z->d++; }
        }
    }
}

static int zinf_dynamic(struct zinf *z)
{
    static const UBYTE order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    int nlen = zinf_bits(z, 5) + 257, ndist = zinf_bits(z, 5) + 1, ncode = zinf_bits(z, 4) + 4;
    int i;
    if (z->err || nlen > 286 || ndist > 30) return 0;
    for (i = 0; i < 19; i++) z->lens[order[i]] = i < ncode ? zinf_bits(z, 3) : 0;
    zinf_build(&z->lit, z->lens, 19);
    for (i = 0; i < nlen + ndist;)
    {
        int sym = zinf_decode(z, &z->lit), rep, len = 0;
        if (sym < 0) return 0;
        if (sym < 16) { z->lens[i++] = sym; continue; }
        if (sym == 16)
        {
            if (i == 0) return 0;
            len = z->lens[i - 1];
            rep = 3 + zinf_bits(z, 2);
        }
        else if (sym == 17) rep = 3 + zinf_bits(z, 3);
        else rep = 11 + zinf_bits(z, 7);
        if (z->err || i + rep > nlen + ndist) return 0;
        while (rep--) z->lens[i++] = len;
    }
    zinf_build(&z->lit, z->lens, nlen);
    zinf_build(&z->dist, z->lens + nlen, ndist);
    return zinf_codes(z);
}

static int zinf_fixed(struct zinf *z)
{
    int i;
    for (i = 0; i < 144; i++) z->lens[i] = 8;
    for (; i < 256; i++) z->lens[i] = 9;
    for (; i < 280; i++) z->lens[i] = 7;
    for (; i < 288; i++) z->lens[i] = 8;
    zinf_build(&z->lit, z->lens, 288);
    for (i = 0; i < 30; i++) z->lens[i] = 5;
    zinf_build(&z->dist, z->lens, 30);
    return zinf_codes(z);
}

static int zinf_stored(struct zinf *z)
{
    ULONG len;
    z->bits = 0; z->nb = 0;
    if (z->se - z->s < 4) return 0;
    len = z->s[0] | (z->s[1] << 8);
    if ((len ^ (z->s[2] | (z->s[3] << 8))) != 0xffff) return 0;
    z->s += 4;
    if (len > (ULONG)(z->se - z->s) || len > (ULONG)(z->de - z->d)) return 0;
    while (len--) *z->d++ = *z->s++;
    return 1;
}

/* inflate a zlib stream (or a raw deflate stream) of srclen bytes into exactly
   dstlen bytes; FALSE on any error or a different size */
static BOOL zune_inflate(const UBYTE *src, ULONG srclen, UBYTE *dst, ULONG dstlen)
{
    struct zinf *z;
    int last, ok = 1;
    BOOL result;
    if (srclen >= 2 && (src[0] & 0x0f) == 8 && ((src[0] << 8) | src[1]) % 31 == 0)
    {
        if (src[1] & 0x20) return FALSE;      /* preset dictionary */
        src += 2; srclen -= 2;
    }
    if (!(z = AllocVec(sizeof(*z), MEMF_ANY))) return FALSE;
    z->s = src; z->se = src + srclen;
    z->d = z->d0 = dst; z->de = dst + dstlen;
    z->bits = 0; z->nb = 0; z->err = 0;
    do
    {
        int type;
        last = zinf_bits(z, 1);
        type = zinf_bits(z, 2);
        if (z->err) { ok = 0; break; }
        if (type == 0) ok = zinf_stored(z);
        else if (type == 1) ok = zinf_fixed(z);
        else if (type == 2) ok = zinf_dynamic(z);
        else ok = 0;
    } while (ok && !last && !z->err);
    result = ok && !z->err && z->d == z->de;
    FreeVec(z);
    return result;
}
