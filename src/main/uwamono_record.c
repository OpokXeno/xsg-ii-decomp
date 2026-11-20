#include "common.h"
#include "main/get_ladder.h"
#include "main/enemy_1.h"
#include "main/pause_menu_page_uwamono.h"
#include "main/xenoActor.h"
#include "shared.h"
#include "main/msg_state.h"
#include "main/game_camera.h"
#include "main/game_data.h"
#include "main/undulate.h"
#include "main/ytachi_test.h"
#include "ov01/data_box.h"
#include "ov01/data_record_get.h"
#include "main/vibration.h"
#include "main/control_entry.h"
#include "main/xgl_font.h"
#include "main/uwamono_record.h"
#include "main/uwamono_objx.h"
#include "main/game_map_object.h"
#include "main/get.h"
#include "main/init_uwamono_sys.h"
#include "main/uwamono_event_check.h"
#include "main/se_bank_load.h"
#include "main/menu_selection.h"
#include "main/sef.h"

/* uwamono records: kind MAP_KIND_FREE is an unused record, MAP_KIND_ACTOR a talk target, MAP_KIND_MODEL a searched object (ytachi_test.h). */
#define UWAMONO_RECORD_COUNT 256
/* record state: 0 out of reach, 1 selected, 2 in reach (ytachi_test.h). */
#define UWAMONO_STATE_OUT_OF_REACH 0
#define UWAMONO_STATE_SELECTED 1
#define UWAMONO_STATE_IN_REACH 2
/* cellMask of a record that every cell reaches. */
#define UWAMONO_CELL_MASK_ALL 0xFF
/* effect spawned for the selection marker, stored at record +0x640. */
#define UWAMONO_EFFECT_SELECTION_MARKER 0x100C801E


/* The two poses of uwamonoSetPairMode, read through this union view (alias set 0). */
typedef union UwamonoPoses {
    Vector4 point[2];
    unsigned int bits[8];
} UwamonoPoses;

/* One-site view of a record whose closed/open rotation pair (+0x5F0) is a UwamonoPoses member: reading the first
 * component through it keeps the address as record + row offset (the order the original emitted) and lets the load
 * schedule ahead of the stores before it. */
typedef struct UwamonoRotationView {
    unsigned char unmodeled_000[0x5F0];
    UwamonoPoses rest;
} UwamonoRotationView;

/* Counts the records from the first one up to the first free record (kind 0), at most 256. */
int uwamonoCountLeadingInUse(void)
{
    int count = 0;
    unsigned char *kindPtr;

    if (uwamonoRecords[0].kind != MAP_KIND_FREE)
    {
        kindPtr = &uwamonoRecords[0].kind;
        for (;;)
        {
            count += 1;
            kindPtr += sizeof(UwamonoRecord);
            if (count < UWAMONO_RECORD_COUNT)
            {
                if (*kindPtr != 0)
                {
                    continue;
                }
            }
            break;
        }
    }
    return count;
}


void uwamonoReleaseRecord(int index)
{
    UwamonoRecord *record;
    int recordIndex;
    int ownedIndex;

    if (index == -1)
    {
        return;
    }
    for (recordIndex = 0; recordIndex < UWAMONO_RECORD_COUNT; recordIndex++)
    {
        record = &uwamonoRecords[recordIndex];
        if (record->kind != MAP_KIND_FREE && (record->parent == index || recordIndex == index))
        {
        for (ownedIndex = 0; ownedIndex < UWAMONO_RECORD_COUNT; ownedIndex++)
        {
            if (uwamonoRecords[ownedIndex].parent == recordIndex)
            {
                uwamonoReleaseRecord(ownedIndex);
            }
        }
        if (record->releaseNextIndex != -1)
        {
            uwamonoReleaseRecord(record->releaseNextIndex);
        }
        if (uwamonoFindSelected() == index)
        {
            UnlockMapUnit();
        }
        switch (record->kind)
        {
        case MAP_KIND_ACTOR:
        case 4:
        case 5:
            uwamonoRecords[recordIndex].body.actor->drawFlags |= 4;
            XenoActorRelease(uwamonoRecords[recordIndex].body.actor);
            record->body.actor = 0;
            break;
        case MAP_KIND_MODEL:
            if (record->key != -1)
            {
                mapSecondHideMask.bits[record->key >> 5] |= 1 << (record->key & 0x1F);
            }
            break;
        case 3:
            mapMainHideMask.bits[record->key >> 5] |= 1 << (record->key & 0x1F);
            break;
        case 6:
            break;
        }
        if (record->handles[0] != 0)
        {
            sefDeleteEffectCf(record->handles[0]);
        }
        if (record->handles[1] != 0)
        {
            sefDeleteEffectCf(record->handles[1]);
        }
        if (record->handles[2] != 0)
        {
            sefDeleteEffectCf(record->handles[2]);
        }
        record->kind = 0;
        record->handles[1] = 0;
        record->handles[2] = 0;
        record->handles[0] = 0;
        if (uwamonoRecords[recordIndex].body.motions != 0)
        {
            uwamonoRecords[recordIndex].body.motions->owner = -1;
        }
        uwamonoResetRecords(recordIndex, 1);
        }
    }
}

/* Index of the first record whose kind and key match, or 256 when none does. */
int uwamonoFindRecord(int kind, int key)
{
    int index;

    for (index = 0; index < UWAMONO_RECORD_COUNT; index++)
    {
        UwamonoRecord *record = &uwamonoRecords[index];

        if (record->kind == kind && record->key == key)
        {
            break;
        }
    }
    return index;
}

/* One 0x40C0-byte record of uwamonoStates; the leading id is -1 when the record is free. */
typedef struct SlotIdRecord {
    int id;
    unsigned char unmodeled_04[0x40C0 - 4];
} SlotIdRecord;


/* Counts the leading records of uwamonoStates whose id is not -1, at most 32. */
int uwamonoStateCountLeading(void)
{
    int count = 0;
    SlotIdRecord *record;

    if (((SlotIdRecord *) uwamonoStates)->id != -1)
    {
        record = (SlotIdRecord *) uwamonoStates;
        for (;;)
        {
            count += 1;
            record += 1;
            if (count < 0x20)
            {
                if (record->id != -1)
                {
                    continue;
                }
            }
            break;
        }
    }
    return count;
}

