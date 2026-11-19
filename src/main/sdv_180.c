#include "common.h"
#include "shared.h"
#include "main/sdv_180.h"
#include "main/sv_get_image_item.h"

void sdvSetAmbState2(int state, int effect_no);
char *srsAnalyzeEftNo(int effectNo, int *charNo, int *effectType);
extern int _sdvAmbFrame;
extern int _sdvAmbState;
extern int charID_0;
extern int eftCate_1;
void sdvSetAmbStateSub(int state, int effect_no, int force);
extern void *xglStudioGetLight2(void);
extern void xglLightIntensityAmbient(void *light, void *ambient);
extern void func_A2C3D8(void *map_rgb);
extern Vector4 *MMathVectorInterpolation(Vector4 *destination,
                                         const Vector4 *first,
                                         const Vector4 *second,
                                         float parameter);
extern float _sdvMapRgb[4];
extern float _sdvAmbient[4];
void sdvSetAmbient(void *map_rgb, void *ambient);
int scGetImmAdrImmIdx2(int base, unsigned short *table, int index);

#define SCRIPT_ADDRESS_NONE 0xFFFF    /* absent table entry or address */

int sefStepEffectRecords(int only);
extern u16 sefGroupKey;

void sefAdvanceGroupKey(void)
{
    sefGroupKey += 1;
    sefStepEffectRecords(0);
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

/* Script effect record: the data of the type-list entry svGetScriptEntry returns. */
typedef struct SefScriptRecord {
    unsigned char unmodeled_00[0x10];
    short kind;                 /* 0x10: 0x3C..0x3F or 0x49 */
    short operand;              /* 0x12: immediate offset, slot count or table index */
    unsigned short offsets[1];  /* 0x14: offset table for kinds 0x3E and 0x3F */
} SefScriptRecord;

/* Inline copy of scOfsToAdr below (written out in place, each lookup shares one
 * register for the loaded and the resolved offset; the original keeps two). */
static inline int scOfsToAdrInline(int offset)
{
    if (offset == SCRIPT_ADDRESS_NONE || offset == -1) {
        offset = 0;
    }
    return offset;
}

/* Takes the effect id in a0 (tu170 0x00261D5C daddu $16,$4,$0, then srsAnalyzeEftNo(effectId, ...)). */
int sefFindRecordIndex(int effectId);
int srsGetEffectSlot(int effectKey);
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

    status = sefFindRecordIndex(key);
    *offset_out = 0;
    scKeyRecordEntry = 0;
    scKeyRecordMode = 0;
    if (status >= 0) {
        entry = svGetScriptEntry(key);
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

void sdvInitAmbient(void)
{
    _sdvAmbFrame = 0;
    _sdvAmbState = 0;
}

void func_00266D88(void)
{
}

char *srsAnalyzeEftNo(int effect_no, int *char_no, int *effect_type);
static int D_0069E324;
static int D_0069E328;

void sdvSetAmbStateSub(int state, int effect_no, int force)
{
    if (force != 0) {
        _sdvAmbState = state;
        return;
    }
    srsAnalyzeEftNo(effect_no, &D_0069E324, &D_0069E328);
    if (D_0069E328 == 9) {
        _sdvAmbState = state;
    }
}

void sdvSetAmbState(int state, int effect_no)
{
    sdvSetAmbStateSub(state, effect_no, 0);
}

void sdvSetAmbState2(int state, int effect_no)
{
    sdvSetAmbStateSub(state, effect_no, 1);
}

void sdvSetAmbient(void *map_rgb, void *ambient)
{
}

void sdvRestoreAmbient(void)
{
    sdvSetAmbient(_sdvMapRgb, _sdvAmbient);
}

void sdvExecAmbient(void)
{
    Vector4 map_rgb;
    Vector4 ambient;
    float parameter;
    int state = _sdvAmbState;

    if (state == 0) {
        return;
    }

    if (state == 1) {
        if (_sdvAmbFrame >= 20) {
            _sdvAmbState = 0;
            _sdvAmbFrame = 20;
            state = 0;
        } else {
            _sdvAmbFrame++;
        }
    } else if (state == 2) {
        if (_sdvAmbFrame <= 0) {
            _sdvAmbFrame = 0;
            state = 0;
            _sdvAmbState = 0;
        } else {
            _sdvAmbFrame--;
        }
    }

    if (state != 0) {
        float half = 0.5f;
        float divisor = 20.0f;

        parameter = (float)_sdvAmbFrame / divisor;
        /* These constrained operations implement the eight evidenced COP2 instructions. */
        __asm__ __volatile__(
            "lqc2 vf1, 0(%1)\n\t"
            "mfc1 $8, %0\n\t"
            "qmtc2 $8, vf2\n\t"
            "vmulx.xyz vf1, vf1, vf2x\n\t"
            "sqc2 vf1, 0(%2)"
            :
            : "f"(half), "r"(&_sdvMapRgb[0]), "r"(&map_rgb)
            : "$8", "memory");
        __asm__ __volatile__(
            "lqc2 vf1, 0(%1)\n\t"
            "mfc1 $8, %0\n\t"
            "qmtc2 $8, vf2\n\t"
            "vmulx.xyz vf1, vf1, vf2x\n\t"
            "sqc2 vf1, 0(%2)"
            :
            : "f"(half), "r"(&_sdvAmbient[0]), "r"(&ambient)
            : "$8", "memory");
        MMathVectorInterpolation(&map_rgb, (const Vector4 *)_sdvMapRgb,
                                 &map_rgb, parameter);
        MMathVectorInterpolation(&ambient, (const Vector4 *)_sdvAmbient,
                                 &ambient, parameter);
        sdvSetAmbient(&map_rgb, &ambient);
    }
}

/* Ambient data (.data), in address order. D_005E6D28 is never read by this TU. */
unsigned char D_005E6D28[24] = { 0 };
float _sdvMapRgb[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
float _sdvAmbient[4] = { 0.25f, 0.25f, 0.25f, 1.0f };
int _sdvAmbFrame = 0;
int _sdvAmbState = 0;
