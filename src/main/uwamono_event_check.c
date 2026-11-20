#include "common.h"
#include "shared.h"
#include "main/game_camera.h"
#include "main/pad.h"
#include "main/xgl_font.h"
#include "main/game_data.h"
#include "main/undulate.h"
#include "main/sef.h"
#include "ov01/sb_vm.h"
#include "main/get.h"
#include "main/res_get_path.h"
#include "main/uwamono_event_check.h"
#include "main/pause_menu_page_uwamono.h"
#include "main/se_bank_load.h"

#include "main/xgl_2.h"
#define UWAMONO_RECORD_COUNT 256
#define FOOT_STEP_TABLE_COUNT 44
#define FOOT_EFFECT_FIRST 500
#define FOOT_EFFECT_SECOND 501
#define FOOT_SOUND_NONE -1
/* 50 degrees in radians: the half-angle of the search fan. */
#define UWAMONO_SEARCH_FAN_ANGLE 0.87266469f

extern int uwamonoEventIds[];
extern unsigned char mapSceneScriptBuffer[];
int Check_InsideID(int index, int other, int third);


typedef struct FootStepEntry {
    int key;
    int rising[4];
    int falling[4];
} FootStepEntry;

typedef struct FootStepTable {
    FootStepEntry entries[FOOT_STEP_TABLE_COUNT];
    unsigned int tailEntries[30];
} FootStepTable;

extern FootStepTable footStepTable;
/* Float view of a word: entry 0 of footStepEffectLevelTable is the denormal 0x00000020, which the compiler flushes if written as a float. */
typedef union FloatWord {
    unsigned int bits;
    float value;
} FloatWord;

extern FloatWord footStepEffectLevelTable[];

