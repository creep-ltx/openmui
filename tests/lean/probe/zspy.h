#define ZSPY_NAME "ZSPY"
#define ZSPY_MAX 512
struct ZSpy {
    struct MsgPort port;
    struct IClass *wincl, *appcl;
    Object *obj[ZSPY_MAX];
    UBYTE kind[ZSPY_MAX];
};
