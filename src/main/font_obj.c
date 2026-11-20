#include "common.h"
#include "main/debug_test.h"
#include "main/xgl_cd_arena.h"
#include "main/xgl_tick.h"
#include "main/font_obj.h"
#include "main/gr_screen.h"
#include "main/cursor_buffer.h"
#include "gs.h"
#include "main/xgl_camera.h"
#include "main/pxy_packet_state.h"

#define FONT_SLOT_COUNT 6
#define FONT_TEXTURE_COUNT 11
#define FONT_DEFAULT_COLOR 0x3F80000080808080LL
#define FONT_DEFAULT_OPACITY 0x80
/* 12.4 vertex bias of the X and Y screen coordinates in fontObjDraw. */
#define FONT_VERTEX_BIAS_X 0x7000
#define FONT_VERTEX_BIAS_Y 0x7200

/* Sets the current pair of a track to (x, y), its scaled copy to (x << 4, y << 4) and the step counter to 1. */
void fontTrackSetPoint(struct FontTrack *track, int x, int y)
{
    track->current_pair.x = x;
    track->current_pair.y = y;
    track->scaled_pair.x = x << 4;
    track->scaled_pair.y = y << 4;
    track->step_count = 1;
}

void fontTrackSetBoth(struct FontTrack *track, int x, int y)
{
    track->target_pair.x = x;
    track->current_pair.x = x;
    track->target_pair.y = y;
    track->current_pair.y = y;
    track->scaled_pair.x = x << 4;
    track->scaled_pair.y = y << 4;
    track->step_count = 1;
}

void fontTrackAdvance(struct FontTrack *track)
{
    int target_x;
    int target_y;

    track->step_count--;
    target_x = track->target_pair.x;
    track->current_pair.x = target_x;
    track->scaled_pair.x = target_x << 4;
    target_y = track->target_pair.y;
    track->current_pair.y = target_y;
    track->scaled_pair.y = target_y << 4;
    if (track->step_count <= 0) {
        track->step_count = 1;
    }
}

#include "shared.h"
#include "main/xgl_packet.h"

/* 1 when the current pair of a track equals its target pair. */
int fontTrackAtTarget(struct FontTrack *track) {
    int at_target = 0;

    if (track->current_pair.x == track->target_pair.x) {
        at_target = track->current_pair.y == track->target_pair.y;
    }
    return at_target;
}

/* .sbss block of the font object, in address order. fontObjSlotTexture also covers the five bytes that the
 * scaffold labelled at 0x69E203 (slot 3 and 4 hold the reset values), so the object is eight bytes. */
static int fontObjBoundSlot[2];
static signed char fontObjSlotTexture[8];
static signed char fontObjSlotState[8];
static u64 fontObjColor;
static int fontObjDepth;
static int fontObjOpacity;
/* The run-list buffers of the six font slots (struct FontRunList once the texture is read), carved from the CD arena. */
static void *fontObjRunLists[6];

void fontObjInitGlobals(void)
{
    void **run_list;
    int slot;

    fontObjBoundSlot[0] = -1;
    run_list = fontObjRunLists;
    for (slot = 0; slot < FONT_SLOT_COUNT; slot++) {
        fontObjSlotTexture[slot] = -1;
        fontObjSlotState[slot] = 0;
        *run_list = 0;
        run_list++;
    }
    fontObjColor = FONT_DEFAULT_COLOR;
    fontObjOpacity = FONT_DEFAULT_OPACITY;
    fontObjDepth = 0;
}

extern signed char fontObjTextureSlot[];
extern char fontObjTexturePathTable[];

int fontObjLoadTexture(int texture_index)
{
    int slot;
    void **run_list_word;

    if (texture_index < 0) {
        return 0;
    }
    if (!(texture_index < FONT_TEXTURE_COUNT)) {
        return 0;
    }
    slot = fontObjTextureSlot[texture_index];
    if (slot < 0) {
        return 0;
    }
    if (fontObjSlotTexture[slot] == (signed char) texture_index) {
        return 1;
    }
    if (fontObjSlotState[slot] == 1) {
        return 0;
    }
    run_list_word = &fontObjRunLists[slot];
    if (*run_list_word == 0) {
        if (slot == 5) {
            fontObjRunLists[5] = xglCdSectorAlloc(0xB800);
        } else if (slot >= 3) {
            *run_list_word = xglCdBufferReserve(0x40800);
        } else {
            *run_list_word = xglCdSectorAlloc(0x40800);
        }
    }
    if (xglCdReadFileAsync(fontObjTexturePathTable + texture_index * 24, fontObjRunLists[slot], 1, (unsigned char) slot) >= 0) {
        fontObjSlotTexture[slot] = (signed char) texture_index;
        fontObjSlotState[slot] = 1;
        return 1;
    }
    return 0;
}

/* Marks a texture slot as loaded (state 2). */
void fontObjMarkSlotLoaded(unsigned char slot) {
    fontObjSlotState[slot] = 2;
}


/* Brackets one TEXFLUSH (GS register 0x3F) write with the GS begin and end helpers. */
void fontObjTexFlush(void) {
    nmlPacketGsInit();
    packet_gs_entry64(GS_TEXFLUSH, 0);
    xglDirectQueueSubmitRef();
}

struct FontRunList {
    int header_words[2];
    int entry_count;
    int entries_offset;
};

struct FontRunEntry {
    u32 packet_info;
    u32 word4;
    u32 word8;
    int count;
    int data_offset;
};

extern u64 fontObjVifPacketData[];
extern u64 fontObjBaseVifPacketData[];

int fontObjBuildVifPacket(int texture_index)
{
    XglPacket *packet;
    struct FontRunList *run_list;
    struct FontRunEntry *entry;
    int entry_index;
    signed char run_slot;
    void *data;
    int count;
    int low;
    int high;
    u64 tag_high;
    u64 tag_low;

    packet = xglPacketGetCurrent();
    if (fontObjLoadTexture(texture_index) == 0) {
        return 0;
    }
    if (fontObjSlotState[fontObjTextureSlot[texture_index]] != 2) {
        return 0;
    }
    run_slot = fontObjTextureSlot[texture_index];
    entry_index = 0;
    run_list = fontObjRunLists[run_slot];
    entry = (struct FontRunEntry *)((u8 *)run_list + run_list->entries_offset);
    if (run_list->entry_count > 0) {
        do {
            low = entry->packet_info & 0xFFFF;
            high = entry->packet_info >> 16;
            if (entry->packet_info == low) {
                high = (low + 0x3F) >> 6;
            }
            tag_high = ((u64)(((u32)0xF0000 + entry->word8) >> 6) << 32) | ((u64)high << 48);
            tag_low = (u64)low | ((u64)entry->word4 << 32);
            fontObjVifPacketData[10] = tag_low;
            fontObjVifPacketData[6] = tag_high;
            sceVif1PkCnt(packet, 0);
            sceVif1PkAddDataN(packet, fontObjVifPacketData, 0x1C);
            entry_index++;
            data = (u8 *)run_list + entry->data_offset;
            count = entry->count;
            entry++;
            sceVif1PkRef(packet, data, count, 0, 0, 0);
        } while (entry_index < run_list->entry_count);
    }
    fontObjBoundSlot[0] = texture_index;
    fontObjTexFlush();
    return 1;
}

int fontObjBuildBaseVifPacket(void)
{
    XglPacket *packet;
    struct FontRunList *run_list;
    struct FontRunEntry *entry;
    int entry_index;
    int run_slot;
    void *data;
    int count;
    int low;
    int high;
    u64 tag_high;
    u64 tag_low;

    packet = xglPacketGetCurrent();
    if (fontObjLoadTexture(0) == 0) {
        return 0;
    }
    if (fontObjSlotState[0] != 2) {
        return 0;
    }
    entry_index = 0;
    run_slot = fontObjTextureSlot[0];
    run_list = fontObjRunLists[run_slot];
    entry = (struct FontRunEntry *)((u8 *)run_list + run_list->entries_offset);
    if (run_list->entry_count > 0) {
        do {
            low = entry->packet_info & 0xFFFF;
            high = entry->packet_info >> 16;
            if (entry->packet_info == low) {
                high = (low + 0x3F) >> 6;
            }
            tag_high = ((u64)(((u32)0xE0000 + entry->word8) >> 6) << 32) | ((u64)high << 48);
            tag_low = (u64)low | ((u64)entry->word4 << 32);
            fontObjBaseVifPacketData[10] = tag_low;
            fontObjBaseVifPacketData[6] = tag_high;
            sceVif1PkCnt(packet, 0);
            sceVif1PkAddDataN(packet, fontObjBaseVifPacketData, 0x1C);
            entry_index++;
            data = (u8 *)run_list + entry->data_offset;
            count = entry->count;
            entry++;
            sceVif1PkRef(packet, data, count, 0, 0, 0);
        } while (entry_index < run_list->entry_count);
    }
    fontObjTexFlush();
    return 1;
}

void fontObjResetState(void) {
    fontObjDepth = 0;
    fontObjBoundSlot[0] = -1;
    fontObjTexFlush();
}

void fontObjResetAndBuild(void) {
    fontObjResetState();
    fontObjBuildBaseVifPacket();
}

void fontObjClearSlots(void)
{
    int slot;

    xglCdWaitIdle();
    for (slot = 0; slot < FONT_SLOT_COUNT; slot++) {
        fontObjSlotTexture[slot] = -1;
        fontObjSlotState[slot] = 0;
    }
}

void fontObjResetBufferSlots(void)
{
    fontObjRunLists[3] = fontObjRunLists[4] = 0;
    fontObjSlotTexture[3] = fontObjSlotTexture[4] = -1;
}

/* Zeroes the six run-list words of fontObjRunLists, from the last slot down. */
void **fontObjClearRunLists(void)
{
    void **run_lists;
    void **cursor;
    int slot;

    fontObjClearSlots();
    run_lists = fontObjRunLists;
    cursor = run_lists + 5;
    for (slot = 0; slot < FONT_SLOT_COUNT; slot++) {
        *cursor = 0;
        cursor--;
    }

    return cursor;
}

/* One 16-byte cell of fontObjCellTable: a glyph rectangle in the 16ths-of-a-pixel units
 * the GS queue takes. kind is nonzero for a glyph bound to a run list. */
struct FontCell {
    unsigned char kind;
    unsigned char unmodeled_01;
    unsigned short x;
    unsigned short y;
    unsigned short w;
    unsigned short h;
    unsigned char unmodeled_0a[2];
    unsigned int clut_block; /* colour table block address; shifted into the TEX0 CBP field */
};

extern struct FontCell fontObjCellTable[];
extern u16 sRenderDepthBase[];
int fontObjBuildVifPacket(int texture_index);

void fontObjDraw(struct FontDrawItem *item)
{
    struct FontCell *cell;
    unsigned int depth_stripe;
    int x0;
    int y0;
    int x1;
    int y1;
    int draw_w;
    int draw_h;
    int ix0;
    int ix1;
    int iy0;
    int iy1;
    int swap;
    unsigned long long alpha;

    if (item == 0) {
        return;
    }
    cell = &fontObjCellTable[item->cell_index];
    if (cell->kind == 0) {
        if (fontObjSlotState[0] != 2) {
            return;
        }
    } else if (fontObjBoundSlot[0] != cell->kind) {
        if (fontObjBuildVifPacket(cell->kind) == 0) {
            return;
        }
    }

    if (item->flags & FONT_DRAW_OFFSET) {
        x0 = ((item->offset_x + cell->x) << 4) + 8;
        y0 = ((item->offset_y + cell->y) << 4) + 8;
    } else {
        x0 = (cell->x << 4) + 8;
        y0 = (cell->y << 4) + 8;
    }
    if (item->flags & 0x2000) {
        y0 += 8;
    }
    x1 = x0 + (cell->w << 4);
    y1 = y0 + (cell->h << 4);
    if (item->flags & FONT_DRAW_USE_SIZE) {
        draw_w = item->size_w;
        draw_h = item->size_h;
    } else {
        draw_h = cell->h;
        draw_w = cell->w;
    }
    if (item->flags & FONT_DRAW_TRIM_BOTH) {
        x1 -= 16;
        y1 -= 16;
    } else {
        if (item->flags & FONT_DRAW_TRIM_X) {
            x1 -= 16;
        }
        if (item->flags & FONT_DRAW_TRIM_Y) {
            y1 -= 16;
        }
    }
    ix0 = item->x;
    ix1 = ix0 + draw_w;
    iy0 = item->y;
    iy1 = iy0 + draw_h;
    if (item->flags & FONT_DRAW_FLIP_X) {
        swap = ix0;
        ix0 = ix1;
        ix1 = swap;
    }
    if (item->flags & FONT_DRAW_FLIP_Y) {
        swap = iy0;
        iy0 = iy1;
        iy1 = swap;
    }

    nmlPacketGsInit();
    if (item->flags & 0x20000) {
        depth_stripe = 0x8080;
        packet_gs_entry64(GS_TEST_1, 0x50000);
        packet_gs_entry64(GS_ZBUF_1, sRenderDepthBase[0] | ((unsigned long long)depth_stripe << 17));
    } else {
        packet_gs_entry64(GS_TEST_1, 0x30000);
        packet_gs_entry64(GS_ZBUF_1, sRenderDepthBase[0] | 0x1000000);
    }

    if (cell->kind == 0) {
        if (item->flags & 0x40) {
            packet_gs_entry64(GS_TEX0_1, ((unsigned long long)cell->clut_block << 37) | 0x2000000665423800ULL);
        } else {
            packet_gs_entry64(GS_TEX0_1, ((unsigned long long)cell->clut_block << 37) | 0x2000000665323800ULL);
        }
    } else if (item->flags & 0x40) {
        packet_gs_entry64(GS_TEX0_1, ((unsigned long long)cell->clut_block << 37) | 0x2000000665423C00ULL);
    } else if (item->flags & 0x10000) {
        packet_gs_entry64(GS_TEX0_1, ((unsigned long long)cell->clut_block << 37) | 0x2000000621343C00ULL);
    } else {
        packet_gs_entry64(GS_TEX0_1, ((unsigned long long)cell->clut_block << 37) | 0x2000000665323C00ULL);
    }

    if (item->flags & 0x8000) {
        if (item->flags & FONT_DRAW_OFFSET) {
            unsigned long long low;
            unsigned long long high;

            low = ((unsigned long long)(cell->w - 1) << 4)
                | ((unsigned long long)(cell->x + item->offset_x) << 34);
            high = ((unsigned long long)(cell->x + item->offset_x) << 14) | 0xF;
            packet_gs_entry64(8, (low | high) | ((unsigned long long)(cell->h - 1) << 24));
        } else {
            unsigned long long low;
            unsigned long long high;

            low = ((unsigned long long)(cell->w - 1) << 4)
                | ((unsigned long long)cell->y << 34);
            high = ((unsigned long long)cell->x << 14) | 0xF;
            packet_gs_entry64(8, (low | high) | ((unsigned long long)(cell->h - 1) << 24));
        }
    } else if (item->flags & FONT_DRAW_OFFSET) {
        unsigned long long low;
        unsigned long long high;

        low = ((unsigned long long)(cell->x + item->offset_x) << 4)
            | ((unsigned long long)(cell->y + cell->h + item->offset_y) << 34);
        high = ((unsigned long long)(cell->x + cell->w + item->offset_x) << 14) | 0xA;
        packet_gs_entry64(8, (low | high) | ((unsigned long long)(cell->y + item->offset_y) << 24));
    } else {
        unsigned long long low;
        unsigned long long high;

        low = ((unsigned long long)cell->x << 4)
            | ((unsigned long long)(cell->y + cell->h) << 34);
        high = ((unsigned long long)(cell->x + cell->w) << 14) | 0xA;
        packet_gs_entry64(8, (low | high) | ((unsigned long long)cell->y << 24));
    }

    if (item->flags & 0x10) {
        packet_gs_entry64(GS_TEX1_1, 0);
    } else {
        packet_gs_entry64(GS_TEX1_1, 0x60);
    }
    if (item->flags & 1) {
        packet_gs_entry64(GS_ALPHA_1, ((unsigned long long)0x80 << 32) | 0x48);
    } else if (item->flags & 2) {
        packet_gs_entry64(GS_ALPHA_1, ((unsigned long long)0x80 << 32) | 0x42);
    } else {
        packet_gs_entry64(GS_ALPHA_1, ((unsigned long long)0x80 << 32) | 0x44);
    }

    if (item->flags & FONT_DRAW_COLOR_ALPHA) {
        alpha = (((unsigned long long)((unsigned char *)&fontObjColor)[3]
                  * (unsigned long long)(unsigned char)fontObjOpacity) >> 7) & 0xFF;
        packet_gs_entry64(GS_RGBAQ, (fontObjColor & 0xFFFFFF) | (alpha << 24));
    } else {
        packet_gs_entry64(GS_RGBAQ, ((unsigned long long)(unsigned char)fontObjOpacity << 24) | 0x3F80000000808080ULL);
    }

    if (item->flags & 0x80) {
        packet_gs_entry64(GS_PRIM, 0x151);
        packet_gs_entry64(3, (unsigned long long)x0 | ((unsigned long long)y0 << 16));
        packet_gs_entry64(5, ((unsigned long long)((item->x << 4) + FONT_VERTEX_BIAS_X))
            | ((unsigned long long)(((item->y + draw_h) << 4) + FONT_VERTEX_BIAS_Y) << 16)
            | ((unsigned long long)fontObjDepth << 32));
        packet_gs_entry64(3, (unsigned long long)x1 | ((unsigned long long)y1 << 16));
        packet_gs_entry64(5, ((unsigned long long)((item->x << 4) + FONT_VERTEX_BIAS_X))
            | ((unsigned long long)((item->y << 4) + FONT_VERTEX_BIAS_Y) << 16)
            | ((unsigned long long)fontObjDepth << 32));
    } else {
        packet_gs_entry64(GS_PRIM, 0x156);
        packet_gs_entry64(3, (unsigned long long)x0 | ((unsigned long long)y0 << 16));
        packet_gs_entry64(5, ((unsigned long long)((ix0 << 4) + FONT_VERTEX_BIAS_X))
            | ((unsigned long long)((iy0 << 4) + FONT_VERTEX_BIAS_Y) << 16)
            | ((unsigned long long)fontObjDepth << 32));
        packet_gs_entry64(3, (unsigned long long)x1 | ((unsigned long long)y1 << 16));
        packet_gs_entry64(5, ((unsigned long long)((ix1 << 4) + FONT_VERTEX_BIAS_X))
            | ((unsigned long long)((iy1 << 4) + FONT_VERTEX_BIAS_Y) << 16)
            | ((unsigned long long)fontObjDepth << 32));
    }
    xglDirectQueueSubmitRef();
}