/* Counts the records whose kind is not 0 and whose state is 2. */
int uwamonoCountInReach(void)
{
    short count = 0;
    short index;

    for (index = 0; index < UWAMONO_RECORD_COUNT; index++)
    {
        UwamonoRecord *record = &uwamonoRecords[index];

        if (record->kind != MAP_KIND_FREE && uwamonoRecords[index].state == 2)
        {
            count++;
        }
    }
    return count;
}


/* Index of the in-use record with a nonzero state nearest to uwamonoCurrentIndex, or -1; the search starts at 1000.0f. */
int uwamonoFindNearest(void)
{
    int nearest = -1;
    float nearestDistance = 1000.0f;
    short index;

    for (index = 0; index < UWAMONO_RECORD_COUNT; index++)
    {
        UwamonoRecord *record = &uwamonoRecords[index];

        if (record->kind != MAP_KIND_FREE && uwamonoRecords[index].state != 0)
        {
            float distance = Get_Distance(&uwamonoRecords[uwamonoCurrentIndex].position, &uwamonoRecords[index].position);

            if (distance < nearestDistance)
            {
                nearestDistance = distance;
                nearest = index;
            }
        }
    }
    return nearest;
}


short uwamonoFindSelected(void)
{
    short result = -1;
    short index;

    for (index = 0; index < UWAMONO_RECORD_COUNT; index++)
    {
        UwamonoRecord *record = &uwamonoRecords[index];

        if (record->kind != MAP_KIND_FREE && record->kind != 4)
        {
            float distance;
            double height;
            float heightDistance;

            distance = Get_Distance3D(&uwamonoRecords[uwamonoCurrentIndex].body.actor->position, &uwamonoRecords[index].position);
            height = uwamonoRecords[uwamonoCurrentIndex].body.actor->position.y - uwamonoRecords[index].position.y;
            if (height < 0)
            {
                height = 0.0 - height;
            }
            heightDistance = height;
            if (!(gameWork.playerParams[0][0] < distance) && !(gameWork.playerParams[1][0] < heightDistance)
                && uwamonoRecords[index].state == 1)
            {
                result = index;
                break;
            }
        }
    }
    return result;
}

/* Releases the two handles of every record that is not free and not kind 4, then clears them. */
void UnlockMapUnit(void)
{
    UwamonoRecord *record = uwamonoRecords;
    int index;

    for (index = 0; index < UWAMONO_RECORD_COUNT; index++)
    {
        if (record->kind != MAP_KIND_FREE && record->kind != 4)
        {
            void *handle_63c = record->handles[1];
            void *handle_640;

            record->state = UWAMONO_STATE_OUT_OF_REACH;
            if (handle_63c != 0)
            {
                sefDeleteEffectCf(handle_63c);
            }
            handle_640 = record->handles[2];
            if (handle_640 != 0)
            {
                sefDeleteEffectCf(handle_640);
            }
            record->handles[2] = 0;
            record->handles[1] = 0;
        }
        record++;
    }
}


void uwamonoUpdateInReach(struct XenoActor *actor, int index)
{
    UwamonoRecord *record;
    UwamonoRecord *object;
    int slot;
    DoublewordAlignedVector4 saved;
    Vector4 hit;
    Vector4 *actorPos;
    Vector4 *recordPos;
    int result;
    int count;
    double height;
    int state;
    float distance;
    float heightDistance;

    slot = index;
    object = &uwamonoRecords[slot];
    record = &uwamonoRecords[slot];
    if (record->kind == MAP_KIND_FREE || record->kind == 4 || record->kind == 6)
    {
        return;
    }
    if (record->flags & 0x2000000)
    {
        return;
    }
    if (gameWork.loadStage != 0)
    {
        return;
    }
    if (mapTextureData == 0)
    {
        return;
    }
    if (mainObject == 0)
    {
        return;
    }
    if (gameWork.control & CONTROL_MAP_LOADED)
    {
        if (gameWork.entryKey != -1)
        {
            return;
        }
    }
    actorPos = &actor->position;
    recordPos = &record->position;
    distance = Get_Distance3D(actorPos, recordPos);
    height = actor->position.y - object->position.y;
    if (height < 0)
    {
        height = -height;
    }
    heightDistance = height;
    saved = record->position;

    if ((unsigned int) (actor->kind - 0x10000) < 0x4000)
    {
        if (record->flags & 0x100000)
        {
            return;
        }
    }
    else
    {
        if (!(record->flags & 0x100000))
        {
            return;
        }
    }
    if (!(record->flags & 0x20000))
    {
        return;
    }
    result = 1;
    if (!(record->flags & 0x200000))
    {
        result = (signed char) AimMapUnitLookCheck(&uwamonoRecords[index]);
    }
    if (result == 1)
    {
        uwamonoDrawLine(actorPos, &saved, 7);
    }
    else if (uwamonoDrawDisabled == 0)
    {
        CrossPointUwamono(actorPos, &saved, actor->radius, &hit);
        uwamonoDrawLine(actorPos, &hit, 1);
    }

    if (distance <= gameWork.playerParams[0][0] && heightDistance <= gameWork.playerParams[1][0]
        && result == 1 && uwamonoLadderState.state == 0 && uwamonoIsActorInCells(index) == result)
    {
        if (record->state == UWAMONO_STATE_OUT_OF_REACH)
        {
            record->state = UWAMONO_STATE_IN_REACH;
            record->unmodeled_630 = 0;
            if (record->handles[1] == 0)
            {
                record->handles[1] = sefCreateEffectCf(0x100C801D, &saved, 0);
            }
            if (record->handles[2] == 0)
            {
                record->handles[2] = sefCreateEffectCf(0x100C801E, &saved, 0);
            }
            seVoiceStartDefault(0x2000003);
        }
        count = uwamonoCountInReach();
        if (count != 1)
        {
            return;
        }
        if (uwamonoFindSelected() != -1)
        {
            return;
        }
        record->state = count;
        if (record->handles[1] == 0)
        {
            record->handles[1] = sefCreateEffectCf(0x100C801D, &saved, 0);
        }
        if (record->handles[2] == 0)
        {
            record->handles[2] = sefCreateEffectCf(0x100C801E, &saved, 0);
        }
        sefDestroyRecordEffectsClearFlag(record->handles[1]);
        sefDestroyRecordEffectsClearFlag(record->handles[2]);
        return;
    }

    state = record->state;
    if (state == 1)
    {
        record->state = UWAMONO_STATE_OUT_OF_REACH;
        sefDeleteEffectCf(record->handles[1]);
        sefDeleteEffectCf(record->handles[2]);
        record->handles[2] = 0;
        record->handles[1] = 0;
        uwamonoMoveSelection(1);
    }
    else if (state == 2)
    {
        record->state = UWAMONO_STATE_OUT_OF_REACH;
        sefDeleteEffectCf(record->handles[1]);
        sefDeleteEffectCf(record->handles[2]);
        record->handles[2] = 0;
        record->handles[1] = 0;
    }
    else
    {
        record->state = UWAMONO_STATE_OUT_OF_REACH;
    }
}

