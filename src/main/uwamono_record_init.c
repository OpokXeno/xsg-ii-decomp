#include "common.h"
#include "shared.h"
#include "ov01/sb_vm.h"
#include "main/uwamono_record_init.h"
#include "main/game_map_object.h"
#include "main/ytachi_test.h"
#include "main/uwamono_record.h"
#include "main/uwamono_objx.h"
#include "main/game_data.h"
#include "main/nml_model_set.h"

/* First .rodata of this file (0x689D20), before the jump table of uwamonoInitRecord; nothing reads it. */
const char partsNamesResourceName[16] = "PartsNames";

/* Empty in the original: a no-op, called by Game. */
void menuSceneSetupHook(void) {
}

/* One 48-byte row of mapObjectResources: the name an entry is matched against, and its scale and motion. */
typedef struct UwamonoNameRow {
    unsigned char name[12];             /* +0x00 */
    unsigned char level;                /* +0x0C */
    unsigned char searchMode;           /* +0x0D */
    unsigned char effectNumber;         /* +0x0E */
    unsigned char unmodeled_0f;
    float scaleX;                       /* +0x10 */
    float scaleY;                       /* +0x14 */
    float scaleZ;                       /* +0x18 */
    unsigned char unmodeled_1c[4];
    short breakSound;                    /* +0x20 */
    unsigned char unmodeled_22[0x30 - 0x22];
} UwamonoNameRow;


/*
 * Field `member` of record `index`, reached as the original does: the record offset and the field offset are summed
 * as one integer and the table address is added last, so every use computes an address of its own.
 */
#define UWAMONO_FIELD(index, member)                                                                   \
    (*({                                                                                               \
        int byteOffset = (index) * sizeof(UwamonoRecord) + (int) &((UwamonoRecord *) 0)->member;       \
        (__typeof__(((UwamonoRecord *) 0)->member) *) ((char *) uwamonoRecords + byteOffset);          \
    }))

enum {
    UWAMONO_NO_KEY = -1,                /* record->key of an object without a map entry */
    UWAMONO_TYPE_ITEM_FIRST = 12,       /* types 0..11 have a reset arm; 12 and up are items ("ExItm") */
    UWAMONO_NAME_LENGTH = 12,           /* entry names are compared up to this many bytes */
    UWAMONO_NAME_ROW_LIMIT = 512        /* object name rows scanned */
};

/*
 * Sets up the search shape of record `index`. A record without a key gets the fixed shape of its mode. Otherwise the
 * key's entry name is looked up in the object name rows: a door takes the row's scale, an "_sh" object is aimed at
 * its linked record, anything else takes the row's scale, level and motions. Returns 0 when nothing matches.
 */
