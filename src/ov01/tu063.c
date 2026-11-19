/*
 * OV01 original TU 63: 0x00aa8200..0x00aa8590 (17 functions)
 */
#include "common.h"
#include "ov01/tu008.h"
#include "ov01/tu063.h"
#include "ov01/tu012.h"
#include "gs.h"

typedef int s32;


int menuTexCellIndex(int key)
{
    StatBlock *unit = batUnitRecordGet(key);
    unsigned short flags = unit->flags;

    if (key >= 1 && key <= 10) {
        return key - 1;
    }
    if (key >= 11 && key <= 15) {
        return key - 4;
    }
    if (key >= 16) {
        switch (flags & 3) {
        case 0:
            return 11;
        case 1:
            return 10;
        case 2:
            return 12;
        }
    }
    return 0;
}

s32 xglPacketTextureTrans(s32 value);

/*
 * Texture flush for the menu: waits on the value of dataStatBufGet, then sends
 * one GS write to register GS_TEXFLUSH with 1.
 */
void menuTexFlush(void) {
    GrPacket packet;
    s32 result;

    result = dataStatBufGet();
    xglPacketTextureTrans(result);
    grGpInit(&packet);
    grGsRegSet(&packet, GS_TEXFLUSH, 1);
    grPacketSend(&packet);
}

void GS_REG_SET(void *packet, int register_id, u64 control);

/* GIF tag of the record: NLOOP 5, EOP set, NREG 1 (the five A+D pairs that follow). */
#define GIF_TAG_FIVE_AD_EOP 0x1000000000008005ULL

/* One 16-byte texture state record: TEX0 bits 0..15 at +0, TEX0 bits 37..52 at +2, ALPHA_1 at +8. */
typedef struct TexStateRecord {
    unsigned short tex0Low;
    unsigned short tex0Clut;
    unsigned char unmodeled_04[0x8 - 0x4];
    u64 alpha;
} TexStateRecord;

/*
 * Emits one 16-byte texture state record into the packet: GS TEX0 from the
 * halfwords at +0 and +2, ALPHA from the word at +8, then TEST, CLAMP and TEX1.
 */
void grPacketTexState(GrPacket *packet, void *record)
{
    TexStateRecord *texState = record;

    GS_REG_SET(packet, GIF_AD, GIF_TAG_FIVE_AD_EOP);
    GS_REG_SET(packet, GS_TEX0_1, texState->tex0Low | ((u64)texState->tex0Clut << 37) | 0x2000000629340000ULL);
    GS_REG_SET(packet, GS_ALPHA_1, texState->alpha);
    GS_REG_SET(packet, GS_TEST_1, 0x3000dULL);
    GS_REG_SET(packet, GS_CLAMP_1, 0);
    GS_REG_SET(packet, GS_TEX1_1, 0);
}

int menuTexCellIndex(int key);
extern TexStateRecord menuTexStateTable[];

/* Emits the record of table menuTexStateTable chosen by key. */
void menuTexStateByKey(GrPacket *packet, s32 key) {
    s32 cellIndex = menuTexCellIndex(key);
    grPacketTexState(packet, menuTexStateTable + cellIndex);
}

extern TexStateRecord menuTexStateAltTable[];

/* The same record chosen by key from the sibling table menuTexStateAltTable (0xA0 further on). */
void menuTexStateAltByKey(void *packet, s32 key) {
    s32 cellIndex = menuTexCellIndex(key);
    grPacketTexState(packet, menuTexStateAltTable + cellIndex);
}

extern TexStateRecord menuSmallCellTexRecord[];

/* Texture state of the small cell sprites (the 24 x 28 grid), one fixed record. */
void menuSmallCellTexState(GrPacket *packet) {
    grPacketTexState(packet, menuSmallCellTexRecord);
}

extern TexStateRecord menuNumTexRecord[];

void menuNumTexState(GrPacket *packet) {
    grPacketTexState(packet, menuNumTexRecord);
}

extern TexStateRecord menuNumStyle1TexRecord[];

void menuNumStyle1TexState(GrPacket *packet) {
    grPacketTexState(packet, menuNumStyle1TexRecord);
}

extern TexStateRecord menuIconTexRecord[];

void menuIconTexState(void *packet) {
    grPacketTexState(packet, menuIconTexRecord);
}

extern TexStateRecord D_00AB6A38[];

void func_00AA8490(void *packet) {
    grPacketTexState(packet, D_00AB6A38);
}

extern TexStateRecord D_00AB6A48[];

void func_00AA84B0(void *packet) {
    grPacketTexState(packet, D_00AB6A48);
}

extern TexStateRecord D_00AB6A58[];

void func_00AA84D0(void *packet) {
    grPacketTexState(packet, D_00AB6A58);
}

extern TexStateRecord D_00AB6A68[];

void func_00AA84F0(void *packet) {
    grPacketTexState(packet, D_00AB6A68);
}

extern TexStateRecord menuPanelTexRecord[];

void menuPanelTexState(GrPacket *packet) {
    grPacketTexState(packet, menuPanelTexRecord);
}

extern TexStateRecord D_00AB6A88[];

void func_00AA8530(GrPacket *packet) {
    grPacketTexState(packet, D_00AB6A88);
}

extern TexStateRecord D_00AB6A98[];

void func_00AA8550(void *packet) {
    grPacketTexState(packet, D_00AB6A98);
}

