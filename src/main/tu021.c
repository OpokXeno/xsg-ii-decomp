#include "common.h"
#include "main/tu021.h"
#include "main/tu022.h"
#include "main/xgl_2.h"

#define SE_PATH_MAX 256
#define SE_BANK_SLOT_COUNT 24

/* The 0x30-byte file-position record xglCdCheckFile fills (CdFilePosition in xgl_cd.c; the
 * integrator reconciles). The loaders here pass the byte at +0x27 on to the driver with the lbn;
 * its meaning is not established. */
typedef struct SeFileRecord {
    unsigned int lbn;
    unsigned char unmodeled_04[0x23];
    unsigned char driver_flag;
    int descriptor;
    int size_bytes;
} SeFileRecord;

int xglCdCheckFile(const char *path, SeFileRecord *record);
void func_00138D78(int slot);
int func_00139060(int slot, unsigned int lbn, int flag);
void func_001391D8(int slot, unsigned int lbn, int flag);
int func_0013A338(int slot, unsigned int lbn, int flag, int option);

/* Loads the bank `name` of `directory` into `slot`: clears the slot, gives the driver the position
 * of <directory><name>.vsf (func_0013A338 when `option` is set, else func_00139060) and then of
 * the .prj file. Returns the size of the .vsf file, 0 for a slot out of range or no name, or the
 * negative result of the failing step. */
int seBankLoadFile(int slot, const char *name, const char *directory, int option)
{
    SeFileRecord record;
    char path[SE_PATH_MAX];
    const char *source;
    char *destination;
    int result;
    int size;

    if (slot >= SE_BANK_SLOT_COUNT) {
        return 0;
    }
    func_00138D78(slot);
    if (name == 0) {
        return 0;
    }
    source = directory;
    destination = path;
    while (*source != 0) {
        *destination = *source;
        destination++;
        source++;
    }
    source = name;
    while (*source != 0) {
        *destination = *source;
        destination++;
        source++;
    }
    destination[0] = '.';
    destination[1] = 'v';
    destination[2] = 's';
    destination[3] = 'f';
    destination[4] = 0;
    result = xglCdCheckFile(path, &record);
    if (result < 0) {
        return result;
    }
    size = record.size_bytes;
    if (option != 0) {
        result = func_0013A338(slot, record.lbn, record.driver_flag, option);
    } else {
        result = func_00139060(slot, record.lbn, record.driver_flag);
    }
    if (result < 0) {
        return result;
    }
    destination[1] = 'p';
    destination[2] = 'r';
    destination[3] = 'j';
    xglCdCheckFile(path, &record);
    func_001391D8(slot, record.lbn, record.driver_flag);
    return size;
}

extern const char *seBankDirectory;

/* Loads the bank `name` into `slot`, its files taken from the global directory seBankDirectory. */
int seBankLoad(int slot, const char *name, int option) {
    return seBankLoadFile(slot, name, seBankDirectory, option);
}

void seBankLoadDefault(int slot, const char *name) {
    seBankLoad(slot, name, 0);
}

int func_0013A098(int slot, int vsf);
int func_0013A128(int slot, int prj);
int func_0013A578(int slot, int vsf, int option);

/* The memory form of seBankLoadFile: `vsf` and `prj` are the two bank files already in memory. */
int seBankLoadMemory(int slot, int vsf, int prj, int option)
{
    int result;

    if (slot >= SE_BANK_SLOT_COUNT) {
        return 0;
    }
    func_00138D78(slot);
    if (vsf == 0) {
        return 0;
    }
    if (prj == 0) {
        return 0;
    }
    if (option != 0) {
        result = func_0013A578(slot, vsf, option);
    } else {
        result = func_0013A098(slot, vsf);
    }
    if (result < 0) {
        return result;
    }
    return func_0013A128(slot, prj);
}

/* soundCtrl (tu022): +3 current song number, +4 master volume, +5 default master level,
 * +6 default voice level, +0x0C master volume ramp. */
#define SOUND_CTRL_SONG 3
#define SOUND_CTRL_MASTER_VOLUME 4
#define SOUND_CTRL_MASTER_LEVEL 5
#define SOUND_CTRL_VOICE_LEVEL 6
#define SOUND_CTRL_MASTER_RAMP 0x0C
#define SOUND_LEVEL_MAX 0x7F

extern unsigned char soundCtrl[];
extern const char *seMidiDirectory;
void func_00139AF0(void);
int func_00139978(int packed, unsigned int lbn, int flag);

