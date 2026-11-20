#include "common.h"
#include "main/save_screen.h"
#include "shared.h"
#include "main/menu_scene.h"
#include "ov02/title.h"
#include "main/ysegf.h"
#include "main/msg_balance.h"
#include "main/xgl_flags.h"
#include "main/xgl_cd.h"
#include "main/se_bank_load.h"
#include "main/sound_driver.h"
#include "main/pad.h"
#include "main/xgl_camera.h"
#include "main/TiCameraSolver.h"
#include "main/xenoActor.h"
#include "gs.h"
#include "main/xgl_2.h"
#include "main/xgl_mc_request.h"
#include "main/game_data.h"
#include "main/gr_screen.h"
#include "main/xgl_packet.h"
#include "main/xgl_thread.h"
#include "main/nml_model_set.h"
#include "main/xgl_light.h"
#include "main/xgl_hdd.h"
#include "main/control_entry.h"
#include "main/xgl_font.h"
#include "ov02/title_hdd_install.h"
#include "ov02/title_game_init.h"
#include "ov02/logo.h"
#include "main/xgl_render.h"
#include "main/xgl_studio.h"
#include "main/get.h"
#include "main/save_party_init.h"

/*
 * titleRunScene zeroes the actor's position and rotation through float arrays, an alias-0 access that keeps the
 * zeroing stores ahead of the flags store. Making them unions in struct XenoActor (main/xenoActor.h) would change
 * main's code, so this one site reaches them through a view that starts like the actor.
 */

typedef struct TitleActorAxesView {
    unsigned char unmodeled_000[0x10];
    AlignedVector4Elements position;                /* 0x010 */
    unsigned char unmodeled_020[0x30];
    AlignedVector4Elements rotation;                /* 0x050 */
} TitleActorAxesView;


/* EE scratchpad RAM. */
#define SCRATCHPAD_BASE 0x70000000

/* Title texture sizes: the object texture is sent in two halves of TITLE_TEXTURE_QWC quadwords. */
#define TITLE_TEXTURE_QWC 0x7000
#define TITLE_TEXTURE_HALF_SIZE (TITLE_TEXTURE_QWC * 16)

/* The fade level (titleState.fade_level) starts at TITLE_FADE_MAX and drops TITLE_FADE_STEP per frame. */
#define TITLE_FADE_MAX 128
#define TITLE_FADE_STEP 4

/* Frames a press is ignored after startup or a cursor move. */
#define TITLE_PREROLL_FRAMES 4

/* Title sound ids: the cursor move and the accept/start press. */
enum {
    TITLE_SE_CURSOR_MOVE = 0x1000001,
    TITLE_SE_ACCEPT = 0x1000002
};


/* Screen height in lines (copyframe's source rectangle). */
#define TITLE_SCREEN_HEIGHT 448

/* A 16-byte slot of three floats: the title light and camera vectors. */
typedef struct TitleVec3Slot {
    float x;
    float y;
    float z;
    unsigned char unmodeled_0c[0x04];
} TitleVec3Slot;

/* The title table block at titleTable: a ready flag, the list index, a count, the table pointer, the record in use and its frame counter. */
typedef struct TitleTableState {
    unsigned char table_ready;
    unsigned char list_index;
    unsigned char unmodeled_02;
    unsigned char table_count;
    union {
        u32 *words;              /* as read: word 0 the size, then the offsets titleTableReadNotify relocates */
        void **entries;          /* once relocated: the record and resource pointers */
    } table;
    TiCameraRecord *record;
    short frame;
    short frame_end;
    /* 0x10: the light set of the title scene (0x180 bytes, up to the end of the block). The code fills it row by row:
     * rows 0-6 (the directions and the view direction) from the rotated titleLightDirection, rows 7-14 (the colors
     * and both ambient colors) copied from titleLightVectors; the shadow and point light rows stay as they are. */
    union {
        XglLightSet set;
        XglFVector rows[24];
    } lights;
} TitleTableState;
/* titleTable.table_ready: where the title scene is in its table. */
enum {
    TITLE_TABLE_IDLE = 0,
    TITLE_TABLE_READY = 1,        /* enough records are loaded for the scene to start */
    TITLE_TABLE_BEGIN_LIST = 2,   /* picks the list at list_index and places its actors */
    TITLE_TABLE_PLAYING = 3       /* steps the frames of the current record */
};
/* table_count holds this value until the offsets of the table are relocated into pointers. */
#define TITLE_TABLE_UNRELOCATED 255
/* Each title list (titleTableLists, and the entries after its two header bytes) ends with this byte. */
#define TITLE_LIST_END 0xFF
/* Records the table must hold before the scene starts; bytes read before the offsets are checked. */
#define TITLE_TABLE_MIN_RECORDS 13
#define TITLE_TABLE_MIN_READ 2048
extern TitleVec3Slot titleLightVectors[];

/* The title state block at titleState: the fade level halfword sits at +0x10. */
typedef struct TitleState {
    u8 *texture;
    u8 *object_buffer;           /* the title object file, read first at 0x01000000: also the texture transfer set */
    int selected_group;
    int hdd_stage;               /* 0x0C: the HDD mount stage from xglHddMount (-1 none) */
    s16 fade_level;
    s16 sprite_fade;
    s16 field_14;
    unsigned char timer;
    unsigned char unmodeled_17[0x09];
    signed char group_slot[4];
    unsigned char unmodeled_24[0x0c];
    Vector4 pan;                 /* 0x30: the camera pan the title scrolls by */
} TitleState;
/* Title state storage: the static block of this TU (titleState, titleTable and the two gaps around them). */
static unsigned char titleStateLeadGap[8];
static TitleState titleState;
static TitleTableState titleTable;