/* The four words at +0x634 of a record (cellMask and the three effect handles), addressed as one block. */
typedef struct UwamonoHandleBlock {
    int cellMask;                       /* record +0x634 */
    void *handle_638;
    void *handle_63c;
    SefEffectRecord *selected;          /* record +0x640: the marker shown on the selected record */
} UwamonoHandleBlock;

/*
 * Field `member` of record `index`, reached as the original does: the record offset and the field offset are summed
 * as one integer and the table address is added last, so every use computes an address of its own (the copies of the
 * record pointer in uwamonoMoveSelection come from that). `uwamonoRecords[index].member` adds the table first.
 */
#define UWAMONO_FIELD(index, member)                                                                   \
    (*({                                                                                               \
        int byteOffset = (index) * sizeof(UwamonoRecord) + (int) &((UwamonoRecord *) 0)->member;       \
        (__typeof__(((UwamonoRecord *) 0)->member) *) ((char *) uwamonoRecords + byteOffset);          \
    }))

/*
 * Moves the selection by `step` records over the records in reach: forward for a positive step, backward for a
 * negative one, and to the nearest record for 0. Stepping puts the old selection back to "in reach", releases its
 * marker, spawns a new marker above it (scaled by the player's form) and selects the record found.
 */
void uwamonoMoveSelection(int step)
{
    int count = uwamonoCountInReach();
    short current = uwamonoFindSelected();
    int moved;
    int probe;
    int index;
    int probeSum;
    SefEffectRecord *marker;
    Vector4 position;

    if (current == -1)
    {
        if (count == 0)
        {
            return;
        }
        for (current = 0; current < UWAMONO_RECORD_COUNT; current++)
        {
            if (uwamonoRecords[current].kind != MAP_KIND_FREE
                && uwamonoRecords[current].kind != 4
                && uwamonoRecords[current].kind != 6
                && uwamonoRecords[current].state == UWAMONO_STATE_IN_REACH)
            {
                break;
            }
        }
    }
    if (step == 0)
    {
        int nearest = uwamonoFindNearest();

        if (nearest == current || nearest == -1)
        {
            return;
        }
        uwamonoRecords[current].state = UWAMONO_STATE_IN_REACH;
        current = nearest;
        uwamonoRecords[current].state = UWAMONO_STATE_SELECTED;
    }
    else if (step >= 0)
    {
        for (moved = 0; moved < step; moved++)
        {
            probe = 1;
            do
            {
                index = (current + probe) - ((current + probe) / 256) * 256;
                probe++;
                if (uwamonoRecords[index].kind != MAP_KIND_FREE && uwamonoRecords[index].kind != 4
                    && uwamonoRecords[index].state != UWAMONO_STATE_OUT_OF_REACH)
                {
                    UwamonoHandleBlock *handles;

                    uwamonoRecords[current].state = UWAMONO_STATE_IN_REACH;
                    handles = (UwamonoHandleBlock *) &UWAMONO_FIELD(current, cellMask);
                    sefDeleteEffectCf(handles->selected);
                    position.x = UWAMONO_FIELD(current, position.x) + UWAMONO_FIELD(current, focusOffset.x);
                    position.y = UWAMONO_FIELD(current, position.y) + UWAMONO_FIELD(current, focusOffset.y)
                                 + UWAMONO_FIELD(current, extent.y) * 0.5f;
                    position.z = UWAMONO_FIELD(current, position.z) + UWAMONO_FIELD(current, focusOffset.z);
                    marker = sefCreateEffectCf(UWAMONO_EFFECT_SELECTION_MARKER, &position, 0);
                    handles->selected = marker;
                    current = index;
                    marker->scale.x = marker->scale.y = marker->scale.z = uwamonoRecords[uwamonoCurrentIndex].form;
                    uwamonoRecords[current].state = UWAMONO_STATE_SELECTED;
                    break;
                }
            } while (probe < 257);
        }
    }
    else
    {
        for (moved = 0; moved < -step; moved++)
        {
            probe = 1;
            do
            {
                probeSum = current - probe + 256;
                index = probeSum - (probeSum / 256) * 256;
                probe++;
                if (uwamonoRecords[index].kind != MAP_KIND_FREE && uwamonoRecords[index].kind != 4
                    && uwamonoRecords[index].state != UWAMONO_STATE_OUT_OF_REACH)
                {
                    UwamonoHandleBlock *handles;

                    uwamonoRecords[current].state = UWAMONO_STATE_IN_REACH;
                    handles = (UwamonoHandleBlock *) &UWAMONO_FIELD(current, cellMask);
                    sefDeleteEffectCf(handles->selected);
                    position.x = UWAMONO_FIELD(current, position.x) + UWAMONO_FIELD(current, focusOffset.x);
                    position.y = UWAMONO_FIELD(current, position.y) + UWAMONO_FIELD(current, focusOffset.y)
                                 + UWAMONO_FIELD(current, extent.y) * 0.5f;
                    position.z = UWAMONO_FIELD(current, position.z) + UWAMONO_FIELD(current, focusOffset.z);
                    marker = sefCreateEffectCf(UWAMONO_EFFECT_SELECTION_MARKER, &position, 0);
                    handles->selected = marker;
                    current = index;
                    marker->scale.x = marker->scale.y = marker->scale.z = uwamonoRecords[uwamonoCurrentIndex].form;
                    uwamonoRecords[current].state = UWAMONO_STATE_SELECTED;
                    break;
                }
            } while (probe < 257);
        }
    }
    seVoiceStartDefault(0x02000004);
}