extern TexStateRecord menuStatusIconsTexRecord[];

void menuStatusIconsTexState(GrPacket *packet) {
    grPacketTexState(packet, menuStatusIconsTexRecord);
}

/* Ten texture state records of the menu cells (see menuTexCellIndex). */
TexStateRecord menuTexStateTable[10] = {
    { 0x3880, 0x3b80, { 0x00, 0x00, 0x00, 0x00 }, 0x44 },
    { 0x3880, 0x3b84, { 0x00, 0x00, 0x00, 0x00 }, 0x44 },
    { 0x3880, 0x3b90, { 0x00, 0x00, 0x00, 0x00 }, 0x44 },
    { 0x3880, 0x3b94, { 0x00, 0x00, 0x00, 0x00 }, 0x44 },
    { 0x3880, 0x3ba0, { 0x00, 0x00, 0x00, 0x00 }, 0x44 },
    { 0x3880, 0x3b88, { 0x00, 0x00, 0x00, 0x00 }, 0x44 },
    { 0x3880, 0x3b8c, { 0x00, 0x00, 0x00, 0x00 }, 0x44 },
    { 0x3880, 0x3b98, { 0x00, 0x00, 0x00, 0x00 }, 0x44 },
    { 0x3880, 0x3b9c, { 0x00, 0x00, 0x00, 0x00 }, 0x44 },
    { 0x3880, 0x3ba8, { 0x00, 0x00, 0x00, 0x00 }, 0x44 },
};

/* The same ten records as menuTexStateTable with other texture bases. */
TexStateRecord menuTexStateAltTable[10] = {
    { 0x3880, 0x3b80, { 0x00, 0x00, 0x00, 0x00 }, 0x44 },
    { 0x38c0, 0x3bc4, { 0x00, 0x00, 0x00, 0x00 }, 0x44 },
    { 0x38c0, 0x3bd0, { 0x00, 0x00, 0x00, 0x00 }, 0x44 },
    { 0x3880, 0x3b94, { 0x00, 0x00, 0x00, 0x00 }, 0x44 },
    { 0x3880, 0x3ba0, { 0x00, 0x00, 0x00, 0x00 }, 0x44 },
    { 0x38c0, 0x3bc8, { 0x00, 0x00, 0x00, 0x00 }, 0x44 },
    { 0x3880, 0x3b8c, { 0x00, 0x00, 0x00, 0x00 }, 0x44 },
    { 0x3880, 0x3b98, { 0x00, 0x00, 0x00, 0x00 }, 0x44 },
    { 0x3880, 0x3b9c, { 0x00, 0x00, 0x00, 0x00 }, 0x44 },
    { 0x3880, 0x3ba8, { 0x00, 0x00, 0x00, 0x00 }, 0x44 },
};

/* Texture state record menuSmallCellTexRecord. */
TexStateRecord menuSmallCellTexRecord[1] = {
    { 0x3800, 0x3b18, { 0x00, 0x00, 0x00, 0x00 }, 0x0 },
};

/* Texture state record menuNumTexRecord. */
TexStateRecord menuNumTexRecord[1] = {
    { 0x3800, 0x3b08, { 0x00, 0x00, 0x00, 0x00 }, 0x44 },
};

/* Texture state record menuNumStyle1TexRecord. */
TexStateRecord menuNumStyle1TexRecord[1] = {
    { 0x3800, 0x3b0c, { 0x00, 0x00, 0x00, 0x00 }, 0x44 },
};

/* Texture state record menuIconTexRecord. */
TexStateRecord menuIconTexRecord[1] = {
    { 0x3800, 0x3b28, { 0x00, 0x00, 0x00, 0x00 }, 0x0 },
};

/* Texture state record D_00AB6A38. */
TexStateRecord D_00AB6A38[1] = {
    { 0x3800, 0x3b1c, { 0x00, 0x00, 0x00, 0x00 }, 0x44 },
};

/* Texture state record D_00AB6A48. */
TexStateRecord D_00AB6A48[1] = {
    { 0x3800, 0x3b2c, { 0x00, 0x00, 0x00, 0x00 }, 0x44 },
};

/* Texture state record D_00AB6A58. */
TexStateRecord D_00AB6A58[1] = {
    { 0x3800, 0x3b3c, { 0x00, 0x00, 0x00, 0x00 }, 0x44 },
};

/* Texture state record D_00AB6A68. */
TexStateRecord D_00AB6A68[1] = {
    { 0x3800, 0x3b38, { 0x00, 0x00, 0x00, 0x00 }, 0x44 },
};

/* Texture state record menuPanelTexRecord. */
TexStateRecord menuPanelTexRecord[1] = {
    { 0x3800, 0x3b48, { 0x00, 0x00, 0x00, 0x00 }, 0x44 },
};

/* Texture state record D_00AB6A88. */
TexStateRecord D_00AB6A88[1] = {
    { 0x3800, 0x3b4c, { 0x00, 0x00, 0x00, 0x00 }, 0x0 },
};

/* Texture state record D_00AB6A98. */
TexStateRecord D_00AB6A98[1] = {
    { 0x3800, 0x3b58, { 0x00, 0x00, 0x00, 0x00 }, 0x44 },
};

/* Texture state of the status icons. */
TexStateRecord menuStatusIconsTexRecord[1] = {
    { 0x3800, 0x3bac, { 0x00, 0x00, 0x00, 0x00 }, 0x44 },
};
