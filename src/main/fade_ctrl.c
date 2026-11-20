#include "common.h"
#include "shared.h"
#include "main/xgl_render.h"
#include "main/fade_ctrl.h"
#include "ov01/sb_vm.h"
#include "ov01/sb_natives.h"

/* Fade record. fadeStartIn/fadeStartColorAlpha copy current into start, set target and
 * delta = (target - start) / totalFrames (fadeDeltaVector) and install fadeUpdate as the hook.
 * While active, fadeUpdate passes current to fadeApplyColor every frame and, unless skipAnimate
 * is set, adds delta to current; when elapsed reaches totalFrames it copies target into current,
 * applies it once more, clears active and the hook. fadeSetColor/fadeReset set color directly;
 * fadeStartIn/fadeStartOut fade color between opaque and transparent. */
typedef struct FadeState {
    int active;         /* +0x00 */
    Vector4 start;      /* +0x04 */
    Vector4 current;    /* +0x14 */
    Vector4 target;     /* +0x24 */
    Vector4 color;      /* +0x34 */
    Vector4 delta;      /* +0x44 */
    int elapsed;        /* +0x54 */
    int totalFrames;    /* +0x58 */
    int skipAnimate;    /* +0x5C */
} FadeState;

extern FadeState fadeState;
/* Hook table; fadeUpdate is stored in slot 0. As int[] with (int) fadeUpdate stores, fd.sh shows
 * fadeSetColor 13/15 and fadeStart* 46/53, 46/53, 31/38, 34/41 equal-in-place (store scheduling);
 * fadeReset, fadeClear and fadeUpdate are identical with either type. */
extern void (*fadeUpdateHook[])(void);
void *memset(void *destination, int value, unsigned int count);
void fadeApplyColor(float red, float green, float blue, float alpha);

/* Resets the fade: black, fully opaque, no fade in progress. */
void fadeReset(void)
{
    memset(&fadeState, 0, sizeof(fadeState));
    fadeState.color.x = 0.0f;
    fadeState.color.y = 0.0f;
    fadeState.color.z = 0.0f;
    fadeState.color.w = 255.0f;
    fadeState.current = fadeState.color;
    xglRenderGlobalFadeInit();
    fadeApplyColor(fadeState.color.x, fadeState.color.y, fadeState.color.z, fadeState.color.w);
    fadeUpdateHook[0] = 0;
}

void fadeDeltaVector(Vector4 *dst, const Vector4 *minuend, const Vector4 *subtrahend, int divisor);
extern int fadeSuppressFlags;

/* Per-frame fade hook: applies current, steps it by delta until totalFrames have elapsed, then
 * snaps to target, applies once more and uninstalls itself. */
void fadeUpdate(void)
{
    int elapsed;

    if (fadeState.active) {
        fadeApplyColor(fadeState.current.x, fadeState.current.y, fadeState.current.z, fadeState.current.w);
        if (!fadeState.skipAnimate) {
            fadeState.current.x += fadeState.delta.x;
            fadeState.current.y += fadeState.delta.y;
            fadeState.current.z += fadeState.delta.z;
            fadeState.current.w += fadeState.delta.w;
            elapsed = fadeState.elapsed + 1;
            fadeState.elapsed = elapsed;
            if (elapsed >= fadeState.totalFrames) {
                fadeState.active = 0;
                fadeState.current = fadeState.target;
                fadeApplyColor(fadeState.current.x, fadeState.current.y, fadeState.current.z, fadeState.current.w);
                fadeUpdateHook[0] = 0;
            }
        }
    }
    if (fadeSuppressFlags) {
        xglRenderGlobalFadeSet(0.0f, 0.0f, 0.0f, 0.0f);
        fadeSuppressFlags = 0;
    }
}

/* Sets the fade colour (opaque) directly and installs the update hook. */
void fadeSetColor(float red, float green, float blue)
{
    fadeState.active = 1;
    fadeState.color.x = red;
    fadeState.color.y = green;
    fadeState.color.z = blue;
    fadeState.color.w = 255.0f;
    fadeUpdateHook[0] = fadeUpdate;
}

