#include "common.h"
#include "shared.h"
#include "main/sce_ipu.h"
/* DMA tag ID (bits 28..30 of the first tag word). */
#define DMA_TAG_ID_CNT 0x10000000U
#define DMA_TAG_ID_REF 0x30000000U
/* Clears the tag ID and IRQ bits, turning a REF tag into REFE. */
#define DMA_TAG_ID_CLEAR_MASK 0x0fffffff
/* Dn_CHCR of the D4 (IPU_to) channel: chain mode, from memory, started. */
#define DMA_CHCR_DIR 0x1
#define DMA_CHCR_MOD_CHAIN 0x4
#define DMA_CHCR_STR 0x100
#define MOVIE_D4_CHCR_CHAIN_START (DMA_CHCR_STR | DMA_CHCR_MOD_CHAIN | DMA_CHCR_DIR)
/* GIF tag fields. */
#define GIF_TAG_EOP 0x8000
#define GIF_TAG_NREG_1 0x1000000000000000
#define GIF_TAG_FLG_IMAGE 0x0800000000000000
#define GIF_REG_AD 14
#define SCE_GS_BITBLTBUF 0x50
#define SCE_GS_TRXPOS 0x51
#define SCE_GS_TRXREG 0x52
#define SCE_GS_TRXDIR 0x53
/* transMode values: which GS buffer a decoded strip is uploaded to, or memory only. */
#define MOVIE_TRANS_BUFFER_0 0
#define MOVIE_TRANS_BUFFER_1 1
#define MOVIE_TRANS_BITBLTBUF 2
/* A strip is one 16-pixel-high row of macroblocks, each macroblock 16x16 RGBA32 pixels. */
#define MOVIE_MACROBLOCK 16
#define MOVIE_MACROBLOCK_BYTES 0x400
#define MOVIE_MACROBLOCK_QWORDS 0x40
/* GIF packets of the strip upload: A+D settings, then one IMAGE tag per macroblock. */
#define MOVIE_GIF_TAG_AD_2 (GIF_TAG_NREG_1 | GIF_TAG_EOP | 2)
#define MOVIE_GIF_TAG_IMAGE_BLOCK (GIF_TAG_FLG_IMAGE | GIF_TAG_EOP | MOVIE_MACROBLOCK_QWORDS)
#define MOVIE_BITBLTBUF_DBW_512 0x8000000000000
#define MOVIE_TRXREG_MACROBLOCK 0x1000000010

typedef struct XglDmaRefTag {
    unsigned int tag;
    unsigned int address;
    unsigned long upperQword;
} XglDmaRefTag;

typedef struct XglMovieSourceHeader XglMovieSourceHeader;

typedef struct XglMovieWork {
    unsigned char unmodeled_00[8];
    unsigned char streamState;      /* +0x08 */
    unsigned char unmodeled_09[0x0b];
    int sourceLength;               /* +0x14 */
    int streamingMode;              /* +0x18 */
    unsigned char unmodeled_1c[4];
    unsigned char *ringBuffer;      /* +0x20 */
    int ringCapacity;               /* +0x24 */
    int ringWrite;                  /* +0x28 */
    int ringRead;                   /* +0x2c */
    unsigned char unmodeled_30[0x10];
    short width;                    /* +0x40 */
    short height;                   /* +0x42 */
    short frameCount;               /* +0x44 */
    short currentFrame;             /* +0x46 */
    int stripQwords;                /* +0x48 */
    unsigned int imageQwords;       /* +0x4c */
    XglDmaRefTag chain[4];          /* +0x50 */
    XglMovieSourceHeader *source;   /* +0x90 */
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
    unsigned int imageAddress[2];   /* +0xb0 */
    unsigned int packetAddress[2];  /* +0xb8 */
    unsigned long long bitbltbuf;   /* +0xc0 */
    unsigned char bufferIndex;      /* +0xc8 */
    unsigned char transMode;        /* +0xc9 */
    unsigned char threshold1;       /* +0xca */
    unsigned char threshold0;       /* +0xcb */
    int missCount;                  /* +0xcc */
} XglMovieWork;

struct XglMovieSourceHeader {
    unsigned int signature;             /* "ipum" */
    unsigned char unmodeled_04[4];
    unsigned short width;               /* +0x08 */
    unsigned short height;              /* +0x0a */
    unsigned short frameCount;          /* +0x0c */
    unsigned char unmodeled_0e[2];
};

