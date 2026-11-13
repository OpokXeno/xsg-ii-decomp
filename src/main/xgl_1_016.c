#include "common.h"
#include "shared.h"
#include "gs.h"
#include "main/tu093.h"
#include "main/xgl_1_016.h"

/* D_ENABLEW bit 16 (CPND): holds all DMA transfers while a channel register is written. */
#define D_ENABLE_HOLD 0x10000
#define D_ENABLEW ((volatile u32 *) 0x1000f590)
/* DMAC channel 4 feeds the IPU (memory to IPU input); its control register takes the transfer mode and start bit. */
#define D4_CHCR ((volatile u32 *) 0x1000b400)
/* The rest of the IPU input channel, the IPU output channel (D3) and the IPU registers. */
#define D4_MADR ((volatile u32 *) 0x1000b410)
#define D4_QWC ((volatile u32 *) 0x1000b420)
#define D4_TADR ((volatile u32 *) 0x1000b430)
#define D3_CHCR ((volatile u32 *) 0x1000b000)
#define IPU_CMD ((volatile u32 *) 0x10002000)
#define IPU_CTRL ((volatile u32 *) 0x10002010)
#define IPU_BP ((volatile u32 *) 0x10002020)
/* EE scratchpad RAM; the two strip packet buffers live in it. */
#define SCRATCHPAD_BASE 0x70000000

extern int DIntr(void);
extern int EIntr(void);

/* Starts or stops the IPU input transfer by writing D4_CHCR with the DMA interrupts held off.
 * Static in this file, as in XS1's xgl_1.c: checkdma's delay-slot filling depends on the
 * definition being in the same translation unit. */
static void setD4_CHCR(u32 value)
{
    DIntr();
    *D_ENABLEW = *D_ENABLEW | D_ENABLE_HOLD;
    *D4_CHCR = value;
    *D_ENABLEW = *D_ENABLEW & ~D_ENABLE_HOLD;
    EIntr();
}

/* Clears the tag ID and IRQ bits, turning a REF tag into REFE. */
#define DMA_TAG_ID_CLEAR_MASK 0x0fffffff
/* Dn_CHCR of the D4 (IPU_to) channel: chain mode, from memory, started. */
#define DMA_CHCR_DIR 0x1
#define DMA_CHCR_MOD_CHAIN 0x4
#define DMA_CHCR_STR 0x100
#define MOVIE_D4_CHCR_CHAIN_START (DMA_CHCR_STR | DMA_CHCR_MOD_CHAIN | DMA_CHCR_DIR)
/* D4_CHCR value that leaves the chain set up but stopped, and the start with the REF tag ID. */
#define MOVIE_D4_CHCR_CHAIN_STOP (DMA_CHCR_MOD_CHAIN | DMA_CHCR_DIR)
#define MOVIE_D4_CHCR_CHAIN_START_REF (DMA_TAG_ID_REF | MOVIE_D4_CHCR_CHAIN_START)
/* GIF tag fields (64-bit view of the tag's low doubleword). */
#define GIF_TAG_EOP 0x8000
#define GIF_TAG_NREG_1 0x1000000000000000
#define GIF_TAG_FLG_IMAGE 0x0800000000000000
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
#define MOVIE_TRXREG_MACROBLOCK GS_SET_TRXREG(MOVIE_MACROBLOCK, MOVIE_MACROBLOCK)

/*
 * Builds the GS upload packet for one 16-line strip at row y: an A+D setting block, then one
 * image block per macroblock with its TRXPOS and TRXDIR, closed by a REF tag. Returns the next free packet.
 */
