#include "common.h"

typedef struct FadeNativeEntry {
    const char *name;
    int (*function)(void *call);
} FadeNativeEntry;

int func_A9F0A8(void *call);
int func_A9F0C0(void *call, int index);
void fadeSetColor(float red, float green, float blue);

/* ColorSet: sets the fade colour from three script arguments (fadeSetColor). */
int ColorSet(void *call) {
    float color[3];
    int i;

    if (func_A9F0A8(call) != 3) {
        return 0;
    }
    for (i = 0; i < 3; i++) {
        color[i] = (float) func_A9F0C0(call, i + 1);
    }
    fadeSetColor(color[0], color[1], color[2]);
    return 1;
}

void fadeStartIn(int frames);

/* Fadein: the overlay alpha fades from 255 to 0 over the frame count (fadeStartIn). */
int Fadein(void *call) {
    int frames;

    if (func_A9F0A8(call) != 1) {
        return 0;
    }
    frames = func_A9F0C0(call, 1);
    fadeStartIn(frames);
    return 1;
}

void fadeStartOut(int frames);

/* Fadeout: the overlay alpha fades from 0 to 255 over the frame count (fadeStartOut). */
int Fadeout(void *call) {
    int frames;

    if (func_A9F0A8(call) != 1) {
        return 0;
    }
    frames = func_A9F0C0(call, 1);
    fadeStartOut(frames);
    return 1;
}

void fadeStartColor(int frames, float red, float green, float blue);

/* Colorfade: fades the colour to red, green, blue over the frame count (fadeStartColor). */
int Colorfade(void *call) {
    float red;
    float green;
    float blue;
    int frames;

    if (func_A9F0A8(call) != 4) {
        return 0;
    }
    red = (float) func_A9F0C0(call, 1);
    green = (float) func_A9F0C0(call, 2);
    blue = (float) func_A9F0C0(call, 3);
    frames = func_A9F0C0(call, 4);
    fadeStartColor(frames, red, green, blue);
    return 1;
}

void fadeStartColorAlpha(int frames, float red, float green, float blue, float alpha);

/* Five-argument fade: colour and alpha over the frame count (fadeStartColorAlpha). */
int ColorfadeAlpha(void *call) {
    float red;
    float green;
    float blue;
    float alpha;
    int frames;

    if (func_A9F0A8(call) != 5) {
        return 0;
    }
    red = (float) func_A9F0C0(call, 1);
    green = (float) func_A9F0C0(call, 2);
    blue = (float) func_A9F0C0(call, 3);
    alpha = (float) func_A9F0C0(call, 4);
    frames = func_A9F0C0(call, 5);
    fadeStartColorAlpha(frames, red, green, blue, alpha);
    return 1;
}

void func_A9CDB0(void *name, void *table);
extern FadeNativeEntry fadeCommandTable[];

/* Registers the fade script commands (command table fadeCommandTable, module name "Samplefade"). */
void fadeRegisterCommands(void) {
    func_A9CDB0("Samplefade", fadeCommandTable);
}

extern const char D_0068D270[];
extern const char D_0068D280[];
extern const char D_0068D290[];
extern char D_0069DC90[];
extern char D_0069DC98[];

FadeNativeEntry fadeCommandTable[6] = {
    { D_0068D290, ColorSet },
    { D_0069DC98, Fadein },
    { D_0069DC90, Fadeout },
    { D_0068D280, Colorfade },
    { D_0068D270, ColorfadeAlpha },
    { 0, 0 },
};