typedef struct XglMovieRenderState {
    unsigned char unmodeled_00[0x14];
    unsigned short bufferBase0;         /* +0x14 */
    unsigned char unmodeled_16[4];
    unsigned short bufferBase1;         /* +0x1a */
    unsigned char unmodeled_1c[0x40];
} XglMovieRenderState;

extern XglMovieRenderState sRender;

/*
 * Builds the GS upload packet for one 16-line strip at row y: an A+D setting block, then one
 * image block per macroblock with its TRXPOS and TRXDIR, closed by a REF tag. Returns the next free packet.
 */
unsigned long *make_gstrans(XglMovieWork *info, int y)
{
    int width = info->width;
    unsigned long *packet = (unsigned long *) info->packetAddress[info->bufferIndex];
    unsigned int image = info->imageAddress[info->bufferIndex];
    int x;

    packet[0] = DMA_TAG_ID_CNT | 3;
    packet[2] = MOVIE_GIF_TAG_AD_2;
    packet[3] = GIF_REG_AD;
    packet[1] = 0;
    switch (info->transMode) {
    case MOVIE_TRANS_BUFFER_0:
    default:
        packet[4] = ((unsigned long) sRender.bufferBase0 << 37) | MOVIE_BITBLTBUF_DBW_512;
        break;
    case MOVIE_TRANS_BUFFER_1:
        packet[4] = ((unsigned long) sRender.bufferBase1 << 37) | MOVIE_BITBLTBUF_DBW_512;
        break;
    case MOVIE_TRANS_BITBLTBUF:
        packet[4] = info->bitbltbuf;
        break;
    }
    packet[5] = SCE_GS_BITBLTBUF;
    packet[6] = MOVIE_TRXREG_MACROBLOCK;
    packet[7] = SCE_GS_TRXREG;
    packet += 8;
    for (x = 0; x < width; x += MOVIE_MACROBLOCK) {
        packet[1] = 0;
        packet[4] = ((unsigned long) x << 32) | ((unsigned long) y << 48);
        packet[0] = DMA_TAG_ID_CNT | 4;
        packet[2] = MOVIE_GIF_TAG_AD_2;
        packet[3] = GIF_REG_AD;
        packet[5] = SCE_GS_TRXPOS;
        packet[6] = 0;
        packet[7] = SCE_GS_TRXDIR;
        packet[8] = MOVIE_GIF_TAG_IMAGE_BLOCK;
        packet[9] = 0;
        packet += 10;
        ((XglDmaRefTag *) packet)->address = image;
        image += MOVIE_MACROBLOCK_BYTES;
        ((XglDmaRefTag *) packet)->tag = DMA_TAG_ID_REF | MOVIE_MACROBLOCK_QWORDS;
        ((XglDmaRefTag *) packet)->upperQword = 0;
        packet += 2;
    }
    ((XglDmaRefTag *) packet)[-1].tag &= DMA_TAG_ID_CLEAR_MASK;
    return packet;
}

INCLUDE_ASM("asm/main/nonmatchings/xgl_1_016", checkdma);

/* The movie work area starts with its CD stream record (0x40 bytes, the
 * width follows at +0x40); xglMovieInfoInit and xglMovieOpen pass it to the
 * xgl_cd stream functions. */
struct CdStreamParam;
void xglCdStreamParamInit(struct CdStreamParam *stream);

/* xglMovieOpen: the largest source the ring takes, and the size it is cut to. */
#define MOVIE_SOURCE_SIZE_LIMIT 0xfffff
#define MOVIE_RING_HALF_BYTES 0xf0000
#define MOVIE_DMA_QWORD 16
/* "ipum" */
#define MOVIE_SIGNATURE_IPUM 0x6d757069

int printf(const char *format, ...);
int xglCdStreamOpen(struct CdStreamParam *stream, int file);
int xglCdStreamReadRing(struct CdStreamParam *stream, int bytes);
void SyncDCache(void *start, void *end);

/*
 * Opens a movie: from a stream (file) through a ring of two source-sized
 * halves fed by the four-tag DMA chain, or from memory as one transfer.
 * Then checks the "ipum" header and takes the size and frame count.
 */