int fptodp(float value);
int dpcmp(int a, int b);
int dpsub(int a, int b);
float dptofp(int value);

typedef struct UwamonoBox {
    Vector4 min;
    Vector4 max;
} UwamonoBox;

typedef struct UwamonoCellBits {
    unsigned char bit[9];
} UwamonoCellBits;

extern const UwamonoCellBits uwamonoCellBitTable;

int uwamonoIsActorInCells(int index)
{
    UwamonoBox box;
    UwamonoCellBits cellBits;
    UwamonoRecord *target = &uwamonoRecords[index];
    struct XenoActor *actor = uwamonoRecords[uwamonoCurrentIndex].body.actor;
    float left;
    float right;
    float back;
    float front;
    int cellMask;
    int xCell;
    int zCell;
    int cellBit;

    cellBits = uwamonoCellBitTable;
    cellMask = target->cellMask;
    if (cellMask == UWAMONO_CELL_MASK_ALL)
    {
        return 1;
    }
    if (cellMask == 0)
    {
        return 0;
    }

    left = target->position.x - target->extent.x;
    right = target->position.x + target->extent.x;
    back = target->position.z - target->extent.z;
    front = target->position.z + target->extent.z;

    box.max.y = target->position.y;
    box.min.y = target->position.y;
    box.min.x = left - 0.5f;
    box.max.x = right + 0.5f;
    box.max.z = back;
    box.min.z = back;
    uwamonoDrawLine(&box.min, &box.max, 0);

    box.max.z = front;
    box.min.z = front;
    uwamonoDrawLine(&box.min, &box.max, 0);

    box.max.x = left;
    box.min.x = left;
    box.min.z = back - 0.5f;
    box.max.z = front + 0.5f;
    uwamonoDrawLine(&box.min, &box.max, 0);

    box.max.x = right;
    box.min.x = right;
    uwamonoDrawLine(&box.min, &box.max, 0);

    if (actor->position.x < left)
    {
        xCell = 0;
    }
    else if (right < actor->position.x)
    {
        xCell = 2;
    }
    else
    {
        xCell = 1;
    }

    if (actor->position.z < back)
    {
        zCell = 0;
    }
    else if (front < actor->position.z)
    {
        zCell = 2;
    }
    else
    {
        zCell = 1;
    }

    cellBit = 1 << cellBits.bit[zCell * 3 + xCell];

    return (target->cellMask & cellBit) != 0;
}


/* Index of the row whose key is `key`, or -1. The row count is the recordCount of the group uwamonoEntryCountPtr. */
int uwamonoFindEntry(int key)
{
    int result;
    int index;
    int found;

    found = -1;
    result = -1;
    if (uwamonoEntryCountPtr != 0)
    {
        for (index = 0; index < ((int) uwamonoEntryCountPtr->recordCount); index++)
        {
            if (uwamonoEntries[index].key == key)
            {
                found = index;
                break;
            }
        }
        result = found;
    }
    return result;
}

/* Angle codes are in 4096ths of a turn. */
extern float uwamonoTwoPi;
float uwamonoTwoPi = 6.28318548f;

/* Writes the position of the row `key` to out (w = 1) and returns its angle; -1.0f when the table is empty. */
float uwamonoGetEntryPose(int key, float *out)
{
    float result = -1.0f;
    int index;
    UwamonoEntry *entry;
    float z;

    if (uwamonoEntryCountPtr != 0)
    {
        index = uwamonoFindEntry(key);
        if (out != 0)
        {
            entry = (UwamonoEntry *) (index * 16 + (int) uwamonoEntries);
            out[0] = entry->x;
            out[1] = entry->y;
            z = entry->z;
            out[3] = 1.0f;
            out[2] = z;
        }
        result = (float) uwamonoEntries[index].angle * (1.0f / 4096.0f) * uwamonoTwoPi;
    }
    return result;
}

void uwamonoSetPairMode(int index, int mode)
{
    UwamonoRecord *pair[2];
    int recordIds[2];
    int ownRow;
    int ownZOff;
    int partnerZOff;
    int ownRowOff;
    int partnerRowOff;
    int partnerPoseOff;
    int partnerRow;
    int side;

    pair[0] = &uwamonoRecords[index];
    if (pair[0]->partnerLink != -1)
    {
        pair[1] = &uwamonoRecords[uwamonoRecords[index].partnerLink];
    }
    if (pair[0]->motionKind != 0)
    {
        recordIds[0] = index;
        recordIds[1] = uwamonoRecords[index].partnerLink;
        if (pair[0]->moveTicks == -1 && (pair[0]->partnerLink == -1 || pair[1]->moveTicks == -1))
        {
            if (mode == 2)
            {
                mode = (pair[0]->openState == 0);
            }
            if (pair[0]->openState != mode)
            {
                if (mode == 0)
                {
                    ownRow = 0;
                    partnerRow = 1;
                }
                else
                {
                    ownRow = 1;
                    partnerRow = 0;
                }
                partnerRowOff = partnerRow * 16;
                ownRowOff = ownRow * 16;
                partnerPoseOff = partnerRowOff + (((unsigned int) partnerRowOff & 0xFF) >> 8);
                side = 0;
                ownZOff = ownRowOff + 0x5f8;
                partnerZOff = partnerPoseOff + 0x5f8;
                for (; side < 2 && (side != 1 || pair[0]->partnerLink != -1); side++)
                {
                    pair[side]->moveTicks = 0;
                    pair[side]->moveDuration = pair[side]->timer;
                    if (side == 0 && pair[0]->moveDuration != 0)
                    {
                        seVoiceStartDefault(pair[0]->moveSound);
                    }
                    if (pair[side]->motionKind == 1)
                    {
                        uwamonoStartMove(recordIds[side], &(&pair[side]->closedPosition)[partnerRow], (short) pair[side]->timer, 0, -1, 0);
                    }
                    else
                    {
                        pair[side]->turnStep = 0;
                        pair[side]->turnCount = pair[side]->timer;
                        ((UwamonoPoses *) &pair[side]->turnFrom)->point[0].x = ((UwamonoRotationView *) pair[side])->rest.point[ownRow].x;
                        ((UwamonoPoses *) &pair[side]->turnFrom)->point[0].y = *(float *)(ownRowOff + (int) pair[side] + 0x5f4);
                        ((UwamonoPoses *) &pair[side]->turnFrom)->point[0].z = *(float *)((char *) pair[side] + ownZOff);
                        (void) ownRowOff;
                        ((UwamonoPoses *) &pair[side]->turnFrom)->point[0].w = 1.0f;
                        ((UwamonoPoses *) &pair[side]->turnFrom)->point[1].x = *(float *)(partnerPoseOff + (int) pair[side] + 0x5f0);
                        ((UwamonoPoses *) &pair[side]->turnFrom)->point[1].y = *(float *)(partnerPoseOff + (int) pair[side] + 0x5f4);
                        ((UwamonoPoses *) &pair[side]->turnFrom)->point[1].z = *(float *)((char *) pair[side] + partnerZOff);
                        ((UwamonoPoses *) &pair[side]->turnFrom)->point[1].w = 1.0f;
                    }
                    pair[side]->openState = mode;
                }
            }
        }
    }
}