/* Loads the song `name` (three digits) from seMidiDirectory: its bank into slot 0, then, for a
 * `packed` id below 0x10000, the .mid file, and records the song number (0xFF when the name does
 * not start with three digits). With no name it returns what the bank load returned. */
int seBgmLoad(int packed, const char *name, int option)
{
    SeFileRecord record;
    char path[SE_PATH_MAX];
    const char *source;
    char *destination;
    int result;
    int high;

    high = packed > 0xFFFF;
    if (!high) {
        func_00139AF0();
    }
    result = seBankLoadFile(0, name, seMidiDirectory, option);
    if (name == 0) {
        return result;
    }
    if (high) {
        return 0;
    }
    source = seMidiDirectory;
    destination = path;
    while (*source != 0) {
        *destination = *source;
        destination++;
        source++;
    }
    source = name;
    if (*source != 0) {
        do {
            *destination = *source;
            destination++;
            source++;
        } while (*source != 0);
    }
    destination[0] = '.';
    destination[1] = 'm';
    destination[2] = 'i';
    destination[3] = 'd';
    destination[4] = 0;
    xglCdCheckFile(path, &record);
    result = func_00139978(packed, record.lbn, record.driver_flag);
    soundCtrl[SOUND_CTRL_SONG] = -1;
    if ((name[0] & 0xF0) == '0' && (name[1] & 0xF0) == '0' && (name[2] & 0xF0) == '0') {
        soundCtrl[SOUND_CTRL_SONG] = (name[0] & 0xF) * 100 + (name[1] & 0xF) * 10 + (name[2] & 0xF);
    }
    return result;
}

/* Loads song `name` (three digits, then .mid) with no option. */
void seBgmLoadDefault(int packed, const char *name) {
    seBgmLoad(packed, name, 0);
}

void func_0013A1B8(int, int);

void func_001A3668(int first, int second, int third, int fourth, int fifth) {
    func_00139AF0();
    seBankLoadMemory(0, third, fourth, fifth);
    if (second != 0) {
        func_0013A1B8(first, second);
    }
}

typedef struct SeSongLevel {
    unsigned char song;
    unsigned char level;
} SeSongLevel;

void func_001394B8(int, int);

/* Sets the level of channel 1 to the level the table gives for the current song; songs that are
 * not listed keep the level they have. */
void seBgmSetSongLevel(void)
{
    SeSongLevel table[] = {
        { 203, 0 },   { 202, 80 },  { 201, 80 },  { 111, 90 },  { 110, 90 },  { 109, 100 }, { 108, 90 },
        { 107, 100 }, { 106, 70 },  { 105, 100 }, { 104, 100 }, { 103, 100 }, { 102, 115 }, { 101, 100 },
        { 17, 100 },  { 16, 100 },  { 15, 100 },  { 14, 100 },  { 13, 100 },  { 12, 100 },  { 11, 110 },
        { 10, 100 },  { 9, 100 },   { 8, 80 },    { 7, 70 },    { 6, 70 },    { 5, 90 },    { 4, 100 },
        { 3, 80 },    { 2, 105 },   { 1, 90 },    { 0, 0 },
    };
    SeSongLevel *entry;

    for (entry = table; entry->song != 0; entry++) {
        if (soundCtrl[SOUND_CTRL_SONG] == entry->song) {
            func_001394B8(1, entry->level);
            break;
        }
    }
}

void func_00139C20(int, int);
void func_001A3900(int, int);
void seVolumeSet(int, int);

/* `packed`: the high 16 bits select the mode, the low 16 bits are the id.
 * `name` is the song name for seBgmLoad; a non-null name also starts playback. */
void func_001A37E0(int packed, const char *name, int option) {
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
            seBgmSetSongLevel();
        }
        break;
    }
}

/* Packed driver messages: a MIDI status byte (with the channel) in bits 8..15, the first data
 * byte in bits 16..23, the second in bits 24..30. */
#define SE_MSG_NOTE_OFF 0x8000
#define SE_MSG_NOTE_ON 0x9000
#define SE_MSG_CONTROL_CHANGE 0xB000
#define SE_MSG_PROGRAM_CHANGE 0xC000
#define SE_MSG_PITCH_BEND 0xE000
#define SE_MSG_CHANNEL(channel) ((channel) << 8)
#define SE_MSG_DATA1(value) ((value) << 16)
#define SE_MSG_DATA2(value) ((value) << 24)
#define SE_MSG_CONTROL(controller, value) (SE_MSG_CONTROL_CHANGE | SE_MSG_DATA1(controller) | SE_MSG_DATA2(value))
#define SE_CONTROLLER_BANK 0x00
#define SE_CONTROLLER_MODULATION 0x01
#define SE_CONTROLLER_DATA_ENTRY 0x06
#define SE_CONTROLLER_VOLUME 0x07
#define SE_CONTROLLER_PAN 0x0A
#define SE_CONTROLLER_EXPRESSION 0x0B
#define SE_CONTROLLER_REVERB 0x5B
#define SE_PAN_CENTRE 0x40
#define SE_CHANNEL_COUNT 16
/* Song 6 gets its own setup of MIDI channel 1 before it starts. */
#define SE_SONG_WITH_CHANNEL_SETUP 6
#define SE_SETUP_CHANNEL SE_MSG_CHANNEL(1)