/* titleFadePacket: u64 words (gs_tag = word 6); the three fade ints sit at byte 0x40 (word 8). */
extern u64 titleFadePacket[];

/* Read-only data of this TU, in address order (before the functions whose switch tables follow it). */
const char titleMusicPath[16] = "yajima\\M-01.adx";
const char titleMusicKeyFormat[16] = "%08x.%08x";
const char titleObjectTexturePath[24] = "yajima\\titleobj.xtx";
const char titleDisc1Path[24] = "yajima\\title.bin";
const char titleDisc2Path[24] = "yajima\\title2.bin";
const char titleBg2Path[24] = "yajima\\titlebg2.bin";

int titleGroupMinus2Dummy(void) {
    return 0;
}

void copyframe(void)
{
    XglPacket *packet;
    u64 *scratch;

    packet = xglPacketGetCurrent();
    scratch = (u64 *)SCRATCHPAD_BASE;
    scratch[0] = 0;
    scratch[1] = 0x51000009;
    scratch[2] = ((u64)0x10000000 << 32) | 0x8008;
    scratch[3] = GIF_AD;
    scratch[4] = (0x80000 | (sRender.vram.width << 5)) |
                 ((u64)sRender.vram.height << 37) | ((u64)0x8000 << 36);
    scratch[5] = GS_BITBLTBUF;
    scratch[6] = 0;
    scratch[7] = GS_TRXPOS;
    scratch[8] = ((u64)TITLE_SCREEN_HEIGHT << 32) | 0x200;
    scratch[9] = GS_TRXREG;
    scratch[10] = 2;
    scratch[11] = GS_TRXDIR;
    scratch[12] = 0;
    scratch[13] = GS_TEXFLUSH;
    scratch[14] = (0x80000 | (sRender.vram.width << 5)) |
                  ((u64)sRender.vram.flip_base << 37) | ((u64)0x8000 << 36);
    scratch[15] = GS_BITBLTBUF;
    scratch[16] = 2;
    scratch[17] = GS_TRXDIR;
    scratch[18] = 0;
    scratch[19] = GS_TEXFLUSH;
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectHLCode(packet, 0);
    sceVif1PkAddDirectDataN(packet, scratch, 10);
    sceVif1PkCloseDirectHLCode(packet);
}

void titleFrameHookDummy(void) {

}

extern const char titleMusicPath[];
extern const char titleMusicKeyFormat[];
int sprintf(char *destination, const char *format, ...);

/* Starts the title music: opens titleMusicPath and starts it in sound slot 0. */
void titleStartBgm(void)
{
    char key[64];
    CdFilePosition position;

    xglCdGetFileData(titleMusicPath, &position);
    sprintf(key, titleMusicKeyFormat, position.lbn,
            (unsigned int) position.stored_bytes >> CD_SECTOR_SHIFT);
    soundSlotSetVolume(0, -1, 0);
    soundSlotStartFile(0, key);
}

/*
 * CdCompletionCallback of the titleBg2Path read: event 3 relocates the table offsets once and marks the table
 * ready after TITLE_TABLE_MIN_RECORDS records; event 4 resets table_count.
 */
void titleTableReadNotify(int event, int size, int tag)
{
    u32 *table;
    u32 *word;
    u32 base;
    u32 used_end;
    int count;
    u32 read_end;

    switch (event) {
    case 3:
        if (size < TITLE_TABLE_MIN_READ) {
            return;
        }
        if (titleTable.table_count == TITLE_TABLE_UNRELOCATED) {
            table = titleTable.table.words;
            base = table[0];
            used_end = (u32) table + base;
            base = (u32) table + ((base + 63) & -64);
            table[0] = base;
            while ((u32) ++table < used_end) {
                *table += base;
            }
            titleTable.table_count = 0;
        }
        word = titleTable.table.words;
        read_end = (u32) word + size;
        count = 0;
        word++;
        for (;;) {
            if (*word > read_end) {
                break;
            }
            word++;
            count++;
        }
        titleTable.table_count = count;
        if (titleTable.table_ready != 0 || count < TITLE_TABLE_MIN_RECORDS) {
            return;
        }
        titleTable.table_ready = TITLE_TABLE_READY;
        break;
    case 4:
        titleTable.table_count = TITLE_TABLE_UNRELOCATED;
        break;
    }
}

/* One 24-byte layout record: a position triple, then the rotation triple (titleActorPlacements[0] + 0xC). */
typedef struct TitleLayoutRecord {
    float position[3];
    float rotation[3];
} TitleLayoutRecord;
extern TitleLayoutRecord titleActorPlacements[];

/* The camera angle pairs (x, y) of the seven light rounds. */
extern float titleLightAngles[][2];

extern u8 *titleTableLists[];
extern Vector4 titleLightDirection;
extern Vector4 titleFogColor;
extern float titleFogDistances[4];
extern int padDebugCount;


