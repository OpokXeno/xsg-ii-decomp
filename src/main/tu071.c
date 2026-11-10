#include "common.h"

int func_A9F0C0(int call, int index);
int func_A9F1C0(int call, int index);
float I2F(int value);
/* res_get_path.c types this ResRecord *; it reads only rec->u.position at offset 0. */
unsigned int seVoicePanVolumeFromPosition(float *position);
void seVoiceStart(int voice, int volume, int pan);
void seVoiceStop(int voice);
extern unsigned char seDefaultVolume[];

/* Native "se" (its name pointer is at 0x3e6f00).  Argument 1 is the voice; argument 2 the
   volume, read from seDefaultVolume (the byte seVol writes) when absent; argument 3 the pan, 0x40 when
   absent.  A volume above 0 calls seVoiceStart with the same three values sePos uses;
   otherwise seVoiceStop is called with the voice. */
int seSetVolumePan(int call) {
    int voice;
    int volume;
    int pan;

    voice = func_A9F0C0(call, 1);
    if (func_A9F1C0(call, 2) != 0) {
        volume = seDefaultVolume[0];
    } else {
        volume = func_A9F0C0(call, 2);
    }
    if (func_A9F1C0(call, 3) == 0) {
        pan = func_A9F0C0(call, 3);
    } else {
        pan = 0x40;
    }
    if (volume > 0) {
        seVoiceStart(voice, volume, pan);
    } else {
        seVoiceStop(voice);
    }
    return 1;
}

int seVol(int call) {
    seDefaultVolume[0] = (unsigned char) func_A9F0C0(call, 1);
    return 1;
}

int sePos(int call) {
    float position[3];
    int voice;
    unsigned int volumePan;

    voice = func_A9F0C0(call, 1);
    position[0] = I2F(func_A9F0C0(call, 2)) / 100.0f;
    position[1] = I2F(func_A9F0C0(call, 3)) / 100.0f;
    position[2] = I2F(func_A9F0C0(call, 4)) / 100.0f;
    volumePan = seVoicePanVolumeFromPosition(position);
    if (volumePan != 0) {
        seVoiceStart(voice, volumePan >> 8, volumePan & 0xFF);
    }
    return 1;
}

void seVoiceRecordSet(unsigned int voice, unsigned int volume, unsigned int pan);

/* Native "bgmse": argument 1 is the voice; argument 2 the volume, -1 when absent; argument 3
   the pan, 0x40 when absent.  The three values go to seVoiceRecordSet. */
int bgmSe(int call) {
    int voice;
    int volume;
    int pan;

    voice = func_A9F0C0(call, 1);
    if (func_A9F1C0(call, 2) == 0) {
        volume = func_A9F0C0(call, 2);
    } else {
        volume = -1;
    }
    if (func_A9F1C0(call, 3) == 0) {
        pan = func_A9F0C0(call, 3);
    } else {
        pan = 0x40;
    }
    seVoiceRecordSet(voice, volume, pan);
    return 1;
}

void seVoiceRecordAddAt(int voice, float *position);

int bgmSePos(int call) {
    float position[3];
    int voice;

    voice = func_A9F0C0(call, 1);
    position[0] = I2F(func_A9F0C0(call, 2)) / 100.0f;
    position[1] = I2F(func_A9F0C0(call, 3)) / 100.0f;
    position[2] = I2F(func_A9F0C0(call, 4)) / 100.0f;
    seVoiceRecordAddAt(voice, position);
    return 1;
}

void seVolumeSet(int value, int flags);

/* Native "bgmFade": argument 1 is the music volume (below 0: the default); argument 2 the
   fade length seVolumeSet hands to seVolumeRampSet, 0 (at once) when absent. */
int bgmFade(int call) {
    int volume;
    int fade;

    volume = func_A9F0C0(call, 1);
    if (func_A9F1C0(call, 2) == 0) {
        fade = func_A9F0C0(call, 2);
    } else {
        fade = 0;
    }
    seVolumeSet(volume, fade);
    return 1;
}

extern unsigned char D_0069CFD0;

int nextBgm(int call) {
    D_0069CFD0 = (unsigned char) func_A9F0C0(call, 1);
    return 1;
}

void seBgmNameSet(signed char *name);
char *func_A9F238(int call, int index);

/* Native "bgm": argument 1 names the track; the name is resolved through func_A9F238 and
   passed to seBgmNameSet, which starts it (an empty name stops the music). */
int bgmSetTrack(int call) {
    int trackRef;
    signed char *trackName;

    trackRef = func_A9F0C0(call, 1);
    trackName = (signed char *) func_A9F238(call, trackRef);
    seBgmNameSet(trackName);
    return 1;
}
