/* The screen fade task (fadeIn, fadeOut). */

#include "common.h"
#include "ov01/entry_first_init.h"
#include "ov01/gr_gp_init.h"
#include "ov01/ov01_task.h"
#include "gs.h"

/* GS ALPHA value that leaves the picture unchanged (the quad computes Cd x FIX, 0x80 is 1.0). */
#define SCREEN_FADE_NEUTRAL 0x80

/* GIF tag of the fade quad: NLOOP 2, EOP set, NREG 1 (A+D). */
#define GIF_TAG_TWO_AD_EOP 0x1000000000008002ULL

/*
 * Screen fade task (the object fadeIn and fadeOut allocate).
 * The words used by the step and draw functions: step at 0x04 and draw at 0x08 (the
 * callbacks), alpha at 0x0C (GS alpha, 0x80 leaves the picture unchanged), alphaStep at 0x10, frameDelay
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

void fadeInObj(struct ScreenFade *fade);
void fadeObjDraw(struct ScreenFade *fade);

void fadeOutObj(struct ScreenFade *fade);

/*
 * Starts a screen fade whose alpha rises from 0 (black) to 0x80 (picture unchanged): allocates a
 * task, stores alphaStep and frameDelay in it and makes it the running fade.
 */
void fadeIn(int alphaStep, int frameDelay)
{
    struct ScreenFade *fade;

    fade = ov01TaskClaim();
    if (fade == 0) {
        return;
    }
    fade->step = fadeInObj;
    fade->alphaStep = alphaStep;
    fade->frameDelay = frameDelay;
    fade->alpha = 0;
    fade->finished = 0;
    fade->draw = fadeObjDraw;
    screenFadeTask = (int)fade;
}

/* Starts a screen fade whose alpha falls from 0x80 to 0. Same layout as the rising fade. */
void fadeOut(int alphaStep, int frameDelay)
{
    struct ScreenFade *fade;

    fade = ov01TaskClaim();
    if (fade == 0) {
        return;
    }
    fade->alpha = SCREEN_FADE_NEUTRAL;
    fade->alphaStep = alphaStep;
    fade->frameDelay = frameDelay;
    fade->finished = 0;
    fade->step = fadeOutObj;
    fade->draw = fadeObjDraw;
    screenFadeTask = (int)fade;
}

/* Nonzero when no screen fade is running. */
int fadeEndChk(void) {
    return screenFadeTask == 0;
}


/* Rising fade step: waits out frameDelay, then adds alphaStep; finished once alpha passes 0x80. */
void fadeInObj(struct ScreenFade *fade)
{
    int nextAlpha;

    if (fade->frameDelay > 0) {
        fade->frameDelay = fade->frameDelay - 1;
        return;
    }
    nextAlpha = fade->alpha + fade->alphaStep;
    fade->alpha = nextAlpha;
    if (nextAlpha > SCREEN_FADE_NEUTRAL) {
        fade->finished = 1;
    }
}

/* Falling fade step: waits out frameDelay, then subtracts alphaStep; finished once alpha reaches 0. */
void fadeOutObj(struct ScreenFade *fade)
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
void fadeObjDraw(struct ScreenFade *fade)
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