/*
 * Title scene step, called every frame by Title: BEGIN_LIST places the actors and light rows of the current list,
 * PLAYING steps the camera and the actors through the current record, then the debug-code check runs.
 */
void titleRunScene(void)
{
    struct XenoActor *actor;
    u8 *cur;
    u8 *entries;
    void **table;
    u32 head;
    u32 raw;
    u32 key;
    u32 pos;
    int n;
    int light_round;
    int prev_count;
    int new_count;
    float rate;
    Vector4 tmp;
    Matrix4 saved;
    u32 entry;
    XglCamera *camera;

    switch (titleTable.table_ready) {
    case TITLE_TABLE_READY:
        titleTable.table_ready = TITLE_TABLE_BEGIN_LIST;
        titleTable.list_index = 0;
    case TITLE_TABLE_BEGIN_LIST:
        cur = titleTableLists[titleTable.list_index];
        table = titleTable.table.entries;
        while (cur == 0 || titleTable.table_count < cur[1]) {
            titleStartBgm();
            titleTable.list_index = 0;
            cur = titleTableLists[0];
        }
        entries = cur + 2;
        head = cur[0];
        while (*cur++ != TITLE_LIST_END) {
        }
        XenoActorPoolInitDefault();
        for (n = 0; entries[n] != TITLE_LIST_END; n++) {
        }
        titleTable.record = table[2 * n + head];
        titleTable.frame = titleTable.record->frameStart;
        titleTable.frame_end = titleTable.record->frameEnd;

        while (1) {
            actor = XenoActorAlloc();
            ((TitleActorAxesView *) actor)->position.component[0] = ((TitleActorAxesView *) actor)->position.component[1] = ((TitleActorAxesView *) actor)->position.component[2] = 0.0f;
            ((TitleActorAxesView *) actor)->rotation.component[0] = ((TitleActorAxesView *) actor)->rotation.component[1] = ((TitleActorAxesView *) actor)->rotation.component[2] = 0.0f;
            actor->drawFlags |= 1;
            key = *cur++;
            raw = key;
            if (raw == 0xFF) {
                key = 0;
            }
            if (raw == 0xFA) {
                key = 19;
            }
            actor->rsrc[XAct_Rsrc_MDL] = (int) table[key];
            actor->rsrc[XAct_Rsrc_TEX] = (int) table[key + 1];
            if (key == 0) {
                break;
            }
            actor->rsrc[XAct_Rsrc_XHR] = (int) table[key + 2];
            for (pos = 0; (entry = entries[pos]) != TITLE_LIST_END; pos++) {
                if (entry == raw) {
                    key = (head + 2 * pos) & 0xFF;
                    break;
                }
            }
            actor->rsrc[XAct_Rsrc_3] = (int) table[key];
            actor->rsrc[XAct_Rsrc_SlotEntry] = (int) table[key + 1];
            key = *cur;
            ((TitleActorAxesView *) actor)->position.component[0] = titleActorPlacements[key].position[0];
            ((TitleActorAxesView *) actor)->position.component[1] = titleActorPlacements[key].position[1];
            ((TitleActorAxesView *) actor)->position.component[2] = titleActorPlacements[key].position[2];
            ((TitleActorAxesView *) actor)->rotation.component[0] = titleActorPlacements[key].rotation[0];
            ((TitleActorAxesView *) actor)->rotation.component[1] = titleActorPlacements[key].rotation[1];
            ((TitleActorAxesView *) actor)->rotation.component[2] = titleActorPlacements[key].rotation[2];
            cur++;
            if (titleTable.list_index == 7 && ((key + 250) & 0xFF) < 2) {
                actor->drawMode = 1;
                actor->drawParam = 0.8f;
            }
            XenoActorAllocMatrices(actor);
            XenoActorSetMode(actor, 0);
        }
        for (light_round = 0; light_round < 7; light_round++) {
            xglMatrixStackUnit();
            xglMatrixStackRotX(titleLightAngles[light_round][0]);
            xglMatrixStackRotY(titleLightAngles[light_round][1]);
            xglMatrixStackSave(saved);
            xglVectorMulMat(&tmp, saved, &titleLightDirection);
            titleTable.lights.rows[light_round][0] = tmp.x;
            titleTable.lights.rows[light_round][1] = tmp.y;
            titleTable.lights.rows[light_round][2] = tmp.z;
        }
        for (light_round = 0; light_round < 8; light_round++) {
            titleTable.lights.rows[7 + light_round][0] = titleLightVectors[light_round].x;
            titleTable.lights.rows[7 + light_round][1] = titleLightVectors[light_round].y;
            titleTable.lights.rows[7 + light_round][2] = titleLightVectors[light_round].z;
        }
        titleState.pan.x = titleState.pan.y = titleState.pan.z = titleState.pan.w = 0.0f;
        titleTable.table_ready = titleTable.table_ready + 1;
    case TITLE_TABLE_PLAYING:
        if (titleState.sprite_fade > 0) {
            titleState.sprite_fade -= 4;
            if (titleState.sprite_fade < 0) {
                titleState.sprite_fade = 0;
            }
        }
        pxySetDefaultLight(&titleTable.lights.set);
        nmlModelSetGlobalFogCol(&titleFogColor);
        nmlModelSetGlobalFogDist(titleFogDistances[0], titleFogDistances[1], titleFogDistances[2], titleFogDistances[3]);
        pxyRequestOverlayPass();
        camera = xglStudioGetCamera2(0);
        tiCameraReadRecord(camera, titleTable.record, I2F(titleTable.frame));
        if (titleState.field_14 == 0) {
            rate = 0.0003f;
            titleState.pan.x -= I2F(PadData[0].leftY) * rate;
            camera->travel.rotation.x = camera->travel.rotation.x + titleState.pan.x;
            titleState.pan.y -= I2F(PadData[0].leftX) * rate;
            camera->travel.rotation.y = camera->travel.rotation.y + titleState.pan.y;
        }
        xglCameraMove(camera);
        XenoActorDoit();
        XenoActorDisp();
        titleTable.frame = titleTable.frame + 1;
        if (titleTable.frame < titleTable.frame_end) {
            break;
        }
        titleTable.table_ready = TITLE_TABLE_BEGIN_LIST;
        titleTable.list_index = titleTable.list_index + 1;
        break;
    case TITLE_TABLE_IDLE:
    case 255:
        break;
    }

    if (titleState.field_14 == 0) {
        if (PadData[0].held & PAD_R3) {
            titleState.timer = 64;
        }
        if (titleState.timer != 0) {
            titleState.timer = titleState.timer - 1;
        }
        if (titleState.timer == 0) {
            padDebugCount = 0;
            titleState.field_14 = 2;
        }
    } else if (titleState.field_14 == 128) {
        prev_count = padDebugCount;
        new_count = padDebugPrintDirection(12);
        if (prev_count != new_count) {
            titleState.timer = 30;
        }
        if (titleState.timer != 0) {
            titleState.timer = titleState.timer - 1;
            if (padDebugCount >= 3) {
                titleState.field_14 = 0;
                titleState.timer = 64;
            }
        }
        if (titleState.timer == 0) {
            padDebugCount = 0;
        }
    } else {
        if (titleState.field_14 < 128) {
            titleState.field_14 = titleState.field_14 + 2;
        }
        if (titleState.field_14 >= 129) {
            titleState.field_14 = 128;
        }
    }
}

