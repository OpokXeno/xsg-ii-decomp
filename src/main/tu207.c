#include "common.h"
#include "shared.h"
#include "main/tu206.h"
#include "vu0_macro.h"
#include "gs.h"
#include "main/tu207.h"
#include "main/tu229.h"
#include "main/xgl_2.h"

/* Inclusive range test: lower <= value <= upper. */
int xglIsInRange(int value, int lower, int upper)
{
  return lower <= value && value <= upper;
}

typedef struct RecordBank {
    GsQueueContext records[10];
} RecordBank;

/* Declared as bytes, as recordClearBank below reads it. recordUpdateRenderHook indexes it as bytes and recordSetSlot
 * steps to the bank as bytes: through ((RecordBank *) recordBanks)[bankIndex] gcc adds the base before the slot
 * offset (fd.sh recordSetSlot: 12/32 in place, 32/32 with the byte step to the bank). */
static unsigned char recordBanks[3720];

/* Local view of sRender (0x00645820; RenderFrameState in main/tu093.h does not model +0x3C): the
 * render callback, called with the frame's packet (DefocusRenderState.callback in game_defocus.c). */
typedef struct RecordRenderState {
    unsigned char unmodeled_00[0x3C];
    void (*callback)(XglPacket *packet);
} RecordRenderState;

extern RecordRenderState sRender;

void recordRenderAll(XglPacket *packet);

/* Installs recordRenderAll as the render callback while any record is used, else removes it. The scan
 * covers 30 banks although recordBanks holds 3 (0xE88 bytes), as the original does. */
void recordUpdateRenderHook(void)
{
    int active;
    int slot;
    int bank;

    active = 0;
    for (slot = 0; slot < 10; slot++) {
        for (bank = 0; bank < 30; bank++) {
            if (recordBanks[bank * sizeof(RecordBank) + slot * sizeof(GsQueueContext)] != 0) {
                active = 1;
            }
        }
    }
    if (active == 0) {
        sRender.callback = 0;
    } else {
        sRender.callback = recordRenderAll;
    }
}

void recordClear(GsQueueContext *record);
/* The red, green and blue bits of a packed colour (alpha cleared). */
#define RECORD_RGB_MASK 0xFFFFFF

/* Flag that suspends the per-frame record updates; 1 while suspended. A struct, not an int: as a scalar,
 * sched2 lifts recordSetPreset case 16's flag load above its params[0] reload (fd.sh 909/913); as a struct member
 * the load stays behind the record stores (913/913). */
typedef struct RecordUpdateFlag {
    int paused;
} RecordUpdateFlag;
extern RecordUpdateFlag recordUpdatePaused;

/*
 * Fills bank bankIndex (nothing happens unless it is below 3) with one of the record presets 0..24 (the second
 * argument, through jtbl_0068D630; any other value leaves the bank cleared): clears the bank's ten records, writes
 * the preset's records, then updates the render callback (recordUpdateRenderHook). Words no record arm names stay
 * params[] (all of types 7 and 16-19).
 * Cases 14 and 15 declare their locals in the case block: at function scope they become global pseudos and $16
 * and $17 trade places from the prologue on (fd.sh 244/913).
 */
