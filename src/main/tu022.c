#include "common.h"
#include "main/tu022.h"

/* Sound driver layer of main/tu022 (KORG IOP driver, bound by soundDriverInit).
 * soundCtrl is the control block; soundSlots holds two sound slots that overlap the
 * records after it. func_00138B48(channel, level) sets a channel level; channel 0 is the master. */

void func_00138B48(int channel, int level);

/* Mutes channels 1 to 3, or restores them to full level 0x7F. */
void soundSetMuted(int muted)
{
    if (muted != 0) {
        func_00138B48(1, 0);
        func_00138B48(2, 0);
        func_00138B48(3, 0);
        return;
    }
    func_00138B48(1, 0x7F);
    func_00138B48(2, 0x7F);
    func_00138B48(3, 0x7F);
}

int func_001393D0(void);

int func_001A4010(void)
{
    return func_001393D0();
}

/* One 16-byte sound slot. Driver calls take driver_id; the create call (0x182908) takes the words
 * at create_params+0 and create_params+4. In the control block soundCtrl, create_params[0] is the
 * master volume (0x6E by default) and create_params[3] the default slot level (0x64). */
typedef struct SoundSlot {
    int driver_id;
    unsigned char create_params[8];
    unsigned char state[4];           /* state[1]: last level set; state[3]: selects the value of soundSlotApply */
} SoundSlot;

extern SoundSlot soundSlots[];
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

/* ADX loader: builds "<adxPathPrefix><name with / as \>.adx", looks it up on the disc
 * (xglCdCheckFile fills a file record: lsn at +0, size at +4), formats the key "lsn.sectors" over the
 * path and gives it to slot index with soundSlotStartFile. */
#define ADX_PATH_MAX 256

/* The disc folder of the ADX files, and the "%08x.%08x" key format (start
 * sector, then sectors). */
extern const char *adxPathPrefix;
extern const char adxSlotKeyFormat[];
int xglCdCheckFile(const char *path, void *record);
int func_002AA5D8(char *destination, const char *format, ...);

void adxSetSlotFile(int index, const char *name)
{
    char path[ADX_PATH_MAX];
    unsigned int record[0xC];
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
    xglCdCheckFile(path, record);
    func_002AA5D8(path, adxSlotKeyFormat, record[0], record[1] >> 11);
    soundSlotStartFile(index, path);
}

void func_00184310(int id, int value);

/* Sets the level of slot index and keeps a copy in state[1]. */
void soundSlotSetLevel(int index, int value)
{
    SoundSlot *slot;

    if (index < 2) {
        slot = &soundSlots[index];
        func_00184310(slot->driver_id, value);
        slot->state[1] = (unsigned char) value;
    }
}

/* Sets the level of slot index's voice, unless the slot already has a level (state[1]). */
void func_001A41C8(int index, int value)
{
    SoundSlot *slot;

    if (index < 2) {
        slot = &soundSlots[index];
        if (slot->state[1] != 0) {
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

int soundSlotReopen(int index);

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
int seVolumeCurve(unsigned char selector);

/* Applies the value that state[3] selects to the slot's driver voice. */
int soundSlotApply(SoundSlot *slot)
{
    int driver_id;
    int value;

    driver_id = slot->driver_id;
    value = seVolumeCurve(slot->state[3]);
    return func_00183BA8(driver_id, value);
}

int seVolumeRampSet(void *, int, int);

/* Volume of slot index. A negative level takes the default level from the control block; slot 0
 * is scaled by the master volume; the level is clamped to 0x7F before it is applied. */
int soundSlotSetVolume(int index, int level, int extra)
{
    SoundSlot *slot;
    int status;

    status = index < 2;
    if (status != 0) {
        slot = &soundSlots[index];
        if (level < 0) {
            level = soundSlots[-1].create_params[3];
        }
        if (index == 0) {
            level = level * soundSlots[-1].create_params[0] / 127;
        }
        status = seVolumeRampSet(slot->state, level < 0x80 ? level : 0x7F, extra);
        if (status >= 0) {
            status = soundSlotApply(slot);
        }
    }
    return status;
}

/* Returns the driver status of slot index's voice, or -1 for an invalid index. */
int func_001832E0(int id);

int soundSlotStatus(int index)
{
    if (index >= 2) {
        return -1;
    }
    return func_001832E0(soundSlots[index].driver_id);
}

int func_00184220(int id);

int func_001A43A8(int index)
{
    if (index >= 2) {
        return -1;
    }
    return func_00184220(soundSlots[index].driver_id);
}

void soundSlotStartFile(int index, const char *fileKey);

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

extern unsigned char soundCtrl[];
void func_0013B488(void);

/* Runs the one-time driver setup when the control block's flag byte is still 0. */
void soundInitIfNeeded(void)
{
    if (soundCtrl[0] == 0) {
        func_0013B488();
    }
}

extern unsigned char D_00644F24[];
void func_0017D150(void);
int seVolumeRampStep(void *);

/* Applies the settings: master volume on channel 0 from the control block state, then both slots. */
void soundApplySettings(void)
{
    unsigned char *cursor;
    SoundSlot *slot;
    int status;
    int index;

    cursor = D_00644F24;
    status = seVolumeRampStep(cursor);
    if (status >= 0) {
        func_00138B48(0, status);
    }
    soundInitIfNeeded();
    slot = (SoundSlot *) (cursor + 4);
    for (index = 1; index >= 0; index--) {
        if (seVolumeRampStep(slot->state) >= 0) {
            soundSlotApply(slot);
        }
        slot++;
    }
    func_0017D150();
}

/* Writes the default control values (master 0x6E, default level 0x64) and sets channels 1 to 3 to 0x7F. */
INCLUDE_ASM("asm/main/nonmatchings/tu022", soundSetDefaults);

void func_0017D138(int value);

int soundVSyncCallback(void)
{
    func_0017D138(0);
    /* sync, ei: orders the driver update and re-enables interrupts before the VSync callback returns */
    __asm__ volatile("sync\n\tei" ::: "memory");
    return 0;
}

/* Reopens slot index: destroys the old voice when there is one, creates a new one from the create
 * parameters, starts it, applies its level and fades it in (50 for slot 0, 70 for slot 1). */
INCLUDE_ASM("asm/main/nonmatchings/tu022", soundSlotReopen);

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

/* Driver setup called from InitializeSystem: binds the KORG IOP driver ("[EE] Failed to bind KORG
 * IOP Sound Driver."), carves the work buffers from WorkEnd, sets the defaults and reopens the slots. */
INCLUDE_ASM("asm/main/nonmatchings/tu022", soundDriverInit);