void Sound_FootStep(int index)
{
    MapCollisionObject probe;
    UwamonoState *motions;
    UwamonoRecord *record;
    struct XenoActor *actor;
    SefEffectRecord *effect;
    float surfaceLevel;
    float soundCueValue;
    int tableIndex;
    int i;
    int soundBank;
    int surfaceKind;
    int previousFrame;
    int currentFrame;
    int soundCue;
    int effectId;
    int stepped;
    Vector4 *position;

    record = &uwamonoRecords[index];
    motions = record->body.motions;
    actor = record->body.actor;
    soundCue = FOOT_SOUND_NONE;
    effectId = -1;
    stepped = 0;
    UnduParamInit(&probe);
    position = &actor->position;
    probe.block = UnduDataGetHeader(0x700, motions->footId);
    UnduCheck(position, 0, &probe);
    soundBank = seFootBankFromKind(record->key, probe.kind);
    surfaceKind = (unsigned char) probe.kind + 1;
    previousFrame = F2I(motions->stepFrame);
    currentFrame = F2I(actor->frame);
    if (record->key <= 0x6FFFF) {
        tableIndex = -1;
        for (i = 0; i < FOOT_STEP_TABLE_COUNT; i++) {
            if (footStepTable.entries[i].key == record->key) {
                tableIndex = i;
                break;
            }
        }
        switch (actor->mode) {
        case 0:
            if (surfaceKind == 10) {
                if (record->form == 1.0f) {
                    if (previousFrame < 4 && currentFrame >= 4) {
                        effectId = FOOT_EFFECT_FIRST;
                        soundCue = -2;
                    }
                    if (previousFrame < 42 && currentFrame >= 42) {
                        effectId = FOOT_EFFECT_SECOND;
                        soundCue = -2;
                    }
                } else if (currentFrame == 5 || currentFrame == 27) {
                    soundCue = -2;
                }
                stepped = 1;
            }
            break;
        case 1:
            if (tableIndex != -1) {
                for (i = 0; i < 4; i++) {
                    if (previousFrame < footStepTable.entries[tableIndex].rising[i] && currentFrame >= footStepTable.entries[tableIndex].rising[i]) {
                        soundCue = soundBank;
                        stepped = 1;
                        effectId = FOOT_EFFECT_FIRST + i % 2;
                        break;
                    }
                }
                if (i != 4) {
                    break;
                }
            }
            {
                if (record->form == 1.0f) {
                    if (previousFrame < 4 && currentFrame >= 4) {
                        effectId = FOOT_EFFECT_FIRST;
                        soundCue = soundBank;
                    }
                    if (previousFrame < 23 && currentFrame >= 23) {
                        soundCue = soundBank;
                        effectId = FOOT_EFFECT_SECOND;
                    }
                } else {
                    if (surfaceKind == 50 && (unsigned int)(record->key - 0x14002) < 2U) {
                        break;
                    }
                    if (previousFrame < 2 && currentFrame >= 2) {
                        effectId = FOOT_EFFECT_FIRST;
                        soundCue = soundBank;
                    }
                    if (previousFrame < 26 && currentFrame >= 26) {
                        soundCue = soundBank;
                        effectId = FOOT_EFFECT_SECOND;
                    }
                }
                goto setMoved;
            }
            break;
        case 2:
            if (tableIndex != -1) {
                for (i = 0; i < 4; i++) {
                    if (previousFrame < footStepTable.entries[tableIndex].falling[i] && currentFrame >= footStepTable.entries[tableIndex].falling[i]) {
                        soundCue = soundBank;
                        stepped = 1;
                        effectId = FOOT_EFFECT_FIRST + i % 2;
                        break;
                    }
                }
                if (i != 4) {
                    break;
                }
            }
            {
                if (record->form == 1.0f) {
                    if (previousFrame < 16 && currentFrame >= 16) {
                        effectId = FOOT_EFFECT_FIRST;
                        soundCue = soundBank;
                    }
                    if (previousFrame < 4 && currentFrame >= 4) {
                        soundCue = soundBank;
                        effectId = FOOT_EFFECT_SECOND;
                    }
                } else {
                    if (surfaceKind == 50 && (unsigned int)(record->key - 0x14002) < 2U) {
                        break;
                    }
                    if ((unsigned int)(record->key - 0x14002) < 2U) {
                        if (previousFrame < 2 && currentFrame >= 2) {
                            effectId = FOOT_EFFECT_FIRST;
                            soundCue = soundBank;
                        }
                        if (previousFrame < 15 && currentFrame >= 15) {
                            soundCue = soundBank;
                            effectId = FOOT_EFFECT_SECOND;
                        }
                    } else {
                        if (previousFrame < 6 && currentFrame >= 6) {
                            effectId = FOOT_EFFECT_FIRST;
                            soundCue = soundBank;
                        }
                        if (previousFrame < 19 && currentFrame >= 19) {
                            soundCue = soundBank;
                            effectId = FOOT_EFFECT_SECOND;
                        }
                    }
                }
/* Case 1 jumps into the tail of case 2: duplicating the three statements into case 1 changes the code. */
setMoved:
                stepped = 1;
                if (tableIndex != -1) {
                    soundCue = FOOT_SOUND_NONE;
                }
            }
            break;
        }
        if (index == uwamonoCurrentIndex && actor->velocity.y != 0.0f) {
            effectId = -1;
            soundCue = FOOT_SOUND_NONE;
        }
        if (!(actor->drawFlags & 0x10000000) && !(record->flags & 0x4000) && effectId != -1) {
            effect = sefCreateEffectCf((effectId << 0xC) | surfaceKind | 0x10000000, 0, 0);
            if ((unsigned int)(surfaceKind - 10) < 10U && (surfaceLevel = footStepEffectLevelTable[surfaceKind].value) != -1000.0f) {
                if (effect != 0) {
                    effect->fieldA5C = surfaceLevel;
                }
            }
            if (effect != 0) {
                sefSetRecordOwner(effect, actor);
            }
        }
        soundCueValue = (float)soundCue;
        motions->stepFrame = actor->frame;
        if (soundCueValue != motions->soundCue) {
            if (soundCue < 0) {
                motions->soundCue = -1.0f;
                return;
            }
            if (stepped == 1) {
                if (index == uwamonoCurrentIndex) {
                    seTonePlay(soundCue);
                } else {
                    int pan = seVoicePanVolumeFromPosition(&position->x);
                    if (pan != 0) {
                        seTonePlay((pan << 8) + soundCue + 0x01000000);
                    }
                }
            }
            motions->soundCue = soundCueValue;
        }
    }
}

