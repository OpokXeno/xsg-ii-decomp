#include "common.h"
#include "main/tu092.h"
#include "main/tu096.h"
#include "main/tu097.h"
#include "main/tu098.h"
#include "gs.h"

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

/* 1 when the current pair of a track equals its target pair. */
int fontTrackAtTarget(struct FontTrack *track) {
    int at_target = 0;

    if (track->current_pair.x == track->target_pair.x) {
        at_target = track->current_pair.y == track->target_pair.y;
    }
    return at_target;
}

extern int fontObjBoundSlot;
extern int fontObjDepth;
extern int fontObjOpacity;
extern signed char fontObjSlotTexture[FONT_SLOT_COUNT];
extern signed char fontObjSlotState[FONT_SLOT_COUNT];
extern int fontObjRunLists[];
extern u64 fontObjColor;

void fontObjInitGlobals(void)
{
    int *run_list;
    int slot;

    fontObjBoundSlot = -1;
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
extern unsigned char D_003EAE40[];
int xglCdSectorAlloc();
int xglCdBufferReserve(long);
int xglCdReadFileAsync();

int fontObjLoadTexture(int texture_index)
{
    int slot;
    int *run_list_word;

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
    if (xglCdReadFileAsync(D_003EAE40 + texture_index * 24, fontObjRunLists[slot], 1, (unsigned char) slot) >= 0) {
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

void xglDirectQueueBegin(void);
void xglDirectQueuePush(int, u64);
void xglDirectQueueSubmitRef(void);

/* Brackets one TEXFLUSH (GS register 0x3F) write with the GS begin and end helpers. */
void fontObjTexFlush(void) {
    xglDirectQueueBegin();
    xglDirectQueuePush(GS_TEXFLUSH, 0);
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
void *xglPacketGetCurrent(void);
void sceVif1PkCnt(void *packet, int count);
void sceVif1PkAddDataN(void *packet, void *data, int count);
void sceVif1PkRef(void *packet, void *data, int count, int a, int b, int c);
int fontObjLoadTexture(int texture_index);

int fontObjBuildVifPacket(int texture_index)
{
    void *packet;
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
    run_list = (struct FontRunList *)fontObjRunLists[run_slot];
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
    fontObjBoundSlot = texture_index;
    fontObjTexFlush();
    return 1;
}

int fontObjBuildBaseVifPacket(void)
{
    void *packet;
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
    run_list = (struct FontRunList *)fontObjRunLists[run_slot];
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
    fontObjBoundSlot = -1;
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

extern signed char D_0069E203[5];

void fontObjResetBufferSlots(void)
{
    fontObjRunLists[3] = fontObjRunLists[4] = 0;
    D_0069E203[0] = D_0069E203[1] = -1;
}

/* Zeroes the six run-list words of fontObjRunLists, from the last slot down. */
int *fontObjClearRunLists(void)
{
    int *run_lists;
    int *cursor;
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
    unsigned int uv;
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
    } else if (fontObjBoundSlot != cell->kind) {
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

    xglDirectQueueBegin();
    if (item->flags & 0x20000) {
        depth_stripe = 0x8080;
        xglDirectQueuePush(GS_TEST_1, 0x50000);
        xglDirectQueuePush(GS_ZBUF_1, sRenderDepthBase[0] | ((unsigned long long)depth_stripe << 17));
    } else {
        xglDirectQueuePush(GS_TEST_1, 0x30000);
        xglDirectQueuePush(GS_ZBUF_1, sRenderDepthBase[0] | 0x1000000);
    }

    if (cell->kind == 0) {
        if (item->flags & 0x40) {
            xglDirectQueuePush(GS_TEX0_1, ((unsigned long long)cell->uv << 37) | 0x2000000665423800ULL);
        } else {
            xglDirectQueuePush(GS_TEX0_1, ((unsigned long long)cell->uv << 37) | 0x2000000665323800ULL);
        }
    } else if (item->flags & 0x40) {
        xglDirectQueuePush(GS_TEX0_1, ((unsigned long long)cell->uv << 37) | 0x2000000665423C00ULL);
    } else if (item->flags & 0x10000) {
        xglDirectQueuePush(GS_TEX0_1, ((unsigned long long)cell->uv << 37) | 0x2000000621343C00ULL);
    } else {
        xglDirectQueuePush(GS_TEX0_1, ((unsigned long long)cell->uv << 37) | 0x2000000665323C00ULL);
    }

    if (item->flags & 0x8000) {
        if (item->flags & FONT_DRAW_OFFSET) {
            unsigned long long low;
            unsigned long long high;

            low = ((unsigned long long)(cell->w - 1) << 4)
                | ((unsigned long long)(cell->x + item->offset_x) << 34);
            high = ((unsigned long long)(cell->x + item->offset_x) << 14) | 0xF;
            xglDirectQueuePush(8, (low | high) | ((unsigned long long)(cell->h - 1) << 24));
        } else {
            unsigned long long low;
            unsigned long long high;

            low = ((unsigned long long)(cell->w - 1) << 4)
                | ((unsigned long long)cell->y << 34);
            high = ((unsigned long long)cell->x << 14) | 0xF;
            xglDirectQueuePush(8, (low | high) | ((unsigned long long)(cell->h - 1) << 24));
        }
    } else if (item->flags & FONT_DRAW_OFFSET) {
        unsigned long long low;
        unsigned long long high;

        low = ((unsigned long long)(cell->x + item->offset_x) << 4)
            | ((unsigned long long)(cell->y + cell->h + item->offset_y) << 34);
        high = ((unsigned long long)(cell->x + cell->w + item->offset_x) << 14) | 0xA;
        xglDirectQueuePush(8, (low | high) | ((unsigned long long)(cell->y + item->offset_y) << 24));
    } else {
        unsigned long long low;
        unsigned long long high;

        low = ((unsigned long long)cell->x << 4)
            | ((unsigned long long)(cell->y + cell->h) << 34);
        high = ((unsigned long long)(cell->x + cell->w) << 14) | 0xA;
        xglDirectQueuePush(8, (low | high) | ((unsigned long long)cell->y << 24));
    }

    if (item->flags & 0x10) {
        xglDirectQueuePush(GS_TEX1_1, 0);
    } else {
        xglDirectQueuePush(GS_TEX1_1, 0x60);
    }
    if (item->flags & 1) {
        xglDirectQueuePush(GS_ALPHA_1, ((unsigned long long)0x80 << 32) | 0x48);
    } else if (item->flags & 2) {
        xglDirectQueuePush(GS_ALPHA_1, ((unsigned long long)0x80 << 32) | 0x42);
    } else {
        xglDirectQueuePush(GS_ALPHA_1, ((unsigned long long)0x80 << 32) | 0x44);
    }

    if (item->flags & FONT_DRAW_COLOR_ALPHA) {
        alpha = (((unsigned long long)((unsigned char *)&fontObjColor)[3]
                  * (unsigned long long)(unsigned char)fontObjOpacity) >> 7) & 0xFF;
        xglDirectQueuePush(GS_RGBAQ, (fontObjColor & 0xFFFFFF) | (alpha << 24));
    } else {
        xglDirectQueuePush(GS_RGBAQ, ((unsigned long long)(unsigned char)fontObjOpacity << 24) | 0x3F80000000808080ULL);
    }

    if (item->flags & 0x80) {
        xglDirectQueuePush(GS_PRIM, 0x151);
        xglDirectQueuePush(3, (unsigned long long)x0 | ((unsigned long long)y0 << 16));
        xglDirectQueuePush(5, ((unsigned long long)((item->x << 4) + FONT_VERTEX_BIAS_X))
            | ((unsigned long long)(((item->y + draw_h) << 4) + FONT_VERTEX_BIAS_Y) << 16)
            | ((unsigned long long)fontObjDepth << 32));
        xglDirectQueuePush(3, (unsigned long long)x1 | ((unsigned long long)y1 << 16));
        xglDirectQueuePush(5, ((unsigned long long)((item->x << 4) + FONT_VERTEX_BIAS_X))
            | ((unsigned long long)((item->y << 4) + FONT_VERTEX_BIAS_Y) << 16)
            | ((unsigned long long)fontObjDepth << 32));
    } else {
        xglDirectQueuePush(GS_PRIM, 0x156);
        xglDirectQueuePush(3, (unsigned long long)x0 | ((unsigned long long)y0 << 16));
        xglDirectQueuePush(5, ((unsigned long long)((ix0 << 4) + FONT_VERTEX_BIAS_X))
            | ((unsigned long long)((iy0 << 4) + FONT_VERTEX_BIAS_Y) << 16)
            | ((unsigned long long)fontObjDepth << 32));
        xglDirectQueuePush(3, (unsigned long long)x1 | ((unsigned long long)y1 << 16));
        xglDirectQueuePush(5, ((unsigned long long)((ix1 << 4) + FONT_VERTEX_BIAS_X))
            | ((unsigned long long)((iy1 << 4) + FONT_VERTEX_BIAS_Y) << 16)
            | ((unsigned long long)fontObjDepth << 32));
    }
    xglDirectQueueSubmitRef();
}

extern u16 D_003E7EE6[];

/* Cell-table fields: the u16 at +6 (and +8 in fontObjCellOffsetY) of 16-byte entry index. */
u16 fontObjCellOffsetX(int index) {
    return D_003E7EE6[index * 8];
}

extern u16 D_003E7EE8[];

u16 fontObjCellOffsetY(int index) {
    return D_003E7EE8[index * 8];
}

void fontObjSetColor(u64 colour) {
    fontObjColor = colour;
}

void fontObjSetColorRgba(int r, int g, int b, int a)
{
    fontObjColor = (long long) r | ((long long) g << 8) | ((long long) b << 16) | ((long long) a << 24) | 0x3F80000000000000LL;
}

void fontObjSetAlpha(int alpha) {
    fontObjColor = (fontObjColor & 0xFFFFFF) | (((long long) alpha & 0xFF) << 0x18);
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

void xglDirectQueueDefer(int, int);

/* Queues one textured box quad: a packet header, the alpha-selected colour word, then the four
 * corner words with the draw bits in the high half, and defers the queue. */
void fontObjDrawQuad(int x0, int y0, int x1, int y1, int rgba_bits, int z_bits)
{
    unsigned int rgba = (unsigned int)rgba_bits;
    unsigned int alpha = rgba >> 24;
    u64 zWord = (u64)z_bits << 32;
    xglDirectQueueBegin();
    xglDirectQueuePush(GS_TEST_1, 0x50000);
    if (alpha < 0x80) {
        xglDirectQueuePush(GS_ZBUF_1, sRenderDepthBase[0] | (0x8080ULL << 17));
        {
            u64 colour = (u8)rgba | (0xFE00ULL << 46);
            u64 colourHi = (u64)(u8)(rgba >> 16) << 16;

            colourHi |= 0x8000ULL << 16;
            colour |= colourHi;
            colour |= (u64)(u8)(rgba >> 8) << 8;
            xglDirectQueuePush(GS_RGBAQ, colour);
        }
        xglDirectQueuePush(GS_ALPHA_1, ((u64)alpha << 32) | 0x64);
        xglDirectQueuePush(GS_PRIM, 0x46);
    } else {
        xglDirectQueuePush(GS_ZBUF_1, sRenderDepthBase[0] | 0x1000000);
        {
            u64 colour = (u8)rgba;

            colour |= 0xFE00ULL << 46;
            colour |= ((u64)(u8)(rgba >> 16) << 16) | ((u64)(u8)(rgba >> 8) << 8);
            colour |= (u64)(u8)alpha << 24;
            xglDirectQueuePush(GS_RGBAQ, colour);
        }
        xglDirectQueuePush(GS_PRIM, 6);
    }
    xglDirectQueuePush(GS_XYZF2, (u64)((x0 << 4) + FONT_VERTEX_BIAS_X) | ((u64)((y0 << 4) + 0x71C0) << 16) | zWord);
    xglDirectQueuePush(GS_XYZF2, (u64)((x1 << 4) + FONT_VERTEX_BIAS_X) | ((u64)((y1 << 4) + 0x71C0) << 16) | zWord);
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

/* The cursor record of a text block or of a cursor buffer entry: the text pointer at +0x10 is
 * stored by fontObjSetText and cursorBufferSetText. */
/* local view: unit_size and text are pointers here (text at 0x10 is mode_flags in the shared CursorBuffer of tu098.h). */
struct FontBlockCursor {
    int cursor;
    int previous_cursor;
    int capacity;
    unsigned char *unit_size;
    unsigned char *text;
    int status_flags;
    int count;
    int state;
    int aux_value;
    unsigned char mode;
    unsigned char step;
};

/* One 0xD4-byte text block (a slot of tu099's TextBoxSlot). flags is zeroed by
 * fontObjInitBlock, as are the four corner words; the cursor record sits at +0x30 and the sub-object, a FontObject, at +0x58. */
typedef struct FontTextBlock {
    int origin_x;
    int origin_y;
    unsigned char unmodeled_08[8];
    int text;
    unsigned char unmodeled_14[8];
    unsigned int flags;
    int corners[4];
    struct FontBlockCursor cursor;
    struct FontObject sub_object;
} FontTextBlock;

union FontCursorCounter {
    unsigned char bytes[4];
    unsigned int packed;
};

struct FontCursorEntry {
    int origin_x;
    int origin_y;
    unsigned char unmodeled_08[4];
    int unmodeled_0c;
    int text;
    unsigned char unmodeled_14[8];
    unsigned int flags;
    int corners[4];
    union FontCursorCounter counter;
    struct FontBlockCursor record;
    struct FontObject sub_object;
};

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

void fontObjSetExtra(struct FontObject *obj, int draw_bits) {
    obj->draw_bits = draw_bits;
}

/* Sets the target pair of track 1 (the size, at +0x1C, see fontObjSetSize) of the object. */
void fontObjSetSizeTarget(struct FontObject *obj, int x, int y) {
    fontTrackSetTarget(&obj->tracks[1], x, y);
}

/* Returns the third track when bit 0x20000 of flags is set, else the object itself. */
void *fontObjGetOriginTrack(struct FontObject *object) {
    if (object->flags & FONT_OBJ_PHASE_RUNNING) {
        return &object->tracks[2];
    }
    return object;
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
        block->origin_x = origin_x;
        block->origin_y = origin_y;
        block->text = text;
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
        goto done;
    }
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
                goto done;
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
                    goto done;
                }
                if (layout->flags & 0x20000) {
                    pos++;
                    goto done;
                }
                if (waitCode == 1) {
                    pos++;
                    layout->flags |= 0x20000;
                    goto done;
                }
                layout->limit = wrapLimit;
                layout->pending = layout->reset;
                if (*pos == 10) {
                    layout->newlines++;
                }
                pos++;
                goto done;
            }
        }
        pos++;
    reload:
        waitCode = 0;
        nextChar = *pos;
        ch = nextChar;
    } while (nextChar != 0);
    layout->flags |= 0x10000;
done:
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



extern unsigned char D_003ED060[];



/* Initialises a text block: origin at (0, 0), a cleared cursor record in mode 1, and the sub-object
 * placed at (16, 352) with the size (480, 80). */
void fontObjInitBlock(FontTextBlock *block)
{
    struct FontObject *subObject = &block->sub_object;

    if (block) {
        fontTrackSetBoth((struct FontTrack *)block, 0, 0);
        fontObjResetCursor(&block->cursor);
        block->cursor.unit_size = D_003ED060;
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

void func_0020E950(void) {
}

void xglFontPrintExtFunc(int handle, void (*fn)(void), int *corners);
void grAddScissorPacket(void);
void xglFontScissorReset(void);

/* Draws one text block: advances the sub-object and its track, draws its box when active, then prints
 * its line through the ext handle (sub-object draw_bits) with the scissor packets around it. */
int fontObjDrawText(FontTextBlock *block)
{
    int offsetX;
    int offsetY;

    if (block != 0) {
        fontObjSubUpdate(&block->sub_object);
        fontTrackEase((struct FontTrack *)block);
        fontObjDrawIfActive(&block->sub_object);
        if (block->sub_object.move_phase != FONT_PHASE_OPENING) {
            offsetX = block->sub_object.tracks[0].current_pair.x + block->origin_x;
            offsetY = block->sub_object.tracks[0].current_pair.y + block->origin_y;
            fontObjGetQuadCorners(&block->sub_object, block->corners);
            block->corners[1] -= 4;
            block->corners[3] -= 4;
            xglFontPrintExtFunc(block->sub_object.draw_bits, grAddScissorPacket, block->corners);
            fontObjLayoutAtOffset(offsetX, offsetY, block->sub_object.draw_bits + 1, (struct FontPoint *)&block->cursor);
            xglFontPrintExtFunc(block->sub_object.draw_bits, xglFontScissorReset, 0);
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
        fontTrackSetBoth((struct FontTrack *)entry, 0, 0);
        fontObjResetCursor(&entry->record);
        entry->record.unit_size = D_003ED060;
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
        fontObjSetExtra(subObject, 0x00fffff0);
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

void func_0020EBF0(void) {
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
            entry->unmodeled_0c = (unsigned int)~(auxValue - limit) * entry->counter.bytes[2];
        }
    }
    remaining = pendingCount;
    if ((signed char)remaining > 0) {
        stepSize = entry->counter.bytes[2];
        offset = entry->unmodeled_0c;
        do {
            nextCount = remaining - 1;
            offset -= stepSize;
            remaining = nextCount;
        } while ((signed char)nextCount > 0);
        entry->counter.bytes[0] = remaining;
        entry->unmodeled_0c = offset;
    }
    fontObjSubUpdate(&entry->sub_object);
    fontTrackEase((struct FontTrack *)entry);
    fontObjDrawIfActive(&entry->sub_object);
    if (entry->sub_object.move_phase != FONT_PHASE_OPENING) {
        offsetX = entry->sub_object.tracks[0].current_pair.x + entry->origin_x;
        offsetY = entry->sub_object.tracks[0].current_pair.y + entry->origin_y;
        fontObjGetQuadCorners(&entry->sub_object, entry->corners);
        entry->corners[1] -= 4;
        entry->corners[3] -= 4;
        xglFontPrintExtFunc(entry->sub_object.draw_bits, grAddScissorPacket, entry->corners);
        fontObjLayoutAtOffset(offsetX, offsetY, entry->sub_object.draw_bits + 1, (struct FontPoint *)&entry->record);
        xglFontPrintExtFunc(entry->sub_object.draw_bits, xglFontScissorReset, 0);
    }
    if ((entry->sub_object.flags & FONT_OBJ_ACTIVE) == 0) {
        entry->flags &= ~CURSOR_FLAG_ACTIVE;
    }
}