void recordSetPreset(int bankIndex, int preset, int depth, int *params)
{
    int slot;
    GsQueueContext *records;
    GsQueueContext *record;

    records = ((RecordBank *)recordBanks)[bankIndex].records;
    if (bankIndex >= 3) {
        return;
    }
    record = records;
    for (slot = 0; slot < 10; slot++) {
        recordClear(record);
        record++;
    }
    switch (preset) {
    case 1:
        records[0].type = 4;
        records[0].u.queue.frame = 0;
        records[0].u.queue.tex0 = 2;
        records[0].u.queue.color = params[0];
        records[0].u.queue.z = depth;
        records[0].u.queue.count = 1;
        records[0].u.queue.textured = 1;
        records[0].u.queue.blend_kind = 0;
        records[0].u.gridFill.cellCount = params[1];
        break;
    case 2:
        records[0].type = 11;
        records[0].u.queue.frame = 2;
        records[0].u.queue.tex0 = 0;
        records[0].u.queue.color = 0x80FFFFFF;
        records[0].u.queue.z = 0x7FFFFFFF;
        records[0].u.queue.count = 1;
        records[0].u.queue.textured = 1;
        records[0].u.queue.blend_kind = 0;
        records[0].u.presetQuad.positionPreset = 0;
        records[0].u.presetQuad.texture.top = 0;
        records[0].u.presetQuad.texture.left = 0;
        records[0].u.presetQuad.texture.right = 512;
        records[0].u.presetQuad.texture.bottom = 448;
        records[1].type = 12;
        records[1].u.queue.frame = 0;
        records[1].u.queue.tex0 = 2;
        records[1].u.queue.color = 0x80FFFFFF;
        records[1].u.queue.z = depth;
        records[1].u.queue.count = 1;
        records[1].u.queue.textured = 1;
        records[1].u.queue.blend_kind = 0;
        records[1].u.presetUvQuad.screen.top = 0;
        records[1].u.presetUvQuad.screen.left = 0;
        records[1].u.presetUvQuad.screen.right = 512;
        records[1].u.presetUvQuad.screen.bottom = 448;
        records[1].u.presetUvQuad.texturePreset = 0;
        break;
    case 3:
        records[0].type = 2;
        records[0].u.queue.tex0 = 0;
        records[1].type = 17;
        records[1].u.queue.frame = 0;
        records[1].u.queue.tex0 = 2;
        records[1].u.queue.color = params[0];
        records[1].u.queue.z = depth;
        records[1].u.queue.count = 1;
        records[1].u.queue.textured = 1;
        records[1].u.queue.blend_kind = params[1];
        records[1].u.params[7] = -params[3];
        records[1].u.params[8] = -params[2];
        records[1].u.params[9] = params[2] + 512;
        records[1].u.params[10] = params[3] + 448;
        records[1].u.params[11] = params[2];
        records[1].u.params[12] = -(params[2] >> 1);
        records[1].u.params[13] = params[3];
        records[1].u.params[14] = -(params[3] >> 1);
        records[1].u.params[15] = 8;
        break;
    case 4:
        records[0].type = 2;
        records[0].u.queue.tex0 = 0;
        records[1].type = 18;
        records[1].u.queue.frame = 0;
        records[1].u.queue.tex0 = 2;
        records[1].u.queue.color = params[0];
        records[1].u.queue.z = depth;
        records[1].u.queue.count = 1;
        records[1].u.queue.textured = 1;
        records[1].u.queue.blend_kind = params[1];
        records[1].u.params[7] = params[5];
        records[1].u.params[8] = -params[3];
        records[1].u.params[9] = -params[4];
        records[1].u.params[10] = params[4] + 512;
        records[1].u.params[11] = params[3] + 448;
        records[1].u.params[15] = params[3];
        records[1].u.params[13] = 20;
        records[1].u.params[14] = params[2];
        records[1].u.params[12] = params[4];
        records[1].u.params[16] = 20;
        records[1].u.params[17] = params[2];
        records[1].u.params[18] = 7;
        break;
    case 5:
        records[0].type = 2;
        records[0].u.queue.tex0 = 0;
        records[1].type = 19;
        records[1].u.queue.frame = 0;
        records[1].u.queue.tex0 = 2;
        records[1].u.queue.color = params[0];
        records[1].u.queue.z = depth;
        records[1].u.queue.count = 1;
        records[1].u.queue.textured = 1;
        records[1].u.queue.blend_kind = params[1];
        records[1].u.params[7] = params[4];
        records[1].u.params[8] = -7;
        records[1].u.params[9] = -7;
        records[1].u.params[10] = 519;
        records[1].u.params[11] = 455;
        records[1].u.params[12] = 256;
        records[1].u.params[13] = 224;
        records[1].u.params[14] = 0;
        records[1].u.params[15] = 360;
        records[1].u.params[16] = params[2];
        records[1].u.params[17] = 6;
        records[1].u.params[18] = params[3];
        records[1].u.params[19] = params[5];
        break;
    case 6:
        records[0].type = 19;
        records[0].u.queue.frame = 0;
        records[0].u.queue.tex0 = 1;
        records[0].u.queue.color = 0x10FFFFFF;
        records[0].u.queue.z = depth;
        records[0].u.queue.count = 1;
        records[0].u.queue.textured = 0;
        records[0].u.queue.blend_kind = 0;
        records[0].u.params[7] = 0x80FFFFFF;
        records[0].u.params[8] = -7;
        records[0].u.params[9] = -7;
        records[0].u.params[10] = 519;
        records[0].u.params[11] = 455;
        records[0].u.params[12] = 256;
        records[0].u.params[13] = 224;
        records[0].u.params[14] = 0;
        records[0].u.params[15] = (params[0] << 6);
        records[0].u.params[16] = 2;
        records[0].u.params[17] = 12;
        records[0].u.params[18] = params[1];
        records[0].u.params[19] = 14;
        break;
    case 7:
        records[0].type = 20;
        records[0].u.queue.frame = 2;
        records[0].u.queue.tex0 = 0;
        records[0].u.queue.color = 0x80FFFFFF;
        records[0].u.queue.z = 0xFFFFFFF;
        records[0].u.queue.count = 1;
        records[0].u.queue.textured = 1;
        records[0].u.queue.blend_kind = 0;
        records[0].u.queue.screen.top = 0;
        records[0].u.queue.screen.left = 0;
        records[0].u.queue.screen.right = 512;
        records[0].u.queue.screen.bottom = 448;
        records[0].u.queue.texture.top = 0;
        records[0].u.queue.texture.left = 0;
        records[0].u.queue.texture.right = 512;
        records[0].u.queue.texture.bottom = 448;
        records[1].type = 20;
        records[1].u.queue.frame = 2;
        records[1].u.queue.tex0 = 0;
        records[1].u.queue.color = params[0];
        records[1].u.queue.z = 0xFFFFFFF;
        records[1].u.queue.count = 1;
        records[1].u.queue.textured = 0;
        records[1].u.queue.blend_kind = 2;
        records[1].u.queue.screen.top = 0;
        records[1].u.queue.screen.left = 0;
        records[1].u.queue.screen.right = 512;
        records[1].u.queue.screen.bottom = 448;
        records[2].type = 13;
        records[2].u.queue.frame = 0;
        records[2].u.queue.tex0 = 2;
        records[2].u.queue.color = 0x40FFFFFF;
        records[2].u.queue.z = depth;
        records[2].u.queue.count = 1;
        records[2].u.queue.textured = 1;
        records[2].u.queue.blend_kind = 1;
        records[2].u.circle.screen.top = 0;
        records[2].u.circle.screen.left = 0;
        records[2].u.circle.screen.right = 512;
        records[2].u.circle.screen.bottom = 448;
        records[2].u.circle.texture.top = 0;
        records[2].u.circle.texture.left = 0;
        records[2].u.circle.texture.right = 512;
        records[2].u.circle.texture.bottom = 448;
        records[2].u.circle.radiusStep = 4;
        records[2].u.circle.ringCount = params[2];
        records[2].u.circle.endColor = (params[0] & RECORD_RGB_MASK);
        records[2].u.circle.startAngle = params[3];
        records[2].u.circle.segments = params[1];
        records[2].u.circle.centerDisabled = 0;
        break;
    case 8:
        records[0].type = 2;
        records[0].u.queue.tex0 = 0;
        records[1].type = 20;
        records[1].u.queue.frame = 2;
        records[1].u.queue.tex0 = 0;
        records[1].u.queue.color =
            (((((128 - params[1]) << 16) | (128 - params[1])) | ((128 - params[1]) << 8)) | 0x80000000);
        records[1].u.queue.z = 0xFFFFFFF;
        records[1].u.queue.count = 1;
        records[1].u.queue.textured = 0;
        records[1].u.queue.blend_kind = 2;
        records[1].u.queue.screen.top = 0;
        records[1].u.queue.screen.left = 0;
        records[1].u.queue.screen.right = 512;
        records[1].u.queue.screen.bottom = 448;
        records[2].type = 20;
        records[2].u.queue.frame = 0;
        records[2].u.queue.tex0 = 0;
        records[2].u.queue.color = ((((params[2] << 16) | params[2]) | (params[2] << 8)) | 0x80000000);
        records[2].u.queue.z = 0xFFFFFFF;
        records[2].u.queue.count = 1;
        records[2].u.queue.textured = 0;
        records[2].u.queue.blend_kind = 2;
        records[2].u.queue.screen.top = 0;
        records[2].u.queue.screen.left = 0;
        records[2].u.queue.screen.right = 512;
        records[2].u.queue.screen.bottom = 448;
        records[3].type = 20;
        records[3].u.queue.frame = 0;
        records[3].u.queue.tex0 = 2;
        records[3].u.queue.color = 0x80FFFFFF;
        records[3].u.queue.z = 0xFFFFFFF;
        records[3].u.queue.count = 1;
        records[3].u.queue.textured = 1;
        records[3].u.queue.blend_kind = 1;
        records[3].u.queue.screen.top = 0;
        records[3].u.queue.screen.left = 0;
        records[3].u.queue.screen.right = 512;
        records[3].u.queue.screen.bottom = 448;
        records[3].u.queue.texture.top = 0;
        records[3].u.queue.texture.left = 0;
        records[3].u.queue.texture.right = 512;
        records[3].u.queue.texture.bottom = 448;
        break;
    case 9:
        records[0].type = 2;
        records[0].u.queue.tex0 = 0;
        records[1].type = 20;
        records[1].u.queue.frame = 2;
        records[1].u.queue.tex0 = 2;
        records[1].u.queue.color = 0x80808080;
        records[1].u.queue.z = 0xFFFFFFF;
        records[1].u.queue.count = params[1];
        records[1].u.queue.textured = 1;
        records[1].u.queue.blend_kind = 2;
        records[1].u.queue.screen.top = 0;
        records[1].u.queue.screen.left = 0;
        records[1].u.queue.screen.right = 512;
        records[1].u.queue.screen.bottom = 448;
        records[1].u.queue.texture.top = 0;
        records[1].u.queue.texture.left = 0;
        records[1].u.queue.texture.right = 512;
        records[1].u.queue.texture.bottom = 448;
        records[2].type = 20;
        records[2].u.queue.frame = 2;
        records[2].u.queue.tex0 = 2;
        records[2].u.queue.color = 0x80808080;
        records[2].u.queue.z = 0xFFFFFFF;
        records[2].u.queue.count = params[1];
        records[2].u.queue.textured = 1;
        records[2].u.queue.blend_kind = 1;
        records[2].u.queue.screen.top = 0;
        records[2].u.queue.screen.left = 0;
        records[2].u.queue.screen.right = 512;
        records[2].u.queue.screen.bottom = 448;
        records[2].u.queue.texture.top = 0;
        records[2].u.queue.texture.left = 0;
        records[2].u.queue.texture.right = 512;
        records[2].u.queue.texture.bottom = 448;
        records[3].type = 20;
        records[3].u.queue.frame = 0;
        records[3].u.queue.tex0 = 2;
        records[3].u.queue.color = params[0];
        records[3].u.queue.z = depth;
        records[3].u.queue.count = 1;
        records[3].u.queue.textured = 1;
        records[3].u.queue.blend_kind = 0;
        records[3].u.queue.screen.top = 0;
        records[3].u.queue.screen.left = 0;
        records[3].u.queue.screen.right = 512;
        records[3].u.queue.screen.bottom = 448;
        records[3].u.queue.texture.top = 0;
        records[3].u.queue.texture.left = 0;
        records[3].u.queue.texture.right = 512;
        records[3].u.queue.texture.bottom = 448;
        break;
    case 10:
        records[0].type = 20;
        records[0].u.queue.frame = 0;
        records[0].u.queue.tex0 = 2;
        records[0].u.queue.color = params[0];
        records[0].u.queue.z = depth;
        records[0].u.queue.count = 1;
        records[0].u.queue.textured = params[1];
        records[0].u.queue.blend_kind = 0;
        records[0].u.queue.screen.top = 0;
        records[0].u.queue.screen.left = 0;
        records[0].u.queue.screen.right = 512;
        records[0].u.queue.screen.bottom = 448;
        records[0].u.queue.texture.top = 0;
        records[0].u.queue.texture.left = 0;
        records[0].u.queue.texture.right = 512;
        records[0].u.queue.texture.bottom = 448;
        break;
    case 11:
        records[0].type = 20;
        records[0].u.queue.frame = 2;
        records[0].u.queue.tex0 = 0;
        records[0].u.queue.color = 0x7FFFFFFF;
        records[0].u.queue.z = 0xFFFFFFF;
        records[0].u.queue.count = 1;
        records[0].u.queue.textured = 1;
        records[0].u.queue.blend_kind = 0;
        records[0].u.queue.screen.top = 0;
        records[0].u.queue.screen.left = 0;
        records[0].u.queue.screen.right = 512;
        records[0].u.queue.screen.bottom = 448;
        records[0].u.queue.texture.top = 0;
        records[0].u.queue.texture.left = 0;
        records[0].u.queue.texture.right = 512;
        records[0].u.queue.texture.bottom = 448;
        records[1].type = 20;
        records[1].u.queue.frame = 0;
        records[1].u.queue.tex0 = 2;
        records[1].u.queue.color = params[0];
        records[1].u.queue.z = depth;
        records[1].u.queue.count = 1;
        records[1].u.queue.textured = 1;
        records[1].u.queue.blend_kind = 0;
        records[1].u.queue.screen.top = params[1];
        records[1].u.queue.screen.left = params[1];
        records[1].u.queue.screen.right = params[1] + 512;
        records[1].u.queue.screen.bottom = params[1] + 448;
        records[1].u.queue.texture.top = 0;
        records[1].u.queue.texture.left = 0;
        records[1].u.queue.texture.right = 512;
        records[1].u.queue.texture.bottom = 448;
        break;
    case 12:
        records[0].type = 20;
        records[0].u.queue.frame = 0;
        records[0].u.queue.tex0 = 1;
        records[0].u.queue.color = params[0];
        records[0].u.queue.z = depth;
        records[0].u.queue.count = 1;
        records[0].u.queue.textured = 1;
        records[0].u.queue.blend_kind = 0;
        records[0].u.queue.screen.top = 0;
        records[0].u.queue.screen.left = 0;
        records[0].u.queue.screen.right = 512;
        records[0].u.queue.screen.bottom = 448;
        records[0].u.queue.texture.top = 0;
        records[0].u.queue.texture.left = 0;
        records[0].u.queue.texture.right = 512;
        records[0].u.queue.texture.bottom = 448;
        break;
    case 13:
        records[0].type = 8;
        records[0].u.queue.frame = 0;
        records[0].u.queue.tex0 = 0;
        records[0].u.queue.color = 0;
        records[0].u.queue.z = depth;
        records[0].u.queue.count = 1;
        records[0].u.queue.textured = 0;
        records[0].u.queue.blend_kind = 0;
        records[0].u.gradient.color0 = ((params[1] << 24) | params[0]);
        records[0].u.gradient.color1 = ((params[2] << 24) | params[0]);
        records[0].u.gradient.top = 0;
        records[0].u.gradient.left = 0;
        records[0].u.gradient.right = 512;
        records[0].u.gradient.bottom = 448;
        records[0].u.gradient.vertical = params[3];
        break;
    case 14: {
        int columns;
        int rows;
        int bottom;
        int top;
        int start;
        int end;
        int halfWidth;
        int halfHeight;

        columns = 9;
        rows = 6;
        bottom = -3;
        top = 451;
        start = -6;
        end = 515;
        halfWidth = (end - start) / columns / 2;
        halfHeight = (top - bottom) / rows / 2;
        records[0].type = 9;
        records[0].u.queue.frame = 0;
        records[0].u.queue.tex0 = 2;
        records[0].u.queue.z = depth;
        records[0].u.queue.count = 1;
        records[0].u.queue.textured = params[4];
        records[0].u.queue.blend_kind = 0;
        records[0].u.turn.bottom = bottom;
        records[0].u.turn.top = top;
        records[0].u.turn.columns = columns;
        records[0].u.turn.rows = rows;
        records[0].u.turn.pivotY = halfHeight;
        records[0].u.turn.maxAngle = 90;
        switch (params[1]) {
        case 1:
            records[0].u.queue.color = params[0];
            records[0].u.turn.color = params[3];
            records[0].u.turn.startX = start;
            records[0].u.turn.endX = end;
            records[0].u.turn.pivotX = halfWidth;
            records[0].u.turn.endAngle[0] = params[2];
            records[0].u.turn.endAngle[1] = params[2];
            records[0].u.turn.endAngle[2] = params[2];
            records[0].u.turn.startAngle[0] = params[2] - 180;
            records[0].u.turn.startAngle[1] = params[2] - 180;
            records[0].u.turn.startAngle[2] = params[2] - 180;
            break;
        case 2:
            records[0].u.queue.color = params[0];
            records[0].u.turn.color = params[3];
            records[0].u.turn.startX = end;
            records[0].u.turn.endX = start;
            records[0].u.turn.pivotX = -halfWidth;
            records[0].u.turn.endAngle[0] = params[2];
            records[0].u.turn.endAngle[1] = params[2];
            records[0].u.turn.endAngle[2] = params[2];
            records[0].u.turn.startAngle[0] = params[2] - 180;
            records[0].u.turn.startAngle[1] = params[2] - 180;
            records[0].u.turn.startAngle[2] = params[2] - 180;
            break;
        }
        break;
    }
    case 15: {
        int columns;
        int rows;
        int top;
        int left;
        int right;
        int bottom;
        int halfWidth;
        int halfHeight;
        int angle;

        columns = 9;
        rows = 6;
        top = 0;
        left = 0;
        right = 512;
        bottom = 448;
        halfWidth = (right - left) / columns / 2;
        halfHeight = (bottom - top) / rows / 2;
        angle = params[2];
        records[0].type = 14;
        records[0].u.queue.frame = 0;
        records[0].u.queue.tex0 = 2;
        records[0].u.queue.color = params[0];
        records[0].u.queue.z = depth;
        records[0].u.queue.count = 1;
        records[0].u.queue.textured = params[4];
        records[0].u.queue.blend_kind = 0;
        records[0].u.grid.endColor = params[3];
        records[0].u.grid.screen.top = top;
        records[0].u.grid.screen.left = left;
        records[0].u.grid.screen.right = right;
        records[0].u.grid.screen.bottom = bottom;
        records[0].u.grid.columns = columns;
        records[0].u.grid.rows = rows;
        records[0].u.grid.centerX = 228;
        records[0].u.grid.centerY = 187;
        records[0].u.grid.radius = 350;
        records[0].u.grid.startAngle[0] = angle;
        records[0].u.grid.startAngle[1] = angle;
        records[0].u.grid.startAngle[2] = angle;
        records[0].u.grid.endAngle[0] = angle - 180;
        records[0].u.grid.endAngle[1] = angle - 180;
        records[0].u.grid.endAngle[2] = angle - 180;
        records[0].u.grid.pivotX = halfWidth;
        records[0].u.grid.pivotY = halfHeight;
        records[0].u.grid.minAngle = 0;
        records[0].u.grid.maxAngle = 90;
        records[0].u.grid.invert = params[1];
        break;
    }
    case 16: {
        int phaseSpeed;

        phaseSpeed = (recordUpdatePaused.paused == 0) ? 184 : 180;
        records[0].type = 10;
        records[0].u.queue.frame = 0;
        records[0].u.queue.tex0 = 0;
        records[0].u.queue.color = (params[0] & RECORD_RGB_MASK);
        records[0].u.queue.z = depth;
        records[0].u.queue.count = 1;
        records[0].u.queue.textured = 0;
        records[0].u.queue.blend_kind = 0;
        records[0].u.ring.endColor = params[0];
        records[0].u.ring.centerX = 256;
        records[0].u.ring.centerY = 224;
        records[0].u.ring.radius = params[1];
        records[0].u.ring.innerRadius = 350;
        records[0].u.ring.phaseSpeed = phaseSpeed;
        records[0].u.ring.segments = 35;
        records[0].u.ring.amplitude = params[2];
        records[1].type = 10;
        records[1].u.queue.frame = 0;
        records[1].u.queue.tex0 = 0;
        records[1].u.queue.color = (params[0] & RECORD_RGB_MASK);
        records[1].u.queue.z = depth;
        records[1].u.queue.count = 1;
        records[1].u.queue.textured = 0;
        records[1].u.queue.blend_kind = 0;
        records[1].u.ring.endColor = params[0];
        records[1].u.ring.centerX = 256;
        records[1].u.ring.centerY = 224;
        records[1].u.ring.radius = params[1];
        records[1].u.ring.innerRadius = 350;
        records[1].u.ring.phaseSpeed = phaseSpeed;
        records[1].u.ring.segments = 35;
        records[1].u.ring.amplitude = params[2];
        break;
    }
    case 17:
        records[0].type = 16;
        records[0].u.queue.frame = 0;
        records[0].u.queue.tex0 = 1;
        records[0].u.queue.color = params[0];
        records[0].u.queue.z = depth;
        records[0].u.queue.count = 1;
        records[0].u.queue.textured = 1;
        records[0].u.queue.blend_kind = 0;
        records[0].u.params[7] = 0;
        records[0].u.params[8] = 0;
        records[0].u.params[9] = 512;
        records[0].u.params[10] = 448;
        records[0].u.params[11] = 0;
        records[0].u.params[12] = 0;
        records[0].u.params[13] = 512;
        records[0].u.params[14] = 448;
        records[0].u.params[15] = params[1];
        records[0].u.params[16] = params[2];
        records[0].u.params[17] = params[3];
        records[0].u.params[18] = params[4];
        records[0].u.params[19] = params[5];
        records[0].u.params[20] = 256;
        records[0].u.params[21] = 124;
        break;
    case 18:
        records[0].type = 20;
        records[0].u.queue.frame = 2;
        records[0].u.queue.tex0 = 0;
        records[0].u.queue.color = 0x7FFFFFFF;
        records[0].u.queue.z = 0x7FFFFFFF;
        records[0].u.queue.count = 1;
        records[0].u.queue.textured = 1;
        records[0].u.queue.blend_kind = 0;
        records[0].u.queue.screen.top = 0;
        records[0].u.queue.screen.left = 0;
        records[0].u.queue.screen.right = 512;
        records[0].u.queue.screen.bottom = 448;
        records[0].u.queue.texture.top = 0;
        records[0].u.queue.texture.left = 0;
        records[0].u.queue.texture.right = 512;
        records[0].u.queue.texture.bottom = 448;
        records[1].type = 20;
        records[1].u.queue.frame = 2;
        records[1].u.queue.tex0 = 0;
        records[1].u.queue.color = 0;
        records[1].u.queue.z = depth;
        records[1].u.queue.count = 1;
        records[1].u.queue.textured = 0;
        records[1].u.queue.blend_kind = 0;
        records[1].u.queue.screen.top = 0;
        records[1].u.queue.screen.left = 0;
        records[1].u.queue.screen.right = 512;
        records[1].u.queue.screen.bottom = 448;
        records[2].type = 15;
        records[2].u.queue.frame = 1;
        records[2].u.queue.tex0 = 1;
        records[2].u.queue.color = 0;
        records[3].type = 20;
        records[3].u.queue.frame = 0;
        records[3].u.queue.tex0 = 2;
        records[3].u.queue.color = params[0];
        records[3].u.queue.z = 0x7FFFFFFF;
        records[3].u.queue.count = 1;
        records[3].u.queue.textured = 1;
        records[3].u.queue.blend_kind = 0;
        records[3].u.queue.screen.top = params[1];
        records[3].u.queue.screen.left = params[1];
        records[3].u.queue.screen.right = params[1] + 512;
        records[3].u.queue.screen.bottom = params[1] + 448;
        records[3].u.queue.texture.top = 0;
        records[3].u.queue.texture.left = 0;
        records[3].u.queue.texture.right = 512;
        records[3].u.queue.texture.bottom = 448;
        records[4].type = 15;
        records[4].u.queue.frame = 0;
        break;
    case 20:
        records[0].type = 1;
        records[0].u.fade.stage = params[0];
        records[0].u.fade.alpha = params[1];
        records[0].u.fade.depth = depth;
        break;
    case 22:
        records[0].type = 2;
        records[0].u.queue.tex0 = 1;
        records[1].type = 5;
        records[1].u.fan.spread = params[0];
        records[1].u.fan.z = depth;
        break;
    case 21:
        records[0].type = 6;
        records[0].u.warp.alpha = params[0];
        records[0].u.warp.z = depth;
        break;
    case 19:
        records[0].type = 3;
        records[0].u.tunnel.step = params[0];
        records[0].u.tunnel.depth = depth;
        break;
    case 23:
        records[0].type = 7;
        records[0].u.params[0] = params[0];
        records[0].u.params[1] = depth;
        break;
    case 0:
    case 24:
        break;
    }
    recordUpdateRenderHook();
}

