#ifndef INCLUDE_MAIN_XGL_DMA_H
#define INCLUDE_MAIN_XGL_DMA_H

/* DMA tag ID (bits 28..30 of the first tag word). */
#define DMA_TAG_ID_CNT 0x10000000U
#define DMA_TAG_ID_REF 0x30000000U
#define DMA_TAG_ID_RET 0x60000000U
/* VIF1 DIRECT code with its quadword count; in a DMA tag it is the high word of the second
 * doubleword. */
#define VIF_DIRECT 0x51000000U
#define DMA_TAG_VIF_DIRECT(qwords) ((u64) (VIF_DIRECT | (qwords)) << 32)

/* A REF DMA tag: tag word, source address, then the VIF codes of the upper doubleword. */
typedef struct XglDmaRefTag {
    unsigned int tag;
    unsigned int address;
    u64 upperQword;
} XglDmaRefTag;

/* Functions of src/main/xgl_dma.c that other files call. */

void xglDmaDirectNormal(unsigned int channel, unsigned int address, unsigned int count);
void xglDmaDirectSrcChain(unsigned int channel, unsigned int address);

#endif /* INCLUDE_MAIN_XGL_DMA_H */