/* Cell-table fields of 16-byte entry index: the width (+6) and the height (+8). */
u16 fontObjCellOffsetX(int index) {
    return fontObjCellTable[index].w;
}

u16 fontObjCellOffsetY(int index) {
    return fontObjCellTable[index].h;
}

void fontObjSetColor(u64 colour) {
    fontObjColor = colour;
}

void fontObjSetColorRgba(int r, int g, int b, int a)
{
    fontObjColor = (long long) r | ((long long) g << 8) | ((long long) b << 16) | ((long long) a << 24) | 0x3F80000000000000LL;
}

void fontObjSetAlpha(u64 alpha) {
    fontObjColor = (fontObjColor & 0xFFFFFF) | ((alpha & 0xFF) << 0x18);
}

u64 fontObjGetColor(void) {
    return fontObjColor;
}

void fontObjResetColor(void) {
    fontObjColor = FONT_DEFAULT_COLOR;
}

/* Sets the depth word that fontObjDraw writes into the high half (Z) of each XYZ2 vertex. */
void fontObjSetDepth(int depth) {
    fontObjDepth = depth;
}

void fontObjSetOpacity(int opacity) {
    fontObjOpacity = opacity;
}

u8 fontObjGetOpacity(void) {
    return fontObjOpacity;
}

void fontObjResetOpacity(void) {
    fontObjOpacity = FONT_DEFAULT_OPACITY;
}

/* Resets all four tracks to (0,0), clears extra state and sets flags to 0x12. */
void fontObjInitObject(struct FontObject *object)
{
    fontTrackSetBoth(&object->tracks[0], 0, 0);
    fontTrackSetBoth(&object->tracks[1], 0, 0);
    fontTrackSetBoth(&object->tracks[2], 0, 0);
    fontTrackSetBoth(&object->tracks[3], 0, 0);
    object->draw_bits = 0;
    object->aux_state = 0;
    object->move_phase = FONT_PHASE_IDLE;
    object->flags = 0x12;
}

/* Starts the opening (phase 1) unless it is already opening. Track 2 heads to the target of track 0
 * and track 3 to the target of track 1. With flags bit 0x2 the tracks start from their current pairs
 * when a closing is in progress, and track 3 starts from the origin otherwise. The active flag 0x10000
 * is set. */
void fontObjStartOpen(struct FontObject *object)
{
    if (object->move_phase == FONT_PHASE_OPENING) {
        return;
    }
    if (object->flags & FONT_OBJ_EASE) {
        if (object->move_phase == FONT_PHASE_CLOSING) {
            fontTrackSetTarget(&object->tracks[2], object->tracks[0].target_pair.x,
                          object->tracks[0].target_pair.y);
            {
                struct FontTrack *track3 = &object->tracks[3];

                fontTrackSetTarget(track3, object->tracks[1].target_pair.x,
                              object->tracks[1].target_pair.y);
            }
        } else {
            fontTrackSetBoth(&object->tracks[2], object->tracks[0].target_pair.x,
                          object->tracks[0].target_pair.y);
            {
                struct FontTrack *track3 = &object->tracks[3];

                fontTrackSetPoint(track3, 0, 0);
                fontTrackSetTarget(track3, object->tracks[1].target_pair.x,
                              object->tracks[1].target_pair.y);
            }
        }
    } else {
        fontTrackSetBoth(&object->tracks[2], object->tracks[0].target_pair.x,
                      object->tracks[0].target_pair.y);
        fontTrackSetBoth(&object->tracks[3], object->tracks[1].target_pair.x,
                      object->tracks[1].target_pair.y);
    }
    object->flags |= FONT_OBJ_ACTIVE;
    object->move_phase = FONT_PHASE_OPENING;
}

/* Starts the closing of the object (phase 2) unless it is already closing: track 2 heads to the
 * target of track 0 and track 3, the size, heads to zero. fontObjSettleClose ends the closing. */
void fontObjStartClose(struct FontObject *object)
{
    unsigned char phase;
    struct FontTrack *size_track;

    phase = object->move_phase;
    if (phase != FONT_PHASE_CLOSING) {
        if (object->flags & FONT_OBJ_EASE) {
            if (phase == FONT_PHASE_OPENING) {
                fontTrackSetTarget(&object->tracks[2], object->tracks[0].target_pair.x,
                              object->tracks[0].target_pair.y);
                size_track = &object->tracks[3];
            } else {
                fontTrackSetPoint(&object->tracks[2], object->tracks[0].current_pair.x,
                              object->tracks[0].current_pair.y);
                fontTrackSetTarget(&object->tracks[2], object->tracks[0].target_pair.x,
                              object->tracks[0].target_pair.y);
                fontTrackSetPoint(&object->tracks[3], object->tracks[1].current_pair.x,
                              object->tracks[1].current_pair.y);
                size_track = &object->tracks[3];
            }
            fontTrackSetTarget(size_track, 0, 0);
        } else {
            fontTrackSetBoth(&object->tracks[2], object->tracks[0].target_pair.x,
                          object->tracks[0].target_pair.y);
            fontTrackSetBoth(&object->tracks[3], 0, 0);
        }
        object->move_phase = FONT_PHASE_CLOSING;
    }
}

void fontObjAdvanceTracks(struct FontObject *object)
{
    fontTrackAdvance(&object->tracks[0]);
    fontTrackAdvance(&object->tracks[1]);
    object->move_phase = FONT_PHASE_IDLE;
    object->flags &= ~FONT_OBJ_ACTIVE;
}

/* Opening step (phase 1), called by fontObjUpdate: advances tracks 2 and 3; once both are at their
 * targets, their current pairs become the new targets of tracks 0 and 1 and the phase returns to idle. */
void fontObjSettleTracks(struct FontObject *object)
{
    if (object->flags & FONT_OBJ_EASE) {
        fontTrackEase(&object->tracks[2]);
        fontTrackEase(&object->tracks[3]);
    } else {
        fontTrackAdvance(&object->tracks[2]);
        fontTrackAdvance(&object->tracks[3]);
    }
    if (fontTrackAtTarget(&object->tracks[2]) != 0 && fontTrackAtTarget(&object->tracks[3]) != 0) {
        fontTrackSetTarget(&object->tracks[0], object->tracks[2].current_pair.x, object->tracks[2].current_pair.y);
        fontTrackSetTarget(&object->tracks[1], object->tracks[3].current_pair.x, object->tracks[3].current_pair.y);
        object->move_phase = FONT_PHASE_IDLE;
    }
}

/* Closing step (phase 2), the counterpart of fontObjSettleTracks: advances tracks 2 and 3 (or
 * steps them with fontTrackEase when flags bit 0x2 is set); once both are at their targets the
 * phase returns to idle and the active flag 0x10000 is cleared. */
void fontObjSettleClose(struct FontObject *object)
{
    if (object->flags & FONT_OBJ_EASE) {
        fontTrackEase(&object->tracks[2]);
        fontTrackEase(&object->tracks[3]);
    } else {
        fontTrackAdvance(&object->tracks[2]);
        fontTrackAdvance(&object->tracks[3]);
    }
    if (fontTrackAtTarget(&object->tracks[2]) != 0 && fontTrackAtTarget(&object->tracks[3]) != 0) {
        object->move_phase = FONT_PHASE_IDLE;
        object->flags &= ~FONT_OBJ_ACTIVE;
    }
}


/* Queues one textured box quad: a packet header, the alpha-selected colour word, then the four
 * corner words with the draw bits in the high half, and defers the queue. */
void fontObjDrawQuad(int x0, int y0, int x1, int y1, int rgba_bits, int z_bits)
{
    unsigned int rgba = (unsigned int)rgba_bits;
    unsigned int alpha = rgba >> 24;
    u64 zWord = (u64)z_bits << 32;
    nmlPacketGsInit();
    packet_gs_entry64(GS_TEST_1, 0x50000);
    if (alpha < 0x80) {
        packet_gs_entry64(GS_ZBUF_1, sRenderDepthBase[0] | (0x8080ULL << 17));
        {
            u64 colour = (u8)rgba | (0xFE00ULL << 46);
            u64 colourHi = (u64)(u8)(rgba >> 16) << 16;

            colourHi |= 0x8000ULL << 16;
            colour |= colourHi;
            colour |= (u64)(u8)(rgba >> 8) << 8;
            packet_gs_entry64(GS_RGBAQ, colour);
        }
        packet_gs_entry64(GS_ALPHA_1, ((u64)alpha << 32) | 0x64);
        packet_gs_entry64(GS_PRIM, 0x46);
    } else {
        packet_gs_entry64(GS_ZBUF_1, sRenderDepthBase[0] | 0x1000000);
        {
            u64 colour = (u8)rgba;

            colour |= 0xFE00ULL << 46;
            colour |= ((u64)(u8)(rgba >> 16) << 16) | ((u64)(u8)(rgba >> 8) << 8);
            colour |= (u64)(u8)alpha << 24;
            packet_gs_entry64(GS_RGBAQ, colour);
        }
        packet_gs_entry64(GS_PRIM, 6);
    }
    packet_gs_entry64(GS_XYZF2, (u64)((x0 << 4) + FONT_VERTEX_BIAS_X) | ((u64)((y0 << 4) + 0x71C0) << 16) | zWord);
    packet_gs_entry64(GS_XYZF2, (u64)((x1 << 4) + FONT_VERTEX_BIAS_X) | ((u64)((y1 << 4) + 0x71C0) << 16) | zWord);
    xglDirectQueueDefer(0, 4);
}

/* Writes the four corners of the box quad to corners[0..3]: the first and second tracks of the
 * object are the pair 2 and 3 when the active bit 0x20000 is set, else the pair 0 and 1. With
 * flags bit 0x10 the origin is the first track's current pair moved by half the distance from the
 * second track to tracks[1]'s target; the far corner is the origin plus the second track's pair. */
void fontObjGetQuadCorners(struct FontObject *object, int *corners)
{
    int flags = object->flags;
    struct FontTrack *track0 = &object->tracks[0];
    struct FontTrack *first = (flags & FONT_OBJ_PHASE_RUNNING) ? &object->tracks[2] : track0;
    struct FontTrack *second = (flags & FONT_OBJ_PHASE_RUNNING) ? &object->tracks[3] : &object->tracks[1];

    if (flags & 0x10) {
        corners[0] = first->current_pair.x +
            ((object->tracks[1].target_pair.x - second->current_pair.x) >> 1);
        corners[1] = first->current_pair.y +
            ((object->tracks[1].target_pair.y - second->current_pair.y) >> 1);
    } else {
        corners[0] = first->current_pair.x;
        corners[1] = first->current_pair.y;
    }
    corners[2] = corners[0] + second->current_pair.x;
    corners[3] = corners[1] + second->current_pair.y;
}

/* 0x40000000 is the bit pattern of 2.0f; the callee takes it in a general register. */
void fontObjDrawQuadFromTracks(struct FontObject *object)
{
    int corners[4];

    fontObjGetQuadCorners(object, corners);
    fontObjDrawQuad(corners[0], corners[1], corners[2], corners[3], 0x40000000, object->draw_bits);
}

/* Advances the object's tracks 0 and 1 (or steps them with fontTrackEase when flags bit 0x2 is set),
 * then runs the opening step (phase 1) or the closing step (phase 2). The active bit 0x20000 is set
 * while a phase runs and cleared otherwise. */
