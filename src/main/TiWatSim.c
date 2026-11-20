/*
 * TiWatSim: the water height simulation (file ../TiWatSim.euc.c in the assert strings).
 * A table holds a count at +0x08 and items from +0x10, 0x110 bytes each. An item keeps a
 * cell grid: cols at +0x08, rows at +0x0C, cell spacing at +0x50, drop amount at +0x58,
 * height buffer A at +0x68, buffer B at +0x6C (read for the normals), a mask at block+0x78,
 * normals at block+0x7C and the block base at +0x84.
 */
#include "common.h"
#include "shared.h"
#include "main/TiWatSim.h"
#include "main/pad.h"
#include "main/xgl_2.h"

/* Cell under the pad cursor: row and column in the item grid. */
static u32 tiWatSimCursorRow;
static u32 tiWatSimCursorCol;

/* Steers the cursor cell with the pad, then lowers its height in buffer A by drop_amount.
 * Pad bits (include/main/pad.h): held PAD_SELECT (0x100) enables the cursor, a fresh press of
 * PAD_SELECT (PadData[0].pressed) jumps to a random interior cell, and PAD_RIGHT / PAD_LEFT / PAD_DOWN /
 * PAD_UP (0x2000 / 0x8000 / 0x4000 / 0x1000) move the column and row by one inside the border. */
void tiWatSimPadDrop(TiWatSimItem *item)
{
    u32 cell_index;

    if (!(PadData[0].held & 0x100)) {
        return;
    }
    if (PadData[0].pressed & 0x100) {
        do {
            tiWatSimCursorRow = xglSRand() % (item->rows - 1);
        } while (tiWatSimCursorRow == 0);
        do {
            tiWatSimCursorCol = xglSRand() % (item->cols - 1);
        } while (tiWatSimCursorCol == 0);
    } else {
        if (PadData[0].held & 0x2000) {
            u32 *cursor_col = &tiWatSimCursorCol;

            if (*cursor_col < item->cols - 2) {
                (*cursor_col)++;
            }
        }
        if (PadData[0].held & 0x8000) {
            u32 *cursor_col = &tiWatSimCursorCol;

            if (*cursor_col >= 2) {
                (*cursor_col)--;
            }
        }
        if (PadData[0].held & 0x4000) {
            u32 *cursor_row = &tiWatSimCursorRow;

            if (*cursor_row < item->rows - 2) {
                (*cursor_row)++;
            }
        }
        if (PadData[0].held & 0x1000) {
            u32 *cursor_row = &tiWatSimCursorRow;

            if (*cursor_row >= 2) {
                (*cursor_row)--;
            }
        }
    }
    cell_index = tiWatSimCursorRow * item->cols + tiWatSimCursorCol;
    item->height_buffer_a[cell_index] = item->height_buffer_a[cell_index] - item->drop_amount;
}

/* Normal of each interior cell from the height differences of buffer B, scaled by the cell spacing. */
void tiWatSimComputeNormals(TiWatSimItem *item)
{
    DoublewordVector4 *normals; /* stores through the union carry alias set 0, like the original */
    u32 x;
    u32 y;
    u32 index;
    u32 row;
    float dx;
    float dy;
    float dz;
    float length_squared;
    float inverse;

    normals = (DoublewordVector4 *) (item->block + item->normals_offset);
    for (y = 1; y < item->rows - 1; y++) {
        row = y * item->cols;
        for (x = 1; x < item->cols - 1; x++) {
            index = row + x;
            dx = item->height_buffer_b[index - 1] - item->height_buffer_b[index + 1];
            normals[index].vector.x = dx;
            dz = item->height_buffer_b[row - item->cols + x] - item->height_buffer_b[row + item->cols + x];
            normals[index].vector.z = dz;
            dy = item->cell_spacing + item->cell_spacing;
            length_squared = dx * dx + dy * dy + dz * dz;
            inverse = 1.0f / length_squared;
            normals[index].vector.x = dx * inverse;
            normals[index].vector.y = dy * inverse;
            normals[index].vector.z = dz * inverse;
        }
    }
}

#include "quadword.h"

extern int tiWatSimRainCountdown;

/* Rain countdown: a new drop lands 10 to 29 updates after the previous one. */
#define TIWATSIM_RAIN_MIN_WAIT 10
#define TIWATSIM_RAIN_WAIT_SPREAD 20

/* Rain: when the countdown is zero, one interior cell of the item loses drop_amount * 4.0f and
 * the countdown is set again. */
void tiWatSimStepRain(TiWatSimItem *item)
{
    int col;
    int row;

    if (tiWatSimRainCountdown == 0) {
        col = xglSRand() % item->cols;
        row = xglSRand() % item->rows;
        if (col != 0 && row != 0 && col != item->cols - 1 && row != item->rows - 1) {
            item->height_buffer_a[row * item->cols + col] -= item->drop_amount * 4.0f;
        }
        tiWatSimRainCountdown = xglSRand() % TIWATSIM_RAIN_WAIT_SPREAD + TIWATSIM_RAIN_MIN_WAIT;
    }
    tiWatSimRainCountdown--;
}

