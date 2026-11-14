#include "common.h"
#include "shared.h"
#include "main/tu051.h"
#include "main/pad.h"
#include "main/xgl_font.h"

#define UWAMONO_RECORD_COUNT 256
#define FOOT_STEP_TABLE_COUNT 44
#define FOOT_EFFECT_FIRST 500
#define FOOT_EFFECT_SECOND 501
#define FOOT_SOUND_NONE -1
/* 50 degrees in radians: the half-angle of the search fan. */
#define UWAMONO_SEARCH_FAN_ANGLE 0.87266469f

extern int uwamonoEventIds[];
extern unsigned char D_003C6DC0[];
void sbThreadEnqueue(void *table, int value, void *args, int flags);
int uwamonoCheckCollision(int index, int other, int third);

typedef struct UwamonoEventArgs {
    int slot;
    unsigned char unmodeled_4[4];
    int first;
    int second;
} UwamonoEventArgs;

int F2I(float value);
int mapFindBlockEntry(int blockId, short kind);
void mapProbeInit(void *probe);
int mapMoveProbe(Vector4 *position, int unused, void *probe);
int seFootBankFromKind(int key, int bank);
int seVoicePanVolumeFromPosition(Vector4 *position);
void sefSetRecordOwner(void *effect, struct XenoActor *actor);
void *sefSpawnEffectRecordById(int id, int arg1, int arg2);
void seTonePlay(int value);

typedef struct FootStepEntry {
    int key;
    int rising[4];
    int falling[4];
} FootStepEntry;

extern FootStepEntry footStepTable[];
extern unsigned char D_003E7AA8[];