extern float uwamonoDoorOpenTurnAngle;
extern float uwamonoPairMoveLinkScale;

/* Starts the lid move of the treasure box at `index`, then hands its partner the saved position and a move. */
/* Pair-move constants (read by uwamonoStartPairMove). */
float uwamonoDoorOpenTurnAngle = -2.09439516f;
float uwamonoPairMoveLinkScale = 0.400000006f;

void uwamonoStartPairMove(int index, int mode, int duration)
{
    UwamonoRecord *record = &uwamonoRecords[index];
    Vector4Elements ones;

    if (record->kind != MAP_KIND_MODEL)
    {
        return;
    }
    if (record->type == -1)
    {
        return;
    }
    if (record->moveTicks != -1)
    {
        return;
    }
    if ((record->flags & 0x02000000) != 0)
    {
        return;
    }
    if (mode == 5)
    {
        mode = (record->openState == 3) ? 4 : 3;
    }
    else if (mode == 3)
    {
        mode = 3;
    }
    if (mode != 3)
    {
        return;
    }
    if (mode != 3)
    {
        return;
    }
    if (duration >= 2)
    {
        seVoiceStartDefault(0x0200000F);
    }
    record->moveDuration = duration;
    record->moveTicks = 0;
    record->turnFrom.x = 0.0f;
    record->turnFrom.y = record->rotation.y;
    record->turnFrom.z = record->rotation.z;
    record->turnFrom.w = 1.0f;
    record->turnTo.x = uwamonoDoorOpenTurnAngle;
    record->turnTo.y = record->rotation.y;
    record->turnTo.z = record->rotation.z;
    record->openState = mode;
    record->turnTo.w = 1.0f;
    if (record->itemLink == -1)
    {
        return;
    }
    {
        int offset = record->itemLink * sizeof(UwamonoRecord) + 0x10;
        DoublewordVector4 *destination = (DoublewordVector4 *) ((unsigned char *) uwamonoRecords + offset);
        destination->words[0] = ((DoublewordVector4 *) &record->position)->words[0];
        destination->words[1] = ((DoublewordVector4 *) &record->position)->words[1];
    }
    {
        int offset = record->itemLink * sizeof(UwamonoRecord) + 0x14;
        float *y = (float *) ((unsigned char *) uwamonoRecords + offset);
        *y += 0.5f;
    }
    {
        int xOffset = 80;
        int yOffset = 84;
        int zOffset = 88;
        (*(float *) ((unsigned char *) uwamonoRecords + record->itemLink * sizeof(UwamonoRecord) + xOffset)) = (*(float *) ((unsigned char *) uwamonoRecords + record->itemLink * sizeof(UwamonoRecord) + yOffset)) = (*(float *) ((unsigned char *) uwamonoRecords + record->itemLink * sizeof(UwamonoRecord) + zOffset)) = 0.0f;
    }
    uwamonoRecords[record->itemLink].scale.w = 1.0f;
    ones.vector.x = ones.vector.y = ones.vector.z = 1.0f;
    ones.vector.w = 1.0f;
    uwamonoStartScale(record->itemLink, &ones, 20);
    uwamonoRecords[record->itemLink].velocity.y = uwamonoPairMoveLinkScale;
    {
        uwamonoRecords[record->itemLink].flags |= 2;
    }
    {
        uwamonoRecords[record->itemLink].flags &= 0xFDFFFFFF;
    }
    uwamonoStartItemFlight(record->itemLink);
    record->itemLink = -1;
}


