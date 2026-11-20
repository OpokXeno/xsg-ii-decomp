#include "common.h"
#include "shared.h"
#include "main/runtime_enepc.h"
#include "main/runtime_uwamono_door.h"
#include "main/runtime.h"
#include "ov01/sb_vm.h"
#include "ov01/sb_natives.h"
#include "main/get_ladder.h"
#include "main/game_camera.h"
#include "main/uwamono_objx.h"
#include "main/init_uwamono_sys.h"
#include "main/sef.h"
#include "main/srs_file.h"

/* Bytes per uwamonoSlotTable entry and per uwamonoRecords record. */
#define UWAMONO_RECORD_SIZE 0x870

/* Record motion kind (+0x5C0): 1 moves between two points, anything else rotates. */
enum {
    UWAMONO_MOTION_MOVE = 1
};


extern float sinf(float angle);
extern float cosf(float angle);

/* The two doublewords of a vector the original copies with ld/sd. */
#define UWAMONO_WORDS(vector) (((DoublewordVector4 *) &(vector))->words)

int uwamonoSetDoorAll(SbThread *thread)
{
    short suppliedCount = 0;
    int recordIndex;
    int shortKey;
    int argIndex;
    float motionDelta[3];
    float scaledValue;
    UwamonoRecord *record;
    UwamonoRecord *linkedRecord;

    if (VM_PLUGIN_CNT_GET(thread) != 13) {
        sbSlotJumpRequest(thread, 2);
        return 0;
    }
    shortKey = vmArgGet(thread, 1);
    recordIndex = uwamonoFindIndexByShortKey(shortKey);
    record = &uwamonoRecords[recordIndex];
    if (record->partnerLink != -1) {
        linkedRecord = &uwamonoRecords[record->partnerLink];
    } else {
        linkedRecord = record;
    }

    for (argIndex = 1; argIndex < 13; argIndex++) {
        if (vmArgTagIsZero(thread, argIndex) == 0) {
            suppliedCount++;
        }
    }
    if (suppliedCount == 2) {
        if (vmArgTagIsZero(thread, 3) == 0) {
            int doorState = vmArgGet(thread, 3);

            linkedRecord->timer = doorState;
            record->timer = doorState;
            return 1;
        }
        if (vmArgTagIsZero(thread, 12) == 0) {
            int openerKind = vmArgGet(thread, 12);

            linkedRecord->openerKind = openerKind;
            record->openerKind = openerKind;
            return 1;
        }
    }

    {
        int motionKind;

        if (vmArgTagIsZero(thread, 2) != 0) {
            motionKind = (unsigned char) record->motionKind;
        } else {
            motionKind = vmArgGet(thread, 2);
        }
        record->motionKind = motionKind;
    }
    {
        int doorState;

        if (vmArgTagIsZero(thread, 3) != 0) {
            doorState = (unsigned short) record->timer;
        } else {
            doorState = vmArgGet(thread, 3);
        }
        linkedRecord->timer = doorState;
        record->timer = doorState;
    }

    if (vmArgTagIsZero(thread, 8) != 0) {
        scaledValue = record->openDistance;
    } else {
        scaledValue = (float) vmArgGet(thread, 8) / 100.0f;
    }
    linkedRecord->openDistance = scaledValue;
    record->openDistance = scaledValue;

    if (vmArgTagIsZero(thread, 9) != 0) {
        scaledValue = record->extent.y;
    } else {
        scaledValue = (float) vmArgGet(thread, 9) / 100.0f;
    }
    record->extent.y = scaledValue;

    {
        float extent;

        if (vmArgTagIsZero(thread, 10) != 0) {
            extent = record->extent.x;
        } else {
            extent = (float) vmArgGet(thread, 10) / 100.0f * 0.5f;
        }
        record->extent.x = extent;
    }

    if (vmArgTagIsZero(thread, 11) != 0) {
        scaledValue = record->swingLength;
    } else {
        scaledValue = (float) vmArgGet(thread, 11) / 100.0f;
    }
    record->swingLength = scaledValue;

    {
        int openerKind;

        if (vmArgTagIsZero(thread, 12) != 0) {
            openerKind = (unsigned char) record->openerKind;
        } else {
            openerKind = vmArgGet(thread, 12);
        }
        linkedRecord->openerKind = openerKind;
        record->openerKind = openerKind;
    }
    {
        int value624;

        if (vmArgTagIsZero(thread, 13) != 0) {
            value624 = record->moveSound;
        } else {
            value624 = vmArgGet(thread, 13);
        }
        linkedRecord->moveSound = value624;
        record->moveSound = value624;
    }

    if (vmArgTagIsZero(thread, 5) != 0 && vmArgTagIsZero(thread, 6) != 0 && vmArgTagIsZero(thread, 7) != 0) {
        float rotationY = record->rotation.y;
        float halfPi = 1.5707964f;
        double sine = sinf(rotationY + halfPi);
        double cosine;
        double reach = record->swingLength;

        if (reach < 0.0) {
            reach = -reach;
        }
        motionDelta[0] = sine * reach;
        motionDelta[1] = 0.0f;
        cosine = cosf(record->rotation.y + halfPi);
        reach = record->swingLength;
        if (reach < 0.0) {
            reach = -reach;
        }
        motionDelta[2] = cosine * reach;
    } else {
        motionDelta[0] = (float) vmArgGet(thread, 5) / 100.0f;
        motionDelta[1] = (float) vmArgGet(thread, 6) / 100.0f;
        motionDelta[2] = (float) vmArgGet(thread, 7) / 100.0f;
    }

    record->openState = 1;
    record->moveTicks = -1;
    if (record->timer == 0 && record->partnerLink != -1) {
        uwamonoRecords[record->partnerLink].timer = 0;
    }

    if (record->motionKind == UWAMONO_MOTION_MOVE) {
        record->closedPosition.vector.x = record->position.x;
        record->closedPosition.vector.y = record->position.y;
        record->closedPosition.vector.z = record->position.z;
        record->openPosition.vector.x = record->position.x + motionDelta[0];
        record->openPosition.vector.y = record->position.y + motionDelta[1];
        record->openPosition.vector.z = record->position.z + motionDelta[2];
        uwamonoClearMoveResetWord(recordIndex);
        uwamonoAddMoveStep(recordIndex, 20.0f, 1.0f);
        uwamonoAddMoveStep(recordIndex, 5.0f, 0.5f);
        uwamonoAddMoveStep(recordIndex, 55.0f, 5.0f);
        uwamonoAddMoveStep(recordIndex, 5.0f, 3.0f);
        uwamonoAddMoveStep(recordIndex, 5.0f, 2.0f);
        uwamonoAddMoveStep(recordIndex, 5.0f, 1.0f);
        uwamonoAddMoveStep(recordIndex, 5.0f, 0.5f);
    } else {
        u64 copyWord;

        record->closedRotation.x = record->rotation.x;
        record->closedRotation.y = record->rotation.y;
        record->closedRotation.z = record->rotation.z;
        /* Both move points start at the position, copied as doublewords. Two
           statements are no-ops kept for the original store order: the low word
           is read back from the first copy (the index is always 0) and the high
           word is stored back to the position between the last two stores. */
        copyWord = UWAMONO_WORDS(record->position)[0];
        UWAMONO_WORDS(record->openPosition)[0] = copyWord;
        copyWord = UWAMONO_WORDS(record->openPosition)[(recordIndex & 0xFF) >> 8];
        UWAMONO_WORDS(record->closedPosition)[0] = copyWord;
        UWAMONO_WORDS(record->openPosition)[1] = UWAMONO_WORDS(record->position)[1];
        UWAMONO_WORDS(record->closedPosition)[1] = UWAMONO_WORDS(record->openPosition)[1];
        record->openRotation.x = record->rotation.x + (motionDelta[0] + motionDelta[0]);
        record->openRotation.y = record->rotation.y + (motionDelta[1] + motionDelta[1]);
        UWAMONO_WORDS(record->position)[1] = UWAMONO_WORDS(record->openPosition)[1];
        record->openRotation.z = record->rotation.z + (motionDelta[2] + motionDelta[2]);
        uwamonoClearRotateResetWord(recordIndex);
        uwamonoAddRotateStep(recordIndex, 20.0f, 1.0f);
        uwamonoAddRotateStep(recordIndex, 5.0f, 0.5f);
        uwamonoAddRotateStep(recordIndex, 55.0f, 5.0f);
        uwamonoAddRotateStep(recordIndex, 5.0f, 3.0f);
        uwamonoAddRotateStep(recordIndex, 5.0f, 2.0f);
        uwamonoAddRotateStep(recordIndex, 5.0f, 1.0f);
        uwamonoAddRotateStep(recordIndex, 5.0f, 0.5f);
    }
    return 1;
}