void uwamonoPlayFootEffects(int index)
{
    unsigned char probe[0x40];
    UwamonoMotionTable *motions;
    UwamonoObject *record;
    struct XenoActor *actor;
    void *effect;
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
    motions = record->motions;
    actor = record->actor;
    soundCue = FOOT_SOUND_NONE;
    effectId = -1;
    stepped = 0;
    mapProbeInit(&probe[0]);
    position = &actor->position;
    *(int *)(probe + 0x18) = mapFindBlockEntry(0x700, *(short *)((unsigned char *)motions + 0x82));
    mapMoveProbe(position, 0, &probe[0]);
    soundBank = seFootBankFromKind(record->key, *(int *)(probe + 0x20));
    surfaceKind = probe[0x20] + 1;
    previousFrame = F2I(*(float *)((unsigned char *)motions + 0x3FD0));
    currentFrame = F2I(*(float *)((unsigned char *)actor + 0x5D0));
    if (record->key <= 0x6FFFF) {
        tableIndex = -1;
        for (i = 0; i < FOOT_STEP_TABLE_COUNT; i++) {
            if (footStepTable[i].key == record->key) {
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
                    if (previousFrame < footStepTable[tableIndex].rising[i] && currentFrame >= footStepTable[tableIndex].rising[i]) {
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
                        goto skipFlag;
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
                    if (previousFrame < footStepTable[tableIndex].falling[i] && currentFrame >= footStepTable[tableIndex].falling[i]) {
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
                        goto skipFlag;
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
setMoved:
                stepped = 1;
                if (tableIndex != -1) {
                    soundCue = FOOT_SOUND_NONE;
                }
            }
            break;
        }
skipFlag:
        if (index == uwamonoCurrentIndex && actor->velocity.y != 0.0f) {
            effectId = -1;
            soundCue = FOOT_SOUND_NONE;
        }
        if (!(actor->drawFlags & 0x10000000) && !(record->flags & 0x4000) && effectId != -1) {
            effect = sefSpawnEffectRecordById((effectId << 0xC) | surfaceKind | 0x10000000, 0, 0);
            if ((unsigned int)(surfaceKind - 10) < 10U && (surfaceLevel = *(float *)(D_003E7AA8 + surfaceKind * 4)) != -1000.0f) {
                if (effect != 0) {
                    *(float *)((unsigned char *)effect + 0xA5C) = surfaceLevel;
                }
            }
            if (effect != 0) {
                sefSetRecordOwner(effect, actor);
            }
        }
        soundCueValue = (float)soundCue;
        *(float *)((unsigned char *)motions + 0x3FD0) = *(float *)((unsigned char *)actor + 0x5D0);
        if (soundCueValue != *(float *)((unsigned char *)motions + 0x3FD4)) {
            if (soundCue < 0) {
                *(float *)((unsigned char *)motions + 0x3FD4) = -1.0f;
                return;
            }
            if (stepped == 1) {
                if (index == uwamonoCurrentIndex) {
                    seTonePlay(soundCue);
                } else {
                    int pan = seVoicePanVolumeFromPosition(position);
                    if (pan != 0) {
                        seTonePlay((pan << 8) + soundCue + 0x01000000);
                    }
                }
            }
            *(float *)((unsigned char *)motions + 0x3FD4) = soundCueValue;
        }
    }
}

/* Queues event kind `slot` (its script id from uwamonoEventIds) with two record ids.
 * The search handlers below all send kind 2. */
void uwamonoSendEvent(int slot, int first, int second)
{
    UwamonoEventArgs args;
    int eventId;

    eventId = uwamonoEventIds[slot];
    if (eventId != -1) {
        args.slot = slot;
        args.first = first;
        args.second = second;
        sbThreadEnqueue(D_003C6DC0, eventId, &args, 0);
    }
}

extern float D_0069B140;
int unduCheckAngle(f32 a, f32 b, f32 c);

void uwamonoSearchArcHit(int index, int other, int third)
{
    UwamonoObject *records;
    UwamonoObject *record;
    struct XenoActor *actor;
    float thirdWidth;
    UwamonoObject *thirdRecord;
    int thirdStride;
    int hit;

    records = uwamonoRecords;
    record = &records[index];
    actor = record->actor;
    hit = uwamonoCheckCollision(index, other, third);
    thirdStride = third * 17;
    if (hit != 0) {
        thirdRecord = &records[third];
        thirdWidth = thirdRecord->turnTo.x;
        if (!(thirdWidth < D_0069B140)
            || unduCheckAngle(actor->direction.y,
                              records[other].turnFrom.x - thirdWidth * 0.5f,
                              records[other].turnFrom.x + thirdWidth * 0.5f) != 0) {
            uwamonoSendEvent(2, records[actor->moved].id, records[other].id);
        }
    }
}

extern u16 textOptionFlags[];
extern int D_002B5D98[];

void uwamonoSearchFacingHit(int index, int other, int third)
{
    UwamonoObject *records;
    UwamonoObject *record;
    struct XenoActor *actor;
    UwamonoObject *thirdRecord;
    float thirdWidth;
    float halfWidth;
    float upper;
    float lower;

    records = uwamonoRecords;
    record = &records[index];
    actor = record->actor;
    if ((D_002B5D98[0] & 0x20000000) != 0) {
        return;
    }
    if ((textOptionFlags[0] & 0x20) == 0) {
        return;
    }
    if (uwamonoCheckCollision(index, other, third) == 0) {
        return;
    }
    records = uwamonoRecords;
    thirdRecord = &uwamonoRecords[third];
    thirdWidth = thirdRecord->turnTo.x;
    halfWidth = thirdWidth * 0.5f;
    upper = uwamonoRecords[other].turnFrom.x + halfWidth;
    lower = uwamonoRecords[other].turnFrom.x - halfWidth;
    if (unduCheckAngle(actor->rotation.y, lower, upper) == 0) {
        return;
    }
    uwamonoSendEvent(2, records[actor->moved].id, records[other].id);
}

void func_00207938(int index, int other, int third)
{
    UwamonoObject *record;
    struct XenoActor *actor;

    record = &uwamonoRecords[index];
    actor = record->actor;
    if (uwamonoCheckCollision(index, other, third) != 0) {
        uwamonoSendEvent(2, uwamonoRecords[actor->moved].id, uwamonoRecords[other].id);
    }
}

void func_002079F0(int index, int other, int third)
{
    UwamonoObject *const records = uwamonoRecords;
    UwamonoObject *record;
    struct XenoActor *actor;
    UwamonoObject *thirdRecord;
    float thirdWidth;
    float halfWidth;
    float upper;
    float lower;
    float otherAngle;

    record = &records[index];
    actor = record->actor;
    if ((D_002B5D98[0] & 0x20000000) != 0) {
        return;
    }
    if ((textOptionFlags[0] & 0x20) == 0) {
        return;
    }
    if (uwamonoCheckCollision(index, other, third) == 0) {
        return;
    }
    thirdRecord = &records[third];
    thirdWidth = thirdRecord->turnTo.x;
    halfWidth = thirdWidth * 0.5f;
    otherAngle = uwamonoRecords[other].turnFrom.x;
    lower = otherAngle - halfWidth;
    upper = otherAngle + halfWidth;
    if (unduCheckAngle(actor->rotation.y, lower, upper) == 0) {
        return;
    }
    uwamonoSendEvent(2, records[actor->moved].id, records[other].id);
}

struct SharedFlags {
    unsigned char unmodeled_00[6];
    unsigned short searchDisabled;
    int flags;
    unsigned char unmodeled_0C[0x10];
    int lockedTarget;
};

extern struct SharedFlags gameWork;
extern PadRecord PadData[];
extern unsigned char D_0069D456;
int unduCheckInsideFan(int arg0, AlignedVector4 *arg1, AlignedVector4 *arg2, int arg3, float arg4, float arg5, float arg6, float arg7);

void uwamonoSearchCircleFan(int index, int other, int third) {
    struct XenoActor *actor = uwamonoRecords[index].actor;

    if ((gameWork.flags & 0x20000000) == 0) {
        if ((PadData[0].pressed & PAD_CIRCLE) != 0) {
            float angle;
            float radius;
            angle = uwamonoRecords[other].turnTo.x / 3.14159265f * 180.0f;
            radius = actor->radius;
            if (unduCheckInsideFan(-1, &actor->position, &uwamonoRecords[other].position, -1, actor->rotation.y, UWAMONO_SEARCH_FAN_ANGLE, angle, radius) != 0) {
                if (uwamonoCheckCollision(index, other, third) != 0) {
                    uwamonoSendEvent(2, uwamonoRecords[actor->moved].id, uwamonoRecords[other].id);
                }
            }
        }
    }
}

void uwamonoSearchPlainHit(int index, int other, int third) {
    struct XenoActor *actor;

    actor = uwamonoRecords[index].actor;
    if (uwamonoCheckCollision(index, other, third) != 0) {
        uwamonoSendEvent(2, uwamonoRecords[actor->moved].id, uwamonoRecords[other].id);
    }
}

void func_00207D58(int index, int other, int third) {
    struct XenoActor *actor = uwamonoRecords[index].actor;

    if ((gameWork.flags & 0x20000000) == 0) {
        if ((PadData[0].pressed & PAD_CIRCLE) != 0) {
            float angle;
            float radius;
            angle = uwamonoRecords[other].turnTo.x / 3.14159265f * 180.0f;
            radius = actor->radius;
            if (unduCheckInsideFan(-1, &actor->position, &uwamonoRecords[other].position, -1, actor->rotation.y, UWAMONO_SEARCH_FAN_ANGLE, angle, radius) != 0) {
                if (uwamonoCheckCollision(index, other, third) != 0) {
                    uwamonoSendEvent(2, uwamonoRecords[actor->moved].id, uwamonoRecords[other].id);
                }
            }
        }
    }
}

void uwamonoSearchCircleFacing(int index, int other, int third) {
    struct XenoActor *actor = uwamonoRecords[index].actor;

    if ((gameWork.flags & 0x20000000) == 0) {
        if ((PadData[0].pressed & PAD_CIRCLE) != 0) {
            int otherIndex = other;
            if (uwamonoCheckCollision(index, other, third) != 0) {
                float centerAngle = uwamonoRecords[otherIndex].turnFrom.x;
                struct UwamonoObject *thirdRecord = &uwamonoRecords[third];
                float halfWidth = thirdRecord->turnTo.x * 0.5f;
                if (unduCheckAngle(actor->rotation.y, centerAngle - halfWidth, centerAngle + halfWidth) != 0) {
                    uwamonoSendEvent(2, uwamonoRecords[actor->moved].id, uwamonoRecords[otherIndex].id);
                }
            }
        }
    }
}

void func_00208008(int index, int other, int third) {
    struct XenoActor *actor;

    actor = uwamonoRecords[index].actor;
    if (uwamonoCheckCollision(index, other, third) != 0) {
        uwamonoSendEvent(2, uwamonoRecords[actor->moved].id, uwamonoRecords[other].id);
    }
}

void func_002080C0(int index, int other, int third) {
    struct XenoActor *actor;

    actor = uwamonoRecords[index].actor;
    if (uwamonoCheckCollision(index, other, third) != 0) {
        uwamonoSendEvent(2, uwamonoRecords[actor->moved].id, uwamonoRecords[other].id);
    }
}

void func_00208178(int index, int other, int third) {
    struct XenoActor *actor;

    actor = uwamonoRecords[index].actor;
    if (uwamonoCheckCollision(index, other, third) != 0) {
        uwamonoSendEvent(2, uwamonoRecords[actor->moved].id, uwamonoRecords[other].id);
    }
}

void uwamonoSelectTarget(int index) {
    struct XenoActor *actor;
    struct UwamonoObject *records;
    float centerY;
    float halfHeight;
    int candidate;
    int linkedIndex;

    actor = uwamonoRecords[index].actor;
    linkedIndex = -1;
    if (gameWork.searchDisabled != 0) {
        return;
    }
    if ((gameWork.flags & 0x20) != 0) {
        if (gameWork.lockedTarget != -1) {
            return;
        }
    }
    for (candidate = 0; candidate < UWAMONO_RECORD_COUNT; candidate++) {
        if (uwamonoRecords[candidate].kind != 7) {
            continue;
        }
        if (uwamonoRecords[candidate].search.shape == 0) {
            continue;
        }
        if (uwamonoRecords[candidate].search.shape == 10) {
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
        linkedIndex = uwamonoRecords[candidate].linkIndex;
        if (uwamonoCheckCollision(index, candidate, linkedIndex) == 1) {
            break;
        }
    }
    if (candidate == UWAMONO_RECORD_COUNT || candidate == -1 || linkedIndex == -1) {
        *(int *)actor->unmodeled_790 = -1;
        return;
    }
    D_0069D456 = 0;
    records = uwamonoRecords;
    switch (records[candidate].search.shape) {
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
            func_00207938(index, candidate, linkedIndex);
        }
        break;
    case 4:
        func_002079F0(index, candidate, linkedIndex);
        break;
    case 5:
        uwamonoSearchCircleFan(index, candidate, linkedIndex);
        break;
    case 6:
        uwamonoSearchPlainHit(index, candidate, linkedIndex);
        break;
    case 7:
        if (*(int *)actor->unmodeled_790 != candidate) {
            func_00208008(index, candidate, linkedIndex);
        }
        break;
    case 11:
        if (*(int *)actor->unmodeled_790 != candidate) {
            func_002080C0(index, candidate, linkedIndex);
        }
        break;
    case 8:
        uwamonoSearchCircleFacing(index, candidate, linkedIndex);
        break;
    case 9:
        func_00208178(index, candidate, linkedIndex);
        break;
    case 10:
        break;
    }
    *(int *)actor->unmodeled_790 = candidate;
    xglFontDebugPrintf(8, 128, "\xa5\xed\xa5\xb1\xa1\xbc\xa5\xbf = %3d,%3d", candidate, linkedIndex);
}
