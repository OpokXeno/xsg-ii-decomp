#include "common.h"
#include "shared.h"
#include "main/tu022.h"
#include "main/xgl_cd.h"

/* Sound driver layer of main/tu022 (KORG IOP driver, bound by soundDriverInit).
 * soundCtrl is the control block (main/tu022.h). func_00138B48(channel, level) sets a channel level;
 * channel 0 is the master. */

/* soundSetDefaults: master volume, default master, voice and slot levels. */
#define SOUND_DEFAULT_MASTER_VOLUME 0x6E
#define SOUND_DEFAULT_MASTER_LEVEL 0x71
#define SOUND_DEFAULT_VOICE_LEVEL 0x64
#define SOUND_DEFAULT_SLOT_LEVEL 0x64

int func_00138B48(int channel, int level);

/* Mutes channels 1 to 3, or restores them to full level. */
void soundSetMuted(int muted)
{
    if (muted != 0) {
        func_00138B48(1, 0);
        func_00138B48(2, 0);
        func_00138B48(3, 0);
        return;
    }
    func_00138B48(1, SOUND_LEVEL_MAX);
    func_00138B48(2, SOUND_LEVEL_MAX);
    func_00138B48(3, SOUND_LEVEL_MAX);
}

int func_001393D0(void);

int func_001A4010(void)
{
    return func_001393D0();
}

void func_00184250(int id, int value);
void func_00184A78(int id, const char *key);

/* Sets the loop flag of the driver voice of slot index (0 or 1), then starts the file named by
 * fileKey on it. The ADX loader passes a key built from the file's start sector and size. */
void soundSlotStartFile(int index, const char *fileKey)
{
    SoundSlot *slot;

    if (index < 2) {
        slot = &soundSlots[index];
        func_00184250(slot->driver_id, 1);
        func_00184A78(slot->driver_id, fileKey);
    }
}

/* ADX loader: builds "<adxPathPrefix><name with / as \>.adx", looks it up on the disc,
 * formats the key "lsn.sectors" over the path and gives it to slot index with soundSlotStartFile. */
#define ADX_PATH_MAX 256

/* The disc folder of the ADX files, and the "%08x.%08x" key format (start
 * sector, then sectors). */
extern const char *adxPathPrefix;
extern const char adxSlotKeyFormat[];
int sprintf(char *destination, const char *format, ...);

void adxSetSlotFile(int index, const char *name)
{
    char path[ADX_PATH_MAX];
    CdFilePosition position;
    const char *source;
    char *destination;
    char character;

    destination = path;
    for (source = adxPathPrefix; *source != 0; source++) {
        *destination++ = *source;
    }
    source = name;
    character = *source++;
    while (character != 0) {
        if (character == '/') {
            character = '\\';
        }
        *destination++ = character;
        character = *source++;
    }
    destination[0] = '.';
    destination[1] = 'a';
    destination[2] = 'd';
    destination[3] = 'x';
    destination[4] = 0;
    xglCdCheckFile(path, &position);
    sprintf(path, adxSlotKeyFormat, position.lbn, (unsigned int) position.stored_bytes >> CD_SECTOR_SHIFT);
    soundSlotStartFile(index, path);
}

void func_00184310(int id, int value);

/* Sets the level of slot index and keeps a copy in ramp.set_level. */
void soundSlotSetLevel(int index, int value)
{
    SoundSlot *slot;

    if (index < 2) {
        slot = &soundSlots[index];
        func_00184310(slot->driver_id, value);
        slot->ramp.set_level = (unsigned char) value;
    }
}

/* Sets the level of slot index's voice, unless the slot already has a level (ramp.set_level). */
void soundSlotSetLevelIfUnset(int index, int value)
{
    SoundSlot *slot;

    if (index < 2) {
        slot = &soundSlots[index];
        if (slot->ramp.set_level != 0) {
            return;
        }
        func_00184310(slot->driver_id, value);
    }
}

void func_00183220(int id);