extern u64 titleSpriteFadePacket[];
extern u32 titleTextureSpritePacket[];
extern u8 titleTextureUploadHeader[];
extern u32 titleSlotPacket[];
/* titleFadeOverlayPacket: u64 words (gs_tag = word 6, fade_tag = word 8). */
extern u64 titleFadeOverlayPacket[];

/* Title per-frame draw callback: the fade overlay, the title sprites and the menu slot sprites. */
void titleDrawFrame(FontDrawCursor *cursor, void *argument)
{
    XglPacket *packet;
    u32 *direct_state;
    int visible;
    int group;
    int column;
    int top;
    int span;
    int inset;
    int slot;
    int i;
    int group_index;
    signed char *slot_ptr;
    u64 tag_bits;

    tag_bits = 0x64;
    packet = cursor->packet;
    if (titleState.sprite_fade > 0) {
        titleSpriteFadePacket[4] = ((u64) titleState.sprite_fade << 32) | tag_bits;
        sceVif1PkAddDirectDataN(packet, titleSpriteFadePacket, 6);
    }
    if (packet->directCode != 0) {
        direct_state = packet->directState;
        if (direct_state != 0) {
            sceVif1PkCloseDirectHLCode(packet);
            if (direct_state[0] == 0x51000000) {
                packet->write_cursor -= 4;
            }
        }
    }
    titleTextureSpritePacket[27] = titleState.field_14;
    sceVif1PkRef(packet, titleTextureUploadHeader, 7, 0, 0, 0);
    sceVif1PkRef(packet, titleState.texture, TITLE_TEXTURE_QWC, 0, 0x51007000, 0);
    titleTextureSpritePacket[33] = 0x71F7;
    titleTextureSpritePacket[41] = 0x7FF7;
    sceVif1PkCnt(packet, 0);
    visible = 0;
    sceVif1PkAddDataN(packet, titleTextureSpritePacket, 0x2C);
    group_index = 3;
    sceVif1PkRef(packet, titleTextureUploadHeader, 7, 0, 0, 0);
    sceVif1PkRef(packet, titleState.texture + TITLE_TEXTURE_HALF_SIZE, TITLE_TEXTURE_QWC, 0, 0x51007000, 0);
    titleTextureSpritePacket[33] = 0x7FF7;
    titleTextureSpritePacket[41] = 0x8DF7;
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddDataN(packet, titleTextureSpritePacket, 0x2C);
    xglPacketTextureTrans((XglTextureTransSet *) titleState.object_buffer);
    titleSlotPacket[35] = titleState.field_14;
    slot_ptr = titleState.group_slot;
    do {
        if (*slot_ptr != -1) {
            visible++;
        }
        slot_ptr++;
        group_index--;
    } while (group_index >= 0);
    for (group_index = 0; group_index < 5; group_index++) {
        top = 0xC4;
        if (visible < 5 && group_index < 4) {
            column = (0x48 / (visible + 1)) + (group_index * (0x48 / visible)) + 0x120;
        } else {
            column = group_index * 0x12 + 0x120;
        }
        span = 0x78;
        inset = 0x12;
        if (titleState.selected_group != group_index) {
            top = 0xD8;
            span = 0x50;
            column += 3;
            inset = 0xC;
        }
        if (group_index == 4) {
            break;
        }
        slot = titleState.group_slot[group_index];
        if (slot != -1) {
            int slot_offset = slot * 0x120;
            int top_pos = top * 16 + 0x6FF8;
            int column_pos = column * 16 + 0x71F7;

            titleSlotPacket[48] = top_pos + span * 16;
            titleSlotPacket[45] = slot_offset + 0x120;
            titleSlotPacket[49] = column_pos + inset * 16;
            titleSlotPacket[37] = slot_offset;
            titleSlotPacket[40] = top_pos;
            titleSlotPacket[41] = column_pos;
            sceVif1PkCnt(packet, 0);
            sceVif1PkAddDataN(packet, titleSlotPacket, 0x34);
        }
    }
    if (titleState.fade_level > 0) {
        titleFadeOverlayPacket[8] = ((u64) titleState.fade_level << 32) | tag_bits;
        titleFadeOverlayPacket[6] = 0x24120000 | (sRender.vram.flip_base << 5) | ((u64) 0xC800 << 19);
        sceVif1PkAddDataN(packet, &titleFadeOverlayPacket, 0x28);
    }
    return xglFontReloadTexture(cursor, 0);
}