/* Called with four arguments by the original; uwamono_record.c defines it with three (see the note below). */
void uwamonoStartPairMove(int index, int a, int b, int c);

int uwamonoSetKowaremonoAll(SbThread *thread)
{
    int recordIndex;
    int shortKey;
    int value;
    float scale;
    int effectKey;
    int enemyLink;
    int itemLink;
    UwamonoRecord *record;

    if (VM_PLUGIN_CNT_GET(thread) < 9) {
        sbSlotJumpRequest(thread, 2);
        return 0;
    }
    shortKey = vmArgGet(thread, 1);
    recordIndex = uwamonoFindIndexByShortKey(shortKey);
    {
        record = &uwamonoRecords[recordIndex];
        if (vmArgTagIsZero(thread, 2) != 0) {
            value = (unsigned char) record->searchMode;
        } else {
            value = vmArgGet(thread, 2);
        }
        record->searchMode = value;
        if (vmArgGetHigh(thread, 3) == 3) {
            if (vmArgTagIsZero(thread, 3) != 0) {
                effectKey = record->effectId;
            } else {
                effectKey = srsEffectNameToID(vmKeyedEntryGet(thread, vmArgGet(thread, 3)));
            }
            record->effectId = effectKey;
        } else {
            if (vmArgTagIsZero(thread, 3) != 0) {
                effectKey = record->effectId;
            } else {
                effectKey = vmArgGet(thread, 3);
            }
            record->effectId = effectKey;
        }
        sefStartEffectKeyDefault(0, record->effectId);
        if (vmArgTagIsZero(thread, 4) != 0) {
            value = record->searchFlag;
        } else {
            value = vmArgGet(thread, 4);
        }
        record->searchFlag = value;
        if (vmArgTagIsZero(thread, 5) != 0) {
            enemyLink = record->enemyLink;
        } else {
            enemyLink = uwamonoFindIndexByShortKey(vmArgGet(thread, 5));
        }
        record->enemyLink = enemyLink;
        if (vmArgTagIsZero(thread, 6) != 0) {
            itemLink = record->itemLink;
        } else {
            itemLink = uwamonoFindIndexByShortKey(vmArgGet(thread, 6));
        }
        record->itemLink = itemLink;
        if (vmArgTagIsZero(thread, 7) != 0) {
            value = record->level;
        } else {
            value = vmArgGet(thread, 7);
        }
        record->level = value;
        if (vmArgTagIsZero(thread, 8) != 0) {
            value = record->triggerSound;
        } else {
            value = vmArgGet(thread, 8);
        }
        record->triggerSound = value;
        if (vmArgTagIsZero(thread, 9) != 0) {
            scale = record->focusOffset.x;
        } else {
            scale = (float) vmArgGet(thread, 9) / 100.0f;
        }
        record->focusOffset.x = scale;
        if (vmArgTagIsZero(thread, 10) != 0) {
            scale = record->focusOffset.y;
        } else {
            scale = (float) vmArgGet(thread, 10) / 100.0f;
        }
        record->focusOffset.y = scale;
        if (vmArgTagIsZero(thread, 11) != 0) {
            scale = record->focusOffset.z;
        } else {
            scale = (float) vmArgGet(thread, 11) / 100.0f;
        }
        record->focusOffset.z = scale;
        if (vmArgTagIsZero(thread, 12) == 0) {
            record->cellMask = 0;
            record->cellMask |= (vmArgGet(thread, 12) & 1);
            record->cellMask |= (((vmArgGet(thread, 12) / 10) & 1) * 2);
            record->cellMask |= (((vmArgGet(thread, 12) / 100) & 1) * 4);
            record->cellMask |= (((vmArgGet(thread, 12) / 1000) & 1) * 8);
            record->cellMask |= (((vmArgGet(thread, 12) / 10000) & 1) * 0x10);
            record->cellMask |= (((vmArgGet(thread, 12) / 100000) & 1) << 5);
            record->cellMask |= (((vmArgGet(thread, 12) / 1000000) & 1) << 6);
            record->cellMask |= (((vmArgGet(thread, 12) / 10000000) & 1) << 7);
        }
        /* Structured if/else folds the itemLink argument of the pair move to -1; the original reloads it, so the jump stays. */
        while (record->itemLink != -1) {
            uwamonoRecords[record->itemLink].flags &= ~2;
            uwamonoRecords[record->itemLink].flags |= 0x02000000;
            goto linkBDone;
        }
        if ((record->kind == MAP_KIND_MODEL) && ((unsigned int) (record->type - 4) < 3U)) {
            uwamonoStartPairMove(recordIndex, 3, 1, record->itemLink);
        }
linkBDone:
        if (record->enemyLink != -1) {
            uwamonoRecords[record->enemyLink].flags &= ~2;
            uwamonoRecords[record->enemyLink].flags |= 0x02000000;
        }
        return 1;
    }
}