/* Fades in over frames: from the opaque fade colour to the transparent one. */
void fadeStartIn(int frames)
{
    fadeState.current.w = 255.0f;
    fadeState.current.x = fadeState.color.x;
    fadeState.current.y = fadeState.color.y;
    fadeState.current.z = fadeState.color.z;
    fadeState.active = 1;
    fadeState.start = fadeState.current;
    fadeState.target = fadeState.color;
    fadeState.target.w = 0.0f;
    fadeDeltaVector(&fadeState.delta, &fadeState.target, &fadeState.start, frames);
    fadeState.totalFrames = frames;
    fadeState.elapsed = 0;
    fadeUpdateHook[0] = fadeUpdate;
}

/* Fades out over frames: from the transparent fade colour to the opaque one. */
void fadeStartOut(int frames)
{
    fadeState.current.w = 0.0f;
    fadeState.current.x = fadeState.color.x;
    fadeState.current.y = fadeState.color.y;
    fadeState.current.z = fadeState.color.z;
    fadeState.active = 1;
    fadeState.start = fadeState.current;
    fadeState.target = fadeState.color;
    fadeState.target.w = 255.0f;
    fadeDeltaVector(&fadeState.delta, &fadeState.target, &fadeState.start, frames);
    fadeState.totalFrames = frames;
    fadeState.elapsed = 0;
    fadeUpdateHook[0] = fadeUpdate;
}

/* Fades over frames from the current colour to red/green/blue, keeping the current alpha. */
void fadeStartColor(int frames, float red, float green, float blue)
{
    fadeState.start = fadeState.current;
    fadeState.active = 1;
    fadeState.target.x = red;
    fadeState.target.y = green;
    fadeState.target.z = blue;
    fadeState.target.w = fadeState.current.w;
    fadeDeltaVector(&fadeState.delta, &fadeState.target, &fadeState.start, frames);
    fadeState.totalFrames = frames;
    fadeState.elapsed = 0;
    fadeUpdateHook[0] = fadeUpdate;
}

/* Fades over frames from red/green/blue at the current alpha to red/green/blue/alpha. */
void fadeStartColorAlpha(int frames, float red, float green, float blue, float alpha)
{
    fadeState.current.x = red;
    fadeState.current.y = green;
    fadeState.current.z = blue;
    fadeState.active = 1;
    fadeState.start = fadeState.current;
    fadeState.target.x = red;
    fadeState.target.y = green;
    fadeState.target.z = blue;
    fadeState.target.w = alpha;
    fadeDeltaVector(&fadeState.delta, &fadeState.target, &fadeState.start, frames);
    fadeState.totalFrames = frames;
    fadeState.elapsed = 0;
    fadeUpdateHook[0] = fadeUpdate;
}

/* Ends any fade: a zero-frame start, the update hook cleared, transparent black. */
void fadeClear(void)
{
    fadeStartIn(0);
    fadeUpdateHook[0] = 0;
    xglRenderGlobalFadeSet(0.0f, 0.0f, 0.0f, 0.0f);
}

/* dst = minuend - subtrahend, component-wise. */
void fadeSubVector(Vector4 *dst, const Vector4 *minuend, const Vector4 *subtrahend)
{
    dst->x = minuend->x - subtrahend->x;
    dst->y = minuend->y - subtrahend->y;
    dst->z = minuend->z - subtrahend->z;
    dst->w = minuend->w - subtrahend->w;
}

/* dst = (minuend - subtrahend) / divisor; a divisor of 0 leaves the difference. */
void fadeDeltaVector(Vector4 *dst, const Vector4 *minuend, const Vector4 *subtrahend, int divisor)
{
    float scale;

    fadeSubVector(dst, minuend, subtrahend);
    if (divisor != 0) {
        scale = (float) divisor;
        dst->x = dst->x / scale;
        dst->y = dst->y / scale;
        dst->z = dst->z / scale;
        dst->w = dst->w / scale;
    }
}