int uwamonoInitRecord(short index)
{
    UwamonoRecord *record = &uwamonoRecords[index];
    UwamonoNameRow *nameRows = mapObjectResources[MAP_RESOURCE_KOWAREMONO];
    signed char *entryName;
    int nameLen;
    int rowIndex;
    char kindTag[64];

    if (record->key == UWAMONO_NO_KEY) {
        if (record->type >= UWAMONO_TYPE_ITEM_FIRST) {
            record->extent.x = 0.25f;
            record->extent.y = 0.5f;
            record->extent.z = 0.25f;
            record->extent.w = 1.0f;
            record->level = 2;
            record->searchMode = 0;
            record->effectId = 0;
            record->triggerSound = 0;
            __builtin_strcpy(kindTag, "ExItm");
        } else {
            switch (record->type) {
            case 0:
            case 1:
                record->extent.x = record->extent.y = record->extent.z = 0.0f;
                record->extent.w = 1.0f;
                record->level = 0;
                record->searchMode = 0;
                record->effectId = 0x10000035;
                record->triggerSound = record->type == 0 ? 0x0200000B : 0x0200000A;
                record->flags |= 0x60000;
                __builtin_strcpy(kindTag, "ExTrp");
                break;
            case 2:
            case 3:
                record->extent.x = record->extent.y = record->extent.z = 0.0f;
                record->extent.w = 1.0f;
                record->level = 0;
                record->searchMode = 0;
                record->effectId = 0x10000035;
                record->triggerSound = record->type == 2 ? 0x0200000B : 0x0200000A;
                record->flags |= 0x160000;
                __builtin_strcpy(kindTag, "ExTrp");
                break;
            case 4:
            case 6:
                record->extent.x = 0.35f;
                record->extent.y = 0.55f;
                record->extent.z = 0.225f;
                record->extent.w = 1.0f;
                record->level = 2;
                record->searchMode = 1;
                record->effectId = 0;
                record->triggerSound = 0;
                record->openState = 4;
                __builtin_strcpy(kindTag, "ExTbx");
                break;
            case 5:
                record->extent.x = 1.75f;
                record->extent.y = 2.5f;
                record->extent.z = 1.6f;
                record->extent.w = 1.0f;
                record->level = 2;
                record->searchMode = 1;
                record->effectId = 0;
                record->triggerSound = 0;
                record->openState = 4;
                __builtin_strcpy(kindTag, "ExTbx");
                break;
            case 7:
            case 8:
            case 9:
                record->extent.x = record->extent.y = record->extent.z = 0.0f;
                record->extent.w = 1.0f;
                record->level = 0;
                record->searchMode = 0;
                record->effectId = 0;
                record->triggerSound = 0;
                __builtin_strcpy(kindTag, "ExSav");
                break;
            default:
                record->extent.x = 0.5f;
                record->extent.y = 1.0f;
                record->extent.z = 0.5f;
                record->extent.w = 1.0f;
                record->level = 1;
                record->searchMode = 1;
                record->effectId = 0x100C8008;
                record->triggerSound = 0;
                record->flags |= 0x60000;
                __builtin_strcpy(kindTag, "Dummy");
                break;
            }
        }
        return 1;
    }

    entryName = pxyGetEntry(mapObjectTable, record->key);
    if (entryName == 0) {
        return 0;
    }
    for (rowIndex = 0; rowIndex < UWAMONO_NAME_ROW_LIMIT && nameRows[rowIndex].name[0] != 0; rowIndex++) {
        for (nameLen = 0; nameLen < UWAMONO_NAME_LENGTH; nameLen++) {
            if (nameRows[rowIndex].name[nameLen] == 0) {
                nameLen = UWAMONO_NAME_LENGTH;
                break;
            }
            if (entryName[nameLen] != nameRows[rowIndex].name[nameLen]) {
                break;
            }
        }

        if (entryName[0] == 'd' && entryName[1] == 'o' && entryName[2] == 'o' && entryName[3] == 'r') {
            record->extent.x = nameRows[rowIndex].scaleX * 0.5f;
            record->extent.y = nameRows[rowIndex].scaleY;
            record->extent.z = nameRows[rowIndex].scaleZ * 0.5f;
            if (nameRows[rowIndex].scaleX == 1.5f) {
                record->moveSound = 0x03000000;
            } else {
                record->moveSound = 0x03000001;
            }
            record->extent.w = 1.0f;
            record->searchMode = 1;
            record->flags &= ~8;
            record->timer = 30;
            record->level = 0;
            record->effectId = 0;
            record->openerKind = 0;
            record->openDistance = 1.0f;
            return 1;
        }

        if (nameLen != UWAMONO_NAME_LENGTH) {
            continue;
        }

        if (entryName[9] == '_' && entryName[10] == 's' && entryName[11] == 'h') {
            float zero = 0.0f;
            int targetRecord;

            record->flags |= 8;
            record->extent.w = 1.0f;
            record->extent.x = record->extent.y = record->extent.z = zero;
            record->level = 0;
            record->searchMode = 0;
            record->effectId = 0;
            record->triggerSound = 0;
            targetRecord = uwamonoFindRecord(2, uwamonoRecords[index].key - 1);
            if ((record->position.x != UWAMONO_FIELD(targetRecord, position.x) || record->position.z != UWAMONO_FIELD(targetRecord, position.z))
                && (UWAMONO_FIELD(targetRecord, rotation.x) != zero || UWAMONO_FIELD(targetRecord, rotation.y) != zero || UWAMONO_FIELD(targetRecord, rotation.z) != zero)) {
                uwamonoSetTarget(index, uwamonoFindRecord(2, uwamonoRecords[index].key - 1), 2);
            } else {
                uwamonoSetTarget(index, uwamonoFindRecord(2, uwamonoRecords[index].key - 1), 1);
            }
            if (entryName[0] == 'c' && entryName[1] == 'k') {
                record->flags &= 0xFFEFFFFF;
            }
            if (entryName[0] == 'r' && entryName[1] == 'k') {
                record->flags |= 0x100000;
            }
            return 1;
        } else {
            record->extent.x = nameRows[rowIndex].scaleX * 0.5f;
            record->extent.y = nameRows[rowIndex].scaleY;
            record->extent.z = nameRows[rowIndex].scaleZ * 0.5f;
            record->extent.w = 1.0f;
            record->level = nameRows[rowIndex].level;
            record->searchMode = nameRows[rowIndex].searchMode;
            record->effectId = (nameRows[rowIndex].effectNumber + 900) | 0x100C8000;
            record->triggerSound = nameRows[rowIndex].breakSound + 0x02000000;
            record->flags |= 0x60008;
            if (entryName[0] == 'c' && entryName[1] == 'k') {
                record->flags &= 0xFFEFFFFF;
            }
            if (entryName[0] == 'r' && entryName[1] == 'k') {
                record->flags |= 0x100000;
            }
            return 1;
        }
    }
    return 0;
}