/* The original calls these with extra arguments (uwamono_record.c defines uwamonoStartPairMove with 3 parameters and
 * uwamonoTriggerRecord and uwamonoMoveSelection with 1); the argument loads are part of the object, so these
 * prototypes stay local instead of including uwamono_record.h. */
void uwamonoSetPairMode(int, int);
void uwamonoTriggerRecord(int, int);
void uwamonoMoveSelection(int, int);

/*
 * Dispatches script argument 2 of this id to one of three helpers after the
 * index lookup on argument 1. Returns 1 when the call was handled.
 */
int uwamonoSendSignal(SbThread *thread)
{
    int recordIndex;
    int signalId;

    if (VM_PLUGIN_CNT_GET(thread) != 2) {
        sbSlotJumpRequest(thread, 2);
        return 0;
    }
    recordIndex = uwamonoFindIndexByShortKey(vmArgGet(thread, 1));
    signalId = vmArgGet(thread, 2);
    recordIndex = recordIndex | ((signalId & 0xFF) >> 8);
    if (recordIndex != -1) {
        if ((unsigned int) signalId < 8) {
            switch (signalId) {
            case 0:
            case 1:
            case 2:
                uwamonoSetPairMode(recordIndex, signalId);
                break;
            case 3:
            case 4:
            case 5:
                break;
            case 6:
                uwamonoTriggerRecord(recordIndex, signalId);
                break;
            case 7:
                uwamonoMoveSelection(1, signalId);
                break;
            }
        }
        return 1;
    }
    return 0;
}