/* Queues event kind `slot` (its script id from uwamonoEventIds) with two record ids.
 * The search handlers below all send kind 2. */
void uwamonoSendEvent(int slot, int first, int second)
{
    SbMessage message;
    int eventId;

    eventId = uwamonoEventIds[slot];
    if (eventId != -1) {
        message.kind = slot;
        message.args[0] = first;
        message.args[1] = second;
        sbThreadEnqueue((struct SbImageHeader *)mapSceneScriptBuffer, eventId, &message, 0);
    }
}


void uwamonoSearchArcHit(int index, int other, int third)
{
    UwamonoRecord *records;
    UwamonoRecord *record;
    struct XenoActor *actor;
    float thirdWidth;
    UwamonoRecord *thirdRecord;
    int thirdStride;
    int hit;

    records = uwamonoRecords;
    record = &records[index];
    actor = record->body.actor;
    hit = Check_InsideID(index, other, third);
    thirdStride = third * 17;
    if (hit != 0) {
        thirdRecord = &records[third];
        thirdWidth = thirdRecord->turnTo.x;
        if (!(thirdWidth < 6.28f)
            || Check_Angle(actor->direction.y,
                              records[other].turnFrom.x - thirdWidth * 0.5f,
                              records[other].turnFrom.x + thirdWidth * 0.5f) != 0) {
            uwamonoSendEvent(2, records[actor->moved].id, records[other].id);
        }
    }
}


void uwamonoSearchFacingHit(int index, int other, int third)
{
    UwamonoRecord *records;
    UwamonoRecord *record;
    struct XenoActor *actor;
    UwamonoRecord *thirdRecord;
    float thirdWidth;
    float halfWidth;
    float upper;
    float lower;

    records = uwamonoRecords;
    record = &records[index];
    actor = record->body.actor;
    if ((gameWork.control & 0x20000000) != 0) {
        return;
    }
    if ((PadData[0].pressed & PAD_CIRCLE) == 0) {
        return;
    }
    if (Check_InsideID(index, other, third) == 0) {
        return;
    }
    records = uwamonoRecords;
    thirdRecord = &uwamonoRecords[third];
    thirdWidth = thirdRecord->turnTo.x;
    halfWidth = thirdWidth * 0.5f;
    upper = uwamonoRecords[other].turnFrom.x + halfWidth;
    lower = uwamonoRecords[other].turnFrom.x - halfWidth;
    if (Check_Angle(actor->rotation.y, lower, upper) == 0) {
        return;
    }
    uwamonoSendEvent(2, records[actor->moved].id, records[other].id);
}

void uwamonoSearchTouchHitShape3(int index, int other, int third)
{
    UwamonoRecord *record;
    struct XenoActor *actor;

    record = &uwamonoRecords[index];
    actor = record->body.actor;
    if (Check_InsideID(index, other, third) != 0) {
        uwamonoSendEvent(2, uwamonoRecords[actor->moved].id, uwamonoRecords[other].id);
    }
}

void uwamonoSearchFacingHitShape4(int index, int other, int third)
{
    UwamonoRecord *const records = uwamonoRecords;
    UwamonoRecord *record;
    struct XenoActor *actor;
    UwamonoRecord *thirdRecord;
    float thirdWidth;
    float halfWidth;
    float upper;
    float lower;
    float otherAngle;

    record = &records[index];
    actor = record->body.actor;
    if ((gameWork.control & 0x20000000) != 0) {
        return;
    }
    if ((PadData[0].pressed & 0x20) == 0) {
        return;
    }
    if (Check_InsideID(index, other, third) == 0) {
        return;
    }
    thirdRecord = &records[third];
    thirdWidth = thirdRecord->turnTo.x;
    halfWidth = thirdWidth * 0.5f;
    otherAngle = uwamonoRecords[other].turnFrom.x;
    lower = otherAngle - halfWidth;
    upper = otherAngle + halfWidth;
    if (Check_Angle(actor->rotation.y, lower, upper) == 0) {
        return;
    }
    uwamonoSendEvent(2, records[actor->moved].id, records[other].id);
}