unsigned long *make_gstrans(XglMovieWork *info, int y)
{
    int width = info->width;
    unsigned long *packet = (unsigned long *) info->packetAddress[info->bufferIndex];
    unsigned int image = (unsigned int) info->imageAddress[info->bufferIndex];
    int x;

    packet[0] = DMA_TAG_ID_CNT | 3;
    packet[2] = MOVIE_GIF_TAG_AD_2;
    packet[3] = GIF_AD;
    packet[1] = 0;
    switch (info->transMode) {
    case MOVIE_TRANS_BUFFER_0:
    default:
        packet[4] = ((unsigned long) sRender.frame_base << 37) | MOVIE_BITBLTBUF_DBW_512;
        break;
    case MOVIE_TRANS_BUFFER_1:
        packet[4] = ((unsigned long) sRender.frame_base_1 << 37) | MOVIE_BITBLTBUF_DBW_512;
        break;
    case MOVIE_TRANS_BITBLTBUF:
        packet[4] = info->bitbltbuf;
        break;
    }
    packet[5] = GS_BITBLTBUF;
    packet[6] = MOVIE_TRXREG_MACROBLOCK;
    packet[7] = GS_TRXREG;
    packet += 8;
    for (x = 0; x < width; x += MOVIE_MACROBLOCK) {
        packet[1] = 0;
        packet[4] = ((unsigned long) x << 32) | ((unsigned long) y << 48);
        packet[0] = DMA_TAG_ID_CNT | 4;
        packet[2] = MOVIE_GIF_TAG_AD_2;
        packet[3] = GIF_AD;
        packet[5] = GS_TRXPOS;
        packet[6] = 0;
        packet[7] = GS_TRXDIR;
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

/* XglMovieWork.decodeState: which ring half the IPU feed reads (the last chunk of the source in
 * the LAST_* states), and the state once the whole source has been sent. */
#define MOVIE_DECODE_HALF0 0
#define MOVIE_DECODE_HALF1 1
#define MOVIE_DECODE_LAST_HALF0 2
#define MOVIE_DECODE_LAST_HALF1 3
#define MOVIE_DECODE_DONE 99

/* Advances the stream's DMA chain once the IPU has consumed the current source chunk. Returns 1
 * when the caller must wait for more stream data. */
int checkdma(XglMovieWork *info)
{
    int wait = 0;
    unsigned int consumed;

    if (info->decodeState == MOVIE_DECODE_DONE) {
        return 0;
    }
    setD4_CHCR(MOVIE_D4_CHCR_CHAIN_STOP);
    consumed = *D4_MADR - (unsigned int)info->source;
    switch (info->decodeState) {
    case MOVIE_DECODE_HALF0:
        if ((unsigned int)info->sourceSize < consumed) {
            if (info->stream.ring.read_position != info->stream.ring.write_position &&
                (info->stream.ring.write_position - consumed) <
                (unsigned int)(info->sourceSize / 2) && !info->stream.exhausted) {
                wait = 1;
                break;
            }
            info->stream.ring.read_position = info->sourceSize;
            if (!info->stream.remaining) {
                *D4_TADR = (unsigned int)&info->chain[3];
                info->decodeState = MOVIE_DECODE_LAST_HALF1;
            } else {
                *D4_TADR = (unsigned int)&info->chain[1];
                info->decodeState = MOVIE_DECODE_HALF1;
            }
        } else {
            *D4_TADR = (unsigned int)&info->chain[2];
        }
        break;
    case MOVIE_DECODE_HALF1:
        if (consumed < (unsigned int)info->sourceSize) {
            if (info->stream.ring.read_position != info->stream.ring.write_position &&
                (info->stream.ring.write_position - consumed) <
                (unsigned int)(info->sourceSize / 2) && !info->stream.exhausted) {
                wait = 1;
                break;
            }
            info->stream.ring.read_position = 0;
            if (!info->stream.remaining) {
                *D4_TADR = (unsigned int)&info->chain[3];
                info->decodeState = MOVIE_DECODE_LAST_HALF0;
            } else {
                *D4_TADR = (unsigned int)&info->chain[2];
                info->decodeState = MOVIE_DECODE_HALF0;
            }
        } else {
            *D4_TADR = (unsigned int)&info->chain[1];
        }
        break;
    case MOVIE_DECODE_LAST_HALF0:
        if ((unsigned int)info->sourceSize < consumed) {
            info->decodeState = MOVIE_DECODE_DONE;
        }
        break;
    case MOVIE_DECODE_LAST_HALF1:
        if (consumed < (unsigned int)info->sourceSize) {
            info->decodeState = MOVIE_DECODE_DONE;
        }
        break;
    }
    if (info->decodeState != MOVIE_DECODE_DONE) {
        setD4_CHCR(MOVIE_D4_CHCR_CHAIN_START_REF);
    } else {
        setD4_CHCR(MOVIE_D4_CHCR_CHAIN_START);
    }
    return wait;
}

/* xglMovieOpen: the largest source the ring takes, and the size it is cut to. */
#define MOVIE_SOURCE_SIZE_LIMIT 0xfffff
#define MOVIE_RING_HALF_BYTES 0xf0000
#define MOVIE_DMA_QWORD 16
/* "ipum" */
#define MOVIE_SIGNATURE_IPUM 0x6d757069

int printf(const char *format, ...);
void SyncDCache(void *start, void *end);

/*
 * Opens a movie: from the file `name` through a ring of two source-sized
 * halves fed by the four-tag DMA chain, or from memory as one transfer.
 * Then checks the "ipum" header and takes the size and frame count.
 */
int xglMovieOpen(XglMovieWork *info, const char *name)
{
    XglMovieSourceHeader *source;
    XglDmaRefTag *chain;

    if (name != 0) {
        info->streamOpen = 1;
        if (info->sourceSize > MOVIE_SOURCE_SIZE_LIMIT) {
            printf("ring buffer size too large.\n");
            info->sourceSize = MOVIE_RING_HALF_BYTES;
        }
        info->stream.ring.capacity = info->sourceSize * 2;
        info->stream.ring.buffer = (unsigned char *) info->source;
        xglCdStreamOpen(&info->stream, name);
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
        chain[3].tag = (info->stream.file_byte_count % info->sourceSize + 15) / MOVIE_DMA_QWORD;
        chain[3].address = (unsigned int) info->source
                                 + info->sourceSize * (info->stream.file_byte_count / info->sourceSize & 1);
        chain[3].upperQword = 0;
        info->stream.ring.read_position = 0;
        info->stream.ring.write_position = 0;
        xglCdStreamReadRing(&info->stream, info->sourceSize);
        info->stream.ring.read_position = 0;
        info->dmaControl = MOVIE_D4_CHCR_CHAIN_START;
        info->dmaTag = chain;
        info->decodeState = MOVIE_DECODE_HALF0;
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
    /* macroblocks per strip, MOVIE_MACROBLOCK_QWORDS each */
    info->stripQwords = (info->width >> 4) << 6;
    info->height = source->height;
    info->imageQwords = (info->height >> 4) * info->stripQwords;
    info->frameCount = source->frameCount;
    info->currentFrame = 0;
    info->bufferIndex = 0;
    SyncDCache(info, info + 1);
    return 0;
}

#define IPU_CTRL_RESET 0x40000000
#define IPU_CMD_BCLR 0
#define IPU_CMD_IDEC 0x10000000
#define IPU_CMD_FDEC 0x40000000
#define IPU_CMD_SETTH 0x90000000
#define IPU_IDEC_QSC_1 0x10000 /* quantiser step code 1 */
#define IPU_IDEC_DTD_SHIFT 24
/* FDEC with FBP n: decodes (and skips) n bits. */
#define IPU_CMD_FDEC_BITS(n) (IPU_CMD_FDEC | (n))
/* IPU_BP: the quadwords still in the IPU input (FP + IFC), and the bit position BP. */
#define IPU_BP_QWORDS(bp) ((((bp) >> 16) & 3) + (((bp) >> 8) & 0xf))
#define IPU_BP_BITS(bp) ((bp) & 0x7f)
/* sceIpuSync timeout, and the busy-poll bound of the D3 wait. */
#define IPU_SYNC_TIMEOUT 0x200
#define MOVIE_POLL_LIMIT 0xFFFFFF
/* transMode: decode into memory only (the strips are copied out of the scratchpad). */
#define MOVIE_TRANS_MEMORY 0xff
#define MOVIE_DMA_MAX_QWORDS 0xffff
#define XGL_DMA_CHANNEL_GIF 2

/* The strip copy moves 16-byte quadwords (lq/sq) as plain C assignments; quadword.h has only the
 * inline-asm macros, which force the addresses into registers and do not schedule like this copy. */
typedef unsigned int Quadword __attribute__((mode(TI)));

int sceIpuSync(int mode, int timeout);
int sceGsSyncPath(int mode, int timeout);
int myDmaDirectFromIPU(u32 address, u32 count);
/* "frame drop\n" and "ipu time out:%d\n" */
extern const char frameDropMessage[];
extern const char ipuTimeoutMessage[];

/*
 * Decodes one movie frame: restarts the IPU feed when its DMA has stopped, checks the stream ring,
 * decodes the strips into the GS (through make_gstrans packets) or into memory, and advances the
 * frame. At the end it stops (loop off) or rewinds; otherwise it saves the IPU bitstream position
 * for the next frame. Returns -1 on an IPU or ring error, 1 at the end, 0 otherwise.
 */
int xglMoviePlay(XglMovieWork *info)
{
    int strip;
    int retry;
    unsigned int header;
    int pitch;
    int x;
    int row;
    Quadword *source;
    Quadword *destination;
    unsigned int work;

    if (info->loop == 0 && info->currentFrame >= info->frameCount) {
        return 1;
    }
    if ((*D4_CHCR & DMA_CHCR_STR) == 0) {
        *IPU_CTRL = IPU_CTRL_RESET;
        if (sceIpuSync(0, IPU_SYNC_TIMEOUT) < 0) {
            return -1;
        }
        *IPU_CMD = IPU_CMD_SETTH | (info->threshold1 << 16) | info->threshold0;
        if (sceIpuSync(0, IPU_SYNC_TIMEOUT) < 0) {
            return -1;
        }
        *IPU_CMD = IPU_CMD_BCLR | info->bitPosition;
        if (sceIpuSync(0, IPU_SYNC_TIMEOUT) < 0) {
            return -1;
        }
        *D4_MADR = info->dmaAddress;
        if (info->dmaQwords > MOVIE_DMA_MAX_QWORDS) {
            *D4_QWC = MOVIE_DMA_MAX_QWORDS;
        } else {
            *D4_QWC = info->dmaQwords;
        }
        *D4_TADR = (unsigned int) info->dmaTag;
        setD4_CHCR(info->dmaControl);
    }
    if (info->streamOpen) {
        if (checkdma(info) != 0) {
            printf(frameDropMessage);
            return -1;
        }
    }
    if (info->transMode != MOVIE_TRANS_MEMORY) {
        sceGsSyncPath(0, 0);
        for (strip = 0; strip < info->height; strip += MOVIE_MACROBLOCK) {
            myDmaDirectFromIPU((u32) info->imageAddress[info->bufferIndex], info->stripQwords);
            if (strip == 0) {
                *IPU_CMD = IPU_CMD_FDEC;
                if (sceIpuSync(0, IPU_SYNC_TIMEOUT) < 0) {
                    return -1;
                }
                header = *IPU_CMD >> 24;
                *IPU_CMD = IPU_CMD_FDEC_BITS(8);
                if (sceIpuSync(0, IPU_SYNC_TIMEOUT) < 0) {
                    return -1;
                }
                /* The picture header byte: its bit 2 is IDEC's DTD flag, the others go to the
                 * decode fields of IPU_CTRL (bits 16..23). */
                *IPU_CTRL = (header & 0xfb) << 16;
                *IPU_CMD = (((header & 4) >> 2) << IPU_IDEC_DTD_SHIFT) | (IPU_CMD_IDEC | IPU_IDEC_QSC_1);
            }
            make_gstrans(info, strip);
            for (retry = 0; retry <= MOVIE_POLL_LIMIT; retry++) {
                if ((*D3_CHCR & DMA_CHCR_STR) == 0) {
                    break;
                }
            }
            xglDmaDirectSrcChain(XGL_DMA_CHANNEL_GIF, info->packetAddress[info->bufferIndex]);
            info->bufferIndex ^= 1;
        }
        sceGsSyncPath(0, 0);
    } else {
        work = SCRATCHPAD_BASE;
        if (info->workBuffer != 0) {
            work = (unsigned int) info->workBuffer;
        }
        for (strip = 0; strip < info->height; strip += MOVIE_MACROBLOCK) {
            myDmaDirectFromIPU(work, info->stripQwords);
            if (strip == 0) {
                *IPU_CMD = IPU_CMD_FDEC;
                if (sceIpuSync(0, IPU_SYNC_TIMEOUT) < 0) {
                    return -1;
                }
                header = *IPU_CMD >> 24;
                *IPU_CMD = IPU_CMD_FDEC_BITS(8);
                if (sceIpuSync(0, IPU_SYNC_TIMEOUT) < 0) {
                    return -1;
                }
                /* The picture header byte: its bit 2 is IDEC's DTD flag, the others go to the
                 * decode fields of IPU_CTRL (bits 16..23). */
                *IPU_CTRL = (header & 0xfb) << 16;
                *IPU_CMD = (((header & 4) >> 2) << IPU_IDEC_DTD_SHIFT) | (IPU_CMD_IDEC | IPU_IDEC_QSC_1);
            }
            for (retry = 0; retry <= MOVIE_POLL_LIMIT; retry++) {
                if ((*D3_CHCR & DMA_CHCR_STR) == 0) {
                    break;
                }
            }
            if (retry > MOVIE_POLL_LIMIT) {
                printf(ipuTimeoutMessage, strip);
            }
            pitch = info->width / 4;
            source = (Quadword *) work;
            destination = (Quadword *) (info->imageAddress[info->bufferIndex] + info->width * strip * 4);
            for (x = 0; x < info->width; x += MOVIE_MACROBLOCK) {
                for (row = MOVIE_MACROBLOCK - 1; row >= 0; row--) {
                    destination[0] = source[0];
                    destination[1] = source[1];
                    destination[2] = source[2];
                    destination[3] = source[3];
                    source += 4;
                    destination += pitch;
                }
                destination = destination - pitch * MOVIE_MACROBLOCK + 4;
            }
        }
        info->bufferIndex ^= 1;
    }
    if (sceIpuSync(0, IPU_SYNC_TIMEOUT) < 0) {
        return -1;
    }
    *IPU_CMD = IPU_CMD_FDEC_BITS(32);
    if (sceIpuSync(0, IPU_SYNC_TIMEOUT) < 0) {
        return -1;
    }
    info->currentFrame++;
    if (info->currentFrame >= info->frameCount) {
        if (info->loop == 0) {
            info->currentFrame = info->frameCount;
            setD4_CHCR(MOVIE_D4_CHCR_CHAIN_STOP);
            DIntr();
            *D_ENABLEW = *D_ENABLEW | D_ENABLE_HOLD;
            *D3_CHCR = 0;
            *D_ENABLEW = *D_ENABLEW & ~D_ENABLE_HOLD;
            EIntr();
        } else {
            xglMovieRewind(info);
        }
        return 1;
    }
    if (info->streamOpen) {
        unsigned int bitstream;

        info->dmaControl = *D4_CHCR;
        setD4_CHCR(MOVIE_D4_CHCR_CHAIN_STOP);
        bitstream = *IPU_BP;
        info->dmaAddress = *D4_MADR - IPU_BP_QWORDS(bitstream) * MOVIE_DMA_QWORD;
        info->dmaQwords = *D4_QWC + IPU_BP_QWORDS(bitstream);
        info->dmaTag = (XglDmaRefTag *) *D4_TADR;
        info->bitPosition = IPU_BP_BITS(bitstream);
    } else {
        unsigned int bitstream;
        unsigned int consumed;

        info->dmaControl = *D4_CHCR;
        setD4_CHCR(MOVIE_D4_CHCR_CHAIN_STOP);
        bitstream = *IPU_BP;
        consumed = *D4_MADR - info->dmaAddress - IPU_BP_QWORDS(bitstream) * MOVIE_DMA_QWORD;
        info->dmaAddress += consumed;
        info->dmaQwords -= consumed / MOVIE_DMA_QWORD;
        info->dmaTag = (XglDmaRefTag *) *D4_TADR;
        info->bitPosition = IPU_BP_BITS(bitstream);
    }
    return 0;
}

/*
 * Restarts a movie at its first frame: stops both IPU DMA channels, seeks a stream back to its first
 * sector and refills the first ring half, and sets up the DMA start as xglMovieOpen does.
 */
int xglMovieRewind(XglMovieWork *info)
{
    info->currentFrame = 0;
    setD4_CHCR(MOVIE_D4_CHCR_CHAIN_STOP);
    DIntr();
    *D_ENABLEW = *D_ENABLEW | D_ENABLE_HOLD;
    *D3_CHCR = 0;
    *D_ENABLEW = *D_ENABLEW & ~D_ENABLE_HOLD;
    EIntr();
    if (info->streamOpen) {
        xglCdStreamRewind(&info->stream);
        info->stream.ring.write_position = 0;
        info->stream.ring.read_position = 0;
        xglCdStreamReadRing(&info->stream, info->sourceSize);
        info->stream.ring.read_position = 0;
        info->dmaControl = MOVIE_D4_CHCR_CHAIN_START;
        info->dmaTag = info->chain;
        info->decodeState = MOVIE_DECODE_HALF0;
        info->dmaAddress = 0;
        info->dmaQwords = 0;
        info->bitPosition = 0;
    } else {
        info->dmaControl = MOVIE_D4_CHCR_CHAIN_START;
        info->dmaAddress = (unsigned int) (info->source + 1);
        info->dmaQwords = (info->sourceSize - 1) / MOVIE_DMA_QWORD;
        info->dmaTag = 0;
        info->bitPosition = 0;
    }
    SyncDCache(info, info + 1);
    return 0;
}

/*
 * Closes the movie stream of info: closes its CD stream while streamOpen is set,
 * clears the D4 channel, writes IPU_CTRL, and returns -1
 * when sceIpuSync reports a failure before or after the write to IPU_CMD.
 */
int xglMovieClose(XglMovieWork *info)
{
    if (info->streamOpen != 0) {
        xglCdStreamClose(&info->stream);
    }
    setD4_CHCR(0);
    *IPU_CTRL = IPU_CTRL_RESET;
    if (sceIpuSync(0, IPU_SYNC_TIMEOUT) < 0) {
        return -1;
    }
    *IPU_CMD = IPU_CMD_BCLR;
    return (sceIpuSync(0, IPU_SYNC_TIMEOUT) >= 0) ? 0 : -1;
}

/* The two packet buffers in scratchpad RAM. */
#define MOVIE_PACKET_ADDRESS_0 (SCRATCHPAD_BASE + 0x40)
#define MOVIE_PACKET_ADDRESS_1 (SCRATCHPAD_BASE + 0x1000)

/* Sets the default packet addresses and transfer thresholds of a movie init record. */
void xglMovieInfoInit(XglMovieWork *info)
{
    xglCdStreamParamInit(&info->stream);
    info->packetAddress[0] = MOVIE_PACKET_ADDRESS_0;
    info->packetAddress[1] = MOVIE_PACKET_ADDRESS_1;
    /* -1: currentFrame -1, transMode MOVIE_TRANS_MEMORY (0xFF) */
    info->transMode = info->currentFrame = -1;
    info->loop = 0;
    info->threshold1 = 0;
    info->threshold0 = 0;
    info->workBuffer = 0;
}
