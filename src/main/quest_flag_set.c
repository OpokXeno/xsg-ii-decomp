#include "common.h"
#include "main/quest_flag_set.h"
#include "shared.h"

/* Table lengths: quest flags 0..35 and citizen flags 0..49. */
#define QUEST_FLAG_COUNT 0x24U
#define CITIZEN_FLAG_COUNT 0x32U

/*
 * Core of the quest-flag setter (script native setQuestFlag):
 * stores value in the 36-entry byte table questFlags; values 2 and 3 also get
 * bit 0x10, which questFlagGet masks off again.
 */
extern u8 questFlags[];

void questFlagSet(u32 index, int value)
{
    if (index < QUEST_FLAG_COUNT) {
        questFlags[index] = (u8) value;
        if (((value + 0xFE) & 0xFF) < 2U) {
            questFlags[index] = (u8) (value | 0x10);
        }
    }
}

extern u8 citizenFlags[];

/* Core of the citizen-flag setter (script native setCitizenFlag): 50-entry byte table. */
void citizenFlagSet(u32 index, int value) {
    if (index < CITIZEN_FLAG_COUNT) {
        citizenFlags[index] = (u8) value;
    }
}

/* Core of the quest-flag getter (script native getQuestFlag): 0 for an index outside the 36-entry table. */
int questFlagGet(u32 index) {
    if (!(index < QUEST_FLAG_COUNT)) {
        return 0;
    }
    return questFlags[index] & 0xEF;
}
