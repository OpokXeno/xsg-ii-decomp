#include "common.h"
#include "main/tu021.h"
#include "main/tu022.h"

INCLUDE_ASM("asm/main/nonmatchings/tu021", seBankLoadFile);

extern int seBankDirectory;
int seBankLoadFile(int, int, int, int);

/* Loads the bank `name` into `slot`, its files taken from the global directory seBankDirectory. */
int seBankLoad(int slot, int name, int option) {
    return seBankLoadFile(slot, name, seBankDirectory, option);
}

void seBankLoadDefault(int slot, int name) {
    seBankLoad(slot, name, 0);
}

INCLUDE_ASM("asm/main/nonmatchings/tu021", func_001A33F8);

INCLUDE_ASM("asm/main/nonmatchings/tu021", seBgmLoad);

void seBgmLoad(int, int, int);

/* Loads song `name` (three digits, then .mid) with no option. */
void seBgmLoadDefault(int packed, int name) {
    seBgmLoad(packed, name, 0);
}

void func_00139AF0(void);
void func_0013A1B8(int, int);
int func_001A33F8(int, int, int, int);

void func_001A3668(int first, int second, int third, int fourth, int fifth) {
    func_00139AF0();
    func_001A33F8(0, third, fourth, fifth);
    if (second != 0) {
        func_0013A1B8(first, second);
    }
}

INCLUDE_ASM("asm/main/nonmatchings/tu021", func_001A3700);

void func_001394B8(int, int);
void func_00139C20(int, int);
void func_001A3700(void);
void func_001A3900(int, int);
void seVolumeSet(int, int);

/* `packed`: the high 16 bits select the mode, the low 16 bits are the id.
 * `name` is the song name for seBgmLoad; a non-null name also starts playback. */
void func_001A37E0(int packed, int name, int option) {
    int *mode_ref;
    int mode;
    int id;
    int next_id;

    id = packed;
    mode = id >> 16;
    id &= 0xffff;
    next_id = id + 0x100;
    switch (mode) {
    case 2:
    case 0:
        func_00139C20(0, id);
        func_001394B8(1, 0);
        mode_ref = &mode;
        seBgmLoad(next_id, name, option);
        if (name != 0) {
            func_001A3900(id, next_id);
            /* the address taken here makes this test reload mode */
            if (*mode_ref == 0) {
                seVolumeSet(-1, 0);
                return;
            }
        }
        return;

    case 1:
        seBgmLoad(0x10100 + id, name, option);
        if (name != 0) {
            seVolumeSet(0, 0);
            seSetMuted(id, 0);
            seVolumeSet(-1, 16);
            func_001A3700();
        }
        break;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/tu021", func_001A3900);

void func_00139430(int, int);
void func_00139CA8(void);

void func_001A3A18(void) {
    func_00139CA8();
    func_00139430(1, 0);
    func_001394B8(1, 0);
}

void func_0013A618(int, int);

void seSetMuted(int first, int flag) {
    func_0013A618(first, flag);
    soundSetMuted(flag);
    if (flag != 0) {
        func_001394B8(1, 0);
        return;
    }
    func_001A3700();
}

/* soundCtrl (tu022): +4 master volume, +5 default master level, +0x0C master volume ramp. */
#define SOUND_CTRL_MASTER_VOLUME 4
#define SOUND_CTRL_MASTER_LEVEL 5
#define SOUND_CTRL_MASTER_RAMP 0x0C
#define SOUND_LEVEL_MAX 0x7F

extern unsigned char soundCtrl[];
int seVolumeRampSet(void *ramp, int level, int step);
void func_00138B48(int channel, int level);

/* Starts the master volume ramp toward level (negative: the default level), scaled by the master
 * volume and clamped to 0x7F; applies the level at once when the ramp returns one. */
void seVolumeSet(int level, int step)
{
    int status;

    if (level < 0) {
        level = soundCtrl[SOUND_CTRL_MASTER_LEVEL];
    }
    level = level * soundCtrl[SOUND_CTRL_MASTER_VOLUME] / SOUND_LEVEL_MAX;
    status = seVolumeRampSet(&soundCtrl[SOUND_CTRL_MASTER_RAMP], level < 0x80 ? level : SOUND_LEVEL_MAX, step);
    if (status >= 0) {
        func_00138B48(0, status);
    }
}

void seVoiceStart(int, int, int);

/* Starts `voice` at the default volume (-1) and centre pan (0x40). */
void seVoiceStartDefault(int voice) {
    seVoiceStart(voice, -1, 0x40);
}

int func_001A3B38(int kind)
{
    int group;

    switch (kind) {
    case 3:
        group = 2;
        break;
    case 4:
        group = 3;
        break;
    default:
        group = 4;
        break;
    }
    return group;
}

INCLUDE_ASM("asm/main/nonmatchings/tu021", seVoiceStart);

INCLUDE_ASM("asm/main/nonmatchings/tu021", seVoiceSetPanVolume);

/* For `packed` (bits 24..30: kind, bits 0..3: channel, bits 0..6: note), sends an expression
 * control change, a program change and a note off to the kind's channel group. */
INCLUDE_ASM("asm/main/nonmatchings/tu021", func_001A3D88);

INCLUDE_ASM("asm/main/nonmatchings/tu021", func_001A3E10);