/* The nearest target of the subject (arg0) within range: a record index, or -1. */
int uwamonoFindTalkTarget(struct XenoActor *subject, float range)
{
    int bestIndex = -1;
    float bestDist = 9999.9f;
    int index;
    UwamonoRecord *record;

    if ((gameWork.control & CONTROL_MAP_LOADED) != 0 && gameWork.entryKey != bestIndex) {
        return -1;
    }
    for (index = 0; index < UWAMONO_RECORD_COUNT; index++) {
        DoublewordVector4 spot;
        float dist;

        record = &uwamonoRecords[index];

        switch (record->kind) {
        case MAP_KIND_MODEL:
            if ((unsigned int) (record->type - 4) < 3) {
                float angleMid;

                spot.vector = record->position;
                dist = Get_Distance(&subject->position, &spot.vector);
                if (bestDist <= dist) {
                    continue;
                }
                if (subject->position.y < record->position.y - uwamonoRecords[uwamonoCurrentIndex].form) {
                    continue;
                }
                if (record->position.y + record->extent.y < subject->position.y) {
                    continue;
                }
                angleMid = record->rotation.y + 3.1415927f;
                if (Check_Angle(subject->rotation.y, angleMid - 0.5235988f, angleMid + 0.5235988f) != 1) {
                    continue;
                }
                if ((unsigned int) (subject->kind + 0xFFFF0000) < 0x4000) {
                    if (Check_InsideFan(-1, &subject->position, subject->rotation.y, 1.5f, &spot.vector, 1.0471976f, 1.0f, -1) == 0) {
                        continue;
                    }
                } else {
                    if (Check_InsideFan(-1, &subject->position, subject->rotation.y, 7.5f, &spot.vector, 1.0471976f, 1.0f, -1) == 0) {
                        continue;
                    }
                }
                if (range < dist) {
                    continue;
                }
                bestDist = dist;
                bestIndex = index;
            } else {
                int linkedIndex;

                if (record->motionKind != 0 && record->moveTicks != -1) {
                    continue;
                }
                linkedIndex = record->partnerLink;
                if (linkedIndex == -1) {
                    spot.vector = record->closedPosition.vector;
                } else {
                    Get_MiddlePoint(&record->closedPosition.vector, &uwamonoRecords[linkedIndex].closedPosition.vector, 1, 2, &spot.vector);
                }
                dist = Get_Distance(&subject->position, &spot.vector);
                if (bestDist <= dist) {
                    continue;
                }
                if (subject->position.y < record->position.y - uwamonoRecords[uwamonoCurrentIndex].form) {
                    continue;
                }
                if (record->position.y + record->extent.y < subject->position.y) {
                    continue;
                }
                if (record->motionKind == 0) {
                    continue;
                }
                if (record->openerKind == 2) {
                    if (record->openDistance * 5.0f < dist) {
                        continue;
                    }
                } else {
                    if (record->openDistance < dist) {
                        continue;
                    }
                }
                if (Check_InsideFan(-1, &subject->position, subject->rotation.y, range, &spot.vector, 1.0471976f, 1.0f, -1) == 0) {
                    continue;
                }
                if (range < dist) {
                    continue;
                }
                bestDist = dist;
                bestIndex = index;
            }
            break;
        case MAP_KIND_ACTOR:
            {
                float reachLimit;

                spot.vector = record->position;
                dist = subject->position.y - spot.vector.y;
                dist = dist * dist;
                if (dist > 4.0f) {
                    continue;
                }
                dist = Get_Distance(&subject->position, &spot.vector);
                if (bestDist <= dist) {
                    continue;
                }
                reachLimit = record->extent.x + subject->radius + 0.1f;
                if ((record->flags & 0x800) == 0) {
                    reachLimit += 0.5f;
                }
                if (reachLimit < dist) {
                    continue;
                }
                if ((record->flags & 0x800) == 0) {
                    if (Check_InsideFan(-1, &subject->position, subject->rotation.y, range, &spot.vector, 1.5707964f, 1.0f, -1) == 0) {
                        continue;
                    }
                }
                if (range < dist) {
                    continue;
                }
                bestDist = dist;
                bestIndex = index;
            }
            break;
        default:
            continue;
        }
    }
    return bestIndex;
}

/* Starts the 60-tick flight of the picked-up item at `index` to the player. */
void uwamonoStartItemFlight(int index)
{
    UwamonoRecord *record = &uwamonoRecords[index];

    record->flags &= ~8;
    record->moveDuration = 0x3C;
    record->moveFrom = record->position;
    record->moveTicks = 0;
    seVoiceStartDefault(0x02000002);
}


/* The object is a 12-byte array (ytachi_test.c), but only its first byte is read: declared as a one-byte scalar the access is
 * $gp-relative, as in the original (a sized or unsized array declaration here changes the code). */
extern unsigned char uwamonoTriggerTypeFlag;
extern char itemMessageEmptyText[];
extern char itemPickupMessageBuffer[];
void strcat(char *a, char *b);

void uwamonoUpdateItemPickups(void)
{
    UwamonoRecord *records;
    UwamonoRecord *record;
    UwamonoEventRow *entry;
    Vector4Elements pose;
    char message[0x100];
    char *text = itemMessageEmptyText;
    int index;
    int count;
    int id;
    int repeat;
    short moveFrameCopy;
    int currentOffset;

    if ((gameWork.control & CONTROL_PAUSED) != 0) {
        return;
    }
    records = (UwamonoRecord *) uwamonoRecords;
    for (index = 0; index < UWAMONO_RECORD_COUNT; index++) {
        record = &records[index];
        if (records[index].kind != MAP_KIND_MODEL) {
            continue;
        }
        if (records[index].type < 12) {
            continue;
        }
        if ((records[index].flags & 0x02000000) != 0) {
            continue;
        }
        if (record->moveTicks == -1 && Get_Distance3D(&record->position, (Vector4 *) (uwamonoCurrentIndex * sizeof(UwamonoRecord) + (int) &records->position)) <= records[uwamonoCurrentIndex].extent.x) {
            uwamonoStartItemFlight(index);
        }
        if (record->moveTicks == -1) {
            continue;
        }

        moveFrameCopy = record->moveTicks;
        currentOffset = uwamonoCurrentIndex * sizeof(UwamonoRecord);
        pose.vector.y = *(float *) (currentOffset + ((currentOffset & 0xFF) >> 8) + (int) records + 20) + 1.0f;
        pose.vector.x = *(float *) (currentOffset + ((currentOffset & 0xFE) >> 8) + (int) records + 16);
        pose.vector.z = *(float *) (currentOffset + ((currentOffset & 0xFC) >> 8) + (int) records + 24);
        pose.vector.w = 1.0f;
        Get_MiddlePoint(&record->moveFrom, &pose.vector, record->moveTicks, record->moveDuration, &record->position);

        if (record->moveTicks == record->moveDuration / 2) {
            record->scale.x = record->scale.y = record->scale.z = 1.0f;
            record->scale.w = 1.0f;
            pose.vector.x = pose.vector.y = pose.vector.z = 0.0f;
            pose.vector.w = 1.0f;
            uwamonoStartScale(index, &pose, record->moveTicks);
        }

        if (record->moveTicks == 1) {
            entry = (UwamonoEventRow *) (record->eventRow * 8 + (int) uwamonoEventTable);
            count = entry->count;
            id = entry->id;
            switch (entry->type) {
            case 1:
                text = hitActionTextGet(id, 0);
                repeat = 0;
                while (repeat < count) {
                    dataBoxIncCategory1(id);
                    repeat++;
                }
                break;
            case 2:
                text = itemTextGet(id, 0);
                repeat = 0;
                while (repeat < count) {
                    dataBoxIncCategory2(id);
                    repeat++;
                }
                break;
            case 3:
                text = dataBoxCategory4TextGet(id, 0);
                repeat = 0;
                while (repeat < count) {
                    dataBoxIncCategory4(id);
                    repeat++;
                }
                break;
            case 4:
                text = dataBoxCategory3TextGet(id, 0);
                if (count > 0) {
                    repeat = count;
                    do {
                        dataBoxIncCategory3(id);
                    } while (--repeat != 0);
                }
                selectionKeyItemBitsFill();
                break;
            }
            gameWork.eventFlags[record->eventRow] = 1;
            {
                extern char itemCountSuffixText[];
                itemCountSuffixText[2] = (char) (count / 10 + 0x30);
                itemCountSuffixText[3] = (char) (count % 10 + 0x30);
            }
            strcpy(message, text);
            {
                extern char itemCountSuffixText[8];
                strcat(message, itemCountSuffixText);
            }
            repeat = itemPickupMessageBuffer[0];
            strcpy(itemPickupMessageBuffer, message);
            msgChoiceShowPair(text, count);
            uwamonoSendEvent(8, *(short *) record, -1);
        }

        if (record->moveTicks >= record->moveDuration) {
            record->moveTicks = -1;
            uwamonoReleaseRecord(index);
        }
    }
}

