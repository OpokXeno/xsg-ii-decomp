#ifndef INCLUDE_MAIN_XGL_HDD_H
#define INCLUDE_MAIN_XGL_HDD_H

/* Functions of src/main/xgl_hdd.c that other files call. */

/* xglHddActivate: with this bit the new state is set without xglCdArcCheck. */
#define HDD_ACTIVATE_NO_ARC_CHECK 0x100

/* The header of the card data image: the begin and end of the transferred
 * payload (carddata.new). */
struct HddTransferInfo {
    int unmodeled_00;
    int begin;
    int end;
};

struct HddSaveImage;

/* The card save request: xglHddMcCheck/xglHddMcCreate read it through
 * `transfer`, which file.c fills with its card data buffer. */
struct HddCheckState {
    int status;
    union {
        unsigned char *bytes; /* file.c: the card data buffer it hands in */
        unsigned char **files; /* yajima_test: its table of card file buffers */
        struct HddTransferInfo *info; /* xgl_hdd: the begin/end transfer header */
        struct HddSaveImage *image; /* xgl_hdd: the whole saved image, header first */
    } transfer;
    int dataSize;
};

struct HddExistRequest {
    int card;
    unsigned char *found;
};

struct HddLoadRequest {
    int card;
    void *data;
    int size;
};

struct HddSaveRequest {
    int card;
    const void *data;
    int size;
};

/* The progress callback of xglHddInstall: a stage (0 or more) or a failure (negative
 * HDD_INSTALL_FAIL_*), an offset and the param xglHddInstall was given. A nonzero return is the
 * caller's request to stop. */
typedef int HddInstallCallback(int stage, int offset, void *param);

int xglHddActivate(int state);
int xglHddCheck2(void);
void xglHddErrorScreen(void);
int xglHddInstall(HddInstallCallback *callback, void *param);
int xglHddUninstall(void);
int xglHddMount(void);
int xglHddCheck(void);
int xglHddMcExist(struct HddExistRequest *request);
int xglHddMcLoad(void *save);
int xglHddMcSave(const struct HddSaveRequest *save);
int xglHddMcCreate(struct HddCheckState *state);

#endif /* INCLUDE_MAIN_XGL_HDD_H */