void fontObjUpdate(struct FontObject *object)
{
    if (object->flags & FONT_OBJ_EASE) {
        fontTrackEase(&object->tracks[0]);
        fontTrackEase(&object->tracks[1]);
    } else {
        fontTrackAdvance(&object->tracks[0]);
        fontTrackAdvance(&object->tracks[1]);
    }
    switch (object->move_phase) {
    case FONT_PHASE_OPENING:
        fontObjSettleTracks(object);
        object->flags |= FONT_OBJ_PHASE_RUNNING;
        break;
    case FONT_PHASE_CLOSING:
        fontObjSettleClose(object);
        object->flags |= FONT_OBJ_PHASE_RUNNING;
        break;
    case FONT_PHASE_IDLE:
    default:
        object->flags &= ~FONT_OBJ_PHASE_RUNNING;
        break;
    }
}

/* Draws the object's box quad only while its active flag (0x10000) is set. */
void fontObjDrawIfActive(struct FontObject *object) {
    if (object->flags & FONT_OBJ_ACTIVE) {
        fontObjDrawQuadFromTracks(object);
    }
}

/* Ticks the sub-object of a text block (block+0x58) through fontObjUpdate. */
void fontObjSubUpdate(struct FontObject *obj) {
    fontObjUpdate(obj);
}

/* Sets track 0 of the object (current and target pair): the position of the box corner. */
void fontObjSetPosition(struct FontObject *obj, int x, int y) {
    fontTrackSetBoth(&obj->tracks[0], x, y);
}

/* Sets track 1 of the object (current and target pair): the size that GetQuadCorners adds to the position. */
void fontObjSetSize(struct FontObject *obj, int x, int y) {
    fontTrackSetBoth(&obj->tracks[1], x, y);
}

/* Sets the target pair of track 0 (the position, see fontObjSetPosition) of the object. */
void fontObjSetPositionTarget(struct FontObject *obj, int x, int y) {
    fontTrackSetTarget(&obj->tracks[0], x, y);
}

void fontObjSetDrawBits(struct FontObject *obj, int draw_bits) {
    obj->draw_bits = draw_bits;
}

/* Sets the target pair of track 1 (the size, at +0x1C, see fontObjSetSize) of the object. */
void fontObjSetSizeTarget(struct FontObject *obj, int x, int y) {
    fontTrackSetTarget(&obj->tracks[1], x, y);
}

/* Returns the third track when bit 0x20000 of flags is set, else the object itself. */
struct FontTrack *fontObjGetOriginTrack(struct FontObject *object) {
    if (object->flags & FONT_OBJ_PHASE_RUNNING) {
        return &object->tracks[2];
    }
    return &object->tracks[0];
}

/* Selects the track of the object that a text block draws from: track 3 while the 0x20000 flag
 * is set, track 1 otherwise. */
struct FontTrack *fontObjGetSizeTrack(struct FontObject *object)
{
    if (object->flags & FONT_OBJ_PHASE_RUNNING) {
        return &object->tracks[3];
    }
    return &object->tracks[1];
}

/* Resets a cursor-buffer record to its idle values; step starts at 1 and mode at 0. */
void fontObjResetCursor(struct FontBlockCursor *cursor)
{
    if (cursor != 0) {
        do {
            cursor->text = 0;
            cursor->step = 1;
            cursor->previous_cursor = 0;
            cursor->cursor = 0;
            cursor->capacity = 0;
            cursor->mode = 0;
            cursor->count = 0;
            cursor->state = 0;
            cursor->aux_value = 0;
        } while (0);
        cursor->unit_size = 0;
    }
}

/* Stores the print origin (origin_x, origin_y) of a text block and the text word at +0x10 that the layout reads. */
void fontObjSetTextPosition(FontTextBlock *block, int origin_x, int origin_y, int text) {
    if (block != 0) {
        block->origin.current_pair.x = origin_x;
        block->origin.current_pair.y = origin_y;
        block->origin.scaled_pair.x = text;
    }
}

/* Three words: an offset point; the first two are displaced and restored around the call. */
struct FontPoint {
    int x;
    int y;
    int z;
};

/* A byte the layout replaced with a terminator (8 bytes: pointer, then the saved byte). */
struct FontLayoutRecord {
    u8 *pos;
    u8 saved;
};

extern struct FontLayoutRecord fontLayoutRestoreTable[];

/* The head of a text block as the layout reads it. */
struct FontLayout {
    int origin_x;
    int origin_y;
    unsigned int color;
    u8 *prefix;
    u8 *text;
    unsigned int flags;
    unsigned int limit;
    int pending;
    int newlines;
    unsigned char reset;
    unsigned char width;
};

/* Kept local (this file does not include main/xgl_font.h): with the header's int parameter and const char
 * text, fontObjLayoutString's call loads *pos once (a move into a0 in the entry delay slot, DIFF); the
 * original loads it again for the call, as with a long parameter. The definition cannot take long: in xgl_font.c a
 * long code adds a dsll/dsra pair (.text DIFF), and unsigned char/short/int parameters leave this caller's move. */
void xglFontPrint(int x, int y, unsigned int color, u8 *text);
int xglFontGetSPcodeSize(long code, u8 *text);

/* Lays out one line of the block's text: prints it through xglFontPrint with each control code
 * temporarily cut out, then restores the bytes. */
void fontObjLayoutString(struct FontPoint *point)
{
    struct FontLayout *layout = (struct FontLayout *) point;
    u8 *pos;
    unsigned int glyphCount;
    int recordIndex;
    int waitCode;
    int ch;
    int escapeCode;
    int nextChar;
    unsigned int wrapLimit;
    struct FontLayoutRecord *newlineRecord;

    if (layout == 0) {
        return;
    }
    if (layout->prefix != 0) {
        xglFontPrint(layout->origin_x, layout->origin_y, layout->color, layout->prefix);
    }
    if (layout->text == 0) {
        return;
    }
    pos = layout->text;
    glyphCount = 0;
    recordIndex = 0;
    ch = *pos;
    waitCode = 0;
    if (ch == 0) {
        layout->flags |= 0x10000;
    } else {
        /* Mechanism: entering the loop past its waitCode reset (and the shared exit tail) leaves it unoptimized by
           gcc 2.96's loop pass; structured loops hoist constants into s4-s7 and change the frame. Dropping the
           top reset (reload sets waitCode too) or the initial one also changes the size. The "\\0" case jumps
           to reload, past the pos++ at the loop bottom; the exits are structured breaks. */
        goto scan;
        do {
            waitCode = 0;
        scan:
            if ((unsigned char) ch < 32 && (unsigned char) ch != 10) {
                pos += xglFontGetSPcodeSize(*pos, pos);
            } else if ((unsigned char) ch == 36) {
                do {
                    pos++;
                } while (*pos != 59 && *pos != 0);
                if (*pos == 0) {
                    break;
                }
            } else {
                if ((unsigned char) ch == 92) {
                    fontLayoutRestoreTable[recordIndex].pos = pos;
                    fontLayoutRestoreTable[recordIndex].saved = *pos;
                    *pos = 7;
                    pos++;
                    recordIndex++;
                    escapeCode = *pos;
                    switch (escapeCode) {
                    default:
                        ch = escapeCode;
                        break;
                    case 0:
                        pos++;
                        pos--;
                        goto reload;
                    case 'n':
                        newlineRecord = &fontLayoutRestoreTable[recordIndex++];
                        newlineRecord->pos = pos;
                        newlineRecord->saved = *pos;
                        ch = *pos = 10;
                        break;
                    case 'k':
                    case 's':
                        waitCode = 1;
                        fontLayoutRestoreTable[recordIndex].pos = pos;
                        fontLayoutRestoreTable[recordIndex].saved = *pos;
                        recordIndex++;
                        ch = *pos = 7;
                        break;
                    }
                }
                if ((signed char) ch < 0) {
                    pos++;
                }
                glyphCount++;
                wrapLimit = layout->limit + layout->width;
                if (glyphCount >= wrapLimit) {
                    if (layout->pending != 0) {
                        pos++;
                        break;
                    }
                    if (layout->flags & 0x20000) {
                        pos++;
                        break;
                    }
                    if (waitCode == 1) {
                        pos++;
                        layout->flags |= 0x20000;
                        break;
                    }
                    layout->limit = wrapLimit;
                    layout->pending = layout->reset;
                    if (*pos == 10) {
                        layout->newlines++;
                    }
                    pos++;
                    break;
                }
            }
            pos++;
        reload:
            waitCode = 0;
            nextChar = *pos;
            ch = nextChar;
            if (nextChar == 0) {
                layout->flags |= 0x10000;
                break;
            }
        } while (1);
    }
    fontLayoutRestoreTable[recordIndex].pos = pos;
    fontLayoutRestoreTable[recordIndex].saved = *pos;
    *pos = 0;
    xglFontPrint(layout->origin_x, layout->origin_y, layout->color, layout->text);
    while (recordIndex >= 0) {
        *fontLayoutRestoreTable[recordIndex].pos = fontLayoutRestoreTable[recordIndex].saved;
        recordIndex--;
    }
    if (layout->pending != 0) {
        layout->pending--;
    }
}

/* Shifts point by (dx, dy), sets z, lays the text out, then restores the point. */
void fontObjLayoutAtOffset(int dx, int dy, int z, struct FontPoint *point)
{
    int saved_z;
    int x;
    int y;

    if (point != 0) {
        x = point->x + dx;
        saved_z = point->z;
        y = point->y + dy;
        point->x = x;
        point->y = y;
        point->z = z;
        fontObjLayoutString(point);
        x = point->x - dx;
        point->z = saved_z;
        y = point->y - dy;
        point->y = y;
        point->x = x;
    }
}


/* Initialises a text block: origin at (0, 0), a cleared cursor record in mode 1, and the sub-object
 * placed at (16, 352) with the size (480, 80). */
void fontObjInitBlock(FontTextBlock *block)
{
    struct FontObject *subObject = &block->sub_object;

    if (block) {
        fontTrackSetBoth(&block->origin, 0, 0);
        fontObjResetCursor(&block->cursor);
        block->cursor.unit_size = fontInitTagText;
        block->cursor.mode = 1;
        block->corners[0] = 0;
        block->corners[1] = 0;
        block->corners[2] = 0;
        block->corners[3] = 0;
        block->flags = 0;
        fontObjInitObject(subObject);
        fontObjSetPosition(subObject, 16, 352);
        fontObjSetSize(subObject, 480, 80);
    }
}

/* Sets the text of a text block: the cursor record restarts with the block's mode as its state. */
void fontObjSetText(FontTextBlock *block, unsigned char *text) {
    struct FontBlockCursor *cursor = &block->cursor;

    if (block == 0) {
        return;
    }
    block->cursor.text = text;
    cursor->status_flags &= ~0x10000;
    cursor->aux_value = 0;
    cursor->state = cursor->mode;
    cursor->count = 0;
}

void fontObjClearText(FontTextBlock *block) {
    fontObjSetText(block, 0);
}

void fontObjTextBlockNop(void) {
}

/* Draws one text block: advances the sub-object and its track, draws its box when active, then prints
 * its line through the ext handle (sub-object draw_bits) with the scissor packets around it. */
int fontObjDrawText(FontTextBlock *block)
{
    int offsetX;
    int offsetY;

    if (block != 0) {
        fontObjSubUpdate(&block->sub_object);
        fontTrackEase(&block->origin);
        fontObjDrawIfActive(&block->sub_object);
        if (block->sub_object.move_phase != FONT_PHASE_OPENING) {
            offsetX = block->sub_object.tracks[0].current_pair.x + block->origin.current_pair.x;
            offsetY = block->sub_object.tracks[0].current_pair.y + block->origin.current_pair.y;
            fontObjGetQuadCorners(&block->sub_object, block->corners);
            block->corners[1] -= 4;
            block->corners[3] -= 4;
            xglFontPrintExtFunc(block->sub_object.draw_bits, ScissorSet, block->corners);
            fontObjLayoutAtOffset(offsetX, offsetY, block->sub_object.draw_bits + 1, (struct FontPoint *)&block->cursor);
            xglFontPrintExtFunc(block->sub_object.draw_bits, ScissorReset, 0);
        }
    }
}

/* Opens the sub-object of a text block through fontObjStartOpen (the phase 1 start). */
void fontObjSubStartOpen(FontTextBlock *block) {
    if (block != 0) {
        fontObjStartOpen(&block->sub_object);
    }
}

/* Starts the closing of the sub-object of a text block through fontObjStartClose. */
void fontObjSubStartClose(FontTextBlock *block) {
    if (block != 0) {
        fontObjStartClose(&block->sub_object);
    }
}

/* Advances the tracks of the sub-object of a text block; the advance ends any move and clears the active flag. */
void fontObjSubAdvanceTracks(FontTextBlock *block) {
    if (block != 0) {
        fontObjAdvanceTracks(&block->sub_object);
    }
}

/* Initialises a cursor buffer entry: origin at (0, 0), a cleared record in mode 1, three header bytes,
 * the corner words and the flags zeroed, and the sub-object placed at (16, 352) with the size (480, 80). */
void cursorBufferInitEntry(struct FontCursorEntry *entry)
{
    struct FontObject *subObject = &entry->sub_object;

    if (entry != 0) {
        fontTrackSetBoth(&entry->origin, 0, 0);
        fontObjResetCursor(&entry->record);
        entry->record.unit_size = fontInitTagText;
        entry->record.mode = 1;
        entry->counter.bytes[0] = 0xff;
        entry->counter.bytes[1] = 0x03;
        entry->counter.bytes[2] = 0x18;
        entry->corners[0] = 0;
        entry->corners[1] = 0;
        entry->corners[2] = 0;
        entry->corners[3] = 0;
        entry->flags = 0;
        fontObjInitObject(subObject);
        fontObjSetDrawBits(subObject, 0x00fffff0);
        fontObjSetPosition(subObject, 0x10, 0x160);
        fontObjSetSize(subObject, 0x1E0, 0x50);
    }
}

void cursorBufferSetText(struct FontCursorEntry *entry, unsigned char *text) {
    struct FontBlockCursor *cursor = &entry->record;

    if (entry == 0) {
        return;
    }
    entry->record.text = text;
    cursor->status_flags &= ~0x10000;
    cursor->aux_value = 0;
    cursor->state = cursor->mode;
    cursor->count = 0;
}

/* Clears the text of a cursor buffer entry: cursorBufferSetText with a null text (its only caller, cursorBufferPush, passes pool entries). */
void cursorBufferClearText(struct FontCursorEntry *entry) {
    cursorBufferSetText(entry, 0);
}

void cursorBufferEntryNop(void) {
}

/* Advances one cursor buffer entry: while its flags word has bit 0x10000 set, the pending count in
 * counter.bytes[0] is consumed (one step of counter.bytes[2] off unmodeled_0c, after the first
 * record-aux check), then the sub-object is updated and drawn and, when it is not in phase 1, its line
 * is printed between the two scissor packets. */