extern float uwamonoFarPartnerScale;
extern float uwamonoTriggerLinkScale;


/* Partner scale constants (read by uwamonoTriggerRecord). */
float uwamonoFarPartnerScale = 0.400000006f;
float uwamonoTriggerLinkScale = 0.400000006f;

void uwamonoTriggerRecord(int index)
{
    UwamonoRecord *record = &uwamonoRecords[index];
    Vector4Elements unit;
    DoublewordVector4 scale;
    Vector4 spawnOut;
    UwamonoRecord *other;
    struct XenoActor *actor;
    UwamonoRecord *partnerBase;
    UwamonoRecord *secondBase;
    int partnerOff;
    int secondOff;
    int farPartner;
    int partnerPosOff;
    long long *partnerPos;
    int partnerYOff;
    float *partnerY;
    int partnerScaleOff;
    float *partnerScale;
    int secondPosOff;
    long long *secondPos;
    int secondYOff;
    float *secondY;

    scale.vector = record->extent;
    scale.vector.z = scale.vector.z + scale.vector.z;
    scale.vector.x = scale.vector.x + scale.vector.x;

    if (record->type == 2 || record->type == 3) {
        spawnOut.z = 5.0f;
        spawnOut.y = 5.0f;
        spawnOut.x = 5.0f;
        spawnOut.w = 1.0f;
        sefSpawnEffectById(record->effectId, &record->position, &spawnOut, &scale.vector, record->rotation.y, 30);
    } else {
        spawnOut.z = 1.0f;
        spawnOut.y = 1.0f;
        spawnOut.x = 1.0f;
        spawnOut.w = 1.0f;
        sefSpawnEffectById(record->effectId, &record->position, &spawnOut, &scale.vector, record->rotation.y, 30);
    }
    if (record->triggerSound != -1) {
        seVoiceStartDefault(record->triggerSound);
    }
    if (gameWork.mapId != 3560) {
        Vibration_Set_Strong(255, 32, 30);
    }
    if ((unsigned int) record->type < 4) {
        uwamonoTriggerTypeFlag = ((unsigned char *) &record->type)[0] & 1;
        switch (record->type) {
        case 0:
            uwamonoAddPoint(&record->position, 300, 3.0f);
            break;
        case 1:
            uwamonoAddPoint(&record->position, 300, 3.0f);
            break;
        case 2:
            uwamonoAddPoint(&record->position, 300, 15.0f);
            break;
        case 3:
            uwamonoAddPoint(&record->position, 300, 15.0f);
            break;
        }
    }

    if (record->itemLink != -1) {
        farPartner = uwamonoRecords[record->itemLink].type >= 12;
        partnerPosOff = record->itemLink * sizeof(UwamonoRecord) + 16;
        partnerPos = (long long *) ((char *) uwamonoRecords + partnerPosOff);
        partnerPos[0] = ((DoublewordVector4 *) &record->position)->words[0];
        partnerPos[1] = ((DoublewordVector4 *) &record->position)->words[1];
        partnerYOff = record->itemLink * sizeof(UwamonoRecord) + 20;
        partnerY = (float *) ((char *) uwamonoRecords + partnerYOff);
        *partnerY += (farPartner == 1) ? 0.5f : 0.0f;
        unit.vector.x = unit.vector.y = unit.vector.z = unit.vector.w = 1.0f;
        partnerBase = uwamonoRecords;
        partnerOff = record->itemLink * sizeof(UwamonoRecord);
        *(float *) (partnerOff + ((partnerOff & 0xFE) >> 8) + (int) partnerBase + 80) = *(float *) (partnerOff + ((partnerOff & 0xFC) >> 8) + (int) partnerBase + 84) = *(float *) (partnerOff + (int) partnerBase + 88) = 0.0f;
        uwamonoRecords[record->itemLink].scale.w = 1.0f;
        uwamonoStartScale(record->itemLink, &unit, (farPartner == 1) ? 20 : 0);
        partnerScaleOff = record->itemLink * sizeof(UwamonoRecord) + 52;
        partnerScale = (float *) ((char *) uwamonoRecords + partnerScaleOff);
        *partnerScale = (farPartner == 1) ? uwamonoFarPartnerScale : 0.0f;
        uwamonoRecords[record->itemLink].flags |= 2;
        uwamonoRecords[record->itemLink].flags &= ~0x02000000;
    }

    if (record->enemyLink != -1) {
        other = &uwamonoRecords[record->enemyLink];
        secondPosOff = record->enemyLink * sizeof(UwamonoRecord) + 16;
        secondPos = (long long *) ((char *) uwamonoRecords + secondPosOff);
        secondPos[0] = ((DoublewordVector4 *) &record->position)->words[0];
        secondPos[1] = ((DoublewordVector4 *) &record->position)->words[1];
        actor = other->body.actor;
        ((DoublewordVector4 *) &actor->position)->words[0] = secondPos[0];
        ((DoublewordVector4 *) &actor->position)->words[1] = secondPos[1];
        unit.vector.x = unit.vector.y = unit.vector.z = unit.vector.w = 1.0f;
        secondYOff = record->enemyLink * sizeof(UwamonoRecord) + 20;
        secondY = (float *) ((char *) uwamonoRecords + secondYOff);
        *secondY += 0.0f;
        secondBase = uwamonoRecords;
        secondOff = record->enemyLink * sizeof(UwamonoRecord);
        *(float *) (secondOff + ((secondOff & 0xFE) >> 8) + (int) secondBase + 80) = *(float *) (secondOff + ((secondOff & 0xFC) >> 8) + (int) secondBase + 84) = *(float *) (secondOff + (int) secondBase + 88) = 0.0f;
        uwamonoRecords[record->enemyLink].scale.w = 1.0f;
        uwamonoStartScale(record->enemyLink, &unit, 20);
        uwamonoRecords[record->enemyLink].velocity.y = uwamonoTriggerLinkScale;
        uwamonoRecords[record->enemyLink].flags |= 2;
        uwamonoRecords[record->enemyLink].flags &= ~0x02000000;
        if (uwamonoRecords[uwamonoCurrentIndex].form != 5.0f) {
            sefCreateEffectCf(0x1003400a, &record->position, 0);
        } else {
            sefCreateEffectCf(0x1003400b, &record->position, 0);
        }
    }
    uwamonoReleaseRecord(index);
}