/* In uwamonoSetCollisionSize and setCollisionScale the extent.y/z/w words of a record are addressed as byte offsets from uwamonoRecords:
 * the field-address forms (&uwamonoRecords[i].extent.y) schedule the address arithmetic differently. */
typedef union WordUnion {
    float f;
    int i;
} WordUnion;

int uwamonoSetCollisionSize(SbThread *thread)
{
    int recordIndex;
    int shortKey;
    int again;

    if (VM_PLUGIN_CNT_GET(thread) == 4) {
        goto start;
    }
    sbSlotJumpRequest(thread, 2);
    return 0;
    /* Tried without the jumps: the plain if + do/for-while(again) forms compile to the same body but reorg then fills the delay slot of
     * the count test differently (beq instead of beql with the li a1,1 copied from the target) and the arg setup order of the first
     * vmArgGet changes; entering the loop by jump keeps `start:` a block of its own, so these two jumps stay. */
    for (;;) {
    start:
    shortKey = vmArgGet(thread, 1);
retry:
    again = 0;
    recordIndex = uwamonoFindIndexByShortKey(shortKey);
    {
        UwamonoScaleRecord *scaleRecord = &uwamonoScaleRecords[recordIndex];
        float scaleValue;

        if (vmArgTagIsZero(thread, 2) != 0) {
            scaleValue = scaleRecord->scale;
        } else {
            float factor = (float) vmArgGet(thread, 2);

            scaleValue = factor / 100.0f * 0.5f;
        }
        scaleRecord->scale = scaleValue;
    }
    {
        int extentYOffset = 0x584 + recordIndex * UWAMONO_RECORD_SIZE;
        float *extentY = (float *) ((unsigned char *) uwamonoRecords + extentYOffset);
        float value;

        if (vmArgTagIsZero(thread, 3) != 0) {
            value = *extentY;
        } else {
            value = (float) vmArgGet(thread, 3) / 100.0f;
        }
        *extentY = value;
    }
    {
        int extentZOffset = recordIndex * UWAMONO_RECORD_SIZE + 0x588;
        float *extentZ = (float *) ((unsigned char *) uwamonoRecords + extentZOffset);
        float value;

        if (vmArgTagIsZero(thread, 4) != 0) {
            value = *extentZ;
        } else {
            float factor = (float) vmArgGet(thread, 4);

            value = factor / 100.0f * 0.5f;
        }
        *extentZ = value;
    }
    {
        int extentWOffset = recordIndex * UWAMONO_RECORD_SIZE + 0x58C;
        WordUnion *word = (WordUnion *) ((unsigned char *) uwamonoRecords + extentWOffset);
        UwamonoRecord *record = &uwamonoRecords[recordIndex];
        struct XenoActor *actorBody;

        word->f = 1.0f;
        actorBody = uwamonoRecords[recordIndex].body.actor;
        if (actorBody != 0) {
            actorBody->radius = uwamonoRecords[recordIndex].extent.x;
            actorBody->height = record->extent.y;
        }
    }
    if (again) {
        goto retry;
    }
    return 1;
    }
}

