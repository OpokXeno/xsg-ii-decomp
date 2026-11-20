#ifndef INCLUDE_MAIN_GIFSTATE_H
#define INCLUDE_MAIN_GIFSTATE_H

#include "shared.h"

/* Types and functions of src/main/gifState.c that other files use. */

/* One impact point of the break effect (integer screen position). */
typedef struct GifBreakPoint {
    int x;
    int y;
    int z;
    int w;
} GifBreakPoint;

/* A float lane that GS packets also read as a raw word. */
typedef union GifFloat {
    f32 f;
    u32 u;
    int i;                    /* GifBreakShard.fall: the frame count of the fall */
} GifFloat;

/* One qword of four GifFloat lanes (x, y, z, w). */
typedef GifFloat GifQuad[4];

/* One shard: a triangular prism (front face, back face 10 units behind, three sides as six
 * triangles) around its centre, with its spin and fall speed. */
typedef struct GifBreakShard {
    GifQuad vertex[24];       /* +0x000 */
    GifQuad projected[24];    /* +0x180 */
    GifQuad center;           /* +0x300 */
    GifQuad spin;             /* +0x310 */
    GifQuad angle;            /* +0x320: the rotation of the last frame */
    GifFloat speed;           /* +0x330 */
    GifFloat fall;            /* +0x334 */
    u8 unmodeled_338[0x8];
} GifBreakShard;

/*
 * One GIF status block. The centre pair (+0x04/+0x08) is written by gifStateSetCenter and the
 * effect alpha pair (+0x0C/+0x10) by gifStateSetEffectAlpha; the state word and the ratio are read
 * by gifStateEffectReady.
 */
typedef struct GifStatusBlock {
    int state;
    int centerX;
    int centerY;
    int effectAlphaType;
    int effectAlpha;
    int fbp;                    /* +0x14: gifStateSetFbp; the source page selector of gifStateSetupContext (0 display, 1 draw) */
    int blur;                   /* +0x18: gifStateSetBlur */
    int depth;                  /* +0x1C: the Z of the camera quad (gifStateSetDepth sets it) */
    f32 ratio;
    f32 ratioStep;              /* +0x24: gifStateSetDecrease; the draw functions lower ratio by it */
    int subWindow[4];           /* +0x28: nmlModelUseSubWindow; word 0 is the flag gifStateDrawBreakShards tests,
                                 * gifStateDrawFadeQuad moves its corners by all four */
    f32 subWindowValue[4];      /* +0x38: nmlModelSetSubWindowValue */
    u8 unmodeled_48[0x50 - 0x48];
    Vector4 mulColor;           /* +0x50: a direction vector nmlModelSetMulColor sets;
                                 * gifStateDrawCameraQuad scales it by blur into its offset */
} GifStatusBlock;

extern GifStatusBlock gifStatus;

/* The effect modes of gifStateSetMode: the index into gifStateEffectDraws, named after its draw function. */
enum GifEffectMode {
    GIF_EFFECT_RADIAL_BLUR,     /* gifStateDrawRadialBlur (gifStateStartWhiteShake's white effect) */
    GIF_EFFECT_RING,            /* gifStateDrawRing */
    GIF_EFFECT_CAMERA_QUAD,     /* gifStateDrawCameraQuad */
    GIF_EFFECT_JITTER_GRID,     /* gifStateDrawJitterGrid */
    GIF_EFFECT_FADE_QUAD,       /* gifStateDrawFadeQuad */
    GIF_EFFECT_BREAK_SHARDS,    /* gifStateDrawBreakShards (gifStateEnableEffect) */
    GIF_EFFECT_NONE,            /* gifStateDrawNone, an empty function */
    GIF_EFFECT_COUNT
};

struct XglPacket;

/* The draw function of an effect: called with the frame packet. */
typedef void (*GifEffectDraw)(struct XglPacket *packet);

/* The mode hook, the word at +0x4C of sRender (0x645820, xgl_render data; gifStateEffectHook is the one at
 * +0x60): gifStateSetMode sets it, gifStateClearModeHook clears it. */
extern GifEffectDraw gifStateModeHook;

void gifStateClearDepthAndFlip(void);
void gifStateSetDepth(int value);
void gifStateClearModeHook(void);
void nmlModelSetMulColor(Vector4 *color);
struct XenoActor;
void gifStateStartWhiteShake(struct XenoActor *actor);
void gifStateEnableEffect(void);
void gifStateSuppressDrawHook(void);
void nmlModelEnableSubWindow0(void);
void nmlModelUseSubWindow(unsigned int index, int enabled);
void gifStateAddReg(int gifAddress, u64 gifData);
void gifStateSetMode(int mode);
void gifStateSetDecrease(f32 decrease);
void gifStateSetCenter(int centerX, int centerY);
void gifStateSetEffectAlpha(int alphaType, int alpha);
void gifStateSetFbp(int fbp);
void gifStateSetBlur(int blur);
void gifStateBuildBreakShards(GifBreakShard *shards, GifBreakPoint *points);
void gifBreakShardUpdate(GifBreakShard *shard, f32 ratio);
int gifStateEffectReady(void);
void gifStateDisableEffect(void);
void gifStateReset(void);
void gifStateFlush(void);
void gifStateUpdateDrawHook(void);
void gifStateClearScreenAndMessages(void);

#endif /* INCLUDE_MAIN_GIFSTATE_H */