/* Stops the driver voice of slot index and sets its level to 0. */
void soundSlotStop(int index)
{
    SoundSlot *slot;

    if (index < 2) {
        slot = &soundSlots[index];
        func_00183220(slot->driver_id);
        func_00184310(slot->driver_id, 0);
    }
}

/* Stops, then reopens, both slots. */
int soundSlotsRestart(void)
{
    int index;
    int more;

    index = 0;
    do {
        soundSlotStop(index);
        soundSlotReopen(index);
        index += 1;
        more = index < 2;
    } while (more != 0);
    return more;
}

int func_00183BA8(int id, int value);

/* Applies the slot's current ramp level, through the volume curve, to its driver voice. */
int soundSlotApply(SoundSlot *slot)
{
    int driver_id;
    int value;

    driver_id = slot->driver_id;
    value = seVolumeCurve(slot->ramp.current);
    return func_00183BA8(driver_id, value);
}

/* Volume of slot index. A negative level takes the default level from the control block; slot 0
 * is scaled by the master volume; the level is clamped to SOUND_LEVEL_MAX before it is applied. */
int soundSlotSetVolume(int index, int level, int extra)
{
    SoundSlot *slot;
    int status;

    status = index < 2;
    if (status != 0) {
        slot = &soundCtrl.slots[index];
        if (level < 0) {
            level = soundCtrl.slot_level;
        }
        if (index == 0) {
            level = level * soundCtrl.master_volume / SOUND_LEVEL_MAX;
        }
        status = seVolumeRampSet(&slot->ramp, level <= SOUND_LEVEL_MAX ? level : SOUND_LEVEL_MAX, extra);
        if (status >= 0) {
            status = soundSlotApply(slot);
        }
    }
    return status;
}

int func_001832E0(int id);

/* Returns the driver status of slot index's voice, or -1 for an invalid index. */
int soundSlotStatus(int index)
{
    if (index >= 2) {
        return -1;
    }
    return func_001832E0(soundSlots[index].driver_id);
}

int func_00184220(int id);

/* Returns func_00184220 of slot index's voice, or -1 for an invalid index. */
int func_001A43A8(int index)
{
    if (index >= 2) {
        return -1;
    }
    return func_00184220(soundSlots[index].driver_id);
}

/* The same as soundSlotStartFile. */
void func_001A43E8(int index, const char *fileKey)
{
    soundSlotStartFile(index, fileKey);
}

void func_0013A7B8(void);
void func_00184918(int flag);

void func_001A4400(int flag)
{
    func_0013A7B8();
    func_00184918(flag == 0);
}

void func_0013B488(void);

/* Runs the one-time driver setup when the control block's flag byte is still 0. */
void soundInitIfNeeded(void)
{
    if (soundCtrl.initialized == 0) {
        func_0013B488();
    }
}

void func_0017D150(void);

/* Steps the master volume ramp (channel 0) and the ramps of both slots, applying the levels that
 * changed. */
void soundApplySettings(void)
{
    SoundSlot *slot;
    int status;
    int index;

    status = seVolumeRampStep(&soundCtrl.master_ramp);
    if (status >= 0) {
        func_00138B48(0, status);
    }
    soundInitIfNeeded();
    slot = soundCtrl.slots;
    for (index = 1; index >= 0; index--) {
        if (seVolumeRampStep(&slot->ramp) >= 0) {
            soundSlotApply(slot);
        }
        slot++;
    }
    func_0017D150();
}

/* Writes the default volume and levels and sets channels 1 to 3 to full level. */
void soundSetDefaults(void)
{
    soundCtrl.master_volume = SOUND_DEFAULT_MASTER_VOLUME;
    soundCtrl.master_level = SOUND_DEFAULT_MASTER_LEVEL;
    soundCtrl.voice_level = SOUND_DEFAULT_VOICE_LEVEL;
    soundCtrl.slot_level = SOUND_DEFAULT_SLOT_LEVEL;
    func_00138B48(1, SOUND_LEVEL_MAX);
    func_00138B48(2, SOUND_LEVEL_MAX);
    func_00138B48(3, SOUND_LEVEL_MAX);
}