extern unsigned char locaterHitFlag;

void EventCheck_Square_Button_Center(int index, int other, int third) {
    struct XenoActor *actor = uwamonoRecords[index].body.actor;

    if ((gameWork.control & 0x20000000) == 0) {
        if ((PadData[0].pressed & PAD_CIRCLE) != 0) {
            float angle;
            float radius;
            angle = uwamonoRecords[other].turnTo.x / 3.14159265f * 180.0f;
            radius = actor->radius;
            if (Check_InsideFan(-1, &actor->position, actor->rotation.y, UWAMONO_SEARCH_FAN_ANGLE, &uwamonoRecords[other].position, angle, radius, -1) != 0) {
                if (Check_InsideID(index, other, third) != 0) {
                    uwamonoSendEvent(2, uwamonoRecords[actor->moved].id, uwamonoRecords[other].id);
                }
            }
        }
    }
}

void uwamonoSearchPlainHit(int index, int other, int third) {
    struct XenoActor *actor;

    actor = uwamonoRecords[index].body.actor;
    if (Check_InsideID(index, other, third) != 0) {
        uwamonoSendEvent(2, uwamonoRecords[actor->moved].id, uwamonoRecords[other].id);
    }
}

void EventCheck_Circle_Button_Center_Unused(int index, int other, int third) {
    struct XenoActor *actor = uwamonoRecords[index].body.actor;

    if ((gameWork.control & 0x20000000) == 0) {
        if ((PadData[0].pressed & PAD_CIRCLE) != 0) {
            float angle;
            float radius;
            angle = uwamonoRecords[other].turnTo.x / 3.14159265f * 180.0f;
            radius = actor->radius;
            if (Check_InsideFan(-1, &actor->position, actor->rotation.y, UWAMONO_SEARCH_FAN_ANGLE, &uwamonoRecords[other].position, angle, radius, -1) != 0) {
                if (Check_InsideID(index, other, third) != 0) {
                    uwamonoSendEvent(2, uwamonoRecords[actor->moved].id, uwamonoRecords[other].id);
                }
            }
        }
    }
}

void uwamonoSearchCircleFacing(int index, int other, int third) {
    struct XenoActor *actor = uwamonoRecords[index].body.actor;

    if ((gameWork.control & 0x20000000) == 0) {
        if ((PadData[0].pressed & PAD_CIRCLE) != 0) {
            int otherIndex = other;
            if (Check_InsideID(index, other, third) != 0) {
                float centerAngle = uwamonoRecords[otherIndex].turnFrom.x;
                struct UwamonoRecord *thirdRecord = &uwamonoRecords[third];
                float halfWidth = thirdRecord->turnTo.x * 0.5f;
                if (Check_Angle(actor->rotation.y, centerAngle - halfWidth, centerAngle + halfWidth) != 0) {
                    uwamonoSendEvent(2, uwamonoRecords[actor->moved].id, uwamonoRecords[otherIndex].id);
                }
            }
        }
    }
}

void uwamonoSearchTouchHitShape7(int index, int other, int third) {
    struct XenoActor *actor;

    actor = uwamonoRecords[index].body.actor;
    if (Check_InsideID(index, other, third) != 0) {
        uwamonoSendEvent(2, uwamonoRecords[actor->moved].id, uwamonoRecords[other].id);
    }
}

void uwamonoSearchTouchHitShape11(int index, int other, int third) {
    struct XenoActor *actor;

    actor = uwamonoRecords[index].body.actor;
    if (Check_InsideID(index, other, third) != 0) {
        uwamonoSendEvent(2, uwamonoRecords[actor->moved].id, uwamonoRecords[other].id);
    }
}

void uwamonoSearchPlainHitShape9(int index, int other, int third) {
    struct XenoActor *actor;

    actor = uwamonoRecords[index].body.actor;
    if (Check_InsideID(index, other, third) != 0) {
        uwamonoSendEvent(2, uwamonoRecords[actor->moved].id, uwamonoRecords[other].id);
    }
}