void func_001388E8(int, int message, int);
void func_00139B70(int, int, int, int);
/* The result is not used here; an int return is what the loop's register choice shows. */
int func_00139D08(int, int channel);

void func_001A3900(int id, int next_id)
{
    int i;

    for (i = 0; i < SE_CHANNEL_COUNT; i++) {
        func_00139D08(0, i);
    }
    if (soundCtrl[SOUND_CTRL_SONG] == SE_SONG_WITH_CHANNEL_SETUP) {
        func_001388E8(0, SE_SETUP_CHANNEL | SE_MSG_PITCH_BEND | SE_MSG_DATA2(0x3F), 0);
        func_001388E8(0, SE_SETUP_CHANNEL | SE_MSG_CONTROL(SE_CONTROLLER_MODULATION, 0), 0);
        func_001388E8(0, SE_SETUP_CHANNEL | SE_MSG_CONTROL(SE_CONTROLLER_DATA_ENTRY, 0), 0);
        func_001388E8(0, SE_SETUP_CHANNEL | SE_MSG_CONTROL(SE_CONTROLLER_VOLUME, 0x41), 0);
        func_001388E8(0, SE_SETUP_CHANNEL | SE_MSG_CONTROL(SE_CONTROLLER_PAN, 0x5A), 0);
        func_001388E8(0, SE_SETUP_CHANNEL | SE_MSG_CONTROL(SE_CONTROLLER_REVERB, 0x7F), 0);
        func_001388E8(0, SE_SETUP_CHANNEL | SE_MSG_CONTROL(SE_CONTROLLER_BANK, 0), 0);
        func_001388E8(0, SE_SETUP_CHANNEL | SE_MSG_PROGRAM_CHANGE | SE_MSG_DATA1(6), 0);
    }
    func_00139B70(0, next_id, id, 1);
}

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
    seBgmSetSongLevel();
}

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

void seVoiceStart(int packed, int volume, int pan);

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

/* Sends one packed message to a channel group. The result is not used here; an int return is what
 * the callers' register choice shows. */
int func_0013A938(int group, int message, int value);

/* Starts the voice `packed` (bits 24..30: kind, bits 0..3: channel, bits 0..6: note) on the
 * kind's channel group: bank select (the kind), pan, expression, program 0, note on. A negative
 * volume takes the default of the kind; the volume is scaled by the master volume. */
void seVoiceStart(int packed, int volume, int pan)
{
    int kind = (packed >> 24) & 0x7F;
    int channel = SE_MSG_CHANNEL(packed & 0xF);
    int note = packed & 0x7F;
    int group = func_001A3B38(kind);

    if (volume < 0) {
        volume = soundCtrl[SOUND_CTRL_VOICE_LEVEL];
        if (kind != 0) {
            if (kind >= 3) {
                if (kind == 7) {
                    volume = soundCtrl[SOUND_CTRL_MASTER_LEVEL];
                }
            } else {
                volume = SOUND_LEVEL_MAX;
            }
        }
    }
    volume = volume * soundCtrl[SOUND_CTRL_MASTER_VOLUME] / SOUND_LEVEL_MAX;
    if (volume >= 0x80) {
        volume = SOUND_LEVEL_MAX;
    }
    func_0013A938(group, channel | SE_MSG_DATA2(kind) | SE_MSG_CONTROL_CHANGE, 0);
    func_0013A938(group, channel | SE_MSG_DATA2(pan)
                  | SE_MSG_CONTROL_CHANGE | SE_MSG_DATA1(SE_CONTROLLER_PAN), 0);
    func_0013A938(group, channel | SE_MSG_DATA2(volume)
                  | SE_MSG_CONTROL_CHANGE | SE_MSG_DATA1(SE_CONTROLLER_EXPRESSION), 0);
    func_0013A938(group, channel | SE_MSG_PROGRAM_CHANGE, 0);
    func_0013A938(group, channel | SE_MSG_DATA1(note) | SE_MSG_NOTE_ON | SE_MSG_DATA2(SOUND_LEVEL_MAX), 0);
}