/* Clears the 16 records: origin position with w = 1, key -1 (free), value 0. */
void uwamonoResetPoints(void)
{
    int index;

    for (index = 0; index < UWAMONO_POINT_COUNT; index++) {
        uwamonoPoints[index].position.x = uwamonoPoints[index].position.y = uwamonoPoints[index].position.z = 0.0f;
        uwamonoPoints[index].position.w = 1.0f;
        uwamonoPoints[index].key = -1;
        uwamonoPoints[index].value = 0.0f;
    }
}

/* Stores the position, value and key in the first free row (key -1); does nothing when no row is free. */
void uwamonoAddPoint(Vector4 *position, int key, float value)
{
    int index;

    for (index = 0; index < UWAMONO_POINT_COUNT; index++) {
        UwamonoPoint *row = &uwamonoPoints[index];

        if (row->key != -1) {
            continue;
        }
        row->key = key;
        row->position = *position;
        row->value = value;
        break;
    }
}


/* The point scan reads the first state of a record's motion block (+0x334) through this view: the stun time at +0x60 as
 * a signed short (UwamonoState declares it unsigned for the enemy code) and the action at +0x0C as a signed byte. */
typedef struct UwamonoMotionScan {
    unsigned char unmodeled_00[0x0C];
    signed char action;
    unsigned char unmodeled_0d[0x60 - 0x0D];
    short hold;
} UwamonoMotionScan;

void uwamonoUpdatePoints(void)
{
    int pointIndex;
    int recordIndex;
    UwamonoPoint *point;
    UwamonoRecord *record;

    for (pointIndex = 0; pointIndex < UWAMONO_POINT_COUNT; pointIndex++) {
        point = &uwamonoPoints[pointIndex];
        if (point->key < 0) {
            continue;
        }
        for (recordIndex = 0; recordIndex < UWAMONO_RECORD_COUNT; recordIndex++) {
            UwamonoMotionScan *motion;
            record = &uwamonoRecords[recordIndex];
            motion = (UwamonoMotionScan *) record->body.motions;

            if (record->kind != MAP_KIND_ACTOR) {
                continue;
            }
            if (motion->hold != 0) {
                continue;
            }
            if (point->value < Get_Distance3D(&point->position, &record->position)) {
                continue;
            }
            if (motion->action == 9) {
                continue;
            }
            if (motion->action != 6) {
                Enemy_ActionReady(recordIndex, 9);
            }
        }
        point->key--;
    }
}

extern char uwamonoSearchDistanceFormat[];

/* Scans the records for one with kind 2 and type in 7..11 (save points and dummies) that lies within half of the
 * current record's form from the current record; prints it and returns its low byte, else -1. */
int uwamonoGetNearbyUnitType(void)
{
    UwamonoRecord *record;
    int index;

    for (index = 0; index < UWAMONO_RECORD_COUNT; index++) {
        record = &uwamonoRecords[index];
        if (record->kind == MAP_KIND_MODEL && (unsigned int) (record->type - 7) < 5) {
            float distance = Get_Distance3D(&uwamonoRecords[uwamonoCurrentIndex].position, &record->position);
            if (distance <= 0.5f * uwamonoRecords[uwamonoCurrentIndex].form) {
                xglFontDebugPrintf(8, 8, uwamonoSearchDistanceFormat, (double) distance);
                return (signed char) record->type;
            }
        }
    }
    return -1;
}

/* Strings and zero buffers of the countdown text (.sdata, in address order). */
/* JP: を00個
 * EN: (after the item name) x00 */
char itemCountSuffixText[8] = "\244\362" "00" "\270\304";
char itemMessageEmptyText[8] = { 0 };
char uwamonoSearchDistanceFormat[8] = "%8f";

const UwamonoCellBits uwamonoCellBitTable = { { 0, 1, 2, 3, 0, 4, 5, 6, 7 } };

/* Zero data after the ladder table (.data, address order). The types are guesses from the sizes; */
/* uwamonoRecords (0x33FDC0) is declared by include/main/ytachi_test.h and is not defined here. */

/* .data of this file (0x3E7270-0x3E7400), also read by runtime.c. */
char itemPickupMessageBuffer[400] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 128, 0, 0, 0,
    255, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 128, 0, 0, 0,
    0, 0, 0, 0, 255, 0, 0, 0,
    0, 0, 0, 0, 128, 0, 0, 0,
    255, 0, 0, 0, 255, 0, 0, 0,
    0, 0, 0, 0, 128, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    255, 0, 0, 0, 128, 0, 0, 0,
    255, 0, 0, 0, 0, 0, 0, 0,
    255, 0, 0, 0, 128, 0, 0, 0,
    0, 0, 0, 0, 255, 0, 0, 0,
    255, 0, 0, 0, 128, 0, 0, 0,
    255, 0, 0, 0, 255, 0, 0, 0,
    255, 0, 0, 0, 128, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};