/* Sets record slotIndex of bank bankIndex to type with its ten parameters, then updates the render callback. */
void recordSetSlot(int bankIndex, int slotIndex, int type, int *params)
{
    int i;

    ((GsQueueContext *)(recordBanks + bankIndex * sizeof(RecordBank)))[slotIndex].type = type;
    for (i = 0; i < 10; i++) {
        ((RecordBank *)recordBanks)[bankIndex].records[slotIndex].u.params[i] = params[i];
    }
    recordUpdateRenderHook();
}

/* Clears the ten records of bank bankIndex. */
void recordClearBank(int bankIndex)
{
    unsigned char *record;
    int slot;

    record = recordBanks + bankIndex * sizeof(RecordBank);
    for (slot = 0; slot < 10; slot++) {
        recordClear((GsQueueContext *) record);
        record += sizeof(GsQueueContext);
    }
}

/* Clears the first three banks' records. */
void recordClearBanks(void)
{
    int bankIndex;

    bankIndex = 0;
    do {
        recordClearBank(bankIndex);
        bankIndex += 1;
    } while (bankIndex < 3);
}

void sceVif1PkCnt(XglPacket *packet, int count);
void sceVif1PkOpenDirectHLCode(XglPacket *packet, int mode);
void sceVif1PkAddDirectDataN(XglPacket *packet, const void *data, int qwc);
void sceVif1PkCloseDirectHLCode(XglPacket *packet);
int recordAnyUsed(void);
/* GIF data sent before (9 quadwords) and after (3 quadwords) the records are drawn. */
extern u64 recordPreDrawGifData[];
extern u64 recordPostDrawGifData[];
/* Draw handlers of record types 1..21. The handlers of types 1, 3, 5, 6 and 7 build their own packet on the
 * scratchpad (xglPacketGetCurrent) and leave the packet argument unused. */
