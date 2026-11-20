#include "common.h"
#include "ov01/sb_vm.h"
#include "ov01/sb_natives.h"
#include "main/se_bank_load.h"
#include "main/res_get_path.h"
#include "main/runtime_se.h"

#include "main/xgl_2.h"
extern unsigned char seDefaultVolume[];

/* Native "se" (its name pointer is at 0x3e6f00).  Argument 1 is the voice; argument 2 the
   volume, read from seDefaultVolume (the byte seVol writes) when absent; argument 3 the pan, 0x40 when
   absent.  A volume above 0 calls seVoiceStart with the same three values sePos uses;
   otherwise seVoiceStop is called with the voice. */
int seSetVolumePan(SbThread *call) {
    int voice;
    int volume;
    int pan;

    voice = vmArgGet(call, 1);
    if (vmArgTagIsZero(call, 2) != 0) {
        volume = seDefaultVolume[0];
    } else {
        volume = vmArgGet(call, 2);
    }
    if (vmArgTagIsZero(call, 3) == 0) {
        pan = vmArgGet(call, 3);
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

int seVol(SbThread *call) {
    seDefaultVolume[0] = (unsigned char) vmArgGet(call, 1);
    return 1;
}

int sePos(SbThread *call) {
    float position[3];
    int voice;
    unsigned int volumePan;

    voice = vmArgGet(call, 1);
    position[0] = I2F(vmArgGet(call, 2)) / 100.0f;
    position[1] = I2F(vmArgGet(call, 3)) / 100.0f;
    position[2] = I2F(vmArgGet(call, 4)) / 100.0f;
    volumePan = seVoicePanVolumeFromPosition(position);
    if (volumePan != 0) {
        seVoiceStart(voice, volumePan >> 8, volumePan & 0xFF);
    }
    return 1;
}

/* Native "bgmse": argument 1 is the voice; argument 2 the volume, -1 when absent; argument 3
   the pan, 0x40 when absent.  The three values go to seVoiceRecordSet. */
int bgmSe(SbThread *call) {
    int voice;
    int volume;
    int pan;

    voice = vmArgGet(call, 1);
    if (vmArgTagIsZero(call, 2) == 0) {
        volume = vmArgGet(call, 2);
    } else {
        volume = -1;
    }
    if (vmArgTagIsZero(call, 3) == 0) {
        pan = vmArgGet(call, 3);
    } else {
        pan = 0x40;
    }
    seVoiceRecordSet(voice, volume, pan);
    return 1;
}

int bgmSePos(SbThread *call) {
    float position[3];
    int voice;

    voice = vmArgGet(call, 1);
    position[0] = I2F(vmArgGet(call, 2)) / 100.0f;
    position[1] = I2F(vmArgGet(call, 3)) / 100.0f;
    position[2] = I2F(vmArgGet(call, 4)) / 100.0f;
    seVoiceRecordAddAt(voice, position);
    return 1;
}

/* Native "bgmFade": argument 1 is the music volume (below 0: the default); argument 2 the
   fade length seVolumeSet hands to seVolumeRampSet, 0 (at once) when absent. */
int bgmFade(SbThread *call) {
    int volume;
    int fade;

    volume = vmArgGet(call, 1);
    if (vmArgTagIsZero(call, 2) == 0) {
        fade = vmArgGet(call, 2);
    } else {
        fade = 0;
    }
    seVolumeSet(volume, fade);
    return 1;
}

int nextBgm(SbThread *call) {
    bgmNameListIndex = (unsigned char) vmArgGet(call, 1);
    return 1;
}

/* Native "bgm": argument 1 names the track; the name is resolved through vmKeyedEntryGet and
   passed to seBgmNameSet, which starts it (an empty name stops the music). */
int bgmSetTrack(SbThread *call) {
    int trackRef;
    char *trackName;

    trackRef = vmArgGet(call, 1);
    trackName = vmKeyedEntryGet(call, trackRef);
    seBgmNameSet(trackName);
    return 1;
}
