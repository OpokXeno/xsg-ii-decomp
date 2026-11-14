#include "common.h"
#include "main/tu021.h"
#include "main/tu022.h"
#include "main/xgl_2.h"
#include "main/xgl_cd.h"

#define SE_PATH_MAX 256
#define SE_BANK_SLOT_COUNT 24

/* The loaders here pass CdFilePosition.unmodeled_27 on to the driver together with the lbn
 * (lbu 0x27($sp) at 0x1A3334, 0x1A3354, 0x1A3394 and 0x1A35A4); its meaning is not established. */
void func_00138D78(int slot);
int func_00139060(int slot, unsigned int lbn, int record_byte_27);
void func_001391D8(int slot, unsigned int lbn, int record_byte_27);
int func_0013A338(int slot, unsigned int lbn, int record_byte_27, int bank_size);

/* Loads the bank `name` of `directory` into `slot`: clears the slot, gives the driver the position
 * of <directory><name>.vsf (func_0013A338 with `bank_size` when it is set, else func_00139060) and then of
 * the .prj file. Returns the size of the .vsf file, 0 for a slot out of range or no name, or the
 * negative result of the failing step. */
int seBankLoadFile(int slot, const char *name, const char *directory, int bank_size)
{
    CdFilePosition record;
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
    if (bank_size != 0) {
        result = func_0013A338(slot, record.lbn, record.unmodeled_27, bank_size);
    } else {
        result = func_00139060(slot, record.lbn, record.unmodeled_27);
    }
    if (result < 0) {
        return result;
    }
    destination[1] = 'p';
    destination[2] = 'r';
    destination[3] = 'j';
    xglCdCheckFile(path, &record);
    func_001391D8(slot, record.lbn, record.unmodeled_27);
    return size;
}

extern const char *seBankDirectory;

/* Loads the bank `name` into `slot`, its files taken from the global directory seBankDirectory.
 * `bank_size` is the room the sound driver keeps for the bank (res_get_path.c's per-slot table:
 * 0x19000, 0x5000 for the system sound effects, ...); 0 loads without one. */
int seBankLoad(int slot, const char *name, int bank_size) {
    return seBankLoadFile(slot, name, seBankDirectory, bank_size);
}

/* Loads the bank `name` into `slot` with no bank size. */
int seBankLoadDefault(int slot, const char *name) {
    return seBankLoad(slot, name, 0);
}

int func_0013A098(int slot, int vsf);
int func_0013A128(int slot, int prj);
int func_0013A578(int slot, int vsf, int bank_size);

/* The memory form of seBankLoadFile: `vsf` and `prj` are the two bank files already in memory. */
int seBankLoadMemory(int slot, int vsf, int prj, int bank_size)
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
    if (bank_size != 0) {
        result = func_0013A578(slot, vsf, bank_size);
    } else {
        result = func_0013A098(slot, vsf);
    }
    if (result < 0) {
        return result;
    }
    return func_0013A128(slot, prj);
}

/* A negative volume or level: the default one. */
#define SE_VOLUME_DEFAULT (-1)
/* soundCtrl.song when the song name does not start with three digits. */
#define SE_SONG_NONE 0xFF
/* A `packed` song id above this value loads the bank only, not the .mid file. */
#define SE_BGM_ID_MAX 0xFFFF

extern const char *seMidiDirectory;
/* Takes the song id in its first argument (move $17,$4 at 0x139AF8). */
void func_00139AF0(int id);
int func_00139978(int packed, unsigned int lbn, int record_byte_27);

/* Loads the song `name` (three digits) from seMidiDirectory: its bank into slot 0, then, for a
 * `packed` id up to SE_BGM_ID_MAX, the .mid file, and records the song number (SE_SONG_NONE when
 * the name does not start with three digits). With no name it returns what the bank load returned. */