extern void (*recordDrawHandlers[])(GsQueueContext *record, XglPacket *packet);

/* Calls the draw handler of every record of type 1..21 in the three banks between two fixed GIF blocks, then
 * clears the banks and removes itself as the render callback. */
void recordRenderAll(XglPacket *packet)
{
    RecordBank *bank;
    GsQueueContext *record;
    int bankIndex;
    int slot;
    int type;

    bank = (RecordBank *)recordBanks;
    if (recordAnyUsed() == 0) {
        return;
    }
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectHLCode(packet, 0);
    sceVif1PkAddDirectDataN(packet, recordPreDrawGifData, 9);
    sceVif1PkCloseDirectHLCode(packet);
    for (bankIndex = 0; bankIndex < 3; bankIndex++) {
        record = bank->records;
        for (slot = 0; slot < 10; slot++) {
            type = record->type;
            /* if (type < 22 && type != 0) gives beq with the slot filled from the call instead of beql */
            switch (type) {
            case 1 ... 21:
                recordDrawHandlers[type - 1](record, packet);
                break;
            }
            record++;
        }
        bank++;
    }
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectHLCode(packet, 0);
    sceVif1PkAddDirectDataN(packet, recordPostDrawGifData, 3);
    sceVif1PkCloseDirectHLCode(packet);
    recordClearBanks();
    recordUpdatePaused.paused = 0;
    sRender.callback = 0;
}

/* True if any of the 30 records (3 banks of 10) has a nonzero first byte. */
int recordAnyUsed(void)
{
    unsigned char *bank;
    unsigned char *record;
    int bankIndex;
    int slot;

    bank = recordBanks;
    for (bankIndex = 0; bankIndex < 3; bankIndex++) {
        for (slot = 0, record = bank; slot < 10; slot++) {
            if (*record != 0) {
                return 1;
            }
            record += sizeof(GsQueueContext);
        }
        bank += sizeof(RecordBank);
    }
    return 0;
}

extern void *memset(void *destination, int value, unsigned int count);

/* Clears one record. */
void recordClear(GsQueueContext *record)
{
  memset(record, 0, sizeof(*record));
}

/* Sets recordUpdatePaused, the flag that suspends the per-frame record updates. */
void recordPauseUpdates(void)
{
  recordUpdatePaused.paused = 1;
}

/* The EE scratchpad, where the packet is built before it is sent. */
#define SCRATCHPAD_BASE 0x70000000

/* GS PRIM primitive types of the triangle strip and the triangle fan (gs.h names the sprite). */
#define GS_PRIM_TRISTRIP 4
#define GS_PRIM_TRIFAN 5


XglPacket *xglPacketGetCurrent(void);
u64 gsMakeTex0(int buffer, int flag);
u64 gsMakeFrame(int buffer, int flag);
int gsMakeAlpha(int blend);
void recordStepAngle(int stage);
int recordStageSize(int mode, int fallback);
int recordStageColor(int mode, int alpha);
int recordPulseColor(void);

/* Draw handler of record type 1 (recordDrawHandlers[0]): one full-screen 512x448 sprite whose texture rectangle is
   inset by the stage's size, in the stage's color; at stage 2 the sprite is drawn a second time in the pulsing
   color. */
