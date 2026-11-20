#include "common.h"
#include "ov01/sb_vm.h"
#include "ov01/sb_natives.h"
#include "main/fade_ctrl.h"

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

extern const char fadeNativeNameColorfade2[];
extern const char fadeNativeNameColorfade[];
extern const char fadeNativeNameColorSet[];
extern char fadeNativeNameFadeout[];
extern char fadeNativeNameFadein[];

SbNativeMethod fadeCommandTable[6] = {
    { fadeNativeNameColorSet, fadeColorSet },
    { fadeNativeNameFadein, Fadein },
    { fadeNativeNameFadeout, Fadeout },
    { fadeNativeNameColorfade, Colorfade },
    { fadeNativeNameColorfade2, ColorfadeAlpha },
    { 0, 0 },
};