/* Title fade transition: steps the fade level down until it reaches zero. */
void titleFadeTransition(int mode)
{
    XglPacket *packet;
    int full_level;

    titleState.fade_level = TITLE_FADE_MAX;
    full_level = TITLE_FADE_MAX;
    do {
        packet = xglPacketGetCurrent();
        if (mode == 0) {
            titleState.fade_level -= 16;
        } else {
            titleState.fade_level -= TITLE_FADE_STEP;
        }
        if (titleState.fade_level < 0) {
            titleState.fade_level = 0;
        }
        titleFadePacket[6] = 0x24120000 | (sRender.vram.flip_base << 5) | ((u64) 0xC800 << 19);
        if (mode == 0) {
            ((int *) titleFadePacket)[18] = titleState.fade_level;
            ((int *) titleFadePacket)[17] = titleState.fade_level;
            ((int *) titleFadePacket)[16] = titleState.fade_level;
        } else {
            int progress = full_level - titleState.fade_level;
            int scaled = progress * 3;

            ((int *) titleFadePacket)[18] = titleState.fade_level;
            ((int *) titleFadePacket)[16] = full_level - progress * 2;
            if (((int *) titleFadePacket)[16] < 0) {
                ((int *) titleFadePacket)[16] = 0;
            }
            ((int *) titleFadePacket)[17] = full_level - scaled / 2;
            if (((int *) titleFadePacket)[17] < 0) {
                ((int *) titleFadePacket)[17] = 0;
            }
        }
        sceVif1PkCnt(packet, 0);
        sceVif1PkAddDataN(packet, &titleFadePacket, 0x24);
        xglSleep();
        if (titleState.fade_level == 0) {
            sceVif1PkCnt(packet, 0);
            sceVif1PkAddDataN(packet, &titleFadePacket, 0x24);
            xglSleep();
        }
    } while (titleState.fade_level > 0);
    copyframe();
    xglSleep();
}

/* The two halfwords of the memory card state that xglMcMain returns; 255 and 128 mean not ready. */
typedef struct TitleMcStatus {
    short status;
    short detail;
} TitleMcStatus;

/* The pad pair as one word: the store goes through this union (alias set 0) so it stays ahead of the halfword reads. */
typedef union TitleMcWord {
    int word;
    TitleMcStatus pair;
} TitleMcWord;

#define ONE_OVERLAY 0x01000000
#define TWO_OVERLAY 0x02000000
extern const char *titleObjectPathPointer;
extern const char titleBg2Path[];
extern const char titleDisc1Path[];
extern const char titleDisc2Path[];