int setCollisionScale(SbThread *thread)
{
    int recordIndex;
    UwamonoRecord *records;

    if (VM_PLUGIN_CNT_GET(thread) != 4) {
        sbSlotJumpRequest(thread, 2);
        return 0;
    }
    records = uwamonoRecords;
    recordIndex = uwamonoFindIndexByShortKey(vmArgGet(thread, 1));
    {
        UwamonoScaleRecord *scaleRecord = &uwamonoScaleRecords[recordIndex];
        float value;

        if (vmArgTagIsZero(thread, 2) != 0) {
            value = scaleRecord->scale;
        } else {
            float factor = (float) vmArgGet(thread, 2);

            value = scaleRecord->scale * factor / 100.0f;
        }
        scaleRecord->scale = value;
    }
    {
        unsigned int extentYOffset = recordIndex * UWAMONO_RECORD_SIZE + 0x584;
        float *extentY = (float *) ((unsigned char *) uwamonoRecords + extentYOffset);
        float value;

        if (vmArgTagIsZero(thread, 3) != 0) {
            value = *extentY;
        } else {
            value = *extentY * (float) vmArgGet(thread, 3) / 100.0f;
        }
        *extentY = value;
    }
    {
        unsigned int extentZOffset = recordIndex * UWAMONO_RECORD_SIZE + 0x588;
        float *extentZ = (float *) ((unsigned char *) uwamonoRecords + extentZOffset);
        float value;

        if (vmArgTagIsZero(thread, 4) != 0) {
            value = *extentZ;
        } else {
            value = *extentZ * (float) vmArgGet(thread, 4) / 100.0f;
        }
        *extentZ = value;
    }
    {
        struct XenoActor *actorBody;

        records = &uwamonoRecords[recordIndex];
        records->extent.w = 1.0f;
        do {
            actorBody = uwamonoRecords[recordIndex].body.actor;
            if (actorBody != 0) {
                actorBody->radius = uwamonoRecords[recordIndex].extent.x;
            }
        } while (0);
    }
    return 1;
}

extern SbNativeMethod uwamonoNativeTable[];
extern char uwamonoModuleName[];

/* Registers the uwamono native table uwamonoNativeTable with the module name uwamonoModuleName. */
void uwamonoRegisterNatives(void) {
    sbNativeOwnerAdd(uwamonoModuleName, uwamonoNativeTable);
}

char uwamonoModuleName[8] = "uwamono";

/* .data of this file (0x3E6FF0): the uwamono script natives. */

/* Native table of the uwamono module. */
SbNativeMethod uwamonoNativeTable[6] = {
    { "setDoorAll", uwamonoSetDoorAll },
    { "setKowaremonoAll", uwamonoSetKowaremonoAll },
    { "SendSignal", uwamonoSendSignal },
    { "setCollisionSize", uwamonoSetCollisionSize },
    { "setCollisionScale", setCollisionScale },
    { 0, 0 }
};