void cursorBufferUpdateEntry(struct FontCursorEntry *entry)
{
    int limit;
    int remaining;
    int offset;
    int stepSize;
    int pendingCount;
    int nextCount;
    int offsetX;
    int offsetY;
    int auxValue;

    if (entry == 0) {
        return;
    }
    if ((entry->flags & CURSOR_FLAG_ACTIVE) == 0) {
        return;
    }
    limit = (signed char)entry->counter.bytes[1];
    auxValue = entry->record.aux_value;
    pendingCount = entry->counter.packed & 0xff;
    if ((unsigned int)auxValue >= (unsigned int)limit - 1) {
        if ((signed char)entry->counter.bytes[0] == -1) {
            entry->origin.target_pair.y = (unsigned int)~(auxValue - limit) * entry->counter.bytes[2];
        }
    }
    remaining = pendingCount;
    if ((signed char)remaining > 0) {
        stepSize = entry->counter.bytes[2];
        offset = entry->origin.target_pair.y;
        do {
            nextCount = remaining - 1;
            offset -= stepSize;
            remaining = nextCount;
        } while ((signed char)nextCount > 0);
        entry->counter.bytes[0] = remaining;
        entry->origin.target_pair.y = offset;
    }
    fontObjSubUpdate(&entry->sub_object);
    fontTrackEase(&entry->origin);
    fontObjDrawIfActive(&entry->sub_object);
    if (entry->sub_object.move_phase != FONT_PHASE_OPENING) {
        offsetX = entry->sub_object.tracks[0].current_pair.x + entry->origin.current_pair.x;
        offsetY = entry->sub_object.tracks[0].current_pair.y + entry->origin.current_pair.y;
        fontObjGetQuadCorners(&entry->sub_object, entry->corners);
        entry->corners[1] -= 4;
        entry->corners[3] -= 4;
        xglFontPrintExtFunc(entry->sub_object.draw_bits, ScissorSet, entry->corners);
        fontObjLayoutAtOffset(offsetX, offsetY, entry->sub_object.draw_bits + 1, (struct FontPoint *)&entry->record);
        xglFontPrintExtFunc(entry->sub_object.draw_bits, ScissorReset, 0);
    }
    if ((entry->sub_object.flags & FONT_OBJ_ACTIVE) == 0) {
        entry->flags &= ~CURSOR_FLAG_ACTIVE;
    }
}

/* The bg00.xtx entry names a file that is not on the disc. */
char fontObjTexturePathTable[264] = "yosikawa\\font.xtx\000\000\000\000\000\000\000yosikawa\\bg00.xtx\000\000\000\000\000\000\000yosikawa\\bg01.xtx\000\000\000\000\000\000\000yosikawa\\obj01.xtx\000\000\000\000\000\000yosikawa\\g2_00.xtx\000\000\000\000\000\000yosikawa\\g2_01.xtx\000\000\000\000\000\000yosikawa\\seg01.xtx\000\000\000\000\000\000yosikawa\\battle01.xtx\000\000\000yosikawa\\ctrl_pic.xtx\000\000\000yosikawa\\ctrl01.xtx\000\000\000\000\000yosikawa\\g2_02.xtx";