void seVoiceSetPanVolume(int packed, int volume, int pan)
{
    int channel = SE_MSG_CHANNEL(packed & 0xF);
    int group = func_001A3B38((packed >> 24) & 0x7F);

    if (volume < 0) {
        volume = soundCtrl[SOUND_CTRL_VOICE_LEVEL];
    }
    volume = volume * soundCtrl[SOUND_CTRL_MASTER_VOLUME] / SOUND_LEVEL_MAX;
    if (volume >= 0x80) {
        volume = SOUND_LEVEL_MAX;
    }
    func_0013A938(group, channel | SE_MSG_DATA2(pan)
                  | SE_MSG_CONTROL_CHANGE | SE_MSG_DATA1(SE_CONTROLLER_PAN), 0);
    func_0013A938(group, channel | SE_MSG_DATA2(volume)
                  | SE_MSG_CONTROL_CHANGE | SE_MSG_DATA1(SE_CONTROLLER_EXPRESSION), 0);
}

/* For `packed` (bits 24..30: kind, bits 0..3: channel, bits 0..6: note), sends an expression
 * control change, a program change and a note off to the kind's channel group. */
void seVoiceStop(int packed)
{
    int group = func_001A3B38((packed >> 24) & 0x7F);
    int channel = packed & 0xF;

    channel = SE_MSG_CHANNEL(channel);
    func_0013A938(group, channel | SE_MSG_CONTROL_CHANGE | SE_MSG_DATA1(SE_CONTROLLER_EXPRESSION), 0);
    /* packed is reused for the note (bits 0..6) */
    packed &= 0x7F;
    func_0013A938(group, channel | SE_MSG_PROGRAM_CHANGE, 0);
    channel |= SE_MSG_DATA1(packed);
    func_0013A938(group, channel | SE_MSG_NOTE_OFF, 0);
}

/* seBendDisabled: nonzero turns the random pitch bend of func_001A3E10 off.
 * seNextChannel: counts the calls; its low four bits pick the MIDI channel. */
extern unsigned char seBendDisabled;
extern unsigned char seNextChannel;

/* Plays bank `packed & 0x7F` on channel group 1, on the next channel in turn, with a random pitch
 * bend and one of two notes. A value above 0xFFFFFF also carries the volume (bits 16..22) and the
 * pan (bits 8..14). A negative value only sets seBendDisabled (to packed + 1). */
void func_001A3E10(int packed)
{
    int channel;
    int random;
    int bend;
    int note;
    int volume;
    int pan;

    if (packed < 0) {
        seBendDisabled = packed + 1;
        return;
    }
    pan = SE_PAN_CENTRE;
    channel = SE_MSG_CHANNEL(seNextChannel++ & 0xF);
    random = xglSRand();
    bend = (random >> 1) & 0x7F;
    if (seBendDisabled != 0) {
        bend = 0;
    }
    note = (random & 1) + 1;
    volume = soundCtrl[SOUND_CTRL_VOICE_LEVEL];
    if (packed > 0xFFFFFF) {
        volume = (packed >> 16) & 0x7F;
        pan = (packed >> 8) & 0x7F;
    }
    /* packed is reused for the bank number (bits 0..6) */
    packed &= 0x7F;
    volume = volume * soundCtrl[SOUND_CTRL_MASTER_VOLUME] / SOUND_LEVEL_MAX;
    if (volume >= 0x80) {
        volume = SOUND_LEVEL_MAX;
    }
    func_0013A938(1, channel | SE_MSG_DATA2(bend) | SE_MSG_PITCH_BEND, 0);
    func_0013A938(1, channel | SE_MSG_DATA2(packed) | SE_MSG_CONTROL_CHANGE, 0);
    func_0013A938(1, channel | SE_MSG_DATA2(pan)
                  | SE_MSG_CONTROL_CHANGE | SE_MSG_DATA1(SE_CONTROLLER_PAN), 0);
    func_0013A938(1, channel | SE_MSG_DATA2(volume)
                  | SE_MSG_CONTROL_CHANGE | SE_MSG_DATA1(SE_CONTROLLER_EXPRESSION), 0);
    func_0013A938(1, channel | SE_MSG_PROGRAM_CHANGE, 0);
    func_0013A938(1, channel | SE_MSG_DATA1(note) | SE_MSG_NOTE_ON | SE_MSG_DATA2(SOUND_LEVEL_MAX), 0);
}