void Check_Locater(int index) {
    struct XenoActor *actor;
    struct UwamonoRecord *records;
    float centerY;
    float halfHeight;
    int candidate;
    int linkedIndex;

    actor = uwamonoRecords[index].body.actor;
    linkedIndex = -1;
    if (gameWork.loadStage != 0) {
        return;
    }
    if ((gameWork.control & CONTROL_MAP_LOADED) != 0) {
        if (gameWork.entryKey != -1) {
            return;
        }
    }
    for (candidate = 0; candidate < UWAMONO_RECORD_COUNT; candidate++) {
        if (uwamonoRecords[candidate].kind != 7) {
            continue;
        }
        if (uwamonoRecords[candidate].searchShape == 0) {
            continue;
        }
        if (uwamonoRecords[candidate].searchShape == 10) {
            continue;
        }
        halfHeight = uwamonoRecords[candidate].extent.y * 0.5f;
        centerY = uwamonoRecords[candidate].position.y;
        if (actor->position.y < centerY - halfHeight) {
            continue;
        }
        if (centerY + halfHeight < actor->position.y) {
            continue;
        }
        linkedIndex = uwamonoRecords[candidate].partnerLink;
        if (Check_InsideID(index, candidate, linkedIndex) == 1) {
            break;
        }
    }
    if (candidate == UWAMONO_RECORD_COUNT || candidate == -1 || linkedIndex == -1) {
        *(int *)actor->unmodeled_790 = -1;
        return;
    }
    locaterHitFlag = 0;
    records = uwamonoRecords;
    switch (records[candidate].searchShape) {
    case 1:
        if (*(int *)actor->unmodeled_790 != candidate) {
            uwamonoSearchArcHit(index, candidate, linkedIndex);
        }
        break;
    case 2:
        uwamonoSearchFacingHit(index, candidate, linkedIndex);
        break;
    case 3:
        if (*(int *)actor->unmodeled_790 != candidate) {
            uwamonoSearchTouchHitShape3(index, candidate, linkedIndex);
        }
        break;
    case 4:
        uwamonoSearchFacingHitShape4(index, candidate, linkedIndex);
        break;
    case 5:
        EventCheck_Square_Button_Center(index, candidate, linkedIndex);
        break;
    case 6:
        uwamonoSearchPlainHit(index, candidate, linkedIndex);
        break;
    case 7:
        if (*(int *)actor->unmodeled_790 != candidate) {
            uwamonoSearchTouchHitShape7(index, candidate, linkedIndex);
        }
        break;
    case 11:
        if (*(int *)actor->unmodeled_790 != candidate) {
            uwamonoSearchTouchHitShape11(index, candidate, linkedIndex);
        }
        break;
    case 8:
        uwamonoSearchCircleFacing(index, candidate, linkedIndex);
        break;
    case 9:
        uwamonoSearchPlainHitShape9(index, candidate, linkedIndex);
        break;
    case 10:
        break;
    }
    *(int *)actor->unmodeled_790 = candidate;
    /* JP: ロケータ = %3d,%3d
     * EN: Locator = %3d,%3d */
    xglFontDebugPrintf(8, 128, "\xa5\xed\xa5\xb1\xa1\xbc\xa5\xbf = %3d,%3d", candidate, linkedIndex);
}

short locaterWork = 0;
unsigned char locaterHitFlag = 0;
int padDebugSector = 0;

int padDebugCount = 0;

