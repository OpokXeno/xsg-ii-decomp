#ifndef INCLUDE_MAIN_XGL_PACKET_SCRATCH_H
#define INCLUDE_MAIN_XGL_PACKET_SCRATCH_H

/* Functions of src/main/xgl_packet_scratch.c that other files call. */

int xglClampInt(int value, int lower, int upper);
int wrapIntRange(int value, int lower, int upper);
void xglPacketScratchAddQuad(long long high, long long low);
void xglPacketScratchBegin(void);
void xglPacketScratchPatchTag(void);
void xglPacketScratchAddDirect(void);

#endif /* INCLUDE_MAIN_XGL_PACKET_SCRATCH_H */