void func_0017D138(int value);

int soundVSyncCallback(void)
{
    func_0017D138(0);
    ExitHandler();
    return 0;
}

void func_00182CE8(int id);
int func_00182908(int mode, unsigned char *buffer, int buffer_size);
void func_00183CA8(int id, int value);
int func_00183E08(int id, int frames);

/* Reopens slot index: destroys the old voice when there is one, creates a new one from the create
 * parameters, starts it, applies its level and fades it in (50 for slot 0, 70 for slot 1). */
int soundSlotReopen(int index)
{
    if (soundCtrl.records[index + 1].driver_id != 0) {
        func_00182CE8(soundCtrl.records[index + 1].driver_id);
    }
    soundCtrl.records[index + 1].driver_id = func_00182908(2, soundCtrl.records[index + 1].create.buffer,
                                                           soundCtrl.records[index + 1].create.buffer_size);
    func_00183CA8(soundCtrl.records[index + 1].driver_id, 0x1E);
    soundSlotSetVolume(index, -1, 0);
    if (index == 0) {
        return func_00183E08(soundCtrl.slots[0].driver_id, 50);
    }
    return func_00183E08(soundCtrl.slots[1].driver_id, 70);
}

/* Reopens both slots. */
void soundSlotsReopen(void)
{
    int unused_local;

    soundSlotReopen(0);
    soundSlotReopen(1);
    unused_local = 0;
    if (unused_local) {
    }
}

void func_00182460(int value);
int func_0017C890(int value);
int sceGsSyncVCallback(int (*callback)(void));
int func_0017B3E0(void);
int func_00138300(void);
int func_00138988(int value);
int func_00138428(int, int);
int func_00138780(int, int);
int func_001396D8(int kind, int channel, int level);
int printf(const char *format, ...);
extern unsigned char *WorkEnd;

/* The work buffers of the two voices, and the part of each the driver uses. */
#define SOUND_SLOT0_BUFFER_BYTES 0x30200
#define SOUND_SLOT0_BUFFER_USED 0x301E4
#define SOUND_SLOT1_BUFFER_BYTES 0x3DA00
#define SOUND_SLOT1_BUFFER_USED 0x3D9E4
#define SOUND_CHANNELS 16

/* Driver setup called from InitializeSystem: binds the KORG IOP driver ("[EE] Failed to bind KORG
 * IOP Sound Driver."), carves the work buffers from WorkEnd, sets the defaults and reopens the slots. */
void soundDriverInit(void)
{
    unsigned char *work;
    int size;
    int channel;

    func_00182460(0);
    func_0017C890(0);
    sceGsSyncVCallback(soundVSyncCallback);
    func_0017B3E0();
    if (func_00138300() != 0) {
        printf("[EE] Failed to bind KORG IOP Sound Driver.\n");
        return;
    }
    func_00138988(0xA1);
    func_00138428(0, 0);
    func_00138780(0x100, 0);
    for (channel = 0; channel < SOUND_CHANNELS; channel++) {
        func_001396D8(1, channel, SOUND_LEVEL_MAX);
        func_001396D8(4, channel, SOUND_LEVEL_MAX);
    }
    soundSetDefaults();
    /* Carve the two voices' work buffers from WorkEnd; size carries the part of each the driver uses. */
    work = WorkEnd;
    soundCtrl.slots[0].create.buffer_size = (size = SOUND_SLOT0_BUFFER_USED);
    soundCtrl.slots[0].create.buffer = work;
    work += SOUND_SLOT0_BUFFER_BYTES;
    soundCtrl.slots[1].create.buffer = work;
    soundCtrl.slots[1].driver_id = 0;
    size = SOUND_SLOT1_BUFFER_USED;
    work += SOUND_SLOT1_BUFFER_BYTES;
    soundCtrl.initialized = 0;
    soundCtrl.slots[0].driver_id = 0;
    soundCtrl.slots[1].create.buffer_size = size;
    soundSlotsReopen();
    WorkEnd = work;
}