FootStepTable footStepTable = {
    {
        { 0x18004, { -1, -1, -1, -1 }, { -1, -1, -1, -1 } },
        { 0x18008, { 14, 28, -1, -1 }, { 4, 14, -1, -1 } },
        { 0x1800A, { -1, -1, -1, -1 }, { -1, -1, -1, -1 } },
        { 0x1800B, { 5, 28, -1, -1 }, { 3, 15, -1, -1 } },
        { 0x1800C, { 0, 3, 9, 13 }, { 1, 4, 7, 10 } },
        { 0x1802B, { 22, 44, -1, -1 }, { 22, 44, -1, -1 } },
        { 0x1803C, { 5, 28, -1, -1 }, { 3, 15, -1, -1 } },
        { 0x18031, { 14, 28, -1, -1 }, { 4, 14, -1, -1 } },
        { 0x18052, { -1, -1, -1, -1 }, { 0, 10, -1, -1 } },
        { 0x18003, { 7, 31, -1, -1 }, { 2, 14, -1, -1 } },
        { 0x18009, { 22, 44, -1, -1 }, { 22, 44, -1, -1 } },
        { 0x1801E, { 10, 30, -1, -1 }, { 11, 23, -1, -1 } },
        { 0x18028, { 22, 44, -1, -1 }, { 22, 44, -1, -1 } },
        { 0x1802C, { 22, 44, -1, -1 }, { 22, 44, -1, -1 } },
        { 0x18032, { 8, 31, -1, -1 }, { 15, 31, -1, -1 } },
        { 0x18034, { 8, 31, -1, -1 }, { 15, 31, -1, -1 } },
        { 0x18036, { -1, -1, -1, -1 }, { -1, -1, -1, -1 } },
        { 0x18017, { 15, 41, -1, -1 }, { 9, 21, -1, -1 } },
        { 0x18019, { 17, 42, -1, -1 }, { 10, 28, -1, -1 } },
        { 0x18001, { 3, 22, -1, -1 }, { 4, 16, -1, -1 } },
        { 0x18006, { 6, 24, -1, -1 }, { 3, 14, -1, -1 } },
        { 0x1802E, { 15, 30, -1, -1 }, { 2, 12, -1, -1 } },
        { 0x18037, { 19, 37, -1, -1 }, { 6, 17, -1, -1 } },
        { 0x18025, { 18, 35, -1, -1 }, { 10, 19, -1, -1 } },
        { 0x1802F, { 18, 35, -1, -1 }, { 10, 19, -1, -1 } },
        { 0x18011, { 20, 39, -1, -1 }, { 12, 23, -1, -1 } },
        { 0x18039, { -1, -1, -1, -1 }, { -1, -1, -1, -1 } },
        { 0x1803B, { -1, -1, -1, -1 }, { -1, -1, -1, -1 } },
        { 0x1803E, { 8, 18, 24, 35 }, { 4, 6, 13, 15 } },
        { 0x18040, { 18, 35, -1, -1 }, { 2, 14, -1, -1 } },
        { 0x18042, { -1, -1, -1, -1 }, { -1, -1, -1, -1 } },
        { 0x18043, { -1, -1, -1, -1 }, { -1, -1, -1, -1 } },
        { 0x18044, { -1, -1, -1, -1 }, { -1, -1, -1, -1 } },
        { 0x18046, { 19, 41, -1, -1 }, { 3, 15, -1, -1 } },
        { 0x18047, { 14, 27, -1, -1 }, { 5, 16, -1, -1 } },
        { 0x1804A, { -1, -1, -1, -1 }, { -1, -1, -1, -1 } },
        { 0x1804B, { 18, 35, -1, -1 }, { -1, -1, -1, -1 } },
        { 0x1803D, { -1, -1, -1, -1 }, { -1, -1, -1, -1 } },
        { 0x18045, { -1, -1, -1, -1 }, { -1, -1, -1, -1 } },
        { 0x18048, { -1, -1, -1, -1 }, { -1, -1, -1, -1 } },
        { 0x1804D, { -1, -1, -1, -1 }, { -1, -1, -1, -1 } },
        { 0x1804E, { -1, -1, -1, -1 }, { -1, -1, -1, -1 } },
        { 0x1803A, { -1, -1, -1, -1 }, { -1, -1, -1, -1 } },
        { 0x18024, { -1, -1, -1, -1 }, { -1, -1, -1, -1 } }
    },
    {
        0x10026U, 0x1U, 0x10U, 0x8000000U, 0x10026U, 0x1U, 0x20U, 0x8000000U,
        0x10026U, 0x2U, 0x10U, 0x8000000U, 0x10026U, 0x2U, 0x20U, 0x8000000U,
        0x10023U, 0x1U, 0x10U, 0x8000000U, 0x10023U, 0x1U, 0x20U, 0x8000000U,
        0x10023U, 0x2U, 0x10U, 0x8000000U, 0x10023U, 0x2U
    }
};