/* Sends a colour given in 0..255 units to xglRenderGlobalFadeSet as 0..1 floats, unless fadeSuppressFlags is set. */
void fadeApplyColor(float red, float green, float blue, float alpha)
{
    if (fadeSuppressFlags == 0) {
        xglRenderGlobalFadeSet(red / 255.0f, green / 255.0f, blue / 255.0f, alpha / 255.0f);
    }
}

/* Fade record (.data), zero until the first fade starts. */
FadeState fadeState = { 0 };

/* The short names of fadeCommandTable (.sdata 0x69DC90-0x69DCA0, address order). */
int fadeSuppressFlags = 0;
char fadeNativeNameFadeout[8] = "Fadeout";
char fadeNativeNameFadein[8] = "Fadein";

/* fadeColorSet: sets the fade colour from three script arguments (fadeSetColor). */
int fadeColorSet(SbThread *call) {
    float color[3];
    int i;

    if (VM_PLUGIN_CNT_GET(call) != 3) {
        return 0;
    }
    for (i = 0; i < 3; i++) {
        color[i] = (float) vmArgGet(call, i + 1);
    }
    fadeSetColor(color[0], color[1], color[2]);
    return 1;
}

/* Fadein: the overlay alpha fades from 255 to 0 over the frame count (fadeStartIn). */
int Fadein(SbThread *call) {
    int frames;

    if (VM_PLUGIN_CNT_GET(call) != 1) {
        return 0;
    }
    frames = vmArgGet(call, 1);
    fadeStartIn(frames);
    return 1;
}

/* Fadeout: the overlay alpha fades from 0 to 255 over the frame count (fadeStartOut). */
int Fadeout(SbThread *call) {
    int frames;

    if (VM_PLUGIN_CNT_GET(call) != 1) {
        return 0;
    }
    frames = vmArgGet(call, 1);
    fadeStartOut(frames);
    return 1;
}

/* Colorfade: fades the colour to red, green, blue over the frame count (fadeStartColor). */
int Colorfade(SbThread *call) {
    float red;
    float green;
    float blue;
    int frames;

    if (VM_PLUGIN_CNT_GET(call) != 4) {
        return 0;
    }
    red = (float) vmArgGet(call, 1);
    green = (float) vmArgGet(call, 2);
    blue = (float) vmArgGet(call, 3);
    frames = vmArgGet(call, 4);
    fadeStartColor(frames, red, green, blue);
    return 1;
}

/* Five-argument fade: colour and alpha over the frame count (fadeStartColorAlpha). */
int ColorfadeAlpha(SbThread *call) {
    float red;
    float green;
    float blue;
    float alpha;
    int frames;

    if (VM_PLUGIN_CNT_GET(call) != 5) {
        return 0;
    }
    red = (float) vmArgGet(call, 1);
    green = (float) vmArgGet(call, 2);
    blue = (float) vmArgGet(call, 3);
    alpha = (float) vmArgGet(call, 4);
    frames = vmArgGet(call, 5);
    fadeStartColorAlpha(frames, red, green, blue, alpha);
    return 1;
}

extern SbNativeMethod fadeCommandTable[];

/* Registers the fade script commands (command table fadeCommandTable, module name "Samplefade"). */
void fadeRegisterCommands(void) {
    sbNativeOwnerAdd("Samplefade", fadeCommandTable);
}

/* The long names of fadeCommandTable (.rodata 0x68D270-0x68D2A0, address order). */
const char fadeNativeNameColorfade2[16] = "Colorfade2";
const char fadeNativeNameColorfade[16] = "Colorfade";
const char fadeNativeNameColorSet[16] = "ColorSet";

SbNativeMethod fadeCommandTable[6] = {
    { fadeNativeNameColorSet, fadeColorSet },
    { fadeNativeNameFadein, Fadein },
    { fadeNativeNameFadeout, Fadeout },
    { fadeNativeNameColorfade, Colorfade },
    { fadeNativeNameColorfade2, ColorfadeAlpha },
    { 0, 0 },
};