void recordDrawType1(GsQueueContext *record, XglPacket *unused)
{
    int stage;
    int alpha;
    int z;
    unsigned int color;
    int size;
    XglPacket *packet;
    GsPair *pairs;
    int pairCount;

    stage = record->u.fade.stage;
    alpha = record->u.fade.alpha;
    z = record->u.fade.depth;
    recordStepAngle(stage);
    color = recordStageColor(stage, alpha);
    size = recordStageSize(stage, alpha);
    packet = xglPacketGetCurrent();
    pairs = (GsPair *) SCRATCHPAD_BASE;
    pairCount = 0;
    /* GIF tag (one A+D register, EOP); the loop count is added once the packet is complete. */
    pairs[pairCount].ad.reg = GIF_AD;
    pairs[pairCount].ad.data = ((u64) 0x10000000 << 32) | 0x8000;
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEXFLUSH;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEST_1;
    pairs[pairCount].ad.data = GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 2);
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEX1_1;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEXA;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_ALPHA_1;
    pairs[pairCount].ad.data = gsMakeAlpha(0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEX0_1;
    pairs[pairCount].ad.data = gsMakeTex0(1, 0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_FRAME_1;
    pairs[pairCount].ad.data = gsMakeFrame(0, 0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_PRIM;
    pairs[pairCount].ad.data = GS_SET_PRIM(GS_PRIM_SPRITE, 1, 1, 0, 1, 0, 1, 0, 0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_RGBAQ;
    pairs[pairCount].ad.data = color;
    pairCount++;
    /* Corners in 12.4 fixed point: (0x7000, 0x7200) is the screen's top left around the GS origin 2048,
       (0x9000, 0x8E00) its bottom right. */
    pairs[pairCount].ad.reg = GS_UV;
    pairs[pairCount].ad.data = GS_SET_UV(size << 4, size << 4);
    pairCount++;
    pairs[pairCount].ad.reg = GS_XYZF2;
    pairs[pairCount].ad.data = GS_SET_XYZ(0x7000, 0x7200, z);
    pairCount++;
    pairs[pairCount].ad.reg = GS_UV;
    pairs[pairCount].ad.data = GS_SET_UV((0x200 - size) << 4, (0x1C0 - size) << 4);
    pairCount++;
    pairs[pairCount].ad.reg = GS_XYZF2;
    pairs[pairCount].ad.data = GS_SET_XYZ(0x9000, 0x8E00, z);
    pairCount++;
    if (stage == 2) {
        color = recordPulseColor();
        pairs[pairCount].ad.reg = GS_RGBAQ;
        pairs[pairCount].ad.data = color;
        pairCount++;
        pairs[pairCount].ad.reg = GS_UV;
        pairs[pairCount].ad.data = GS_SET_UV(size << 4, size << 4);
        pairCount++;
        pairs[pairCount].ad.reg = GS_XYZF2;
        pairs[pairCount].ad.data = GS_SET_XYZ(0x7000, 0x7200, z);
        pairCount++;
        pairs[pairCount].ad.reg = GS_UV;
        pairs[pairCount].ad.data = GS_SET_UV((0x200 - size) << 4, (0x1C0 - size) << 4);
        pairCount++;
        pairs[pairCount].ad.reg = GS_XYZF2;
        pairs[pairCount].ad.data = GS_SET_XYZ(0x9000, 0x8E00, z);
        pairCount++;
    }
    pairs[0].ad.data += pairCount - 1;
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectHLCode(packet, 0);
    sceVif1PkAddDirectDataN(packet, pairs, pairCount);
    sceVif1PkCloseDirectHLCode(packet);
}

/* Mode selector: mode 0 and 2 return fallback, mode 1 returns 5, anything else returns 0. */
int recordStageSize(int mode, int fallback)
{
    switch (mode) {
    case 0:
    case 2:
        return fallback;
    case 1:
        return 5;
    default:
        return 0;
    }
}

int recordAngle = 0;
float sinf(float angle);

/* Sprite color of stage mode: grey 0x808080 under alpha at stage 1, the fixed 0x40808080 (alpha 0x40, whatever alpha
   is) at stage 0, at stage 2 the alpha 80 + 5 * sin(recordAngle degrees) put into 0..127 by wrapIntRange; 0 for any
   other stage. */
int recordStageColor(int mode, int alpha)
{
    switch (mode) {
    case 1:
        return (alpha << 24) | 0x808080;
    case 0:
        return 0x40808080;
    case 2:
        alpha = wrapIntRange((int) (sinf(recordAngle * 3.141592 / 180.0) * 5.0f + 80.0f), 0, 0x7F);
        return (alpha << 24) | 0x808080;
    default:
        return 0;
    }
}

float cosf(float angle);

/* Pulsing color: grey 0x808080 under the alpha 80 + 5 * cos(recordAngle degrees), put into 0..127 by
   wrapIntRange. */
int recordPulseColor(void)
{
    int alpha;

    alpha = wrapIntRange((int) (cosf(recordAngle * 3.141592 / 180.0) * 5.0f + 80.0f), 0, 0x7F);
    return (alpha << 24) | 0x808080;
}

/* At stage 2, while updates are not suspended, steps the angle recordAngle by 92 and wraps it into 0..360. */
void recordStepAngle(int stage)
{
    if (recordUpdatePaused.paused == 0 && stage == 2) {
        recordAngle += 0x5C;
        recordAngle = wrapIntRange(recordAngle, 0, 0x168);
    }
}

void spriteBatchDraw(GsQueueContext *context, XglPacket *packet);

/* Draw handler of record type 2 (recordDrawHandlers[1]): draws the record's texture (its tex0 word) over the whole
   512x448
   screen through a type 20 sprite record. */
void recordDrawType2(GsQueueContext *record, XglPacket *packet)
{
    GsQueueContext sprite;

    sprite.type = 0x14;
    sprite.u.queue.frame = 2;
    sprite.u.queue.tex0 = record->u.queue.tex0;
    sprite.u.queue.color = 0x80FFFFFF;
    sprite.u.queue.z = 0x7FFFFFFF;
    sprite.u.queue.count = 1;
    sprite.u.queue.textured = 1;
    sprite.u.queue.blend_kind = 0;
    sprite.u.queue.screen.top = 0;
    sprite.u.queue.screen.left = 0;
    sprite.u.queue.screen.right = 0x200;
    sprite.u.queue.screen.bottom = 0x1C0;
    sprite.u.queue.texture.top = 0;
    sprite.u.queue.texture.left = 0;
    sprite.u.queue.texture.right = 0x200;
    sprite.u.queue.texture.bottom = 0x1C0;
    spriteBatchDraw(&sprite, packet);
}

void recordAdvanceRingRadius(int step);
void recordRingPoint(float *point, float *clampedPoint, float angle, float radius);
float recordTunnelDegToRad = 0.0174532887f;
int recordTunnelRadius = 380;

/* Ring spacing of the type 3 tunnel, the radius its first ring is kept below and the radius its rings stop at. */
#define RECORD_TUNNEL_RING_SPACING 25
#define RECORD_TUNNEL_WRAP_RADIUS 406
#define RECORD_TUNNEL_END_RADIUS 620

/* Draw handler of record type 3 (recordDrawHandlers[2]): a triangle fan from the screen centre through octagonal rings
   of
   radius recordTunnelRadius, recordTunnelRadius + RECORD_TUNNEL_RING_SPACING, ... below RECORD_TUNNEL_END_RADIUS
   (recordAdvanceRingRadius advances recordTunnelRadius by the record's step); each ring vertex takes its texture
   coordinates from the point clamped to radius 224, at the record's depth. */
void recordDrawType3(GsQueueContext *record, XglPacket *unused)
{
    int z;
    XglPacket *packet;
    GsPair *pairs;
    int pairCount;
    int radius;
    int side;
    float point[2];
    float clampedPoint[2];

    z = record->u.tunnel.depth;
    recordAdvanceRingRadius(record->u.tunnel.step);
    packet = xglPacketGetCurrent();
    pairs = (GsPair *) SCRATCHPAD_BASE;
    pairCount = 0;
    /* GIF tag (one A+D register, EOP); the loop count is added once the packet is complete. */
    pairs[pairCount].ad.reg = GIF_AD;
    pairs[pairCount].ad.data = ((u64) 0x10000000 << 32) | 0x8000;
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEXFLUSH;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEST_1;
    pairs[pairCount].ad.data = GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 2);
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEX1_1;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEXA;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_ALPHA_1;
    pairs[pairCount].ad.data = gsMakeAlpha(0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEX0_1;
    pairs[pairCount].ad.data = gsMakeTex0(1, 0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_FRAME_1;
    pairs[pairCount].ad.data = gsMakeFrame(0, 0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_PRIM;
    pairs[pairCount].ad.data = GS_SET_PRIM(GS_PRIM_TRIFAN, 1, 1, 0, 1, 0, 1, 0, 0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_RGBAQ;
    pairs[pairCount].ad.data = GS_SET_RGBAQ(0x80, 0x80, 0x80, 0, 0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_UV;
    pairs[pairCount].ad.data = GS_SET_UV(256 << 4, 224 << 4);
    pairCount++;
    pairs[pairCount].ad.reg = GS_XYZF2;
    pairs[pairCount].ad.data = GS_SET_XYZ(0x8000, 0x8000, z);
    pairCount++;
    pairs[pairCount].ad.reg = GS_RGBAQ;
    pairs[pairCount].ad.data = GS_SET_RGBAQ(0x80, 0x80, 0x80, 0x23, 0);
    pairCount++;
    /* point is in screen pixels: + (2048 - 256, 2048 - 224) puts it around the GS origin, in 12.4 fixed point.
       The first vertex of a ring goes to the XYZ3 register, without a drawing kick; recordTunnelDegToRad converts
       degrees to radians. */
    for (radius = recordTunnelRadius; radius < RECORD_TUNNEL_END_RADIUS; radius += RECORD_TUNNEL_RING_SPACING) {
        recordRingPoint(point, clampedPoint, 0.0f, radius);
        pairs[pairCount].ad.reg = GS_UV;
        pairs[pairCount].ad.data = GS_SET_UV(clampedPoint[0] * 16.0f, clampedPoint[1] * 16.0f);
        pairCount++;
        pairs[pairCount].ad.reg = GS_XYZ3;
        pairs[pairCount].ad.data = GS_SET_XYZ((int) ((point[0] + 1792.0f) * 16.0f),
                                              (int) ((point[1] + 1824.0f) * 16.0f), z);
        pairCount++;
        for (side = 1; side <= 8; side++) {
            recordRingPoint(point, clampedPoint, side * 45 * recordTunnelDegToRad, radius);
            pairs[pairCount].ad.reg = GS_UV;
            pairs[pairCount].ad.data = GS_SET_UV(clampedPoint[0] * 16.0f, clampedPoint[1] * 16.0f);
            pairCount++;
            pairs[pairCount].ad.reg = GS_XYZF2;
            pairs[pairCount].ad.data = GS_SET_XYZ((int) ((point[0] + 1792.0f) * 16.0f),
                                                  (int) ((point[1] + 1824.0f) * 16.0f), z);
            pairCount++;
        }
    }
    pairs[0].ad.data += pairCount - 1;
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectHLCode(packet, 0);
    sceVif1PkAddDirectDataN(packet, pairs, pairCount);
    sceVif1PkCloseDirectHLCode(packet);
}

/* point: the screen position at angle (radians) and radius around the screen centre (256, 224); clampedPoint: the
   same with the radius clamped to 224. */
void recordRingPoint(float *point, float *clampedPoint, float angle, float radius)
{
    float cosine;
    float sine;

    cosine = cosf(angle);
    sine = sinf(angle);
    point[0] = radius * cosine + 256.0f;
    point[1] = radius * sine + 224.0f;
    if (radius > 224.0f) {
        radius = 224.0f;
    }
    clampedPoint[0] = radius * cosine + 256.0f;
    clampedPoint[1] = radius * sine + 224.0f;
}

/* While updates are not suspended, advances recordTunnelRadius by step and steps it back below
 * RECORD_TUNNEL_WRAP_RADIUS one ring spacing at a time. */
void recordAdvanceRingRadius(int step)
{
    if (recordUpdatePaused.paused == 0) {
        recordTunnelRadius += step;
        while (recordTunnelRadius >= RECORD_TUNNEL_WRAP_RADIUS) {
            recordTunnelRadius -= RECORD_TUNNEL_RING_SPACING;
        }
    }
}

/* Row length of the cell grid: 50 x 50 flags (cleared by recordGridReset), of which recordGridWidth
 * x recordGridHeight are in use; recordDrawType4 indexes a cell as column + row * 50. */
#define RECORD_GRID_SIDE 50

int recordGridWidth = 15;
int recordGridHeight = 15;
/* signed char: recordDrawType4 reads the flags with lb (fd.sh with unsigned char: lbu $2,0($4) at its first flag
 * read, 221/225). */
extern signed char recordGridCells[];

void gsQueueBeginSprites(GsQueueContext *context);
void gsQueueAddTexturedVertex(XglPacket *unused, Vector4 *position, Vector4 *uv, GsQueueContext *context);
void gsQueueAddTexturedKickVertex(XglPacket *packet, Vector4 *position, Vector4 *uv, GsQueueContext *context);
void gsQueueFlush(XglPacket *packet);

/*
 * Draw handler of record type 4 (recordDrawHandlers[3]): sets random free cells of the grid until cellCount (clamped to
 * 0..width * height) are set, or clears random set ones down to it, then draws each set cell as one sprite of
 * 512 / width x 448 / height pixels at the record's depth, count times. Reading count before depth gives the
 * original's depth reload register (fd.sh: depth first 216/225, count first 225/225); each random loop declares
 * its own cell, randomColumn and randomRow (at function scope fd.sh gives 33/225).
 */
void recordDrawType4(GsQueueContext *record, XglPacket *packet)
{
    Vector4 position;
    Vector4 kickPosition;
    int pass;
    int depth;
    int cellHeight;
    int cellWidth;
    int cellTotal;
    int cellCount;
    int count;
    int filled;
    int row;
    int column;
    int toSet;
    int toClear;

    cellTotal = recordGridHeight * recordGridWidth;
    cellCount = record->u.gridFill.cellCount;
    count = record->u.queue.count;
    depth = record->u.queue.z;
    if (cellCount < 0) {
        cellCount = 0;
    }
    if (cellCount > cellTotal) {
        cellCount = cellTotal;
    }
    filled = 0;
    for (row = 0; row < recordGridHeight; row++) {
        for (column = 0; column < recordGridWidth; column++) {
            if (recordGridCells[column + row * RECORD_GRID_SIDE] == 1) {
                filled++;
            }
        }
    }
    toSet = cellCount - filled;
    toClear = filled - cellCount;
    for (column = 0; column < toSet; column++) {
        signed char *cell;
        int randomColumn;
        int randomRow;

        do {
            randomColumn = xglSRand() % recordGridWidth;
            randomRow = xglSRand() % recordGridHeight;
            cell = &recordGridCells[randomColumn + randomRow * RECORD_GRID_SIDE];
        } while (*cell == 1);
        *cell = 1;
    }
    for (column = 0; column < toClear; column++) {
        signed char *cell;
        int randomColumn;
        int randomRow;

        do {
            randomColumn = xglSRand() % recordGridWidth;
            randomRow = xglSRand() % recordGridHeight;
            cell = &recordGridCells[randomColumn + randomRow * RECORD_GRID_SIDE];
        } while (*cell == 0);
        *cell = 0;
    }
    cellWidth = 512 / recordGridWidth;
    cellHeight = 448 / recordGridHeight;
    gsQueueBeginSprites(record);
    for (pass = 0; pass < count; pass++) {
        for (row = 0; row < recordGridHeight; row++) {
            for (column = 0; column < recordGridWidth; column++) {
                if (recordGridCells[column + row * RECORD_GRID_SIDE] != 0) {
                    position.x = cellWidth * column;
                    position.y = cellHeight * row;
                    position.z = depth;
                    kickPosition.x = cellWidth * (column + 1);
                    kickPosition.y = cellHeight * (row + 1);
                    kickPosition.z = depth;
                    gsQueueAddTexturedVertex(packet, &position, &position, record);
                    gsQueueAddTexturedKickVertex(packet, &kickPosition, &kickPosition, record);
                }
            }
        }
    }
    gsQueueFlush(packet);
}

/* Clears the cell grid and sets its width and height (cells across and down). */
void recordGridReset(int width, int height)
{
    memset(recordGridCells, 0, RECORD_GRID_SIDE * RECORD_GRID_SIDE * sizeof(recordGridCells[0]));
    recordGridWidth = width;
    recordGridHeight = height;
}

/* Number of cells in the grid: width * height. */
int recordGridCellCount(void)
{
  return recordGridWidth * recordGridHeight;
}

void recordStepWarp(void);
void recordWarpCorner(Vector4 *result, int x, int y);

/* XYZ of a point in screen pixels: + (2048 - 256, 2048 - 224) puts it around the GS origin, in 12.4 fixed point. */
#define RECORD_XYZ(x, y, z) GS_SET_XYZ((int) (((x) + 1792.0f) * 16.0f), (int) (((y) + 1824.0f) * 16.0f), z)

/*
 * Draw handler of record type 5 (recordDrawHandlers[4]): draws the screen texture as a triangle fan around the screen
 * centre (256, 224). The four edge midpoints move out by an eighth of the record's spread, each corner
 * by spread along one axis and half of it along the other, a quarter turn further at each corner. The fan goes
 * around twice and closes at its first point. Z and fog take the record's z word.
 */
void recordDrawType5(GsQueueContext *record, XglPacket *unused)
{
    GsPair *pairs;
    XglPacket *packet;
    int spread;
    int z;
    int eighth;
    int pairCount;
    int lap;

    spread = record->u.fan.spread;
    z = record->u.fan.z;
    packet = xglPacketGetCurrent();
    eighth = spread >> 3;
    pairs = (GsPair *) SCRATCHPAD_BASE;
    pairCount = 0;
    pairs[pairCount].ad.reg = GIF_AD;
    pairs[pairCount].ad.data = ((u64)0x10000000 << 32) | 0x8000;
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEXFLUSH;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEST_1;
    pairs[pairCount].ad.data = GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 2);
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEX1_1;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEXA;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_ALPHA_1;
    pairs[pairCount].ad.data = gsMakeAlpha(0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEX0_1;
    pairs[pairCount].ad.data = gsMakeTex0(2, 0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_FRAME_1;
    pairs[pairCount].ad.data = gsMakeFrame(0, 0);
    pairCount++;
    pairs[pairCount].ad.data = GS_SET_PRIM(GS_PRIM_TRIFAN, 1, 1, 0, 1, 0, 1, 0, 0);
    pairs[pairCount].ad.reg = GS_PRIM;
    pairCount++;
    pairs[pairCount].ad.reg = GS_RGBAQ;
    pairs[pairCount].ad.data = 0x40808080;
    pairCount++;
    pairs[pairCount].ad.reg = GS_UV;
    pairs[pairCount].ad.data = GS_SET_UV(256 * 16, 224 * 16);
    pairCount++;
    pairs[pairCount].ad.reg = GS_XYZF2;
    pairs[pairCount].ad.data = RECORD_XYZ(256, 224, z);
    pairCount++;
    for (lap = 0; lap < 2; lap++) {
        /* Declared in the loop: at function scope gcc leaves the 512 + half chain in the loop (fd.sh 16/331). */
        int half = spread >> 1;

        pairs[pairCount].ad.reg = GS_UV;
        pairs[pairCount].ad.data = GS_SET_UV(512 * 16, 224 * 16);
        pairCount++;
        pairs[pairCount].ad.reg = GS_XYZF2;
        pairs[pairCount].ad.data = RECORD_XYZ(512 + eighth, 224, z);
        pairCount++;
        pairs[pairCount].ad.reg = GS_UV;
        pairs[pairCount].ad.data = GS_SET_UV(512 * 16, 448 * 16);
        pairCount++;
        pairs[pairCount].ad.reg = GS_XYZF2;
        pairs[pairCount].ad.data = RECORD_XYZ(512 + spread, 448 + half, z);
        pairCount++;
        pairs[pairCount].ad.reg = GS_UV;
        pairs[pairCount].ad.data = GS_SET_UV(256 * 16, 448 * 16);
        pairCount++;
        pairs[pairCount].ad.reg = GS_XYZF2;
        pairs[pairCount].ad.data = RECORD_XYZ(256, 448 + eighth, z);
        pairCount++;
        pairs[pairCount].ad.reg = GS_UV;
        pairs[pairCount].ad.data = GS_SET_UV(0, 448 * 16);
        pairCount++;
        pairs[pairCount].ad.reg = GS_XYZF2;
        pairs[pairCount].ad.data = RECORD_XYZ(-half, 448 + spread, z);
        pairCount++;
        pairs[pairCount].ad.reg = GS_UV;
        pairs[pairCount].ad.data = GS_SET_UV(0, 224 * 16);
        pairCount++;
        pairs[pairCount].ad.reg = GS_XYZF2;
        pairs[pairCount].ad.data = RECORD_XYZ(-eighth, 224, z);
        pairCount++;
        pairs[pairCount].ad.reg = GS_UV;
        pairs[pairCount].ad.data = GS_SET_UV(0, 0);
        pairCount++;
        pairs[pairCount].ad.reg = GS_XYZF2;
        pairs[pairCount].ad.data = RECORD_XYZ(-spread, -half, z);
        pairCount++;
        pairs[pairCount].ad.reg = GS_UV;
        pairs[pairCount].ad.data = GS_SET_UV(256 * 16, 0);
        pairCount++;
        pairs[pairCount].ad.reg = GS_XYZF2;
        pairs[pairCount].ad.data = RECORD_XYZ(256, -eighth, z);
        pairCount++;
        pairs[pairCount].ad.reg = GS_UV;
        pairs[pairCount].ad.data = GS_SET_UV(512 * 16, 0);
        pairCount++;
        pairs[pairCount].ad.reg = GS_XYZF2;
        pairs[pairCount].ad.data = RECORD_XYZ(512 + half, -spread, z);
        pairCount++;
    }
    pairs[pairCount].ad.reg = GS_UV;
    pairs[pairCount].ad.data = GS_SET_UV(512 * 16, 224 * 16);
    pairCount++;
    pairs[pairCount].ad.reg = GS_XYZF2;
    pairs[pairCount].ad.data = RECORD_XYZ(512 + eighth, 224, z);
    pairCount++;
    pairs[0].ad.data += pairCount - 1;
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectHLCode(packet, 0);
    sceVif1PkAddDirectDataN(packet, pairs, pairCount);
    sceVif1PkCloseDirectHLCode(packet);
}

/*
 * Draw handler of record type 6 (recordDrawHandlers[5]): draws the 512 x 448 screen texture as one triangle fan of its
   four
 * corners, each corner moved by recordWarpCorner. XYZF2 takes the record's z as the whole upper word (Z and fog).
 */
void recordDrawType6(GsQueueContext *record, XglPacket *unused)
{
    Vector4 point;
    GsPair *pairs;
    XglPacket *packet;
    int alpha;
    int z;
    int pairCount;

    alpha = record->u.warp.alpha;
    z = record->u.warp.z;
    recordStepWarp();
    packet = xglPacketGetCurrent();
    pairs = (GsPair *) SCRATCHPAD_BASE;
    pairCount = 0;
    pairs[pairCount].ad.reg = GIF_AD;
    pairs[pairCount].ad.data = ((u64)0x10000000 << 32) | 0x8000;
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEXFLUSH;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEST_1;
    pairs[pairCount].ad.data = GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 2);
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEX1_1;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEXA;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_ALPHA_1;
    pairs[pairCount].ad.data = gsMakeAlpha(0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEX0_1;
    pairs[pairCount].ad.data = gsMakeTex0(1, 0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_FRAME_1;
    pairs[pairCount].ad.data = gsMakeFrame(0, 0);
    pairCount++;
    pairs[pairCount].ad.data = GS_SET_PRIM(GS_PRIM_TRIFAN, 1, 1, 0, 1, 0, 1, 0, 0);
    pairs[pairCount].ad.reg = GS_PRIM;
    pairCount++;
    /* Grey 0x808080 with the record's alpha. */
    pairs[pairCount].ad.reg = GS_RGBAQ;
    pairs[pairCount].ad.data = (unsigned int)(alpha << 24 | 0x808080);
    pairCount++;
    recordWarpCorner(&point, 0, 0);
    pairs[pairCount].ad.reg = GS_UV;
    pairs[pairCount].ad.data = GS_SET_UV(0, 0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_XYZF2;
    pairs[pairCount].ad.data = GS_SET_XYZ((int)((point.x + 1792.0f) * 16.0f), (int)((point.y + 1824.0f) * 16.0f), z);
    pairCount++;
    recordWarpCorner(&point, 512, 0);
    pairs[pairCount].ad.reg = GS_UV;
    pairs[pairCount].ad.data = GS_SET_UV(512 * 16, 0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_XYZF2;
    pairs[pairCount].ad.data = GS_SET_XYZ((int)((point.x + 1792.0f) * 16.0f), (int)((point.y + 1824.0f) * 16.0f), z);
    pairCount++;
    recordWarpCorner(&point, 512, 448);
    pairs[pairCount].ad.reg = GS_UV;
    pairs[pairCount].ad.data = GS_SET_UV(512 * 16, 448 * 16);
    pairCount++;
    pairs[pairCount].ad.reg = GS_XYZF2;
    pairs[pairCount].ad.data = GS_SET_XYZ((int)((point.x + 1792.0f) * 16.0f), (int)((point.y + 1824.0f) * 16.0f), z);
    pairCount++;
    recordWarpCorner(&point, 0, 448);
    pairs[pairCount].ad.reg = GS_UV;
    pairs[pairCount].ad.data = GS_SET_UV(0, 448 * 16);
    pairCount++;
    pairs[pairCount].ad.reg = GS_XYZF2;
    pairs[pairCount].ad.data = GS_SET_XYZ((int)((point.x + 1792.0f) * 16.0f), (int)((point.y + 1824.0f) * 16.0f), z);
    pairCount++;
    pairs[0].ad.data += pairCount - 1;
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectHLCode(packet, 0);
    sceVif1PkAddDirectDataN(packet, pairs, pairCount);
    sceVif1PkCloseDirectHLCode(packet);
}

float recordWarpAngle = -1.37f;
float recordWarpVelocity = 0.07f;

/* Integrates recordWarpAngle by recordWarpVelocity and stops the motion once it crosses -1.0f. */
void recordStepWarp(void)
{
    float velocity;
    float next;

    if (recordUpdatePaused.paused != 0) {
        return;
    }

    velocity = recordWarpVelocity;
    next = recordWarpAngle + velocity;
    recordWarpAngle = next;
    if (velocity > 0.0f) {
        if (next > -1.0f) {
            recordWarpVelocity = 0.0f;
        }
    } else {
        if (next < -1.0f) {
            recordWarpVelocity = 0.0f;
        }
    }
}

void xglMatrixUnit(Matrix4 matrix);
void xglMatrixRotZ(Matrix4 destination, Matrix4 source, f32 angle);
void xglMatrixScale(Matrix4 destination, Matrix4 source, const Vector4 *scale);
const AlignedVector4 recordWarpCenter = { 256.0f, 224.0f, 0.0f, 0.0f };
const AlignedVector4 recordWarpScale = { 1.0f, 1.0f, 0.0f, 1.0f };
float recordWarpDegToRad = 0.0174532887f;

/* Rotates the point (x, y) about recordWarpCenter by recordWarpAngle * recordWarpDegToRad, scales it by
 * recordWarpScale and stores the resulting x and y in result. */
void recordWarpCorner(Vector4 *result, int x, int y)
{
    AlignedVector4 point = { x, y, 0.0f, 0.0f };
    AlignedVector4 center = recordWarpCenter;
    AlignedVector4 scale = recordWarpScale;
    AlignedVector4 offset;
    Matrix4 rotation;
    Matrix4 scaling;
    float angle;

    angle = recordWarpAngle * recordWarpDegToRad;
    xglMatrixUnit(rotation);
    xglMatrixUnit(scaling);
    xglMatrixRotZ(rotation, rotation, angle);
    xglMatrixScale(scaling, scaling, &scale);
    Vu0SubVector(&offset, &point, &center);
    xglVectorMulMat(&offset, rotation, &offset);
    xglVectorMulMat(&offset, scaling, &offset);
    Vu0AddVector(&offset, &offset, &center);
    result->x = offset.x;
    result->y = offset.y;
}

/* Points on the ring: 0..360 degrees in 18-degree steps. */
#define RING_STEPS 21
#define RING_STEP_DEGREES 18

void recordStepRing(void);
int recordRingInnerColor(void);
int recordDefaultColor(void);
void recordRingStep(float angle, Vector4 *screenPoints, Vector4 *texturePoints);
float recordRingDegToRad = 0.0174532887f;

/*
 * Draw handler of record type 7 (recordDrawHandlers[6]): draws a textured ring as one triangle strip: for every step an
 * inner vertex (recordRingInnerColor's colour) and an outer one (recordDefaultColor), Z and fog 0x7FFFFFFF.
 */
void recordDrawType7(GsQueueContext *unusedRecord, XglPacket *unused)
{
    Vector4 screenPoints;
    Vector4 texturePoints;
    GsPair *pairs;
    XglPacket *packet;
    unsigned int innerColor;
    unsigned int outerColor;
    int pairCount;
    int step;
    int degrees;

    recordStepRing();
    innerColor = recordRingInnerColor();
    outerColor = recordDefaultColor();
    packet = xglPacketGetCurrent();
    pairs = (GsPair *) SCRATCHPAD_BASE;
    pairCount = 0;
    pairs[pairCount].ad.reg = GIF_AD;
    pairs[pairCount].ad.data = ((u64)0x10000000 << 32) | 0x8000;
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEXFLUSH;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEST_1;
    pairs[pairCount].ad.data = GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 2);
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEX1_1;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEXA;
    pairs[pairCount].ad.data = 0;
    pairCount++;
    pairs[pairCount].ad.reg = GS_ALPHA_1;
    pairs[pairCount].ad.data = gsMakeAlpha(0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_TEX0_1;
    pairs[pairCount].ad.data = gsMakeTex0(1, 0);
    pairCount++;
    pairs[pairCount].ad.reg = GS_FRAME_1;
    pairs[pairCount].ad.data = gsMakeFrame(0, 0);
    pairCount++;
    pairs[pairCount].ad.data = GS_SET_PRIM(GS_PRIM_TRISTRIP, 1, 1, 0, 1, 0, 1, 0, 0);
    pairs[pairCount].ad.reg = GS_PRIM;
    pairCount++;
    degrees = 0;
    for (step = 0; step < RING_STEPS; step++) {
        recordRingStep((float)degrees * recordRingDegToRad, &screenPoints, &texturePoints);
        pairs[pairCount].ad.reg = GS_RGBAQ;
        pairs[pairCount].ad.data = innerColor;
        pairCount++;
        pairs[pairCount].ad.reg = GS_UV;
        pairs[pairCount].ad.data = GS_SET_UV(texturePoints.x * 16.0f, texturePoints.y * 16.0f);
        pairCount++;
        pairs[pairCount].ad.reg = GS_XYZF2;
        pairs[pairCount].ad.data = GS_SET_XYZ((int)((screenPoints.x + 1792.0f) * 16.0f),
                                              (int)((screenPoints.y + 1824.0f) * 16.0f), 0x7FFFFFFF);
        pairCount++;
        pairs[pairCount].ad.data = outerColor;
        pairs[pairCount].ad.reg = GS_RGBAQ;
        pairCount++;
        pairs[pairCount].ad.reg = GS_UV;
        pairs[pairCount].ad.data = GS_SET_UV(texturePoints.z * 16.0f, texturePoints.w * 16.0f);
        pairCount++;
        pairs[pairCount].ad.reg = GS_XYZF2;
        pairs[pairCount].ad.data = GS_SET_XYZ((int)((screenPoints.z + 1792.0f) * 16.0f),
                                              (int)((screenPoints.w + 1824.0f) * 16.0f), 0x7FFFFFFF);
        pairCount++;
        degrees += RING_STEP_DEGREES;
    }
    pairs[0].ad.data += pairCount - 1;
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectHLCode(packet, 0);
    sceVif1PkAddDirectDataN(packet, pairs, pairCount);
    sceVif1PkCloseDirectHLCode(packet);
}

void recordStepRing(void)
{
}

int recordRingInnerColor(void)
{
  return 0x40808080;
}

int recordDefaultColor(void)
{
  return 0x40808080;
}

/*
 * One step of the ring for angle: texturePoints gets the texture coordinates around (256, 224), radius 150 in x/y
 * and 300 in z/w; screenPoints the screen positions, radius 150 in x/y and 350 in z/w.
 */
void recordRingStep(float angle, Vector4 *screenPoints, Vector4 *texturePoints)
{
    float textureSin;
    float textureCos;
    float screenSin;
    float screenCos;
    float radius;

    /* Set before the first sinf: the shared 150.0f then stays in one callee-saved register. */
    radius = 150.0f;
    textureSin = sinf(angle);
    textureCos = cosf(angle);
    texturePoints->x = textureCos * radius + 256.0f;
    texturePoints->y = textureSin * radius + 224.0f;
    texturePoints->z = textureCos * 300.0f + 256.0f;
    texturePoints->w = textureSin * 300.0f + 224.0f;
    /* The original adds 0.0f to the angle before the second pair of calls (mtc1 $0 / add.s); without this line the
       function compiles 8 bytes shorter. */
    angle += 0.0f;
    screenSin = sinf(angle);
    screenCos = cosf(angle);
    screenPoints->x = screenCos * radius + 256.0f;
    screenPoints->y = screenSin * radius + 224.0f;
    screenPoints->z = screenCos * 350.0f + 256.0f;
    screenPoints->w = screenSin * 350.0f + 224.0f;
}

/* Recovered data of this TU, in address order (the original .data and .bss of main/tu207). */
u64 recordPreDrawGifData[18] = {
    0x1000000000008008, 14, 0, 63, 0x131000000, 78, 1, 70,
    0x6FC007FC00A, 8, 0, 74, 97, 20, 0, 73,
    0x1BF000001FF0000, 64
};

/* Draw handlers of types 8..21, defined in tu208-tu212. */
void recordDrawStrip(GsQueueContext *record, XglPacket *packet);
void recordDrawTurnColumns(GsQueueContext *record, XglPacket *packet);
void orbitDrawRing(GsQueueContext *record, XglPacket *packet);
void quadDrawPreset(GsQueueContext *record, XglPacket *packet);
void quadDrawPresetUv(GsQueueContext *record, XglPacket *packet);
void circleDrawSegments(GsQueueContext *record, XglPacket *packet);
void gridDrawCells(GsQueueContext *record, XglPacket *packet);
void quadDrawRotating(GsQueueContext *record, XglPacket *packet);
void pointGridDraw(GsQueueContext *record, XglPacket *packet);
void quadOrbitGridDraw(GsQueueContext *record, XglPacket *packet);
void ringWaveGridDraw(GsQueueContext *record, XglPacket *packet);
/* These two are defined with other parameter lists (int *record; no parameters) and still fill their slots. */
void recordSetAlphaFlags();
void recordDrawType21();
void (*recordDrawHandlers[22])(GsQueueContext *record, XglPacket *packet) = {
    recordDrawType1,
    recordDrawType2,
    recordDrawType3,
    recordDrawType4,
    recordDrawType5,
    recordDrawType6,
    recordDrawType7,
    recordDrawStrip,
    recordDrawTurnColumns,
    orbitDrawRing,
    quadDrawPreset,
    quadDrawPresetUv,
    circleDrawSegments,
    gridDrawCells,
    recordSetAlphaFlags,
    quadDrawRotating,
    pointGridDraw,
    quadOrbitGridDraw,
    ringWaveGridDraw,
    spriteBatchDraw,
    recordDrawType21,
    0
};
u64 recordPostDrawGifData[6] = {
    0x1000000000008002, 14, 0, 63, 0x131000000, 78
};
signed char recordGridCells[2500] = { 0 };