FloatWord footStepEffectLevelTable[20] = {
    { 0x00000020u },
    { 0x01000000u },
    { 0x00000000u },
    { 0x00000000u },
    { 0x00000000u },
    { 0x00000000u },
    { 0x00000000u },
    { 0x00000000u },
    { 0x00000000u },
    { 0x00000000u },
    { 0x3F000000u },
    { 0xC47A0000u },
    { 0xC08CCCCDu },
    { 0xC15CCCCDu },
    { 0xC0266666u },
    { 0xBF000000u },
    { 0x3E99999Au },
    { 0x3727C5ACu },
    { 0xC0B66666u },
    { 0xC47A0000u }
};

/* Script id of each event kind (uwamonoSendEvent); -1 sends nothing. Kind 8 (uwamono_record.c) is read one entry past
 * these eight in the original. */
int uwamonoEventIds[8] = { 0 };

/* Two endpoints of the sector at angle +/- half its width, both one unit out from source. */
typedef struct UwamonoSector {
    unsigned char unmodeled_0000[0xA0];
    float angle;                        /* +0xA0 */
    unsigned char unmodeled_00A4[0xC];
    float width;                        /* +0xB0 */
} UwamonoSector;

void uwamonoGetSectorEnds(Vector4 *source, UwamonoSector *sector) {
    Vector4 leftEnd;
    Vector4 rightEnd;

    Get_Point_By_AngleLength(source, &leftEnd, sector->angle - sector->width * 0.5f, 1.0f);
    Get_Point_By_AngleLength(source, &rightEnd, sector->angle + sector->width * 0.5f, 1.0f);
    uwamonoDrawLine(source, &leftEnd, 2);
    uwamonoDrawLine(source, &rightEnd, 3);
}

/* JP: 距離差 = %8f, %8f
 * EN: Distance difference = %8f, %8f */
const char insideDistanceDebugFormat[32] = "\265\367\316\245\272\271 = %8f, %8f";

