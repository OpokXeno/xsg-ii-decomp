#ifndef INCLUDE_MAIN_XGL_1_016_H
#define INCLUDE_MAIN_XGL_1_016_H

#include "main/xgl_cd.h"
#include "main/xgl_dma.h"

/* The IPU movie player of src/main/xgl_1_016.c: its work record and the functions other files call. */

typedef struct XglMovieSourceHeader XglMovieSourceHeader;

typedef struct XglMovieWork {
    /* The CD stream a streamed movie is read through; its ring is the source buffer. checkdma
     * reads stream.exhausted, and stream.remaining is nonzero while the file has bytes left. */
    CdStreamParam stream;           /* +0x00 */
    short width;                    /* +0x40 */
    short height;                   /* +0x42 */
    short frameCount;               /* +0x44 */
    short currentFrame;             /* +0x46 */
    int stripQwords;                /* +0x48 */
    unsigned int imageQwords;       /* +0x4c */
    XglDmaRefTag chain[4];          /* +0x50 */
    XglMovieSourceHeader *source;   /* +0x90: the source buffer (the ring when streaming) */
    int sourceSize;                 /* +0x94 */
    unsigned char decodeState;      /* +0x98 */
    unsigned char streamOpen;       /* +0x99 */
    unsigned char loop;             /* +0x9a */
    unsigned char unmodeled_9b[4];
    unsigned char bitPosition;      /* +0x9f */
    unsigned int dmaControl;        /* +0xa0 */
    unsigned int dmaAddress;        /* +0xa4 */
    unsigned int dmaQwords;         /* +0xa8 */
    XglDmaRefTag *dmaTag;           /* +0xac */
    unsigned char *imageAddress[2]; /* +0xb0: the two decoded frame buffers */
    unsigned int packetAddress[2];  /* +0xb8 */
    unsigned long long bitbltbuf;   /* +0xc0 */
    unsigned char bufferIndex;      /* +0xc8 */
    unsigned char transMode;        /* +0xc9 */
    unsigned char threshold1;       /* +0xca */
    unsigned char threshold0;       /* +0xcb */
    /* Memory-mode decode buffer; 0 uses the scratchpad. */
    unsigned char *workBuffer;      /* +0xcc */
} XglMovieWork;

struct XglMovieSourceHeader {
    unsigned int signature;             /* "ipum" */
    unsigned char unmodeled_04[4];
    unsigned short width;               /* +0x08 */
    unsigned short height;              /* +0x0a */
    unsigned short frameCount;          /* +0x0c */
    unsigned char unmodeled_0e[2];
};

void xglMovieInfoInit(XglMovieWork *info);
int xglMovieOpen(XglMovieWork *info, const char *name);
int xglMoviePlay(XglMovieWork *info);
int xglMovieRewind(XglMovieWork *info);
int xglMovieClose(XglMovieWork *info);

#endif /* INCLUDE_MAIN_XGL_1_016_H */