int xglMovieOpen(XglMovieWork *info, int stream)
{
    XglMovieSourceHeader *source;
    XglDmaRefTag *chain;

    if (stream) {
        info->streamOpen = 1;
        if (info->sourceSize > MOVIE_SOURCE_SIZE_LIMIT) {
            printf("ring buffer size too large.\n");
            info->sourceSize = MOVIE_RING_HALF_BYTES;
        }
        info->ringCapacity = info->sourceSize * 2;
        info->ringBuffer = (unsigned char *) info->source;
        xglCdStreamOpen((struct CdStreamParam *) info, stream);
        chain = info->chain;
        chain[0].tag = info->sourceSize / MOVIE_DMA_QWORD - 1;
        chain[0].address = (unsigned int) (info->source + 1);
        chain[0].upperQword = 0;
        chain[1].tag = info->sourceSize / MOVIE_DMA_QWORD;
        chain[1].address = (unsigned int) info->source;
        chain[1].upperQword = 0;
        chain[2].tag = info->sourceSize / MOVIE_DMA_QWORD;
        chain[2].address = (unsigned int) info->source + info->sourceSize;
        chain[2].upperQword = 0;
        chain[3].tag = (info->sourceLength % info->sourceSize + 15) / MOVIE_DMA_QWORD;
        chain[3].address = (unsigned int) info->source
                                 + info->sourceSize * (info->sourceLength / info->sourceSize & 1);
        chain[3].upperQword = 0;
        info->ringRead = 0;
        info->ringWrite = 0;
        xglCdStreamReadRing((struct CdStreamParam *) info, info->sourceSize);
        info->ringRead = 0;
        info->dmaControl = MOVIE_D4_CHCR_CHAIN_START;
        info->dmaTag = chain;
        info->decodeState = 0;
        info->dmaAddress = 0;
        info->dmaQwords = 0;
        info->bitPosition = 0;
    } else {
        info->dmaControl = MOVIE_D4_CHCR_CHAIN_START;
        info->dmaAddress = (unsigned int) (info->source + 1);
        info->dmaQwords = (info->sourceSize - 1) / MOVIE_DMA_QWORD;
        info->streamOpen = 0;
        info->dmaTag = 0;
        info->bitPosition = 0;
    }
    source = info->source;
    if (source->signature != MOVIE_SIGNATURE_IPUM) {
        xglMovieClose(info);
        return 1;
    }
    info->width = source->width;
    info->stripQwords = (info->width >> 4) << 6;
    info->height = source->height;
    info->imageQwords = (info->height >> 4) * info->stripQwords;
    info->frameCount = source->frameCount;
    info->currentFrame = 0;
    info->bufferIndex = 0;
    SyncDCache(info, info + 1);
    return 0;
}

INCLUDE_ASM("asm/main/nonmatchings/xgl_1_016", xglMoviePlay);

INCLUDE_ASM("asm/main/nonmatchings/xgl_1_016", xglMovieRewind);

typedef signed char s8;

typedef int s32;

int func_00131380(int, int);
void xglCdStreamClose(void);

/*
 * Closes the movie stream of info: runs xglCdStreamClose while streamOpen is set,
 * clears the D4 channel, writes IPU_CTRL, and returns -1
 * when func_00131380 reports a failure before or after the write to IPU_CMD.
 */
/* EE IPU registers. */
#define IPU_CMD (*(volatile u32 *) 0x10002000)
#define IPU_CTRL (*(volatile u32 *) 0x10002010)

int xglMovieClose(XglMovieWork *info) {
    if (info->streamOpen != 0) {
        xglCdStreamClose();
    }
    setD4_CHCR(0);
    IPU_CTRL = 0x40000000;
    if (func_00131380(0, 0x200) < 0) {
        return -1;
    }
    IPU_CMD = 0;
    return (func_00131380(0, 0x200) >= 0) ? 0 : -1;
}


/* The two packet buffers in scratchpad RAM. */
#define MOVIE_PACKET_ADDRESS_0 0x70000040
#define MOVIE_PACKET_ADDRESS_1 0x70001000

void xglMovieInfoInit(XglMovieWork *info)
{
    xglCdStreamParamInit((struct CdStreamParam *) info);
    info->packetAddress[0] = MOVIE_PACKET_ADDRESS_0;
    info->packetAddress[1] = MOVIE_PACKET_ADDRESS_1;
    info->transMode = info->currentFrame = -1;
    info->loop = 0;
    info->threshold1 = 0;
    info->threshold0 = 0;
    info->missCount = 0;
}