int Check_InsideID(int moverIndex, int sectorIndex, int otherIndex)
{
    UwamonoRecord *mover = &uwamonoRecords[moverIndex];
    UwamonoRecord *sector = &uwamonoRecords[sectorIndex];
    UwamonoRecord *other = &uwamonoRecords[otherIndex];
    Vector4 moverAhead;
    Vector4 rangeMin;
    Vector4 rangeMax;
    double moverDistance;
    AlignedVector4 segmentStart;
    AlignedVector4 segmentEnd;
    float distance;
    float flatHeight;
    DoublewordVector4 sectorEnd;
    union {
        int limitWords[4];
        float limitFloats[4];
    } limitBlock;
    AlignedVector4 moverPosition;
    Vector4 *previousPosition;
    struct XenoActor *moverActor;

    moverActor = uwamonoRecords[moverIndex].body.actor;
    moverPosition = moverActor->position;
    limitBlock.limitWords[0] = 0xFF;
    limitBlock.limitWords[1] = 0x7F;
    limitBlock.limitWords[2] = 0;
    limitBlock.limitWords[3] = 0x7F;
    segmentStart.x = sector->position.x;
    segmentStart.y = sector->position.y;
    segmentStart.z = sector->position.z;
    segmentStart.w = 1.0f;
    segmentEnd.x = other->position.x;
    segmentEnd.y = other->position.y;
    segmentEnd.z = other->position.z;
    segmentEnd.w = 1.0f;

    switch (sector->searchShape - 1) {
    case 0:
        previousPosition = &moverActor->previous.vector;
        uwamonoDrawLine(&segmentStart, &segmentEnd, 3);
        Get_MiddlePoint(&segmentStart, &segmentEnd, 1, 2, &sectorEnd.vector);
        uwamonoGetSectorEnds(&sectorEnd.vector, (UwamonoSector *)sector);
        if (Check_CrossingOver(&sector->position, &other->position, previousPosition, &moverPosition)) {
            return 1;
        }
        if (uwamonoQuadEdgesVisible(&moverPosition, &sector->previous, &sector->position, &other->position, &other->previous)) {
            return 1;
        }
        if (uwamonoQuadEdgesVisible(previousPosition, &sector->previous, &sector->position, &other->position, &other->previous)) {
            return 1;
        }
        if (uwamonoQuadEdgesVisible(&moverPosition, &other->previous, &other->position, &sector->position, &sector->previous)) {
            return 1;
        }
        return uwamonoQuadEdgesVisible(previousPosition, &other->previous, &other->position, &sector->position, &sector->previous) != 0;
    case 1:
        Get_Point_By_AngleLength(&moverPosition, &moverAhead, moverActor->rotation.y, mover->form);
        uwamonoDrawLine(&segmentStart, &segmentEnd, 3);
        Get_MiddlePoint(&segmentStart, &segmentEnd, 1, 2, &sectorEnd.vector);
        uwamonoGetSectorEnds(&sectorEnd.vector, (UwamonoSector *)sector);
        return Check_CrossingOver(&sector->position, &other->position, &moverAhead, &moverPosition);
    case 2:
    case 3:
    case 4:
    case 5:
        distance = Get_Distance(&sector->position, &other->position);
        segmentEnd.y = 0.0f;
        flatHeight = segmentEnd.y;
        segmentEnd.x = 0.1f;
        DispPillar(&segmentStart, 0.1f, flatHeight, 1);
        segmentEnd.x = distance;
        segmentEnd.y = flatHeight;
        DispPillar(&segmentStart, distance, flatHeight, 1);
        sectorEnd.words[0] = *(u64 *)&segmentStart;
        sectorEnd.words[1] = *(u64 *)&segmentStart.z;
        moverDistance = Get_Distance(&sector->position, &moverPosition);
        xglFontDebugPrintf(8, 0x6C, insideDistanceDebugFormat, (double)distance, moverDistance);
        uwamonoGetSectorEnds(&sectorEnd.vector, (UwamonoSector *)sector);
        if (distance < Get_Distance(&sector->position, &moverPosition)) {
            return 0;
        }
        return 1;
    case 6:
    case 7:
    case 8:
        distance = Get_Distance(&sector->position, &other->position);
        sectorEnd.words[0] = *(u64 *)&segmentStart;
        sectorEnd.words[1] = *(u64 *)&segmentStart.z;
        segmentEnd.x = segmentStart.x + distance;
        segmentEnd.z = segmentStart.z + distance;
        segmentStart.x -= distance;
        segmentStart.z -= distance;
        uwamonoDrawSegmentBox(&segmentStart, &segmentEnd, limitBlock.limitWords);
        rangeMin.z = sector->position.z - distance;
        rangeMax.z = sector->position.z + distance;
        rangeMin.x = sector->position.x - distance;
        rangeMax.x = sector->position.x + distance;
        uwamonoGetSectorEnds(&sectorEnd.vector, (UwamonoSector *)sector);
        break;
    case 10: {
        float sideSpan;
        distance = Get_Distance(&sector->position, &other->position);
        sideSpan = 0.3f;
        sectorEnd.words[0] = *(u64 *)&segmentStart;
        sectorEnd.words[1] = *(u64 *)&segmentStart.z;
        segmentEnd.x = segmentStart.x + sideSpan;
        segmentEnd.z = segmentStart.z;
        segmentStart.x -= sideSpan;
        segmentStart.z -= distance;
        uwamonoDrawSegmentBox(&segmentStart, &segmentEnd, limitBlock.limitWords);
        rangeMin.x = sector->position.x - sideSpan;
        rangeMax.x = sector->position.x + sideSpan;
        rangeMin.z = sector->position.z - distance;
        rangeMax.z = sector->position.z;
        uwamonoGetSectorEnds(&sectorEnd.vector, (UwamonoSector *)sector);
        break;
    }
    default:
        return 0;
    }

    if (moverPosition.x < rangeMin.x || rangeMax.x < moverPosition.x || moverPosition.z < rangeMin.z || rangeMax.z < moverPosition.z) {
        return 0;
    }
    return 1;
}