/* The title screen: loads the title assets, then runs the menu until it returns the next scene id. */
unsigned int Title(void)
{
    int save_found[3];
    TitleMcStatus mc_status;
    McRequestParam mc_request;
    struct HddExistRequest hdd_request;
    int disc;
    const char *path;
    int size;
    int input_lockout;
    int move_dir;
    int pad_repeat;
    int prev_group;
    int next_group;
    int result;

    xglRenderClearDepth();
    copyframe();
    do {
        ((TitleMcWord *) &mc_status)->word = xglMcMain();
    } while (mc_status.status == 255 || mc_status.status == 128 || mc_status.detail == 255 || mc_status.detail == 128);

    mc_request.file = -1;
    mc_request.data = WorkEnd;
    xglMcRequest(0, 2, &mc_request, &save_found[0]);
    xglMcRequest(1, 2, &mc_request, &save_found[1]);
    hdd_request.card = -1;
    hdd_request.found = WorkEnd;
    save_found[2] = xglHddMcExist(&hdd_request);
    xglFontLoad(1, CD_NOTIFY_DUMMY);

    for (;;) {
        disc = xglCdDiskCheck();
        if (disc == 1) {
            path = titleDisc1Path;
            break;
        }
        if (disc == 2) {
            path = titleDisc2Path;
            break;
        }
    }

    titleState.object_buffer = (u8 *) 0x01000000;
    size = xglCdGetFileSize(titleObjectPathPointer);
    xglCdReadFile(titleObjectPathPointer, titleState.object_buffer, 1, CD_NOTIFY_DUMMY);
    titleState.texture = titleState.object_buffer + ((size + 63) & -64);
    size = xglCdGetFileSize(path);
    xglCdReadFile(path, titleState.texture, 1, CD_NOTIFY_DUMMY);
    titleTable.table_count = TITLE_TABLE_UNRELOCATED;
    titleTable.table.words = (u32 *) ((u32) titleState.texture + ((size + 63) & -64));

    do {
        ((TitleMcWord *) &mc_status)->word = xglMcMain();
        xglSleep();
    } while (xglCdServiceIfPending() != 0 || mc_status.status == 255 || mc_status.status == 128 || mc_status.detail == 255 || mc_status.detail == 128);

    if (save_found[0] > 0 || save_found[1] > 0 || save_found[2] > 0) {
        titleState.selected_group = 1;
    } else {
        titleState.selected_group = 0;
    }

    switch (xglHddMount()) {
    case 0:
        titleState.hdd_stage = 0;
        break;
    case -2:
        titleState.hdd_stage = 2;
        break;
    case -3:
        titleState.hdd_stage = 3;
        break;
    case -4:
        titleState.hdd_stage = 4;
        break;
    case -5:
    case 1:
        titleState.hdd_stage = 1;
        break;
    default:
        titleState.hdd_stage = -1;
        break;
    }
    titleState.group_slot[2] = -1;
    titleState.group_slot[3] = -1;
    titleState.group_slot[0] = 0;
    titleState.group_slot[1] = 1;
    if (titleState.hdd_stage != -1) {
        if (titleState.hdd_stage != 1) {
            titleState.group_slot[2] = 2;
        } else {
            titleState.group_slot[2] = 3;
        }
    }

    titleTable.table_ready = TITLE_TABLE_IDLE;
    input_lockout = TITLE_PREROLL_FRAMES;
    soundSlotStop(0);
    soundSlotStop(1);
    titleStartBgm();
    xglCdReadFile(titleBg2Path, titleTable.table.words, 1, titleTableReadNotify);
    xglRenderVSyncInterval = 3;
    copyframe();
    titleState.fade_level = TITLE_FADE_MAX;
    titleState.sprite_fade = 128;
    titleState.field_14 = 128;
    titleState.timer = 0;
    padDebugCount = 0;

    for (;;) {
        if (titleState.field_14 != 0) {
            if (input_lockout > 0) {
                input_lockout--;
            } else if (titleState.fade_level == 0 && (PadData[0].pressed & (PAD_START | PAD_CIRCLE)) != 0) {
                seVoiceStartDefault(TITLE_SE_ACCEPT);
                xglRenderVSyncInterval = 2;
                soundSlotsRestart();
                xglCdReadCancel(0);
                XenoActorPoolInitDefault();
                copyframe();
                xglSleep();
                break;
            }

            move_dir = 0;
            pad_repeat = PadData[0].directionRepeated;
            if ((pad_repeat & PAD_UP) != 0) {
                move_dir = -1;
            }
            if ((pad_repeat & PAD_DOWN) != 0) {
                move_dir = 1;
            }
            if (move_dir != 0) {
                prev_group = titleState.selected_group;
                next_group = prev_group;
                for (;;) {
                    next_group += move_dir;
                    if (next_group < 0) {
                        next_group = 0;
                        break;
                    }
                    if (next_group >= 4) {
                        next_group = titleState.selected_group;
                        break;
                    }
                    if (titleState.group_slot[next_group] != -1) {
                        break;
                    }
                }
                if (next_group != titleState.selected_group) {
                    titleState.selected_group = next_group;
                    seVoiceStartDefault(TITLE_SE_CURSOR_MOVE);
                    if (input_lockout < TITLE_PREROLL_FRAMES) {
                        input_lockout = TITLE_PREROLL_FRAMES;
                    }
                }
            }
        }

        if (titleState.fade_level > 0) {
            titleState.fade_level -= TITLE_FADE_STEP;
            if (titleState.fade_level < 0) {
                titleState.fade_level = 0;
            }
        }
        titleRunScene();
        titleFrameHookDummy();
        xglFontPrintExtFunc(0, titleDrawFrame, 0);
        xglSleep();
    }
    switch (titleState.selected_group) {
    case -2:
        return titleGroupMinus2Dummy();
    case -1:
        return TWO_OVERLAY + (int) logoShowCredits;
    case 0:
    case 1:
    case 2:
    case 3:
        switch (titleState.group_slot[titleState.selected_group]) {
        case 0:
            TitleGameInit();
            titleFadeTransition(0);
            configScreenRun();
            if ((gameWork.control & CONTROL_SOFT_RESET) != 0) {
                return 0;
            }
            gameWork.nextMap = 7001;
            gameWork.loadStage = 0;
            return ONE_OVERLAY + (int) Game;
        case 1:
            TitleGameInit();
            titleFadeTransition(0);
            result = MenuFileMain(1);
            if (result == 1) {
                gameWork.loadStage = result;
                return ONE_OVERLAY + (int) Game;
            }
            return 0;
        case 2:
        case 3:
            if (titleState.hdd_stage >= 0) {
                TitleHddInstall(titleState.hdd_stage);
            }
            return 0;
        case 4:
            titleFadeTransition(1);
            return -1;
        case 5:
        default:
            return 0;
        }
    case 4:
        menuRequestLoad();
        TitleGameInit();
        gameWork.nextMap = 10;
        gameWork.loadStage = 0;
        return ONE_OVERLAY + (int) Game;
    default:
        return 0;
    }
}