int seBgmLoad(int packed, const char *name, int bank_size)
{
    CdFilePosition record;
    char path[SE_PATH_MAX];
    const char *source;
    char *destination;
    int result;
    int bank_only;

    bank_only = packed > SE_BGM_ID_MAX;
    if (!bank_only) {
        func_00139AF0(packed);
    }
    result = seBankLoadFile(0, name, seMidiDirectory, bank_size);
    if (name == 0) {
        return result;
    }
    if (bank_only) {
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
    /* if + do/while: the original tests the first character apart and loads each one once */
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
    result = func_00139978(packed, record.lbn, record.unmodeled_27);
    soundCtrl.song = SE_SONG_NONE;
    if ((name[0] & 0xF0) == '0' && (name[1] & 0xF0) == '0' && (name[2] & 0xF0) == '0') {
        soundCtrl.song = (name[0] & 0xF) * 100 + (name[1] & 0xF) * 10 + (name[2] & 0xF);
    }
    return result;
}

/* Loads song `name` (three digits, then .mid) with no bank size. */
void seBgmLoadDefault(int packed, const char *name) {
    seBgmLoad(packed, name, 0);
}

void func_0013A1B8(int, int);

/* The memory form of seBgmLoad: selects song `first`, loads its bank from the files `vsf` and `prj`
 * already in memory into slot 0 and, when `second` is set, passes it on with the song. */
void seBgmLoadMemory(int first, int second, int vsf, int prj, int bank_size) {
    func_00139AF0(first);
    seBankLoadMemory(0, vsf, prj, bank_size);
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
        if (soundCtrl.song == entry->song) {
            func_001394B8(1, entry->level);
            break;
        }
    }
}

void func_00139C20(int, int);
void seBgmStart(int, int);
void seVolumeSet(int, int);

/* `packed`: the high 16 bits select the mode, the low 16 bits are the id.
 * `name` is the song name for seBgmLoad; a non-null name also starts playback. SE_BGM_START sets the
 * default volume afterwards, SE_BGM_START_KEEP_VOLUME keeps it; SE_BGM_BANK loads the bank only,
 * unmutes and fades the default volume in. seBgmLoad gets the id plus SE_BGM_LOAD_OFFSET. */
