#ifndef INCLUDE_MAIN_TALK_SCRIPT_H
#define INCLUDE_MAIN_TALK_SCRIPT_H

#include "gs.h"
#include "main/xgl_packet.h"
#include "shared.h"
#include "main/interp_update.h"

/* Functions of src/main/talk_script.c that other files call. */

/* A GsAdPair of a packet built in the scratchpad (record_render.c builds them too), read through a union: an access
 * through a union gets alias set 0 in gcc 2.96, which changes the scheduling of the pair stores (a plain GsAdPair
 * changes talk_script, talk_draw_fade and record_render). */
typedef union GsPair {
    GsAdPair ad;
    long qword[2];
} GsPair;

/* talkWindowDrawFrame draws the page cursor of an active text slot while it is open and no longer
 * revealing text (bit 0x10, INTERP_FLAG_SCAN of msg_slot_core.h, clear). */

/* Frame modes of talkWindowDrawFrame; 3 is the sized frame of talkWindowFrameRect. */
#define TALK_FRAME_FULL 0
#define TALK_FRAME_OPENING 1
#define TALK_FRAME_CLOSING 2
#define TALK_FRAME_SIZED 3

/* Screen area of the talk frames (a 512x448 display): the last pixel column and row. */
#define TALK_SCREEN_RIGHT 0x1FF
#define TALK_SCREEN_BOTTOM 0x1BF

/* GIF tag of the A+D packets the talk code builds in the scratchpad: GIF_TAG_ONE_AD_EOP (gs.h) with
 * NLOOP 0; the pair count is added to it when the packet is sent. */
#define TALK_GIF_TAG_AD_EOP (GIF_TAG_ONE_AD_EOP - 1)

#include "main/msg_slot_core.h"

extern int talkActive;
extern void *talkTextureHandle;

void talkWindowDrawFrame(MsgSlot *window, int mode, float scale);

int textSlotWrapWidth(int value);
int interpLineScrollY(int value);
void talkEndTexture(void);
void talkDeactivate(void);
void talkSetActive(void);
void talkStartTextureLoad(void);
void talkLoadOmiyaTexture(void);
void talkCloseTexture(void);
void talkSetPending(int value);
int talkTakePending(void);
void talkDrawFramesBasic(MsgSlot *slot);
void talkDrawFramesWithCursor(MsgSlot *slot);
void talkDrawFramesWithBox(MsgSlot *slot);
void talkDrawFrameRects(MsgSlot *slot);
void talkDrawFramesWithFade(MsgSlot *slot);
void talkDrawListFooterWindow(MsgSlot *slot);

#endif /* INCLUDE_MAIN_TALK_SCRIPT_H */