/* Initialized data of this TU, in address order. */
TitleLayoutRecord titleActorPlacements[10] = {
    { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
    { { 0.3310075f, 0.0f, 3.6509488f }, { 0.0f, 0.0f, 0.0f } },
    { { 8.446873f, 0.0f, 3.655942f }, { 0.0f, 0.0f, 0.0f } },
    { { -8.376025f, 0.0f, 3.6576529f }, { 0.0f, 0.0f, 0.0f } },
    { { -0.25603572f, 0.0f, 3.7308059f }, { 0.0f, 0.0f, 0.0f } },
    { { 0.03792591f, 0.7523239f, -4.097623f }, { 2.792527f, 0.0f, 3.1415927f } },
    { { -9.9662895f, 0.0f, 3.6576529f }, { 0.0f, 0.0f, 0.0f } },
    { { 1.3410023f, 0.0f, 3.7308059f }, { 0.0f, 0.0f, 0.0f } },
    { { 22.52556f, 0.0f, 17.207914f }, { 0.0f, -1.5707964f, 0.0f } },
    { { 1.7654405f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } }
};
u8 titleActorList00[24] = {
    0x08, 0x0D, 0x02, 0x05, 0xFF, 0x02, 0x00, 0x05, 0x00, 0x02, 0x01, 0x05,
    0x02, 0x02, 0x03, 0x05, 0x04, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
u8 titleActorList01[24] = {
    0x1C, 0x29, 0xFA, 0x0D, 0x10, 0x13, 0x16, 0x19, 0xFF, 0xFA, 0x00, 0x0D,
    0x00, 0x10, 0x00, 0x13, 0x00, 0x16, 0x00, 0x19, 0x00, 0xFF, 0x00, 0x00
};
u8 titleActorList02[24] = {
    0x29, 0x36, 0xFA, 0x0D, 0x10, 0x13, 0x16, 0x19, 0xFF, 0xFA, 0x00, 0x0D,
    0x00, 0x10, 0x00, 0x13, 0x00, 0x16, 0x00, 0x19, 0x00, 0xFF, 0x00, 0x00
};
u8 titleActorList03[32] = {
    0x39, 0x48, 0xFA, 0x36, 0x0D, 0x10, 0x13, 0x16, 0x19, 0xFF, 0xFA, 0x00,
    0x36, 0x00, 0x0D, 0x00, 0x10, 0x00, 0x13, 0x00, 0x16, 0x00, 0x19, 0x00,
    0x36, 0x05, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00
};
u8 titleActorList04[24] = {
    0x48, 0x53, 0x0D, 0x10, 0x13, 0x16, 0x19, 0xFF, 0x0D, 0x00, 0x10, 0x00,
    0x13, 0x00, 0x16, 0x00, 0x19, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00
};
u8 titleActorList05[24] = {
    0x53, 0x5E, 0x0D, 0x10, 0x13, 0x16, 0x19, 0xFF, 0x0D, 0x00, 0x10, 0x00,
    0x13, 0x00, 0x16, 0x00, 0x19, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00
};
u8 titleActorList06[24] = {
    0x5E, 0x69, 0x0D, 0x10, 0x13, 0x16, 0x19, 0xFF, 0x0D, 0x00, 0x10, 0x00,
    0x13, 0x00, 0x16, 0x00, 0x19, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00
};
u8 titleActorList07[24] = {
    0x69, 0x70, 0x02, 0x05, 0x0D, 0xFF, 0x0D, 0x00, 0x02, 0x00, 0x05, 0x00,
    0x02, 0x01, 0x05, 0x02, 0x02, 0x06, 0x05, 0x07, 0xFF, 0x00, 0x00, 0x00
};
u8 titleActorList08[24] = {
    0x73, 0x7E, 0x0D, 0x10, 0x13, 0x16, 0x70, 0xFF, 0x0D, 0x00, 0x10, 0x00,
    0x13, 0x00, 0x16, 0x00, 0x70, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00
};
u8 titleActorList09[24] = {
    0x7E, 0x8B, 0x05, 0x0D, 0x10, 0x13, 0x16, 0x70, 0xFF, 0x0D, 0x00, 0x10,
    0x00, 0x13, 0x00, 0x16, 0x00, 0x70, 0x00, 0x05, 0x00, 0x05, 0x08, 0xFF
};
u8 titleActorList10[32] = {
    0x8E, 0x9D, 0x36, 0x0D, 0x10, 0x13, 0x16, 0x8B, 0x70, 0xFF, 0x36, 0x00,
    0x0D, 0x00, 0x10, 0x00, 0x13, 0x00, 0x16, 0x00, 0x8B, 0x00, 0x70, 0x00,
    0x36, 0x09, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00
};
u8 titleActorList11[32] = {
    0x9D, 0xAC, 0x36, 0x0D, 0x10, 0x13, 0x16, 0x8B, 0x70, 0xFF, 0x36, 0x00,
    0x0D, 0x00, 0x10, 0x00, 0x13, 0x00, 0x16, 0x00, 0x8B, 0x00, 0x70, 0x00,
    0x36, 0x09, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00
};
u8 titleActorList12[32] = {
    0xAF, 0xBE, 0xFA, 0x0D, 0x10, 0x13, 0x16, 0x8B, 0xAC, 0xFF, 0xFA, 0x00,
    0x0D, 0x00, 0x10, 0x00, 0x13, 0x00, 0x16, 0x00, 0x8B, 0x00, 0xAC, 0x00,
    0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
u8 *titleTableLists[14] = { titleActorList00, titleActorList01, titleActorList02, titleActorList03, titleActorList04, titleActorList05, titleActorList06, titleActorList07, titleActorList08, titleActorList09, titleActorList10, titleActorList11, titleActorList12, 0 };
float titleLightAngles[7][2] = {
    { -0.261956f, 0.531208f },
    { 2.356194f, 1.273357f },
    { 0.349066f, 3.786631f },
    { 0.0f, 0.0f },
    { 0.0f, 0.0f },
    { 0.0f, 0.0f },
    { 1.570796f, 0.0f }
};
TitleVec3Slot titleLightVectors[8] = {
    { 0.8f, 0.8f, 0.8f },
    { 0.5f, 0.5f, 0.5f },
    { 0.5f, 0.5f, 0.5f },
    { 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f },
    { 0.25f, 0.25f, 0.25f },
    { 0.25f, 0.25f, 0.25f }
};
Vector4 titleLightDirection = { 0.0f, 0.0f, 1.0f, 1.0f };
Vector4 titleFogColor = { 178.5f, 186.15001f, 255.0f, 0.0f };
float titleFogDistances[4] = { 0.0f, 3e+01f, 0.3f, 0.39f };
u64 titleSpriteFadePacket[12] = {
    0x5023400000008001ULL, 0x00000000000551EEULL, 0x0000000000030000ULL,
    0x0000000000000047ULL, 0x0000000000000000ULL, 0x0000000000000042ULL,
    0x000000FF000000FFULL, 0x00000080000000FFULL, 0x000071F700006FF8ULL,
    0x0000000000000000ULL, 0x00008DF700008FF8ULL, 0x0000000000000000ULL
};
u32 titleTextureSpritePacket[44] = {
    0x00000000, 0x00000000, 0x00000000, 0x5000000A, 0x00008001, 0x90AB4000,
    0x3531E6EE, 0x00000005, 0x00000000, 0x00000000, 0x0000003F, 0x00000000,
    0x00030000, 0x00000000, 0x00000047, 0x00000000, 0x64023800, 0x00000006,
    0x00000006, 0x00000000, 0x00000044, 0x00000080, 0x00000042, 0x00000000,
    0x00000080, 0x00000080, 0x00000080, 0x00000080, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00006FF8, 0x000071F7, 0x00000000, 0x00000000,
    0x00002000, 0x00000E00, 0x00000000, 0x00000000, 0x00008FF8, 0x00008DF7,
    0x00000000, 0x00000000
};
u8 titleTextureUploadHeader[112] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x00, 0x51, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
    0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x38, 0x08, 0x00, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x51, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0xE0, 0x00, 0x00, 0x00,
    0x52, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
};
u32 titleSlotPacket[52] = {
    0x00000000, 0x00000000, 0x00000000, 0x5000000C, 0x00008001, 0xB0AB4000,
    0x31EEEEEE, 0x00000535, 0x00000000, 0x00000000, 0x0000003F, 0x00000000,
    0x00000005, 0x00000000, 0x00000008, 0x00000000, 0x00000060, 0x00000000,
    0x00000014, 0x00000000, 0x00030000, 0x00000000, 0x00000047, 0x00000000,
    0x64023800, 0x00000006, 0x00000006, 0x00000000, 0x00000044, 0x00000080,
    0x00000042, 0x00000000, 0x00000080, 0x00000080, 0x00000080, 0x00000080,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00006FF8, 0x000081F7,
    0x00000000, 0x00000000, 0x00000780, 0x00000480, 0x00000000, 0x00000000,
    0x00007778, 0x00007677, 0x00000000, 0x00000000
};
u64 titleFadeOverlayPacket[20] = {
    0x0000000000000000ULL, 0x5000000900000000ULL, 0x80AB400000008001ULL,
    0x0000000053531E6EULL, 0x0000000000030000ULL, 0x0000000000000047ULL,
    0x0000000000000000ULL, 0x0000000000000006ULL, 0x0000000000000000ULL,
    0x0000000000000042ULL, 0x0000008000000080ULL, 0x0000008000000080ULL,
    0x0000000000000000ULL, 0x0000000000000000ULL, 0x000071F700006FF8ULL,
    0x0000000000000000ULL, 0x00001C0000002000ULL, 0x0000000000000000ULL,
    0x00008DF700008FF8ULL, 0x0000000000000000ULL
};
u64 titleFadePacket[18] = {
    0x0000000000000000ULL, 0x5000000800000000ULL, 0x708B400000008001ULL,
    0x000000000535316EULL, 0x0000000000030000ULL, 0x0000000000000047ULL,
    0x0000000000000000ULL, 0x0000000000000006ULL, 0x0000008000000080ULL,
    0x0000008000000080ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
    0x000071F700006FF8ULL, 0x0000000000000000ULL, 0x00001C0000002000ULL,
    0x0000000000000000ULL, 0x00008DF700008FF8ULL, 0x0000000000000000ULL
};
const char *titleObjectPathPointer = titleObjectTexturePath;