#include "main/assert_fail.h"
#define NULL ((void *) 0)

extern const char tiWatSimWaterAssertText[];
extern const char tiWatSimPosAssertText[];
extern const char tiWatSimSourceFile[];

/* Pushes the height buffer down at the grid cell under pPos: the cell loses drop_amount, its four neighbours half of it. */
void tiWatSimDisturbAt(TiWatSimItem *item, Vector4 *pPos)
{
    Vector4 pos;
    float half;
    float *mask;
    int col;
    int row;
    int index;

    half = item->drop_amount * 0.5f;
    mask = (float *) (item->block + item->mask_offset);
    if (item == NULL) {
        assertFailed(tiWatSimWaterAssertText, tiWatSimSourceFile, 218);
    }
    if (pPos == NULL) {
        assertFailed(tiWatSimPosAssertText, tiWatSimSourceFile, 219);
    }
    xglVectorMulMat(&pos, item->matrix.rows, pPos);
    pos.x = pos.x - item->grid_origin_x;
    pos.z = pos.z - item->grid_origin_z;
    col = (int) (pos.x / item->cell_spacing);
    row = (int) (pos.z / -item->cell_spacing);
    if (col > 0 && col < item->cols - 1 && row > 0 && row < item->rows - 1) {
        index = row * item->cols + col;
        if (mask[index] != 0.0f) {
            item->height_buffer_a[index] = item->height_buffer_a[index] - item->drop_amount;
            if (col - 1 >= 0) {
                item->height_buffer_a[index - 1] = item->height_buffer_a[index - 1] - half;
            }
            if (row - 1 >= 0) {
                item->height_buffer_a[(row - 1) * item->cols + col] = item->height_buffer_a[(row - 1) * item->cols + col] - half;
            }
            if (col + 1 < item->cols) {
                item->height_buffer_a[index + 1] = item->height_buffer_a[index + 1] - half;
            }
            if (row + 1 < item->rows) {
                item->height_buffer_a[(row + 1) * item->cols + col] = item->height_buffer_a[(row + 1) * item->cols + col] - half;
            }
        }
    }
}

/* Disturbs every item of the table at the position pointed to by pPos. */
void tiWatSimDisturbAll(TiWaterSub *table, Vector4 *pPos)
{
    TiWatSimItem *item;
    u32 i;

    item = table->items;
    for (i = 0; i < table->active; i++) {
        tiWatSimDisturbAt(item, pPos);
        item++;
    }
}

/* Gives each masked cell of the item a random height in a band scaled by range and clears buffer B. */
void tiWatSimRandomizeItem(TiWatSimItem *item, int range)
{
    float *mask;
    u32 i;

    mask = (float *) (item->block + item->mask_offset);
    for (i = 0; i < item->cell_count; i++) {
        if (mask[i] != 0.0f) {
            item->height_buffer_a[i] = (float) (xglSRand() % range) * 0.01f - (float) range * 0.005f;
            item->height_buffer_b[i] = 0.0f;
        }
    }
}

/* Randomizes every item of the table with the same range. */
void tiWatSimRandomizeAll(TiWaterSub *table, int range)
{
    TiWatSimItem *item;
    u32 i;

    item = table->items;
    for (i = 0; i < table->active; i++) {
        tiWatSimRandomizeItem(item, range);
        item++;
    }
}

extern Vector4 tiWatSimSourcePos;

void tiWatSimStoreSourcePos(const Vector4 *position)
{
    QuadwordCopy(&tiWatSimSourcePos, position);
}

extern Vector4 tiWatSimSecondSourcePos;

void tiWatSimStoreSecondSourcePos(const Vector4 *position)
{
    QuadwordCopy(&tiWatSimSecondSourcePos, position);
}

/* Bump allocator: returns the cursor and advances it by size, rounded up to 16 bytes. */
u32 tiWatSimAllocAligned16(u32 *cursor, u32 size)
{
    u32 old;

    old = *cursor;
    *cursor = (old + size + 0xF) & ~0xF;
    return old;
}

/* Bump allocator without alignment: returns the cursor and advances it by size. */
u32 tiWatSimAlloc(u32 *cursor, u32 size)
{
    u32 old;

    old = *cursor;
    *cursor = old + size;
    return old;
}

const char tiWatSimWaterAssertText[8] = "pWater";

const char tiWatSimSourceFile[24] = "../TiWatSim.euc.c";

const char tiWatSimPosAssertText[8] = "pPos";

Vector4 tiWatSimSourcePos = { .x = 0.0f, .y = 0.0f, .z = 0.0f, .w = 1.0f };

Vector4 tiWatSimSourcePosTransformed = { .x = 5.0f, .y = 0.0f, .z = 5.0f, .w = 1.0f };

Vector4 tiWatSimSecondSourcePos = { .x = 0.0f, .y = 0.0f, .z = 0.0f, .w = 0.0f };

int tiWatSimRainCountdown = 0;
int tiWatSimUnusedWordA = 0;
int tiWatSimUnusedWordB = 0;
int tiWatSimUnusedWordC = 0;