#define SE_BGM_START 0
#define SE_BGM_BANK 1
#define SE_BGM_START_KEEP_VOLUME 2
#define SE_BGM_LOAD_OFFSET 0x100
/* Above SE_BGM_ID_MAX: seBgmLoad loads the bank only. */
#define SE_BGM_BANK_ONLY 0x10000
#define SE_BGM_FADE_STEP 16
void seBgmPlay(int packed, const char *name, int bank_size) {
    int *mode_ref;
    int mode;
    int id;
    int next_id;

    id = packed;
    mode = id >> 16;
    id &= 0xffff;
    next_id = id + SE_BGM_LOAD_OFFSET;
    switch (mode) {
    case SE_BGM_START_KEEP_VOLUME:
    case SE_BGM_START:
        func_00139C20(0, id);
        func_001394B8(1, 0);
        mode_ref = &mode;
        seBgmLoad(next_id, name, bank_size);
        if (name != 0) {
            seBgmStart(id, next_id);
            /* the address taken here makes this test reload mode */
            if (*mode_ref == SE_BGM_START) {
                seVolumeSet(SE_VOLUME_DEFAULT, 0);
                return;
            }
        }
        return;

    case SE_BGM_BANK:
        seBgmLoad(SE_BGM_BANK_ONLY + SE_BGM_LOAD_OFFSET + id, name, bank_size);
        if (name != 0) {
            seVolumeSet(0, 0);
            seSetMuted(id, 0);
            seVolumeSet(SE_VOLUME_DEFAULT, SE_BGM_FADE_STEP);
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
/* A packed voice id: kind in bits 24..30, MIDI channel in bits 0..3, note in bits 0..6. */
#define SE_VOICE_KIND(packed) (((packed) >> 24) & 0x7F)
#define SE_VOICE_CHANNEL(packed) ((packed) & 0xF)
#define SE_VOICE_NOTE(packed) ((packed) & 0x7F)
/* Default volume by kind in seVoiceStart: kinds 1 and 2 (below SE_VOICE_KIND_LEVELLED) play at
 * SOUND_LEVEL_MAX, SE_VOICE_KIND_MASTER_LEVEL at the default master level. */
#define SE_VOICE_KIND_LEVELLED 3
#define SE_VOICE_KIND_MASTER_LEVEL 7
/* seTonePlay: a value above SE_TONE_PLAIN_MAX also carries a volume and a pan. */
#define SE_TONE_PLAIN_MAX 0xFFFFFF
#define SE_TONE_BANK(packed) ((packed) & 0x7F)
#define SE_TONE_VOLUME(packed) (((packed) >> 16) & 0x7F)
#define SE_TONE_PAN(packed) (((packed) >> 8) & 0x7F)
#define SE_CHANNEL_COUNT 16
/* Song 6 gets its own setup of MIDI channel 1 before it starts. */
#define SE_SONG_WITH_CHANNEL_SETUP 6
#define SE_SETUP_CHANNEL SE_MSG_CHANNEL(1)

void func_001388E8(int, int message, int);
void func_00139B70(int, int, int, int);
/* Returns the reply word of its driver call (lw $2,0x10($sp) at 0x139D7C); not used here. */
int func_00139D08(int, int channel);

/* Starts song `id` (loaded as `next_id` by seBgmLoad): resets the 16 channels, gives song
 * SE_SONG_WITH_CHANNEL_SETUP its own setup of MIDI channel 1, then starts playback. */
void seBgmStart(int id, int next_id)
{
    int i;

    for (i = 0; i < SE_CHANNEL_COUNT; i++) {
        func_00139D08(0, i);
    }
    if (soundCtrl.song == SE_SONG_WITH_CHANNEL_SETUP) {
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

int func_00138B48(int channel, int level);

/* Starts the master volume ramp toward level (negative: the default level), scaled by the master
 * volume and clamped to SOUND_LEVEL_MAX; applies the level at once when the ramp returns one. */
void seVolumeSet(int level, int step)
{
    int status;

    if (level < 0) {
        level = soundCtrl.master_level;
    }
    level = level * soundCtrl.master_volume / SOUND_LEVEL_MAX;
    status = seVolumeRampSet(&soundCtrl.master_ramp, level <= SOUND_LEVEL_MAX ? level : SOUND_LEVEL_MAX, step);
    if (status >= 0) {
        func_00138B48(0, status);
    }
}

/* Starts `voice` at the default volume and centre pan. */
void seVoiceStartDefault(int voice) {
    seVoiceStart(voice, SE_VOLUME_DEFAULT, SE_PAN_CENTRE);
}

/* The channel group a voice kind plays on: kinds 3 and 4 have their own group, every other kind
 * shares SE_GROUP_VOICE. */
#define SE_VOICE_KIND_3 3
#define SE_VOICE_KIND_4 4
#define SE_GROUP_OF_KIND_3 2
#define SE_GROUP_OF_KIND_4 3
#define SE_GROUP_VOICE 4
/* seTonePlay's channel group. */
#define SE_GROUP_TONE 1

int seVoiceKindGroup(int kind)
{
    int group;

    switch (kind) {
    case SE_VOICE_KIND_3:
        group = SE_GROUP_OF_KIND_3;
        break;
    case SE_VOICE_KIND_4:
        group = SE_GROUP_OF_KIND_4;
        break;
    default:
        group = SE_GROUP_VOICE;
        break;
    }
    return group;
}

/* Queues one packed message for a channel group. It returns 0, or -1 when the queue is busy or
 * full (move $2,$16 at 0x13A9BC and 0x13A9C4); the callers here do not use the result. */
int func_0013A938(int group, int message, int value);

/* Starts `voice` (bits 24..30: kind, bits 0..3: channel, bits 0..6: note) on the
 * kind's channel group: bank select (the kind), pan, expression, program 0, note on. A negative
 * volume takes the default of the kind (kinds 1 and 2: SOUND_LEVEL_MAX, kind 7: the default
 * master level, every other kind: the default voice level); the volume is then scaled by the
 * master volume. */
void seVoiceStart(int voice, int volume, int pan)
{
    int kind = SE_VOICE_KIND(voice);
    int channel = SE_MSG_CHANNEL(SE_VOICE_CHANNEL(voice));
    int note = SE_VOICE_NOTE(voice);
    int group = seVoiceKindGroup(kind);

    if (volume < 0) {
        volume = soundCtrl.voice_level;
        if (kind != 0) {
            if (kind >= SE_VOICE_KIND_LEVELLED) {
                if (kind == SE_VOICE_KIND_MASTER_LEVEL) {
                    volume = soundCtrl.master_level;
                }
            } else {
                volume = SOUND_LEVEL_MAX;
            }
        }
    }
    volume = volume * soundCtrl.master_volume / SOUND_LEVEL_MAX;
    if (volume > SOUND_LEVEL_MAX) {
        volume = SOUND_LEVEL_MAX;
    }
    func_0013A938(group, channel | SE_MSG_CONTROL(SE_CONTROLLER_BANK, kind), 0);
    func_0013A938(group, channel | SE_MSG_DATA2(pan)
                  | SE_MSG_CONTROL_CHANGE | SE_MSG_DATA1(SE_CONTROLLER_PAN), 0);
    func_0013A938(group, channel | SE_MSG_DATA2(volume)
                  | SE_MSG_CONTROL_CHANGE | SE_MSG_DATA1(SE_CONTROLLER_EXPRESSION), 0);
    func_0013A938(group, channel | SE_MSG_PROGRAM_CHANGE, 0);
    func_0013A938(group, channel | SE_MSG_DATA1(note) | SE_MSG_NOTE_ON | SE_MSG_DATA2(SOUND_LEVEL_MAX), 0);
}

/* Sets pan and volume of a playing `voice` (a negative volume: the default voice level, scaled by
 * the master volume). */
void seVoiceSetPanVolume(int voice, int volume, int pan)
{
    int channel = SE_MSG_CHANNEL(SE_VOICE_CHANNEL(voice));
    int group = seVoiceKindGroup(SE_VOICE_KIND(voice));

    if (volume < 0) {
        volume = soundCtrl.voice_level;
    }
    volume = volume * soundCtrl.master_volume / SOUND_LEVEL_MAX;
    if (volume > SOUND_LEVEL_MAX) {
        volume = SOUND_LEVEL_MAX;
    }
    func_0013A938(group, channel | SE_MSG_DATA2(pan)
                  | SE_MSG_CONTROL_CHANGE | SE_MSG_DATA1(SE_CONTROLLER_PAN), 0);
    func_0013A938(group, channel | SE_MSG_DATA2(volume)
                  | SE_MSG_CONTROL_CHANGE | SE_MSG_DATA1(SE_CONTROLLER_EXPRESSION), 0);
}

/* For `voice` (bits 24..30: kind, bits 0..3: channel, bits 0..6: note), sends an expression
 * control change, a program change and a note off to the kind's channel group. */
void seVoiceStop(int voice)
{
    int group = seVoiceKindGroup(SE_VOICE_KIND(voice));
    int channel = SE_VOICE_CHANNEL(voice);

    /* shifted in a second step: the one-step form takes other registers */
    channel = SE_MSG_CHANNEL(channel);
    func_0013A938(group, channel | SE_MSG_CONTROL_CHANGE | SE_MSG_DATA1(SE_CONTROLLER_EXPRESSION), 0);
    /* voice is reused for the note; where this statement stands sets the first call's argument order */
    voice = SE_VOICE_NOTE(voice);
    func_0013A938(group, channel | SE_MSG_PROGRAM_CHANGE, 0);
    channel |= SE_MSG_DATA1(voice);
    func_0013A938(group, channel | SE_MSG_NOTE_OFF, 0);
}

/* seBendDisabled: nonzero turns the random pitch bend of seTonePlay off.
 * seNextChannel: counts the calls; its low four bits pick the MIDI channel. */
extern unsigned char seBendDisabled;
extern unsigned char seNextChannel;

/* Plays bank `packed & 0x7F` on channel group 1, on the next channel in turn, with a random pitch
 * bend and one of two notes. A value above 0xFFFFFF also carries the volume (bits 16..22) and the
 * pan (bits 8..14). A negative value only sets seBendDisabled (to packed + 1). */
void seTonePlay(int packed)
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
    channel = SE_MSG_CHANNEL(SE_VOICE_CHANNEL(seNextChannel++));
    random = xglSRand();
    bend = (random >> 1) & 0x7F;
    if (seBendDisabled != 0) {
        bend = 0;
    }
    note = (random & 1) + 1;
    volume = soundCtrl.voice_level;
    if (packed > SE_TONE_PLAIN_MAX) {
        volume = SE_TONE_VOLUME(packed);
        pan = SE_TONE_PAN(packed);
    }
    /* packed is reused for the bank number */
    packed = SE_TONE_BANK(packed);
    volume = volume * soundCtrl.master_volume / SOUND_LEVEL_MAX;
    if (volume > SOUND_LEVEL_MAX) {
        volume = SOUND_LEVEL_MAX;
    }
    func_0013A938(SE_GROUP_TONE, channel | SE_MSG_DATA2(bend) | SE_MSG_PITCH_BEND, 0);
    func_0013A938(SE_GROUP_TONE, channel | SE_MSG_DATA2(packed) | SE_MSG_CONTROL_CHANGE, 0);
    func_0013A938(SE_GROUP_TONE, channel | SE_MSG_DATA2(pan)
                  | SE_MSG_CONTROL_CHANGE | SE_MSG_DATA1(SE_CONTROLLER_PAN), 0);
    func_0013A938(SE_GROUP_TONE, channel | SE_MSG_DATA2(volume)
                  | SE_MSG_CONTROL_CHANGE | SE_MSG_DATA1(SE_CONTROLLER_EXPRESSION), 0);
    func_0013A938(SE_GROUP_TONE, channel | SE_MSG_PROGRAM_CHANGE, 0);
    func_0013A938(SE_GROUP_TONE, channel | SE_MSG_DATA1(note) | SE_MSG_NOTE_ON | SE_MSG_DATA2(SOUND_LEVEL_MAX), 0);
}
