/*
 * OV01 original TU 64: the screen fade task (screenFadeInStart, screenFadeOutStart)
 */

#include "common.h"
#include "ov01/tu064.h"
#include "ov01/tu048.h"
#include "gs.h"

/* Alpha of a fully opaque screen fade (GS ALPHA value of the fix). */
#define SCREEN_FADE_OPAQUE 0x80

/* GIF tag of the fade quad: NLOOP 2, EOP set, NREG 1 (A+D). */
#define GIF_TAG_TWO_AD_EOP 0x1000000000008002ULL

/*
 * Screen fade task (the object screenFadeInStart and screenFadeOutStart allocate).
 * The words used by the step and draw functions: step at 0x04 and draw at 0x08 (the
 * callbacks), alpha at 0x0C (GS alpha, 0x80 is opaque), alphaStep at 0x10, frameDelay
 * at 0x14 and finished at 0x18. Nothing else is evidenced.
 */
struct ScreenFade {
    unsigned char unmodeled_00[4];
    void (*step)(struct ScreenFade *fade);
    void (*draw)(struct ScreenFade *fade);
    int alpha;
    int alphaStep;
    int frameDelay;
    int finished;
};

void *ov01TaskClaim(void);
void screenFadeInStep(struct ScreenFade *fade);
void screenFadeDraw(struct ScreenFade *fade);
extern int screenFadeTask;

void screenFadeOutStep(struct ScreenFade *fade);

/*
 * Starts a screen fade whose alpha rises from 0 to 0x80 (opaque): allocates a
 * task, stores alphaStep and frameDelay in it and makes it the running fade.
 */
void screenFadeInStart(int alphaStep, int frameDelay)
{
    struct ScreenFade *fade;

    fade = ov01TaskClaim();
    if (fade == 0) {
        return;
    }
    fade->step = screenFadeInStep;
    fade->alphaStep = alphaStep;
    fade->frameDelay = frameDelay;
    fade->alpha = 0;
    fade->finished = 0;
    fade->draw = screenFadeDraw;
    screenFadeTask = (int)fade;
}

/* Starts a screen fade whose alpha falls from 0x80 to 0. Same layout as the rising fade. */
void screenFadeOutStart(int alphaStep, int frameDelay)
{
    struct ScreenFade *fade;

    fade = ov01TaskClaim();
    if (fade == 0) {
        return;
    }
    fade->alpha = SCREEN_FADE_OPAQUE;
    fade->alphaStep = alphaStep;
    fade->frameDelay = frameDelay;
    fade->finished = 0;
    fade->step = screenFadeOutStep;
    fade->draw = screenFadeDraw;
    screenFadeTask = (int)fade;
}

/* Nonzero when no screen fade is running. */
int screenFadeIdle(void) {
    return screenFadeTask == 0;
}


typedef union GrGifWord {
    unsigned long long bits;
    unsigned int lanes[2];
} GrGifWord;

typedef struct GrGifOpenTag {
    GrGifWord control;
    GrGifWord register_id;
} GrGifOpenTag;

typedef struct GrScreenVertex {
    int x;
    int y;
    int z;
    int flags;
} GrScreenVertex;

typedef union GrGifQuad {
    GrGifOpenTag tag;
    GrScreenVertex xyz;
} GrGifQuad;

#define GR_PACKET_QUAD(packet, index) ((GrGifQuad *)(packet)->data + (index))

void ov01TaskRelease(void *task);

static inline void grGifOpenRegister(GrPacket *packet, unsigned long long register_id)
{
    GrGifOpenTag *tags = packet->data;
    tags[packet->count].register_id.bits = register_id;
}

/* Rising fade step: waits out frameDelay, then adds alphaStep; finished once alpha passes 0x80. */
void screenFadeInStep(struct ScreenFade *fade)
{
    int nextAlpha;

    if (fade->frameDelay > 0) {
        fade->frameDelay = fade->frameDelay - 1;
        return;
    }
    nextAlpha = fade->alpha + fade->alphaStep;
    fade->alpha = nextAlpha;
    if (nextAlpha > SCREEN_FADE_OPAQUE) {
        fade->finished = 1;
    }
}

/* Falling fade step: waits out frameDelay, then subtracts alphaStep; finished once alpha reaches 0. */
void screenFadeOutStep(struct ScreenFade *fade)
{
    int nextAlpha;

    if (fade->frameDelay > 0) {
        fade->frameDelay = fade->frameDelay - 1;
        return;
    }
    nextAlpha = fade->alpha - fade->alphaStep;
    fade->alpha = nextAlpha;
    if (nextAlpha <= 0) {
        fade->finished = 1;
    }
}

/* Draws the fade as a full-screen sprite blended at the clamped alpha; a finished fade is removed and clears the running-fade global. */
void screenFadeDraw(struct ScreenFade *fade)
{
    GrPacket packet;
    int *count;
    unsigned int fadeDepth;
    int alpha;

    fadeDepth = 0x00fffffc;
    alpha = fade->alpha;
    if (alpha < 0) {
        alpha = 0;
    }
    grGpInit(&packet);
    grGifOpenRegister(&packet, GIF_AD);
    count = &packet.count;
    GR_PACKET_QUAD(&packet, (*count)++)->tag.control.bits = GIF_TAG_TWO_AD_EOP;

    grGifOpenRegister(&packet, GS_ALPHA_1);
    count = &packet.count;
    GR_PACKET_QUAD(&packet, (*count)++)->tag.control.bits = ((unsigned long long)alpha << 32) | 0xa9;

    grGifOpenRegister(&packet, GS_TEST_1);
    count = &packet.count;
    GR_PACKET_QUAD(&packet, (*count)++)->tag.control.bits = 0x50000;

    grGifOpenRegister(&packet, GS_XYZ2);
    count = &packet.count;
    GR_PACKET_QUAD(&packet, (*count)++)->tag.control.bits = 0x1023400000008002ULL;

    GR_PACKET_QUAD(&packet, packet.count)->xyz.x = 0x7000;
    GR_PACKET_QUAD(&packet, packet.count)->xyz.y = 0x7200;
    GR_PACKET_QUAD(&packet, packet.count)->xyz.z = fadeDepth;
    GR_PACKET_QUAD(&packet, (*count)++)->xyz.flags = 0;

    GR_PACKET_QUAD(&packet, packet.count)->xyz.x = 0x9000;
    GR_PACKET_QUAD(&packet, packet.count)->xyz.y = 0x8e00;
    GR_PACKET_QUAD(&packet, packet.count)->xyz.z = fadeDepth;
    GR_PACKET_QUAD(&packet, (*count)++)->xyz.flags = 0;

    grPacketSend(&packet);
    if (fade->finished == 1) {
        ov01TaskRelease(fade);
        screenFadeTask = 0;
    }
}

int screenFadeTask;
