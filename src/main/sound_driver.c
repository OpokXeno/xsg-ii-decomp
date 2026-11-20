#include "common.h"
#include "shared.h"
#include "main/sound_driver.h"
#include "main/xgl_cd.h"
#include "main/control_entry.h"

/* Sound driver layer of main/sound_driver.c (KORG IOP driver, bound by soundDriverInit).
 * soundCtrl is the control block (main/sound_driver.h). korgEvagSetUnitVolume(channel, level) sets a channel level;
 * channel 0 is the master. */

/* soundSetDefaults: master volume, default master, voice and slot levels. */
#define SOUND_DEFAULT_MASTER_VOLUME 0x6E
#define SOUND_DEFAULT_MASTER_LEVEL 0x71
#define SOUND_DEFAULT_VOICE_LEVEL 0x64
#define SOUND_DEFAULT_SLOT_LEVEL 0x64

int korgEvagSetUnitVolume(int channel, int level);

/* Mutes channels 1 to 3, or restores them to full level. */
void soundSetMuted(int muted)
{
    if (muted != 0) {
        korgEvagSetUnitVolume(1, 0);
        korgEvagSetUnitVolume(2, 0);
        korgEvagSetUnitVolume(3, 0);
        return;
    }
    korgEvagSetUnitVolume(1, SOUND_LEVEL_MAX);
    korgEvagSetUnitVolume(2, SOUND_LEVEL_MAX);
    korgEvagSetUnitVolume(3, SOUND_LEVEL_MAX);
}

int korgEvagAllUnitsSoundOff(void);

int soundDriverReset(void)
{
    return korgEvagAllUnitsSoundOff();
}

void ADXT_SetLpFlg(int id, int value);
void ADXT_StartFname(int id, const char *key);

/* Sets the loop flag of the driver voice of slot index (0 or 1), then starts the file named by
 * fileKey on it. The ADX loader passes a key built from the file's start sector and size. */
void soundSlotStartFile(int index, const char *fileKey)
{
    SoundSlot *slot;

    if (index < 2) {
        slot = &soundCtrl.slots[index];
        ADXT_SetLpFlg(slot->driver_id, 1);
        ADXT_StartFname(slot->driver_id, fileKey);
    }
}

/* ADX loader: builds "<adxPathPrefix><name with / as \>.adx", looks it up on the disc,
 * formats the key "lsn.sectors" over the path and gives it to slot index with soundSlotStartFile. */
#define ADX_PATH_MAX 256

/* The disc folder of the ADX files, and the "%08x.%08x" key format (start
 * sector, then sectors). */
extern const char *adxPathPrefix;
extern const char adxSlotKeyFormat[];
const char *adxPathPrefix = "sound\\adx\\";
const char adxSlotKeyFormat[16] = "%08x.%08x";
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
    xglCdGetFileData(path, &position);
    sprintf(path, adxSlotKeyFormat, position.lbn, (unsigned int) position.stored_bytes >> CD_SECTOR_SHIFT);
    soundSlotStartFile(index, path);
}

void ADXT_Pause(int id, int value);

/* Sets the level of slot index and keeps a copy in ramp.set_level. */
void soundSlotSetPause(int index, int value)
{
    SoundSlot *slot;

    if (index < 2) {
        slot = &soundCtrl.slots[index];
        ADXT_Pause(slot->driver_id, value);
        slot->ramp.set_level = (unsigned char) value;
    }
}

/* Sets the level of slot index's voice, unless the slot already has a level (ramp.set_level). */
void soundSlotSetPauseIfNotHeld(int index, int value)
{
    SoundSlot *slot;

    if (index < 2) {
        slot = &soundCtrl.slots[index];
        if (slot->ramp.set_level != 0) {
            return;
        }
        ADXT_Pause(slot->driver_id, value);
    }
}

void ADXT_Stop(int id);