signed char fontObjTextureSlot[24] = { 0, -1, 2, 1, 3, 4, 3, 5, 3, 4, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

u64 fontObjVifPacketData[14] = { 0x0000000000000000ULL, 0x5100000600000000ULL, 0x5000000000008001ULL, 0x00000000000eeeeeULL, 0x0000000000000000ULL, 0x000000000000003fULL, 0x0000000000000000ULL, 0x0000000000000050ULL, 0x0000000000000000ULL, 0x0000000000000051ULL, 0x0000000000000000ULL, 0x0000000000000052ULL, 0x0000000000000000ULL, 0x0000000000000053ULL };

u64 fontObjBaseVifPacketData[14] = { 0x0000000000000000ULL, 0x5100000600000000ULL, 0x5000000000008001ULL, 0x00000000000eeeeeULL, 0x0000000000000000ULL, 0x000000000000003fULL, 0x0000000000000000ULL, 0x0000000000000050ULL, 0x0000000000000000ULL, 0x0000000000000051ULL, 0x0000000000000000ULL, 0x0000000000000052ULL, 0x0000000000000000ULL, 0x0000000000000053ULL };

struct FontCell fontObjCellTable[758] = {
    { 0, 0, 0, 443, 24, 24, { 0, 0 }, 0x00003b3cu },
    { 0, 0, 24, 443, 8, 24, { 0, 0 }, 0x00003b3cu },
    { 0, 0, 32, 443, 24, 24, { 0, 0 }, 0x00003b3cu },
    { 0, 0, 0, 435, 24, 8, { 0, 0 }, 0x00003b3cu },
    { 0, 0, 24, 435, 8, 8, { 0, 0 }, 0x00003b3cu },
    { 0, 0, 32, 435, 24, 8, { 0, 0 }, 0x00003b3cu },
    { 0, 0, 0, 411, 24, 24, { 0, 0 }, 0x00003b3cu },
    { 0, 0, 24, 411, 8, 24, { 0, 0 }, 0x00003b3cu },
    { 0, 0, 32, 411, 24, 24, { 0, 0 }, 0x00003b3cu },
    { 0, 0, 56, 451, 16, 16, { 0, 0 }, 0x00003bc0u },
    { 0, 0, 72, 451, 1, 16, { 0, 0 }, 0x00003bc0u },
    { 0, 0, 73, 451, 16, 16, { 0, 0 }, 0x00003bc0u },
    { 0, 0, 56, 450, 16, 1, { 0, 0 }, 0x00003bc0u },
    { 0, 0, 72, 450, 1, 1, { 0, 0 }, 0x00003bc0u },
    { 0, 0, 73, 450, 16, 1, { 0, 0 }, 0x00003bc0u },
    { 0, 0, 56, 434, 16, 16, { 0, 0 }, 0x00003bc0u },
    { 0, 0, 72, 434, 1, 16, { 0, 0 }, 0x00003bc0u },
    { 0, 0, 73, 434, 16, 16, { 0, 0 }, 0x00003bc0u },
    { 0, 0, 0, 256, 18, 22, { 0, 0 }, 0x00003bb4u },
    { 0, 0, 0, 278, 17, 22, { 0, 0 }, 0x00003bb4u },
    { 0, 0, 0, 300, 15, 22, { 0, 0 }, 0x00003bb4u },
    { 0, 0, 180, 300, 17, 22, { 0, 0 }, 0x00003bb4u },
    { 0, 0, 0, 14, 9, 14, { 0, 0 }, 0x00003900u },
    { 0, 0, 0, 28, 7, 14, { 0, 0 }, 0x00003900u },
    { 0, 0, 0, 42, 8, 11, { 0, 0 }, 0x00003901u },
    { 0, 0, 0, 52, 8, 11, { 0, 0 }, 0x00003901u },
    { 0, 0, 0, 62, 7, 11, { 0, 0 }, 0x00003901u },
    { 0, 0, 70, 62, 6, 11, { 0, 0 }, 0x00003901u },
    { 0, 0, 108, 72, 39, 11, { 0, 0 }, 0x00003904u },
    { 0, 0, 0, 72, 9, 11, { 0, 0 }, 0x00003904u },
    { 0, 0, 0, 82, 7, 9, { 0, 0 }, 0x00003905u },
    { 0, 0, 0, 90, 59, 11, { 0, 0 }, 0x00003904u },
    { 0, 0, 243, 14, 8, 14, { 0, 0 }, 0x00003900u },
    { 0, 0, 0, 110, 43, 11, { 0, 0 }, 0x00003904u },
    { 0, 0, 0, 120, 34, 11, { 0, 0 }, 0x00003904u },
    { 0, 0, 0, 339, 16, 16, { 0, 0 }, 0x00003bc4u },
    { 0, 0, 16, 339, 1, 16, { 0, 0 }, 0x00003bc4u },
    { 0, 0, 17, 339, 16, 16, { 0, 0 }, 0x00003bc4u },
    { 0, 0, 0, 338, 16, 1, { 0, 0 }, 0x00003bc4u },
    { 0, 0, 17, 338, 16, 1, { 0, 0 }, 0x00003bc4u },
    { 0, 0, 0, 322, 16, 16, { 0, 0 }, 0x00003bc4u },
    { 0, 0, 16, 322, 1, 16, { 0, 0 }, 0x00003bc4u },
    { 0, 0, 17, 322, 16, 16, { 0, 0 }, 0x00003bc4u },
    { 0, 0, 33, 322, 40, 33, { 0, 0 }, 0x00003bc4u },
    { 0, 0, 0, 355, 12, 14, { 0, 0 }, 0x00003bc4u },
    { 0, 0, 12, 355, 12, 14, { 0, 0 }, 0x00003bc4u },
    { 0, 0, 24, 355, 16, 16, { 0, 0 }, 0x00003bc4u },
    { 0, 0, 73, 339, 16, 16, { 0, 0 }, 0x00003bc4u },
    { 0, 0, 89, 339, 1, 16, { 0, 0 }, 0x00003bc4u },
    { 0, 0, 90, 339, 16, 16, { 0, 0 }, 0x00003bc4u },
    { 0, 0, 73, 338, 16, 1, { 0, 0 }, 0x00003bc4u },
    { 0, 0, 89, 338, 1, 1, { 0, 0 }, 0x00003bc4u },
    { 0, 0, 73, 322, 16, 16, { 0, 0 }, 0x00003bc4u },
    { 0, 0, 90, 322, 16, 16, { 0, 0 }, 0x00003bc4u },
    { 0, 0, 106, 322, 40, 33, { 0, 0 }, 0x00003bc4u },
    { 0, 0, 146, 322, 40, 33, { 0, 0 }, 0x00003bc4u },
    { 0, 0, 197, 332, 30, 30, { 0, 0 }, 0x00003bc4u },
    { 0, 0, 229, 332, 128, 30, { 0, 0 }, 0x00003bc4u },
    { 0, 0, 357, 332, 30, 30, { 0, 0 }, 0x00003bc4u },
    { 0, 0, 387, 300, 30, 128, { 0, 0 }, 0x00003bc4u },
    { 0, 0, 227, 330, 2, 2, { 0, 0 }, 0x00003bc4u },
    { 0, 0, 417, 300, 30, 128, { 0, 0 }, 0x00003bc4u },
    { 0, 0, 197, 300, 30, 30, { 0, 0 }, 0x00003bc4u },
    { 0, 0, 229, 300, 128, 30, { 0, 0 }, 0x00003bc4u },
    { 0, 0, 357, 300, 30, 30, { 0, 0 }, 0x00003bc4u },
    { 0, 0, 447, 347, 48, 47, { 0, 0 }, 0x00003bc4u },
    { 0, 0, 447, 300, 48, 47, { 0, 0 }, 0x00003bc4u },
    { 0, 0, 0, 130, 54, 11, { 0, 0 }, 0x00003904u },
    { 0, 0, 40, 355, 18, 21, { 0, 0 }, 0x00003bc4u },
    { 0, 0, 84, 28, 7, 14, { 0, 0 }, 0x00003900u },
    { 0, 0, 468, 256, 15, 22, { 0, 0 }, 0x00003bb4u },
    { 0, 0, 58, 355, 18, 21, { 0, 0 }, 0x00003b48u },
    { 0, 0, 76, 355, 18, 21, { 0, 0 }, 0x00003b40u },
    { 0, 0, 94, 355, 18, 21, { 0, 0 }, 0x00003b4cu },
    { 0, 0, 112, 355, 18, 21, { 0, 0 }, 0x00003b44u },
    { 0, 0, 98, 28, 7, 14, { 0, 0 }, 0x00003900u },
    { 0, 0, 459, 278, 15, 22, { 0, 0 }, 0x00003bb4u },
    { 0, 0, 400, 496, 16, 16, { 0, 0 }, 0x00003b78u },
    { 0, 0, 400, 488, 16, 8, { 0, 0 }, 0x00003b78u },
    { 0, 0, 400, 472, 16, 16, { 0, 0 }, 0x00003b78u },
    { 0, 0, 416, 472, 8, 16, { 0, 0 }, 0x00003b78u },
    { 0, 0, 424, 472, 16, 16, { 0, 0 }, 0x00003b78u },
    { 0, 0, 336, 496, 8, 16, { 0, 0 }, 0x00003b78u },
    { 0, 0, 344, 496, 8, 16, { 0, 0 }, 0x00003b78u },
    { 0, 0, 352, 496, 8, 16, { 0, 0 }, 0x00003b78u },
    { 0, 0, 472, 496, 16, 16, { 0, 0 }, 0x00003b7cu },
    { 0, 0, 488, 496, 8, 16, { 0, 0 }, 0x00003b7cu },
    { 0, 0, 496, 496, 16, 16, { 0, 0 }, 0x00003b7cu },
    { 0, 0, 496, 488, 16, 8, { 0, 0 }, 0x00003b7cu },
    { 0, 0, 496, 472, 16, 16, { 0, 0 }, 0x00003b7cu },
    { 0, 0, 360, 496, 8, 16, { 0, 0 }, 0x00003b7cu },
    { 0, 0, 368, 474, 32, 19, { 0, 0 }, 0x00003b78u },
    { 0, 0, 368, 454, 32, 20, { 0, 0 }, 0x00003b7cu },
    { 0, 0, 416, 496, 16, 16, { 0, 0 }, 0x00003b78u },
    { 0, 0, 432, 496, 8, 16, { 0, 0 }, 0x00003b78u },
    { 0, 0, 440, 496, 16, 16, { 0, 0 }, 0x00003b78u },
    { 0, 0, 440, 488, 16, 8, { 0, 0 }, 0x00003b78u },
    { 0, 0, 440, 472, 16, 16, { 0, 0 }, 0x00003b78u },
    { 0, 0, 368, 493, 32, 19, { 0, 0 }, 0x00003b78u },
    { 0, 0, 456, 496, 16, 16, { 0, 0 }, 0x00003b7cu },
    { 0, 0, 456, 488, 16, 8, { 0, 0 }, 0x00003b7cu },
    { 0, 0, 456, 472, 16, 16, { 0, 0 }, 0x00003b7cu },
    { 0, 0, 472, 472, 8, 16, { 0, 0 }, 0x00003b7cu },
    { 0, 0, 480, 472, 16, 16, { 0, 0 }, 0x00003b7cu },
    { 0, 0, 440, 452, 32, 20, { 0, 0 }, 0x00003b7cu },
    { 0, 0, 400, 462, 16, 10, { 0, 0 }, 0x00003b78u },
    { 0, 0, 400, 452, 16, 10, { 0, 0 }, 0x00003b78u },
    { 0, 0, 416, 452, 8, 10, { 0, 0 }, 0x00003b78u },
    { 0, 0, 424, 452, 16, 10, { 0, 0 }, 0x00003b78u },
    { 0, 0, 472, 462, 16, 10, { 0, 0 }, 0x00003b7cu },
    { 0, 0, 488, 462, 8, 10, { 0, 0 }, 0x00003b7cu },
    { 0, 0, 496, 462, 16, 10, { 0, 0 }, 0x00003b7cu },
    { 0, 0, 496, 452, 16, 10, { 0, 0 }, 0x00003b7cu },
    { 0, 0, 347, 402, 16, 8, { 0, 0 }, 0x00003bc0u },
    { 0, 0, 363, 402, 8, 8, { 0, 0 }, 0x00003bc0u },
    { 0, 0, 371, 402, 16, 8, { 0, 0 }, 0x00003bc0u },
    { 0, 0, 347, 410, 16, 40, { 0, 0 }, 0x00003bc0u },
    { 0, 0, 363, 410, 8, 40, { 0, 0 }, 0x00003bc0u },
    { 0, 0, 371, 410, 16, 40, { 0, 0 }, 0x00003bc0u },
    { 0, 0, 363, 362, 8, 40, { 0, 0 }, 0x00003bc0u },
    { 0, 0, 371, 362, 16, 40, { 0, 0 }, 0x00003bc0u },
    { 0, 0, 347, 362, 16, 40, { 0, 0 }, 0x00003bc0u },
    { 0, 0, 0, 140, 7, 11, { 0, 0 }, 0x00003900u },
    { 0, 0, 0, 482, 11, 15, { 0, 0 }, 0x00003bb4u },
    { 0, 0, 0, 151, 7, 10, { 0, 0 }, 0x00003900u },
    { 0, 0, 0, 498, 11, 14, { 0, 0 }, 0x00003bb4u },
    { 0, 0, 0, 161, 6, 10, { 0, 0 }, 0x00003900u },
    { 0, 0, 0, 468, 10, 14, { 0, 0 }, 0x00003bb4u },
    { 0, 0, 0, 171, 12, 14, { 0, 0 }, 0x00003900u },
    { 0, 0, 0, 101, 43, 9, { 0, 0 }, 0x00003904u },
    { 0, 0, 187, 362, 160, 18, { 0, 0 }, 0x00003bc4u },
    { 0, 0, 130, 355, 16, 18, { 0, 0 }, 0x00003bc4u },
    { 0, 0, 146, 355, 16, 18, { 0, 0 }, 0x00003b48u },
    { 0, 0, 162, 355, 16, 18, { 0, 0 }, 0x00003b40u },
    { 0, 0, 146, 373, 16, 18, { 0, 0 }, 0x00003b4cu },
    { 0, 0, 162, 373, 16, 18, { 0, 0 }, 0x00003b44u },
    { 0, 0, 256, 22, 16, 22, { 0, 0 }, 0x000038dcu },
    { 0, 0, 272, 22, 32, 22, { 0, 0 }, 0x000038dcu },
    { 0, 0, 304, 22, 16, 22, { 0, 0 }, 0x000038dcu },
    { 0, 0, 320, 22, 166, 22, { 0, 0 }, 0x000038dcu },
    { 0, 0, 487, 22, 16, 22, { 0, 0 }, 0x000038dcu },
    { 0, 0, 256, 88, 16, 54, { 0, 0 }, 0x000038dcu },
    { 0, 0, 272, 88, 16, 54, { 0, 0 }, 0x000038dcu },
    { 0, 0, 256, 0, 16, 22, { 0, 0 }, 0x000038dcu },
    { 0, 0, 272, 0, 120, 22, { 0, 0 }, 0x000038dcu },
    { 0, 0, 320, 88, 16, 22, { 0, 0 }, 0x000038dcu },
    { 0, 0, 392, 0, 95, 22, { 0, 0 }, 0x000038dcu },
    { 0, 0, 487, 0, 16, 22, { 0, 0 }, 0x000038dcu },
    { 0, 0, 256, 66, 16, 22, { 0, 0 }, 0x000038dcu },
    { 0, 0, 272, 66, 32, 22, { 0, 0 }, 0x000038dcu },
    { 0, 0, 304, 66, 16, 22, { 0, 0 }, 0x000038dcu },
    { 0, 0, 320, 66, 166, 22, { 0, 0 }, 0x000038dcu },
    { 0, 0, 487, 66, 16, 22, { 0, 0 }, 0x000038dcu },
    { 0, 0, 288, 88, 16, 54, { 0, 0 }, 0x000038dcu },
    { 0, 0, 304, 88, 16, 54, { 0, 0 }, 0x000038dcu },
    { 0, 0, 256, 44, 16, 22, { 0, 0 }, 0x000038dcu },
    { 0, 0, 272, 44, 120, 22, { 0, 0 }, 0x000038dcu },
    { 0, 0, 320, 110, 16, 22, { 0, 0 }, 0x000038dcu },
    { 0, 0, 392, 44, 95, 22, { 0, 0 }, 0x000038dcu },
    { 0, 0, 487, 44, 16, 22, { 0, 0 }, 0x000038dcu },
    { 0, 0, 0, 0, 10, 14, { 0, 0 }, 0x00003900u },
    { 0, 0, 112, 28, 10, 14, { 0, 0 }, 0x00003900u },
    { 0, 0, 109, 411, 22, 20, { 0, 0 }, 0x00003bb4u },
    { 0, 0, 131, 411, 22, 20, { 0, 0 }, 0x00003bb4u },
    { 0, 0, 159, 391, 31, 20, { 0, 0 }, 0x00003bb4u },
    { 0, 0, 159, 411, 31, 20, { 0, 0 }, 0x00003bb4u },
    { 0, 0, 111, 391, 20, 20, { 0, 0 }, 0x00003bb4u },
    { 0, 0, 56, 411, 53, 20, { 0, 0 }, 0x00003bb4u },
    { 0, 0, 131, 391, 28, 20, { 0, 0 }, 0x00003bb4u },
    { 0, 0, 130, 373, 16, 16, { 0, 0 }, 0x00003bb4u },
    { 0, 0, 200, 432, 22, 20, { 0, 0 }, 0x00003b34u },
    { 0, 0, 200, 452, 22, 20, { 0, 0 }, 0x00003b34u },
    { 0, 0, 447, 394, 31, 20, { 0, 0 }, 0x00003b34u },
    { 0, 0, 478, 394, 31, 20, { 0, 0 }, 0x00003b34u },
    { 0, 0, 327, 412, 20, 20, { 0, 0 }, 0x00003b34u },
    { 0, 0, 387, 428, 53, 20, { 0, 0 }, 0x00003b34u },
    { 0, 0, 340, 461, 28, 20, { 0, 0 }, 0x00003b34u },
    { 0, 0, 206, 416, 16, 16, { 0, 0 }, 0x00003b34u },
    { 0, 0, 384, 120, 32, 32, { 0, 0 }, 0x00003b3cu },
    { 0, 0, 416, 120, 32, 32, { 0, 0 }, 0x00003b3cu },
    { 0, 0, 384, 88, 32, 32, { 0, 0 }, 0x00003b3cu },
    { 0, 0, 416, 88, 32, 32, { 0, 0 }, 0x00003b3cu },
    { 0, 0, 448, 120, 32, 32, { 0, 0 }, 0x00003bc0u },
    { 0, 0, 480, 120, 32, 32, { 0, 0 }, 0x00003bc0u },
    { 0, 0, 448, 88, 32, 32, { 0, 0 }, 0x00003bc0u },
    { 0, 0, 480, 88, 32, 32, { 0, 0 }, 0x00003bc0u },
    { 0, 0, 336, 481, 32, 5, { 0, 0 }, 0x00003b30u },
    { 0, 0, 336, 486, 32, 5, { 0, 0 }, 0x00003b30u },
    { 0, 0, 336, 491, 31, 5, { 0, 0 }, 0x00003b30u },
    { 0, 0, 327, 380, 20, 16, { 0, 0 }, 0x00003954u },
    { 0, 0, 327, 396, 20, 16, { 0, 0 }, 0x00003954u },
    { 0, 0, 89, 432, 111, 35, { 0, 0 }, 0x00003bb4u },
    { 0, 0, 0, 376, 111, 35, { 0, 0 }, 0x00003b34u },
    { 0, 0, 188, 472, 34, 8, { 0, 0 }, 0x00003b34u },
    { 0, 0, 256, 142, 38, 78, { 0, 0 }, 0x00003968u },
    { 0, 0, 256, 220, 22, 26, { 0, 0 }, 0x00003968u },
    { 0, 0, 294, 142, 38, 78, { 0, 0 }, 0x0000396cu },
    { 0, 0, 278, 220, 22, 26, { 0, 0 }, 0x0000396cu },
    { 0, 0, 332, 168, 38, 78, { 0, 0 }, 0x00003978u },
    { 0, 0, 300, 220, 22, 26, { 0, 0 }, 0x00003978u },
    { 0, 0, 186, 322, 10, 16, { 0, 0 }, 0x00003b34u },
    { 1, 0, 0, 0, 512, 448, { 0, 0 }, 0x00003ff4u },
    { 2, 0, 0, 0, 80, 32, { 0, 0 }, 0x00003fd0u },
    { 2, 0, 80, 0, 80, 32, { 0, 0 }, 0x00003fd4u },
    { 2, 0, 0, 32, 80, 32, { 0, 0 }, 0x00003fd0u },
    { 2, 0, 80, 32, 80, 32, { 0, 0 }, 0x00003fd4u },
    { 2, 0, 0, 64, 80, 32, { 0, 0 }, 0x00003fd0u },
    { 2, 0, 80, 64, 80, 32, { 0, 0 }, 0x00003fd4u },
    { 2, 0, 0, 96, 80, 32, { 0, 0 }, 0x00003fd0u },
    { 2, 0, 80, 96, 80, 32, { 0, 0 }, 0x00003fd4u },
    { 2, 0, 0, 128, 80, 32, { 0, 0 }, 0x00003fd0u },
    { 2, 0, 80, 128, 80, 32, { 0, 0 }, 0x00003fd4u },
    { 2, 0, 0, 160, 80, 32, { 0, 0 }, 0x00003fd0u },
    { 2, 0, 80, 160, 80, 32, { 0, 0 }, 0x00003fd4u },
    { 2, 0, 160, 0, 96, 32, { 0, 0 }, 0x00003fd0u },
    { 2, 0, 256, 0, 96, 32, { 0, 0 }, 0x00003fd4u },
    { 2, 0, 160, 32, 96, 32, { 0, 0 }, 0x00003fd0u },
    { 2, 0, 256, 32, 96, 32, { 0, 0 }, 0x00003fd4u },
    { 2, 0, 160, 64, 96, 32, { 0, 0 }, 0x00003fd0u },
    { 2, 0, 256, 64, 96, 32, { 0, 0 }, 0x00003fd4u },
    { 2, 0, 160, 96, 96, 32, { 0, 0 }, 0x00003fd0u },
    { 2, 0, 256, 96, 96, 32, { 0, 0 }, 0x00003fd4u },
    { 2, 0, 160, 128, 40, 32, { 0, 0 }, 0x00003fd0u },
    { 2, 0, 0, 434, 96, 48, { 0, 0 }, 0x00003fe4u },
    { 2, 0, 160, 160, 18, 18, { 0, 0 }, 0x00003fc4u },
    { 2, 0, 178, 160, 16, 20, { 0, 0 }, 0x00003fc4u },
    { 2, 0, 194, 160, 16, 20, { 0, 0 }, 0x00003fc4u },
    { 2, 0, 96, 434, 138, 40, { 0, 0 }, 0x00003fe4u },
    { 2, 0, 0, 192, 512, 27, { 0, 0 }, 0x00003fc0u },
    { 2, 0, 0, 219, 512, 50, { 0, 0 }, 0x00003fc0u },
    { 2, 0, 0, 269, 512, 106, { 0, 0 }, 0x00003fe0u },
    { 2, 0, 0, 375, 512, 40, { 0, 0 }, 0x00003fe0u },
    { 2, 0, 0, 415, 512, 16, { 0, 0 }, 0x00003fe0u },
    { 2, 0, 0, 431, 512, 3, { 0, 0 }, 0x00003fe0u },
    { 2, 0, 234, 434, 16, 16, { 0, 0 }, 0x00003fe0u },
    { 2, 0, 160, 150, 10, 10, { 0, 0 }, 0x00003fd0u },
    { 2, 0, 170, 150, 20, 10, { 0, 0 }, 0x00003fd0u },
    { 2, 0, 190, 150, 10, 10, { 0, 0 }, 0x00003fd0u },
    { 2, 0, 160, 138, 10, 12, { 0, 0 }, 0x00003fd0u },
    { 2, 0, 170, 138, 20, 12, { 0, 0 }, 0x00003fd0u },
    { 2, 0, 190, 138, 10, 12, { 0, 0 }, 0x00003fd0u },
    { 2, 0, 160, 128, 10, 10, { 0, 0 }, 0x00003fd0u },
    { 2, 0, 170, 128, 20, 10, { 0, 0 }, 0x00003fd0u },
    { 2, 0, 190, 128, 10, 10, { 0, 0 }, 0x00003fd0u },
    { 2, 0, 0, 462, 20, 20, { 0, 0 }, 0x00003fe4u },
    { 2, 0, 20, 462, 56, 20, { 0, 0 }, 0x00003fe4u },
    { 2, 0, 76, 462, 20, 20, { 0, 0 }, 0x00003fe4u },
    { 2, 0, 0, 454, 20, 8, { 0, 0 }, 0x00003fe4u },
    { 2, 0, 20, 454, 56, 8, { 0, 0 }, 0x00003fe4u },
    { 2, 0, 76, 454, 20, 8, { 0, 0 }, 0x00003fe4u },
    { 2, 0, 0, 434, 20, 20, { 0, 0 }, 0x00003fe4u },
    { 2, 0, 20, 434, 56, 20, { 0, 0 }, 0x00003fe4u },
    { 2, 0, 76, 434, 20, 20, { 0, 0 }, 0x00003fe4u },
    { 3, 0, 56, 48, 8, 40, { 0, 0 }, 0x00003c74u },
    { 3, 0, 64, 48, 16, 40, { 0, 0 }, 0x00003c74u },
    { 3, 0, 0, 40, 16, 8, { 0, 0 }, 0x00003c74u },
    { 3, 0, 16, 40, 8, 8, { 0, 0 }, 0x00003c74u },
    { 3, 0, 64, 40, 16, 8, { 0, 0 }, 0x00003c74u },
    { 3, 0, 0, 0, 16, 40, { 0, 0 }, 0x00003c74u },
    { 3, 0, 16, 0, 8, 40, { 0, 0 }, 0x00003c74u },
    { 3, 0, 80, 0, 16, 40, { 0, 0 }, 0x00003c74u },
    { 3, 0, 0, 48, 16, 40, { 0, 0 }, 0x00003c74u },
    { 3, 0, 16, 48, 8, 40, { 0, 0 }, 0x00003c74u },
    { 3, 0, 80, 48, 16, 40, { 0, 0 }, 0x00003c74u },
    { 3, 0, 56, 0, 8, 40, { 0, 0 }, 0x00003c74u },
    { 3, 0, 64, 0, 16, 40, { 0, 0 }, 0x00003c74u },
    { 3, 0, 24, 48, 32, 40, { 0, 0 }, 0x00003c74u },
    { 3, 0, 24, 0, 32, 40, { 0, 0 }, 0x00003c74u },
    { 3, 0, 96, 24, 16, 16, { 0, 0 }, 0x00003d7cu },
    { 3, 0, 96, 16, 16, 8, { 0, 0 }, 0x00003d7cu },
    { 3, 0, 96, 0, 16, 16, { 0, 0 }, 0x00003d7cu },
    { 3, 0, 112, 0, 8, 16, { 0, 0 }, 0x00003d7cu },
    { 3, 0, 120, 0, 16, 16, { 0, 0 }, 0x00003d7cu },
    { 3, 0, 152, 0, 8, 16, { 0, 0 }, 0x00003d7cu },
    { 3, 0, 160, 0, 8, 16, { 0, 0 }, 0x00003d7cu },
    { 3, 0, 168, 0, 8, 16, { 0, 0 }, 0x00003d7cu },
    { 3, 0, 374, 113, 100, 18, { 0, 0 }, 0x00003cfcu },
    { 3, 0, 374, 131, 100, 18, { 0, 0 }, 0x00003d74u },
    { 3, 0, 96, 57, 16, 16, { 0, 0 }, 0x00003c7cu },
    { 3, 0, 112, 57, 1, 16, { 0, 0 }, 0x00003c7cu },
    { 3, 0, 113, 57, 16, 16, { 0, 0 }, 0x00003c7cu },
    { 3, 0, 96, 56, 16, 1, { 0, 0 }, 0x00003c7cu },
    { 3, 0, 112, 56, 1, 1, { 0, 0 }, 0x00003c7cu },
    { 3, 0, 113, 56, 16, 1, { 0, 0 }, 0x00003c7cu },
    { 3, 0, 96, 40, 16, 16, { 0, 0 }, 0x00003c7cu },
    { 3, 0, 112, 40, 1, 16, { 0, 0 }, 0x00003c7cu },
    { 3, 0, 113, 40, 16, 16, { 0, 0 }, 0x00003c7cu },
    { 3, 0, 129, 40, 40, 33, { 0, 0 }, 0x00003c7cu },
    { 3, 0, 188, 213, 117, 19, { 0, 0 }, 0x00003df4u },
    { 3, 0, 188, 232, 117, 19, { 0, 0 }, 0x00003dfcu },
    { 3, 0, 112, 24, 16, 16, { 0, 0 }, 0x00003cf0u },
    { 3, 0, 128, 24, 8, 16, { 0, 0 }, 0x00003cf0u },
    { 3, 0, 136, 24, 16, 16, { 0, 0 }, 0x00003cf0u },
    { 3, 0, 136, 16, 16, 8, { 0, 0 }, 0x00003cf0u },
    { 3, 0, 136, 0, 16, 16, { 0, 0 }, 0x00003cf0u },
    { 3, 0, 176, 0, 8, 16, { 0, 0 }, 0x00003cf0u },
    { 3, 0, 157, 259, 146, 20, { 0, 0 }, 0x00003c74u },
    { 3, 0, 369, 229, 93, 20, { 0, 0 }, 0x00003c74u },
    { 3, 0, 369, 169, 94, 20, { 0, 0 }, 0x00003c74u },
    { 3, 0, 369, 149, 90, 20, { 0, 0 }, 0x00003c74u },
    { 3, 0, 303, 259, 117, 20, { 0, 0 }, 0x00003c74u },
    { 3, 0, 369, 209, 94, 20, { 0, 0 }, 0x00003c74u },
    { 3, 0, 369, 189, 93, 20, { 0, 0 }, 0x00003c74u },
    { 3, 0, 374, 73, 24, 29, { 0, 0 }, 0x00003c70u },
    { 3, 0, 412, 73, 26, 29, { 0, 0 }, 0x00003c70u },
    { 3, 0, 317, 424, 16, 18, { 0, 0 }, 0x00003ff0u },
    { 3, 0, 333, 424, 16, 18, { 0, 0 }, 0x00003f78u },
    { 3, 0, 349, 424, 16, 18, { 0, 0 }, 0x00003f70u },
    { 3, 0, 246, 133, 16, 16, { 0, 0 }, 0x00003fe4u },
    { 3, 0, 420, 263, 40, 16, { 0, 0 }, 0x00003c7cu },
    { 3, 0, 460, 279, 16, 25, { 0, 0 }, 0x00003c7cu },
    { 3, 0, 476, 279, 1, 25, { 0, 0 }, 0x00003c7cu },
    { 3, 0, 462, 233, 14, 16, { 0, 0 }, 0x00003f6cu },
    { 3, 0, 152, 489, 64, 18, { 0, 0 }, 0x00003c74u },
    { 3, 0, 188, 128, 10, 21, { 0, 0 }, 0x00003c7cu },
    { 3, 0, 198, 128, 10, 21, { 0, 0 }, 0x00003c7cu },
    { 3, 0, 208, 128, 10, 21, { 0, 0 }, 0x00003c7cu },
    { 3, 0, 352, 464, 60, 20, { 0, 0 }, 0x00003cfcu },
    { 3, 0, 352, 484, 60, 20, { 0, 0 }, 0x00003d74u },
    { 3, 0, 352, 444, 60, 20, { 0, 0 }, 0x00003c74u },
    { 3, 0, 376, 424, 36, 20, { 0, 0 }, 0x00003c74u },
    { 3, 0, 0, 489, 152, 20, { 0, 0 }, 0x00003c74u },
    { 3, 0, 169, 33, 16, 40, { 0, 0 }, 0x00003c74u },
    { 3, 0, 0, 88, 38, 78, { 0, 0 }, 0x00003ffcu },
    { 3, 0, 166, 77, 22, 26, { 0, 0 }, 0x00003ffcu },
    { 3, 0, 38, 88, 38, 78, { 0, 0 }, 0x00003ff4u },
    { 3, 0, 166, 103, 22, 26, { 0, 0 }, 0x00003ff4u },
    { 3, 0, 76, 88, 38, 78, { 0, 0 }, 0x00003f7cu },
    { 3, 0, 166, 129, 22, 26, { 0, 0 }, 0x00003f7cu },
    { 3, 0, 0, 166, 38, 78, { 0, 0 }, 0x00003f74u },
    { 3, 0, 166, 155, 22, 26, { 0, 0 }, 0x00003f74u },
    { 3, 0, 38, 166, 38, 78, { 0, 0 }, 0x00003efcu },
    { 3, 0, 166, 181, 22, 26, { 0, 0 }, 0x00003efcu },
    { 3, 0, 76, 166, 38, 78, { 0, 0 }, 0x00003ef4u },
    { 3, 0, 166, 207, 22, 26, { 0, 0 }, 0x00003ef4u },
    { 3, 0, 114, 166, 38, 78, { 0, 0 }, 0x00003e7cu },
    { 3, 0, 166, 233, 22, 26, { 0, 0 }, 0x00003e7cu },
    { 3, 0, 260, 0, 38, 78, { 0, 0 }, 0x00003c30u },
    { 3, 0, 238, 0, 22, 26, { 0, 0 }, 0x00003c30u },
    { 3, 0, 298, 0, 38, 78, { 0, 0 }, 0x00003c38u },
    { 3, 0, 238, 26, 22, 26, { 0, 0 }, 0x00003c38u },
    { 3, 0, 336, 0, 38, 78, { 0, 0 }, 0x00003cb0u },
    { 3, 0, 238, 52, 22, 26, { 0, 0 }, 0x00003cb0u },
    { 3, 0, 114, 88, 38, 78, { 0, 0 }, 0x00003ff8u },
    { 3, 0, 295, 424, 22, 26, { 0, 0 }, 0x00003ff8u },
    { 3, 0, 392, 391, 22, 26, { 0, 0 }, 0x00003f64u },
    { 3, 0, 401, 73, 11, 16, { 0, 0 }, 0x00003fecu },
    { 3, 0, 398, 90, 9, 12, { 0, 0 }, 0x00003fecu },
    { 3, 0, 398, 102, 6, 6, { 0, 0 }, 0x00003fecu },
    { 3, 0, 216, 489, 14, 19, { 0, 0 }, 0x00003fecu },
    { 3, 0, 230, 489, 14, 19, { 0, 0 }, 0x00003fecu },
    { 3, 0, 244, 489, 14, 19, { 0, 0 }, 0x00003fecu },
    { 3, 0, 258, 489, 14, 19, { 0, 0 }, 0x00003fecu },
    { 3, 0, 272, 489, 16, 17, { 0, 0 }, 0x00003c7cu },
    { 3, 0, 288, 489, 16, 17, { 0, 0 }, 0x00003c7cu },
    { 3, 0, 304, 489, 16, 17, { 0, 0 }, 0x00003c7cu },
    { 3, 0, 336, 444, 16, 20, { 0, 0 }, 0x00003c78u },
    { 3, 0, 336, 464, 16, 20, { 0, 0 }, 0x00003c78u },
    { 3, 0, 328, 98, 18, 31, { 0, 0 }, 0x00003c70u },
    { 3, 0, 233, 80, 8, 18, { 0, 0 }, 0x00003fecu },
    { 3, 0, 236, 98, 18, 31, { 0, 0 }, 0x00003fecu },
    { 3, 0, 254, 98, 18, 31, { 0, 0 }, 0x00003c70u },
    { 3, 0, 272, 98, 28, 31, { 0, 0 }, 0x00003fecu },
    { 3, 0, 300, 98, 28, 31, { 0, 0 }, 0x00003c70u },
    { 3, 0, 0, 244, 66, 35, { 0, 0 }, 0x00003cf4u },
    { 3, 0, 0, 279, 66, 35, { 0, 0 }, 0x00003c64u },
    { 3, 0, 66, 244, 91, 35, { 0, 0 }, 0x00003cf4u },
    { 3, 0, 66, 279, 91, 35, { 0, 0 }, 0x00003c64u },
    { 3, 0, 157, 279, 68, 35, { 0, 0 }, 0x00003cf4u },
    { 3, 0, 0, 349, 68, 35, { 0, 0 }, 0x00003c64u },
    { 3, 0, 210, 454, 56, 35, { 0, 0 }, 0x00003cf4u },
    { 3, 0, 68, 349, 56, 35, { 0, 0 }, 0x00003c64u },
    { 3, 0, 113, 454, 97, 35, { 0, 0 }, 0x00003cf4u },
    { 3, 0, 295, 384, 97, 35, { 0, 0 }, 0x00003c64u },
    { 3, 0, 124, 314, 58, 35, { 0, 0 }, 0x00003cf4u },
    { 3, 0, 124, 349, 58, 35, { 0, 0 }, 0x00003c64u },
    { 3, 0, 182, 314, 58, 35, { 0, 0 }, 0x00003cf4u },
    { 3, 0, 182, 349, 58, 35, { 0, 0 }, 0x00003c64u },
    { 3, 0, 0, 384, 82, 35, { 0, 0 }, 0x00003cf4u },
    { 3, 0, 0, 419, 82, 35, { 0, 0 }, 0x00003c64u },
    { 3, 0, 82, 384, 104, 35, { 0, 0 }, 0x00003cf4u },
    { 3, 0, 82, 419, 104, 35, { 0, 0 }, 0x00003c64u },
    { 3, 0, 383, 314, 82, 35, { 0, 0 }, 0x00003cf4u },
    { 3, 0, 383, 349, 82, 35, { 0, 0 }, 0x00003c64u },
    { 3, 0, 240, 314, 64, 35, { 0, 0 }, 0x00003cf4u },
    { 3, 0, 240, 349, 64, 35, { 0, 0 }, 0x00003c64u },
    { 3, 0, 304, 314, 79, 35, { 0, 0 }, 0x00003cf4u },
    { 3, 0, 304, 349, 79, 35, { 0, 0 }, 0x00003c64u },
    { 3, 0, 186, 384, 109, 35, { 0, 0 }, 0x00003cf4u },
    { 3, 0, 186, 419, 109, 35, { 0, 0 }, 0x00003c64u },
    { 3, 0, 259, 279, 71, 35, { 0, 0 }, 0x00003cf4u },
    { 3, 0, 330, 279, 71, 35, { 0, 0 }, 0x00003c64u },
    { 3, 0, 266, 454, 59, 35, { 0, 0 }, 0x00003cf4u },
    { 3, 0, 401, 279, 59, 35, { 0, 0 }, 0x00003c64u },
    { 3, 0, 0, 454, 113, 35, { 0, 0 }, 0x00003cf4u },
    { 3, 0, 0, 314, 113, 35, { 0, 0 }, 0x00003c64u },
    { 3, 0, 282, 149, 87, 18, { 0, 0 }, 0x00003c6cu },
    { 4, 0, 0, 0, 512, 48, { 0, 0 }, 0x00003ffcu },
    { 4, 0, 0, 48, 512, 320, { 0, 0 }, 0x00003ffcu },
    { 4, 0, 0, 368, 512, 80, { 0, 0 }, 0x00003ffcu },
    { 4, 0, 0, 448, 512, 27, { 0, 0 }, 0x00003ff8u },
    { 5, 0, 56, 48, 8, 40, { 0, 0 }, 0x00003e78u },
    { 5, 0, 64, 48, 16, 40, { 0, 0 }, 0x00003e78u },
    { 5, 0, 0, 40, 16, 8, { 0, 0 }, 0x00003e78u },
    { 5, 0, 16, 40, 8, 8, { 0, 0 }, 0x00003e78u },
    { 5, 0, 64, 40, 16, 8, { 0, 0 }, 0x00003e78u },
    { 5, 0, 0, 0, 16, 40, { 0, 0 }, 0x00003e78u },
    { 5, 0, 16, 0, 8, 40, { 0, 0 }, 0x00003e78u },
    { 5, 0, 80, 0, 16, 40, { 0, 0 }, 0x00003e78u },
    { 5, 0, 0, 48, 16, 40, { 0, 0 }, 0x00003e78u },
    { 5, 0, 16, 48, 8, 40, { 0, 0 }, 0x00003e78u },
    { 5, 0, 80, 48, 16, 40, { 0, 0 }, 0x00003e78u },
    { 5, 0, 56, 0, 8, 40, { 0, 0 }, 0x00003e78u },
    { 5, 0, 64, 0, 16, 40, { 0, 0 }, 0x00003e78u },
    { 5, 0, 24, 48, 32, 40, { 0, 0 }, 0x00003e78u },
    { 5, 0, 24, 0, 32, 40, { 0, 0 }, 0x00003e78u },
    { 5, 0, 96, 24, 16, 16, { 0, 0 }, 0x00003fe8u },
    { 5, 0, 96, 16, 16, 8, { 0, 0 }, 0x00003fe8u },
    { 5, 0, 96, 0, 16, 16, { 0, 0 }, 0x00003fe8u },
    { 5, 0, 112, 0, 8, 16, { 0, 0 }, 0x00003fe8u },
    { 5, 0, 120, 0, 16, 16, { 0, 0 }, 0x00003fe8u },
    { 5, 0, 152, 0, 8, 16, { 0, 0 }, 0x00003fe8u },
    { 5, 0, 160, 0, 8, 16, { 0, 0 }, 0x00003fe8u },
    { 5, 0, 168, 0, 8, 16, { 0, 0 }, 0x00003fe8u },
    { 5, 0, 112, 24, 16, 16, { 0, 0 }, 0x00003f9cu },
    { 5, 0, 128, 24, 8, 16, { 0, 0 }, 0x00003f9cu },
    { 5, 0, 136, 24, 16, 16, { 0, 0 }, 0x00003f9cu },
    { 5, 0, 136, 16, 16, 8, { 0, 0 }, 0x00003f9cu },
    { 5, 0, 136, 0, 16, 16, { 0, 0 }, 0x00003f9cu },
    { 5, 0, 176, 0, 8, 16, { 0, 0 }, 0x00003f9cu },
    { 5, 0, 40, 112, 16, 16, { 0, 0 }, 0x00003f98u },
    { 5, 0, 64, 112, 16, 16, { 0, 0 }, 0x00003f98u },
    { 5, 0, 56, 104, 8, 8, { 0, 0 }, 0x00003f98u },
    { 5, 0, 40, 88, 16, 16, { 0, 0 }, 0x00003f98u },
    { 5, 0, 64, 88, 16, 16, { 0, 0 }, 0x00003f98u },
    { 5, 0, 341, 32, 139, 23, { 0, 0 }, 0x00003f9cu },
    { 5, 0, 184, 0, 32, 20, { 0, 0 }, 0x00003f9cu },
    { 5, 0, 341, 55, 139, 23, { 0, 0 }, 0x00003fe8u },
    { 5, 0, 184, 20, 32, 19, { 0, 0 }, 0x00003fe8u },
    { 5, 0, 96, 78, 16, 19, { 0, 0 }, 0x00003e78u },
    { 5, 0, 112, 78, 8, 19, { 0, 0 }, 0x00003e78u },
    { 5, 0, 120, 78, 16, 19, { 0, 0 }, 0x00003e78u },
    { 5, 0, 96, 40, 16, 30, { 0, 0 }, 0x00003e78u },
    { 5, 0, 112, 40, 8, 30, { 0, 0 }, 0x00003e78u },
    { 5, 0, 120, 40, 16, 30, { 0, 0 }, 0x00003e78u },
    { 5, 0, 136, 78, 16, 19, { 0, 0 }, 0x00003e78u },
    { 5, 0, 152, 78, 8, 19, { 0, 0 }, 0x00003e78u },
    { 5, 0, 160, 78, 16, 19, { 0, 0 }, 0x00003e78u },
    { 5, 0, 136, 70, 16, 8, { 0, 0 }, 0x00003e78u },
    { 5, 0, 160, 70, 16, 8, { 0, 0 }, 0x00003e78u },
    { 5, 0, 136, 40, 16, 30, { 0, 0 }, 0x00003e78u },
    { 5, 0, 152, 40, 8, 30, { 0, 0 }, 0x00003e78u },
    { 5, 0, 160, 40, 16, 30, { 0, 0 }, 0x00003e78u },
    { 5, 0, 0, 219, 187, 18, { 0, 0 }, 0x00003e78u },
    { 5, 0, 226, 297, 83, 20, { 0, 0 }, 0x00003e78u },
    { 5, 0, 176, 40, 23, 26, { 0, 0 }, 0x00003e78u },
    { 5, 0, 199, 40, 23, 26, { 0, 0 }, 0x00003e78u },
    { 5, 0, 176, 66, 23, 26, { 0, 0 }, 0x00003e78u },
    { 5, 0, 96, 121, 16, 16, { 0, 0 }, 0x00003e78u },
    { 5, 0, 112, 121, 8, 16, { 0, 0 }, 0x00003e78u },
    { 5, 0, 120, 121, 16, 16, { 0, 0 }, 0x00003e78u },
    { 5, 0, 96, 113, 16, 8, { 0, 0 }, 0x00003e78u },
    { 5, 0, 120, 113, 16, 8, { 0, 0 }, 0x00003e78u },
    { 5, 0, 96, 97, 16, 16, { 0, 0 }, 0x00003e78u },
    { 5, 0, 112, 97, 8, 16, { 0, 0 }, 0x00003e78u },
    { 5, 0, 120, 97, 16, 16, { 0, 0 }, 0x00003e78u },
    { 5, 0, 256, 24, 16, 16, { 0, 0 }, 0x00003fa8u },
    { 5, 0, 272, 24, 8, 16, { 0, 0 }, 0x00003fa8u },
    { 5, 0, 280, 24, 16, 16, { 0, 0 }, 0x00003fa8u },
    { 5, 0, 256, 16, 16, 8, { 0, 0 }, 0x00003fa8u },
    { 5, 0, 280, 16, 16, 8, { 0, 0 }, 0x00003fa8u },
    { 5, 0, 256, 0, 16, 16, { 0, 0 }, 0x00003fa8u },
    { 5, 0, 272, 0, 8, 16, { 0, 0 }, 0x00003fa8u },
    { 5, 0, 280, 0, 16, 16, { 0, 0 }, 0x00003fa8u },
    { 5, 0, 480, 238, 16, 16, { 0, 0 }, 0x00003f8cu },
    { 5, 0, 296, 16, 184, 16, { 0, 0 }, 0x00003f8cu },
    { 5, 0, 496, 238, 16, 16, { 0, 0 }, 0x00003f8cu },
    { 5, 0, 480, 16, 16, 222, { 0, 0 }, 0x00003f8cu },
    { 5, 0, 496, 16, 16, 222, { 0, 0 }, 0x00003f8cu },
    { 5, 0, 480, 0, 16, 16, { 0, 0 }, 0x00003f8cu },
    { 5, 0, 296, 0, 184, 16, { 0, 0 }, 0x00003f8cu },
    { 5, 0, 496, 0, 16, 16, { 0, 0 }, 0x00003f8cu },
    { 5, 0, 0, 112, 16, 16, { 0, 0 }, 0x00003fe8u },
    { 5, 0, 16, 112, 8, 16, { 0, 0 }, 0x00003fe8u },
    { 5, 0, 24, 112, 16, 16, { 0, 0 }, 0x00003fe8u },
    { 5, 0, 0, 104, 16, 8, { 0, 0 }, 0x00003fe8u },
    { 5, 0, 24, 104, 16, 8, { 0, 0 }, 0x00003fe8u },
    { 5, 0, 0, 88, 16, 16, { 0, 0 }, 0x00003fe8u },
    { 5, 0, 16, 88, 8, 16, { 0, 0 }, 0x00003fe8u },
    { 5, 0, 24, 88, 16, 16, { 0, 0 }, 0x00003fe8u },
    { 5, 0, 0, 277, 177, 20, { 0, 0 }, 0x00003e78u },
    { 5, 0, 368, 78, 112, 127, { 0, 0 }, 0x00003fc8u },
    { 5, 0, 222, 40, 55, 59, { 0, 0 }, 0x00003fccu },
    { 5, 0, 277, 40, 55, 59, { 0, 0 }, 0x00003fd8u },
    { 5, 0, 198, 99, 79, 91, { 0, 0 }, 0x00003fccu },
    { 5, 0, 277, 99, 79, 91, { 0, 0 }, 0x00003fd8u },
    { 5, 0, 0, 317, 100, 15, { 0, 0 }, 0x00003facu },
    { 5, 0, 100, 317, 100, 15, { 0, 0 }, 0x00003facu },
    { 5, 0, 200, 317, 117, 15, { 0, 0 }, 0x00003facu },
    { 5, 0, 317, 317, 117, 15, { 0, 0 }, 0x00003facu },
    { 5, 0, 406, 287, 26, 27, { 0, 0 }, 0x00003fbcu },
    { 5, 0, 337, 232, 143, 27, { 0, 0 }, 0x00003fbcu },
    { 5, 0, 337, 205, 143, 27, { 0, 0 }, 0x00003fe8u },
    { 5, 0, 380, 287, 26, 27, { 0, 0 }, 0x00003fe8u },
    { 5, 0, 0, 161, 173, 58, { 0, 0 }, 0x00003e7cu },
    { 5, 0, 142, 99, 56, 58, { 0, 0 }, 0x00003e7cu },
    { 5, 0, 0, 237, 129, 20, { 0, 0 }, 0x00003e78u },
    { 5, 0, 28, 155, 14, 6, { 0, 0 }, 0x00003fecu },
    { 5, 0, 0, 139, 28, 22, { 0, 0 }, 0x00003fecu },
    { 5, 0, 236, 190, 38, 51, { 0, 0 }, 0x00003fb8u },
    { 5, 0, 274, 190, 38, 51, { 0, 0 }, 0x00003fdcu },
    { 5, 0, 198, 190, 38, 51, { 0, 0 }, 0x00003f88u },
    { 5, 0, 28, 149, 14, 6, { 0, 0 }, 0x00003fecu },
    { 5, 0, 42, 139, 28, 22, { 0, 0 }, 0x00003fecu },
    { 5, 0, 0, 297, 226, 20, { 0, 0 }, 0x00003e78u },
    { 5, 0, 0, 257, 130, 20, { 0, 0 }, 0x00003e78u },
    { 5, 0, 129, 237, 59, 20, { 0, 0 }, 0x00003e78u },
    { 5, 0, 0, 332, 374, 5, { 0, 0 }, 0x00003fe8u },
    { 5, 0, 0, 337, 512, 71, { 0, 0 }, 0x00003fe8u },
    { 5, 0, 0, 408, 512, 69, { 0, 0 }, 0x00003fe8u },
    { 5, 0, 177, 266, 117, 31, { 0, 0 }, 0x00003ff8u },
    { 5, 0, 130, 257, 39, 20, { 0, 0 }, 0x00003e78u },
    { 5, 0, 294, 259, 138, 28, { 0, 0 }, 0x00003fdcu },
    { 5, 0, 70, 143, 45, 18, { 0, 0 }, 0x00003ffcu },
    { 5, 0, 0, 219, 17, 18, { 0, 0 }, 0x00003e78u },
    { 5, 0, 152, 16, 8, 22, { 0, 0 }, 0x00003fe8u },
    { 5, 0, 160, 16, 8, 22, { 0, 0 }, 0x00003fe8u },
    { 5, 0, 168, 16, 8, 22, { 0, 0 }, 0x00003fe8u },
    { 5, 0, 216, 0, 8, 22, { 0, 0 }, 0x00003f9cu },
    { 5, 0, 224, 0, 8, 22, { 0, 0 }, 0x00003f9cu },
    { 5, 0, 232, 0, 8, 22, { 0, 0 }, 0x00003f9cu },
    { 5, 0, 199, 66, 22, 14, { 0, 0 }, 0x00003fe8u },
    { 5, 0, 199, 80, 22, 14, { 0, 0 }, 0x00003f9cu },
    { 5, 0, 198, 241, 68, 20, { 0, 0 }, 0x00003e78u },
    { 5, 0, 316, 287, 64, 20, { 0, 0 }, 0x00003e78u },
    { 6, 0, 56, 48, 8, 40, { 0, 0 }, 0x00003c74u },
    { 6, 0, 64, 48, 16, 40, { 0, 0 }, 0x00003c74u },
    { 6, 0, 0, 40, 16, 8, { 0, 0 }, 0x00003c74u },
    { 6, 0, 16, 40, 8, 8, { 0, 0 }, 0x00003c74u },
    { 6, 0, 64, 40, 16, 8, { 0, 0 }, 0x00003c74u },
    { 6, 0, 0, 0, 16, 40, { 0, 0 }, 0x00003c74u },
    { 6, 0, 16, 0, 8, 40, { 0, 0 }, 0x00003c74u },
    { 6, 0, 80, 0, 16, 40, { 0, 0 }, 0x00003c74u },
    { 6, 0, 0, 48, 16, 40, { 0, 0 }, 0x00003c74u },
    { 6, 0, 16, 48, 8, 40, { 0, 0 }, 0x00003c74u },
    { 6, 0, 80, 48, 16, 40, { 0, 0 }, 0x00003c74u },
    { 6, 0, 56, 0, 8, 40, { 0, 0 }, 0x00003c74u },
    { 6, 0, 64, 0, 16, 40, { 0, 0 }, 0x00003c74u },
    { 6, 0, 24, 48, 32, 40, { 0, 0 }, 0x00003c74u },
    { 6, 0, 24, 0, 32, 40, { 0, 0 }, 0x00003c74u },
    { 6, 0, 96, 24, 16, 16, { 0, 0 }, 0x00003d7cu },
    { 6, 0, 96, 16, 16, 8, { 0, 0 }, 0x00003d7cu },
    { 6, 0, 96, 0, 16, 16, { 0, 0 }, 0x00003d7cu },
    { 6, 0, 112, 0, 8, 16, { 0, 0 }, 0x00003d7cu },
    { 6, 0, 120, 0, 16, 16, { 0, 0 }, 0x00003d7cu },
    { 6, 0, 152, 0, 8, 16, { 0, 0 }, 0x00003d7cu },
    { 6, 0, 160, 0, 8, 16, { 0, 0 }, 0x00003d7cu },
    { 6, 0, 168, 0, 8, 16, { 0, 0 }, 0x00003d7cu },
    { 6, 0, 112, 24, 16, 16, { 0, 0 }, 0x00003d70u },
    { 6, 0, 128, 24, 8, 16, { 0, 0 }, 0x00003d70u },
    { 6, 0, 136, 24, 16, 16, { 0, 0 }, 0x00003d70u },
    { 6, 0, 136, 16, 16, 8, { 0, 0 }, 0x00003d70u },
    { 6, 0, 136, 0, 16, 16, { 0, 0 }, 0x00003d70u },
    { 6, 0, 176, 0, 8, 16, { 0, 0 }, 0x00003d70u },
    { 6, 0, 40, 112, 16, 16, { 0, 0 }, 0x00003d78u },
    { 6, 0, 64, 112, 16, 16, { 0, 0 }, 0x00003d78u },
    { 6, 0, 56, 104, 8, 8, { 0, 0 }, 0x00003d78u },
    { 6, 0, 40, 88, 16, 16, { 0, 0 }, 0x00003d78u },
    { 6, 0, 64, 88, 16, 16, { 0, 0 }, 0x00003d78u },
    { 6, 0, 0, 112, 16, 17, { 0, 0 }, 0x00003d7cu },
    { 6, 0, 16, 112, 8, 17, { 0, 0 }, 0x00003d7cu },
    { 6, 0, 24, 112, 16, 17, { 0, 0 }, 0x00003d7cu },
    { 6, 0, 0, 104, 16, 8, { 0, 0 }, 0x00003d7cu },
    { 6, 0, 24, 104, 16, 8, { 0, 0 }, 0x00003d7cu },
    { 6, 0, 0, 88, 16, 16, { 0, 0 }, 0x00003d7cu },
    { 6, 0, 16, 88, 8, 16, { 0, 0 }, 0x00003d7cu },
    { 6, 0, 24, 88, 16, 16, { 0, 0 }, 0x00003d7cu },
    { 6, 0, 0, 183, 17, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 17, 183, 17, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 34, 183, 17, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 51, 183, 17, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 68, 183, 17, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 85, 183, 17, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 102, 183, 17, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 0, 201, 17, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 17, 201, 17, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 34, 201, 17, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 51, 201, 17, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 0, 277, 448, 2, { 0, 0 }, 0x00003cf4u },
    { 6, 0, 0, 129, 112, 18, { 0, 0 }, 0x00003c7cu },
    { 6, 0, 0, 147, 152, 18, { 0, 0 }, 0x00003c7cu },
    { 6, 0, 0, 165, 152, 18, { 0, 0 }, 0x00003c7cu },
    { 6, 0, 236, 143, 76, 65, { 0, 0 }, 0x00003cfcu },
    { 6, 0, 184, 0, 128, 143, { 0, 0 }, 0x00003cfcu },
    { 6, 0, 312, 0, 139, 23, { 0, 0 }, 0x00003d70u },
    { 6, 0, 312, 46, 60, 43, { 0, 0 }, 0x00003d70u },
    { 6, 0, 312, 89, 32, 20, { 0, 0 }, 0x00003d70u },
    { 6, 0, 312, 23, 139, 23, { 0, 0 }, 0x00003d7cu },
    { 6, 0, 372, 46, 60, 43, { 0, 0 }, 0x00003d7cu },
    { 6, 0, 344, 90, 32, 19, { 0, 0 }, 0x00003d7cu },
    { 6, 0, 96, 78, 16, 19, { 0, 0 }, 0x00003c74u },
    { 6, 0, 112, 78, 8, 19, { 0, 0 }, 0x00003c74u },
    { 6, 0, 120, 78, 16, 19, { 0, 0 }, 0x00003c74u },
    { 6, 0, 96, 40, 16, 30, { 0, 0 }, 0x00003c74u },
    { 6, 0, 112, 40, 8, 30, { 0, 0 }, 0x00003c74u },
    { 6, 0, 120, 40, 16, 30, { 0, 0 }, 0x00003c74u },
    { 6, 0, 136, 78, 16, 19, { 0, 0 }, 0x00003c74u },
    { 6, 0, 152, 78, 8, 19, { 0, 0 }, 0x00003c74u },
    { 6, 0, 160, 78, 16, 19, { 0, 0 }, 0x00003c74u },
    { 6, 0, 136, 70, 16, 8, { 0, 0 }, 0x00003c74u },
    { 6, 0, 160, 70, 16, 8, { 0, 0 }, 0x00003c74u },
    { 6, 0, 136, 40, 16, 30, { 0, 0 }, 0x00003c74u },
    { 6, 0, 152, 40, 8, 30, { 0, 0 }, 0x00003c74u },
    { 6, 0, 160, 40, 16, 30, { 0, 0 }, 0x00003c74u },
    { 6, 0, 390, 89, 14, 6, { 0, 0 }, 0x00003cf8u },
    { 6, 0, 404, 89, 28, 22, { 0, 0 }, 0x00003cf8u },
    { 6, 0, 0, 219, 151, 20, { 0, 0 }, 0x00003c74u },
    { 6, 0, 0, 239, 187, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 0, 258, 83, 19, { 0, 0 }, 0x00003c74u },
    { 6, 0, 83, 259, 18, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 101, 259, 18, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 152, 147, 23, 26, { 0, 0 }, 0x00003c74u },
    { 6, 0, 175, 147, 23, 26, { 0, 0 }, 0x00003c74u },
    { 6, 0, 198, 147, 23, 26, { 0, 0 }, 0x00003c74u },
    { 6, 0, 0, 279, 512, 80, { 0, 0 }, 0x00003d74u },
    { 6, 0, 0, 359, 512, 69, { 0, 0 }, 0x00003d7cu },
    { 6, 0, 136, 121, 16, 16, { 0, 0 }, 0x00003d7cu },
    { 6, 0, 152, 121, 8, 16, { 0, 0 }, 0x00003d7cu },
    { 6, 0, 160, 121, 16, 16, { 0, 0 }, 0x00003d7cu },
    { 6, 0, 136, 113, 16, 8, { 0, 0 }, 0x00003d7cu },
    { 6, 0, 160, 113, 16, 8, { 0, 0 }, 0x00003d7cu },
    { 6, 0, 136, 97, 16, 16, { 0, 0 }, 0x00003d7cu },
    { 6, 0, 152, 97, 8, 16, { 0, 0 }, 0x00003d7cu },
    { 6, 0, 160, 97, 16, 16, { 0, 0 }, 0x00003d7cu },
    { 6, 0, 119, 259, 374, 5, { 0, 0 }, 0x00003d7cu },
    { 6, 0, 0, 239, 17, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 96, 97, 16, 32, { 0, 0 }, 0x00003c74u },
    { 6, 0, 112, 97, 8, 32, { 0, 0 }, 0x00003c74u },
    { 6, 0, 120, 97, 16, 32, { 0, 0 }, 0x00003c74u },
    { 6, 0, 339, 200, 151, 20, { 0, 0 }, 0x00003c74u },
    { 6, 0, 256, 220, 18, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 274, 220, 18, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 292, 220, 18, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 310, 220, 18, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 328, 220, 18, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 346, 220, 18, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 364, 220, 18, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 382, 220, 18, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 400, 220, 18, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 418, 220, 18, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 436, 220, 18, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 454, 220, 18, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 472, 220, 18, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 256, 238, 18, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 274, 238, 18, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 292, 238, 18, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 310, 238, 18, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 328, 238, 18, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 346, 238, 18, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 364, 238, 18, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 382, 238, 18, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 400, 238, 18, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 418, 238, 18, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 436, 238, 18, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 454, 238, 18, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 472, 238, 18, 18, { 0, 0 }, 0x00003c74u },
    { 6, 0, 152, 16, 8, 22, { 0, 0 }, 0x00003d7cu },
    { 6, 0, 160, 16, 8, 22, { 0, 0 }, 0x00003d7cu },
    { 6, 0, 168, 16, 8, 22, { 0, 0 }, 0x00003d7cu },
    { 6, 0, 312, 109, 8, 22, { 0, 0 }, 0x00003d70u },
    { 6, 0, 320, 109, 8, 22, { 0, 0 }, 0x00003d70u },
    { 6, 0, 328, 109, 8, 22, { 0, 0 }, 0x00003d70u },
    { 6, 0, 176, 16, 8, 22, { 0, 0 }, 0x00003d70u },
    { 7, 0, 0, 32, 32, 32, { 0, 0 }, 0x00003c28u },
    { 7, 0, 32, 32, 32, 32, { 0, 0 }, 0x00003c28u },
    { 7, 0, 0, 0, 32, 32, { 0, 0 }, 0x00003c28u },
    { 7, 0, 32, 0, 32, 32, { 0, 0 }, 0x00003c28u },
    { 7, 0, 32, 128, 32, 32, { 0, 0 }, 0x00003c2cu },
    { 7, 0, 64, 128, 32, 32, { 0, 0 }, 0x00003c2cu },
    { 7, 0, 96, 128, 32, 32, { 0, 0 }, 0x00003c2cu },
    { 7, 0, 96, 96, 32, 32, { 0, 0 }, 0x00003c2cu },
    { 7, 0, 96, 64, 32, 32, { 0, 0 }, 0x00003c2cu },
    { 7, 0, 0, 128, 32, 32, { 0, 0 }, 0x00003c24u },
    { 7, 0, 0, 96, 32, 32, { 0, 0 }, 0x00003c24u },
    { 7, 0, 0, 64, 32, 32, { 0, 0 }, 0x00003c24u },
    { 7, 0, 32, 64, 32, 32, { 0, 0 }, 0x00003c24u },
    { 7, 0, 64, 64, 32, 32, { 0, 0 }, 0x00003c24u },
    { 7, 0, 64, 32, 32, 32, { 0, 0 }, 0x00003c20u },
    { 7, 0, 96, 32, 32, 32, { 0, 0 }, 0x00003c20u },
    { 7, 0, 64, 0, 32, 32, { 0, 0 }, 0x00003c20u },
    { 7, 0, 96, 0, 32, 32, { 0, 0 }, 0x00003c20u },
    { 7, 0, 0, 160, 32, 5, { 0, 0 }, 0x00003d24u },
    { 7, 0, 32, 160, 32, 5, { 0, 0 }, 0x00003d24u },
    { 7, 0, 64, 160, 32, 5, { 0, 0 }, 0x00003d24u },
    { 7, 0, 128, 64, 32, 32, { 0, 0 }, 0x00003c24u },
    { 7, 0, 32, 96, 32, 32, { 0, 0 }, 0x00003c24u },
    { 7, 0, 128, 96, 32, 32, { 0, 0 }, 0x00003c24u },
    { 7, 0, 64, 96, 32, 32, { 0, 0 }, 0x00003c2cu },
    { 7, 0, 0, 165, 8, 11, { 0, 0 }, 0x00003d2cu },
    { 7, 0, 0, 176, 8, 11, { 0, 0 }, 0x00003d2cu },
    { 7, 0, 138, 154, 7, 11, { 0, 0 }, 0x00003d2cu },
    { 7, 0, 138, 143, 6, 11, { 0, 0 }, 0x00003d2cu },
    { 7, 0, 0, 187, 117, 19, { 0, 0 }, 0x00003c30u },
    { 8, 0, 0, 0, 512, 164, { 0, 0 }, 0x00003e88u },
    { 8, 0, 0, 164, 512, 164, { 0, 0 }, 0x00003e8cu },
    { 8, 0, 0, 328, 12, 5, { 0, 0 }, 0x00003e98u },
    { 9, 0, 0, 0, 512, 288, { 0, 0 }, 0x00003fffu },
    { 9, 0, 0, 288, 512, 288, { 0, 0 }, 0x00003fffu },
    { 9, 0, 230, 992, 20, 18, { 0, 0 }, 0x00003fffu },
    { 9, 0, 190, 992, 20, 18, { 0, 0 }, 0x00003fffu },
    { 9, 0, 250, 992, 20, 18, { 0, 0 }, 0x00003fffu },
    { 9, 0, 210, 992, 20, 18, { 0, 0 }, 0x00003fffu },
    { 9, 0, 270, 992, 20, 18, { 0, 0 }, 0x00003fffu },
    { 9, 0, 0, 992, 38, 22, { 0, 0 }, 0x00003fffu },
    { 9, 0, 38, 992, 38, 22, { 0, 0 }, 0x00003fffu },
    { 9, 0, 76, 992, 38, 22, { 0, 0 }, 0x00003fffu },
    { 9, 0, 114, 992, 38, 22, { 0, 0 }, 0x00003fffu },
    { 9, 0, 152, 992, 38, 22, { 0, 0 }, 0x00003fffu },
    { 9, 0, 0, 576, 512, 288, { 0, 0 }, 0x00003fffu },
    { 9, 0, 0, 864, 64, 128, { 0, 0 }, 0x00003fffu },
    { 9, 0, 64, 864, 64, 128, { 0, 0 }, 0x00003fffu },
    { 10, 0, 0, 80, 184, 22, { 0, 0 }, 0x00003e0cu },
    { 10, 0, 118, 0, 118, 22, { 0, 0 }, 0x00003e18u },
    { 10, 0, 0, 22, 118, 22, { 0, 0 }, 0x00003e18u },
    { 10, 0, 118, 22, 118, 22, { 0, 0 }, 0x00003e18u },
    { 10, 0, 0, 0, 118, 22, { 0, 0 }, 0x00003e18u },
    { 10, 0, 0, 102, 76, 22, { 0, 0 }, 0x00003e1cu },
    { 10, 0, 76, 102, 76, 22, { 0, 0 }, 0x00003e0cu },
    { 10, 0, 0, 124, 116, 18, { 0, 0 }, 0x00003e0cu },
    { 10, 0, 184, 80, 47, 20, { 0, 0 }, 0x00003e18u },
    { 10, 0, 160, 102, 30, 34, { 0, 0 }, 0x00003e1cu },
    { 10, 0, 0, 44, 18, 18, { 0, 0 }, 0x00003e08u },
    { 10, 0, 18, 44, 18, 18, { 0, 0 }, 0x00003e08u },
    { 10, 0, 36, 44, 18, 18, { 0, 0 }, 0x00003e08u },
    { 10, 0, 54, 44, 18, 18, { 0, 0 }, 0x00003e08u },
    { 10, 0, 72, 44, 18, 18, { 0, 0 }, 0x00003e08u },
    { 10, 0, 90, 44, 18, 18, { 0, 0 }, 0x00003e08u },
    { 10, 0, 108, 44, 18, 18, { 0, 0 }, 0x00003e08u },
    { 10, 0, 126, 44, 18, 18, { 0, 0 }, 0x00003e08u },
    { 10, 0, 144, 44, 18, 18, { 0, 0 }, 0x00003e08u },
    { 10, 0, 162, 44, 18, 18, { 0, 0 }, 0x00003e08u },
    { 10, 0, 180, 44, 18, 18, { 0, 0 }, 0x00003e08u },
    { 10, 0, 198, 44, 18, 18, { 0, 0 }, 0x00003e08u },
    { 10, 0, 216, 44, 18, 18, { 0, 0 }, 0x00003e08u },
    { 10, 0, 0, 62, 18, 18, { 0, 0 }, 0x00003e08u },
    { 10, 0, 18, 62, 18, 18, { 0, 0 }, 0x00003e08u },
    { 10, 0, 36, 62, 18, 18, { 0, 0 }, 0x00003e08u },
    { 10, 0, 54, 62, 18, 18, { 0, 0 }, 0x00003e08u },
    { 10, 0, 72, 62, 18, 18, { 0, 0 }, 0x00003e08u },
    { 10, 0, 90, 62, 18, 18, { 0, 0 }, 0x00003e08u },
    { 10, 0, 108, 62, 18, 18, { 0, 0 }, 0x00003e08u },
    { 10, 0, 126, 62, 18, 18, { 0, 0 }, 0x00003e08u },
    { 10, 0, 144, 62, 18, 18, { 0, 0 }, 0x00003e08u },
    { 10, 0, 162, 62, 18, 18, { 0, 0 }, 0x00003e08u },
    { 10, 0, 180, 62, 18, 18, { 0, 0 }, 0x00003e08u },
    { 10, 0, 198, 62, 18, 18, { 0, 0 }, 0x00003e08u },
    { 10, 0, 216, 62, 18, 18, { 0, 0 }, 0x00003e08u },
    { 10, 0, 152, 102, 8, 22, { 0, 0 }, 0x00003e18u },
};

/* String data of text_box.c (rodata) read by the script table below. */
