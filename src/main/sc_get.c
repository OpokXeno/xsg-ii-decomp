#include "common.h"
#include "shared.h"
#include "main/sc_get.h"
#include "main/sv_get_image_item.h"
#include "main/xgl_studio.h"
#include "main/sef.h"
#include "main/sdv.h"
#include "main/srs.h"

extern int charID_0;
extern int eftCate_1;
extern void xglLightIntensityAmbient(void *light, void *ambient);
extern void sdvDebugSetMapRgb(void *map_rgb);

#define SCRIPT_ADDRESS_NONE 0xFFFF    /* absent table entry or address */

extern u16 sefGroupKey;

void sefAdvanceGroupKey(void)
{
    sefGroupKey += 1;
    sefExecScheduler(0);
}

int scGetImmAdrImmIdx2(int base, unsigned short *table, int index)
{
    int offset;
    int address;

    offset = table[index];
    if (offset == SCRIPT_ADDRESS_NONE ||offset == -1) {
        offset = 0;
    }
    address = 0;
    if (offset != 0) {
        address = base + offset * 2;
    }
    return address;
}

short scGetImmNumIdx(short *table, int index)
{
    return table[index];
}

/* Inline copy of scOfsToAdr below (written out in place, each lookup shares one
 * register for the loaded and the resolved offset; the original keeps two). */
static inline int scOfsToAdrInline(int offset)
{
    if (offset == SCRIPT_ADDRESS_NONE || offset == -1) {
        offset = 0;
    }
    return offset;
}

/* Takes the effect id in a0 (sef.c 0x00261D5C daddu $16,$4,$0, then srsAnalyzeEftNo(effectId, ...)). */
extern int scKeyRecordMode;
extern int scKeyRecordGroupCount;
extern short *scKeyRecordEntry;

#define SEF_OPERAND_WHOLE      0x4000    /* kind 0x3E: operand kept whole in scKeyRecordGroupCount */
#define SEF_OPERAND_INDEX_MASK 0x3FFF    /* kind 0x3E: otherwise only these bits */

/* Looks up the script record of key and resolves its offset by kind: 0x3C/0x3D
 * immediate, 0x3E table entry index, 0x3F table entry srsGetEffectSlot(key);
 * kind 0x49 publishes the operand address in scKeyRecordEntry instead. Returns the
 * record and stores the offset in *offset_out, or returns 0.
 * The dead store to value in the 0x49 branch keeps value live past the 0x3F
 * lookup; without it (compiled again 2026-11-18) gcc folds the loaded and the
 * resolved offset into one register ($4 for the original's $6/$4) and adds a nop. */
SefScriptRecord *scGetKeyRecord(int key, int *offset_out, int index)
{
    SvTypeListEntry *entry;
    SefScriptRecord *record;
    SefScriptRecord *found;
    int value;
    int offset;
    int slot;
    int kind;
    int status;

    status = sefSearchMapperIndex(key);
    *offset_out = 0;
    scKeyRecordEntry = 0;
    scKeyRecordMode = 0;
    if (status >= 0) {
        entry = svGetScript(key);
        if (entry != 0) {
            record = (SefScriptRecord *)entry->data;
            offset = 0;
            if (record != 0) {
                scKeyRecordMode = kind = record->kind;
                if ((unsigned short)(record->kind - 0x3C) < 2) {
                    value = (unsigned short)record->operand;
                    offset = scOfsToAdrInline(value);
                } else if (kind == 0x3E) {
                    slot = record->operand;
                    if (!(slot & SEF_OPERAND_WHOLE)) {
                        scKeyRecordGroupCount = slot & SEF_OPERAND_INDEX_MASK;
                    } else {
                        scKeyRecordGroupCount = slot;
                    }
                    value = record->offsets[index];
                    offset = scOfsToAdrInline(value);
                } else if (kind == 0x3F) {
                    slot = srsGetEffectSlot(key);
                    if (slot < record->operand) {
                        value = record->offsets[slot];
                        offset = scOfsToAdrInline(value);
                    }
                } else if (kind == 0x49) {
                    value = 0; /* dead store, see above: without it the 0x3E/0x3F lookups use one register */
                    scKeyRecordEntry = &record->operand;
                    return record;
                }
                found = 0;
                if (offset != 0) {
                    *offset_out = offset;
                    found = record;
                }
                return found;
            }
        }
    }
    return 0;
}

SefScriptRecord *scGetKeyRecordFirst(int key, int *offset_out)
{
    return scGetKeyRecord(key, offset_out, 0);
}

int scOfsToAdr(int offset)
{
    if (offset == SCRIPT_ADDRESS_NONE ||offset == -1) {
        offset = 0;
    }
    return offset;
}

/* Ambient data (.data), in address order. sdvAmbientWork is never read by this TU. */
unsigned char sdvAmbientWork[24] = { 0 };