/* Stops the driver voice of slot index and sets its level to 0. */
void soundSlotStop(int index)
{
    SoundSlot *slot;

    if (index < 2) {
        slot = &soundCtrl.slots[index];
        ADXT_Stop(slot->driver_id);
        ADXT_Pause(slot->driver_id, 0);
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

int ADXT_SetOutVol(int id, int value);

/* Applies the slot's current ramp level, through the volume curve, to its driver voice. */
int soundSlotApply(SoundSlot *slot)
{
    int driver_id;
    int value;

    driver_id = slot->driver_id;
    value = seVolumeCurve(slot->ramp.current);
    return ADXT_SetOutVol(driver_id, value);
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

int ADXT_GetStat(int id);

/* Returns the driver status of slot index's voice, or -1 for an invalid index. */
int soundSlotStatus(int index)
{
    if (index >= 2) {
        return -1;
    }
    return ADXT_GetStat(soundCtrl.slots[index].driver_id);
}

int ADXT_GetLpCnt(int id);

/* Returns ADXT_GetLpCnt of slot index's voice, or -1 for an invalid index. */
int soundSlotLoopCountGet(int index)
{
    if (index >= 2) {
        return -1;
    }
    return ADXT_GetLpCnt(soundCtrl.slots[index].driver_id);
}

/* The same as soundSlotStartFile. */
void soundSlotStartKey(int index, const char *fileKey)
{
    soundSlotStartFile(index, fileKey);
}

void korgEvagSetGlobalOutputMode(void);
void ADXT_SetOutputMono(int flag);

void soundStereoSet(int flag)
{
    korgEvagSetGlobalOutputMode();
    ADXT_SetOutputMono(flag == 0);
}

void korgEvagMidiProcBatchFlush(void);

/* Runs the one-time driver setup when the control block's flag byte is still 0. */
void soundInitIfNeeded(void)
{
    if (soundCtrl.initialized == 0) {
        korgEvagMidiProcBatchFlush();
    }
}

void ADXM_ExecMain(void);

/* Steps the master volume ramp (channel 0) and the ramps of both slots, applying the levels that
 * changed. */
void soundApplySettings(void)
{
    SoundSlot *slot;
    int status;
    int index;

    status = seVolumeRampStep(&soundCtrl.master_ramp);
    if (status >= 0) {
        korgEvagSetUnitVolume(0, status);
    }
    soundInitIfNeeded();
    slot = soundCtrl.slots;
    for (index = 1; index >= 0; index--) {
        if (seVolumeRampStep(&slot->ramp) >= 0) {
            soundSlotApply(slot);
        }
        slot++;
    }
    ADXM_ExecMain();
}

/* Writes the default volume and levels and sets channels 1 to 3 to full level. */
void soundSetDefaults(void)
{
    soundCtrl.master_volume = SOUND_DEFAULT_MASTER_VOLUME;
    soundCtrl.master_level = SOUND_DEFAULT_MASTER_LEVEL;
    soundCtrl.voice_level = SOUND_DEFAULT_VOICE_LEVEL;
    soundCtrl.slot_level = SOUND_DEFAULT_SLOT_LEVEL;
    korgEvagSetUnitVolume(1, SOUND_LEVEL_MAX);
    korgEvagSetUnitVolume(2, SOUND_LEVEL_MAX);
    korgEvagSetUnitVolume(3, SOUND_LEVEL_MAX);
}

void ADXM_ExecVint(int value);

int soundVSyncCallback(void)
{
    ADXM_ExecVint(0);
    ExitHandler();
    return 0;
}

void ADXT_Destroy(int id);
int ADXT_Create(int mode, unsigned char *buffer, int buffer_size);
void ADXT_SetSvrFreq(int id, int value);
int ADXT_SetReloadSct(int id, int frames);

/* Reopens slot index: destroys the old voice when there is one, creates a new one from the create
 * parameters, starts it, applies its level and fades it in (50 for slot 0, 70 for slot 1). */
int soundSlotReopen(int index)
{
    if (soundCtrl.records[index + 1].driver_id != 0) {
        ADXT_Destroy(soundCtrl.records[index + 1].driver_id);
    }
    soundCtrl.records[index + 1].driver_id = ADXT_Create(2, soundCtrl.records[index + 1].create.buffer,
                                                           soundCtrl.records[index + 1].create.buffer_size);
    ADXT_SetSvrFreq(soundCtrl.records[index + 1].driver_id, 0x1E);
    soundSlotSetVolume(index, -1, 0);
    if (index == 0) {
        return ADXT_SetReloadSct(soundCtrl.slots[0].driver_id, 50);
    }
    return ADXT_SetReloadSct(soundCtrl.slots[1].driver_id, 70);
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

void ADXPS2_Setup(int value);
int ADXM_Setup(int value);
int sceGsSyncVCallback(int (*callback)(void));
int ADXT_Init(void);
int korgEvagBind(void);
int korgEvagInitSpu2(int value);
int korgEvagInit(int, int);
int korgEvagAllocVagParam(int, int);
int korgEvagSetChannelPriority(int kind, int channel, int level);
int printf(const char *format, ...);

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

    ADXPS2_Setup(0);
    ADXM_Setup(0);
    sceGsSyncVCallback(soundVSyncCallback);
    ADXT_Init();
    if (korgEvagBind() != 0) {
        printf("[EE] Failed to bind KORG IOP Sound Driver.\n");
        return;
    }
    korgEvagInitSpu2(0xA1);
    korgEvagInit(0, 0);
    korgEvagAllocVagParam(0x100, 0);
    for (channel = 0; channel < SOUND_CHANNELS; channel++) {
        korgEvagSetChannelPriority(1, channel, SOUND_LEVEL_MAX);
        korgEvagSetChannelPriority(4, channel, SOUND_LEVEL_MAX);
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

/* Sound control block (main/sound_driver.h): the song number, default volumes and the two slots, all zero
 * until the driver setup. */
SoundControl soundCtrl = { 0 };

/* Interrupt-slot map of the xgl interrupt layer (xgl_dma_handler.h): eleven free slots and slot 11 taken. */
volatile int xglIntcSlotMap[12] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0 };

/* Interrupt-timer state of the driver (xgl_dma_handler.h): timer active flag, start count and accumulated ticks. */
int intrTimerActive = 0;
int intrTimerStart = 0;
int intrTimerAccum = 0;
