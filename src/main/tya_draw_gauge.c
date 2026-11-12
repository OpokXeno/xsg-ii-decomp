#include "common.h"
#include "debug_break.h"
#include "shared.h"

#define NULL ((void *) 0)

/* Gauge geometry (in pixels) and the filled / unfilled colours. */
typedef struct TyaGaugeArgs {
    short x;
    short y;
    unsigned int vertex_attribute;
    short width;
    short height;
    unsigned short current;
    unsigned short maximum;
    unsigned char filled_color[4];
    unsigned char unfilled_color[4];
} TyaGaugeArgs;

/* One GS vertex: X and Y in 12.4 fixed point, then the attribute word. */
typedef struct TyaGaugeVertex {
    unsigned int x;
    unsigned int y;
    unsigned int attribute;
    unsigned int unmodeled_0C;
} TyaGaugeVertex;

/* DMA/VIF/GIF packet of the two gauge sprites, uploaded as 9 quadwords. */
typedef struct TyaGaugePacket {
    unsigned int dma_vif_tag[4];
    unsigned int vif_gif_setup[4];
    unsigned int gif_control[4];
    unsigned int filled_color[4];
    TyaGaugeVertex filled_vertices[2];
    unsigned int unfilled_color[4];
    TyaGaugeVertex unfilled_vertices[2];
} TyaGaugePacket;

#define TYA_GAUGE_COORDINATE_SCALE 0x10
#define TYA_GAUGE_X_ORIGIN 0x6FF8
#define TYA_GAUGE_Y_ORIGIN 0x71F7

extern TyaGaugePacket D_002BD570;

void pxyAddEntry(int key, void *data, int qwc);

/* Draws a horizontal gauge filled to current / maximum of its width. */
void tyaDrawGauge(TyaGaugeArgs *gauge)
{
    int left_x;
    int top_y;
    int right_x;
    int bottom_y;
    int fill_width;
    int fill_x;

    left_x = (gauge->x * TYA_GAUGE_COORDINATE_SCALE) + TYA_GAUGE_X_ORIGIN;
    top_y = (gauge->y * TYA_GAUGE_COORDINATE_SCALE) + TYA_GAUGE_Y_ORIGIN;
    right_x = left_x + (gauge->width * TYA_GAUGE_COORDINATE_SCALE);
    bottom_y = top_y + (gauge->height * TYA_GAUGE_COORDINATE_SCALE);
    fill_width = (gauge->current * gauge->width) / gauge->maximum;
    if (fill_width < 0) {
        fill_width = 0;
    }
    if (gauge->width < fill_width) {
        fill_width = gauge->width;
    }
    fill_x = left_x + (fill_width * TYA_GAUGE_COORDINATE_SCALE);

    D_002BD570.filled_color[0] = gauge->filled_color[0];
    D_002BD570.filled_color[1] = gauge->filled_color[1];
    D_002BD570.filled_color[2] = gauge->filled_color[2];
    D_002BD570.filled_color[3] = gauge->filled_color[3];
    D_002BD570.filled_vertices[0].x = left_x;
    D_002BD570.filled_vertices[0].y = top_y;
    D_002BD570.filled_vertices[0].attribute = gauge->vertex_attribute;
    D_002BD570.filled_vertices[1].x = fill_x;
    D_002BD570.filled_vertices[1].y = bottom_y;
    D_002BD570.filled_vertices[1].attribute = gauge->vertex_attribute;

    D_002BD570.unfilled_color[0] = gauge->unfilled_color[0];
    D_002BD570.unfilled_color[1] = gauge->unfilled_color[1];
    D_002BD570.unfilled_color[2] = gauge->unfilled_color[2];
    D_002BD570.unfilled_color[3] = gauge->unfilled_color[3];
    D_002BD570.unfilled_vertices[0].x = fill_x;
    D_002BD570.unfilled_vertices[0].y = top_y;
    D_002BD570.unfilled_vertices[0].attribute = gauge->vertex_attribute;
    D_002BD570.unfilled_vertices[1].x = right_x;
    D_002BD570.unfilled_vertices[1].y = bottom_y;
    D_002BD570.unfilled_vertices[1].attribute = gauge->vertex_attribute;

    pxyAddEntry(4, &D_002BD570, 9);
}

/* Empty body; no caller is known. */
void func_001EECD8(void)
{
}

/* The 0x40-byte pose block a record's transform pointer refers to; copied whole
 * (eight doubleword moves), row[3] is the translation (init_uwamono_sys.c). */
typedef struct UwamonoPose {
    AlignedVector4 row[4];
} UwamonoPose;

/* A route node of UwamonoState (uwamonoAddRouteNode links them). The reset
 * writes it through the union: that keeps the compiler from moving the node's
 * stores past the link-count stores that follow them. */
typedef union RouteNode {
    float component[4];
    AlignedVector4 position;
} RouteNode;

/* A path point of UwamonoState (the name and layout of tu072.c's EnepcPoint). */
typedef union EnepcPoint {
    float coords[4];
    AlignedVector4 vector;
} EnepcPoint;

/* One 0x40C0-byte motion/state block of D_002BD600, sizeof 0x40C0, 16-aligned
 * (uwamonoCreateRecord: n * 0x40C0); a record's motions pointer (+0x334) refers
 * to it. Field names follow src/main/tu085.c and tu086.c; the unmodeled_
 * members are only known by their reset value. */
typedef struct UwamonoState {
    int owner;                              /* +0x0000: the record index, -1 when free */
    short index;                            /* +0x0004: the block's own index */
    unsigned char unmodeled_0006[2];
    int unmodeled_0008;                     /* -1 on reset */
    signed char action;                     /* +0x000C */
    unsigned char unmodeled_000d;
    short unmodeled_000e;
    AlignedVector4 unmodeled_0010;          /* x, y, z zeroed on reset */
    AlignedVector4 home;                    /* +0x0020: x, y, z zeroed on reset */
    float speed;                            /* +0x0030 */
    float floatParams[4];                   /* +0x0034 */
    float angle;                            /* +0x0044: radians */
    float floatParams48[2];                 /* +0x0048 */
    short shortParams[4];                   /* +0x0050 */
    short trailStep;                        /* +0x0058 */
    unsigned short timer;                   /* +0x005A: ticks since the current action started */
    short timerLimit;                       /* +0x005C */
    unsigned char unmodeled_005e;
    unsigned char mode;                     /* +0x005F */
    short unmodeled_0060;
    char names[2][8];                       /* +0x0062 */
    short unmodeled_0072[2];
    unsigned char unmodeled_0076;
    unsigned char unmodeled_0077;
    short unmodeled_0078;
    short unmodeled_007a;
    int id;                                 /* +0x007C */
    short groundId;                         /* +0x0080: map block 0x100 entry of the actor's ground, -1 on reset */
    short footId;                           /* +0x0082: map block 0x700 entry of the footstep sound, -1 on reset */
    unsigned char unmodeled_0084[0xC];
    float trail[256][4];                    /* +0x0090: x, y, z zeroed on reset; tu073's Vector4
                                             * trail[256] seen as float[4] rows: the Vector4 view
                                             * changes the reset loop's addressing */
    short trailCount;                       /* +0x1090 */
    unsigned char unmodeled_1092[0xE];
    EnepcPoint points[256];                 /* +0x10A0: path points, reset to (0, 0, 0, 1) */
    short pointLinks[256][4];               /* +0x20A0 */
    short pointLinkValues[256][4];          /* +0x28A0 */
    short unmodeled_30a0;
    short unmodeled_30a2;
    short unmodeled_30a4;
    short pointCount;                       /* +0x30A6 */
    unsigned char unmodeled_30a8[8];
    RouteNode routeNodes[64];               /* +0x30B0: reset to (0, 0, 0, 1) */
    short routeLinks[64][4];                /* +0x34B0: per node, the linked node ids */
    short routeLinkCounts[64];              /* +0x36B0 */
    short routeParents[64];                 /* +0x3730: the node uwamonoAddRouteNode linked from,
                                             * -1 when added without one; 0 on reset */
    short routeNodeCount;                   /* +0x37B0 */
    unsigned char unmodeled_37b2[0xE];
    Vector4 path[128];                      /* +0x37C0: only x zeroed on reset */
    short pathStep;                         /* +0x3FC0: -1 on reset */
    short unmodeled_3fc2;
    int unmodeled_3fc4[7];                  /* +0x3FC4 */
    AlignedVector4 unmodeled_3fe0;          /* x, y, z zeroed on reset */
    int flags;                              /* +0x3FF0 */
    short unmodeled_3ff4;                   /* -1 on reset */
    unsigned char unmodeled_3ff6[0xA];
    float unmodeled_4000;
    unsigned char unmodeled_4004[0xC];
    int seIds[3];                           /* +0x4010: -1 on reset */
    unsigned char unmodeled_401c;           /* 1 on reset */
    unsigned char unmodeled_401d;
    short unmodeled_401e;
    int unmodeled_4020;                     /* 99 on reset */
    int unmodeled_4024;                     /* -1 on reset */
    short targetPriority;                   /* +0x4028 */
    short targetSlot;                       /* +0x402A: -1 when no target is held */
    unsigned char unmodeled_402c[4];
    AlignedVector4 unmodeled_4030;          /* x, y, z zeroed on reset */
    short unmodeled_4040;
    short groups[8];                        /* +0x4042: -1 on reset */
    unsigned char unmodeled_4052[2];
    int together[4];                        /* +0x4054: -1 on reset */
    short battle;                           /* +0x4064 */
    unsigned char unmodeled_4066[2];
    int effect;                             /* +0x4068 */
    int unmodeled_406c;
    short unmodeled_4070;
    unsigned char unmodeled_4072[2];
    int resourceId;                         /* +0x4074: -1 on reset */
    int walk[16];                           /* +0x4078: identity order on reset */
    unsigned char unmodeled_40b8[8];
} UwamonoState;

/* One 0x870-byte record of uwamonoRecords (field names follow tu050.c's
 * MapObjectRecord; the unmodeled_ members are only known by their reset
 * value here). */
typedef struct UwamonoRecord {
    short id;                               /* +0x000 */
    unsigned char kind;                     /* +0x002: MAP_KIND_* */
    unsigned char unmodeled_003;
    int key;                                /* +0x004 */
    int model;                              /* +0x008: -1 for none */
    unsigned char unmodeled_00c[4];
    AlignedVector4 position;                /* +0x010 */
    unsigned char unmodeled_020[0x10];
    AlignedVector4 unmodeled_030;           /* +0x030: reset to (0, 0, 0, 1) */
    AlignedVector4 rotation;                /* +0x040 */
    AlignedVector4 scale;                   /* +0x050 */
    AlignedVector4 pivot;                   /* +0x060 */
    float transparency;                     /* +0x070: 0..100 */
    unsigned char unmodeled_074[0x200 - 0x74];
    AlignedVector4 tilt;                    /* +0x200 */
    short moveEndMode;                      /* +0x210: -1 on reset */
    short moveStep;                         /* +0x212 */
    short moveCount;                        /* +0x214 */
    short turnStep;                         /* +0x216 */
    short turnCount;                        /* +0x218 */
    short scaleStep;                        /* +0x21A */
    short scaleCount;                       /* +0x21C */
    short transparencyStep;                 /* +0x21E: -1 on reset */
    unsigned char unmodeled_220[0x248 - 0x220];
    int eventSlot;                          /* +0x248: -1 on reset */
    short eventTimer;                       /* +0x24C */
    unsigned char unmodeled_24e[2];
    int event;                              /* +0x250 */
    int eventState;                         /* +0x254 */
    int eventStateArg;                      /* +0x258 */
    short tiltStep;                         /* +0x25C */
    short tiltCount;                        /* +0x25E */
    signed char rotationOrder;              /* +0x260: 5 on reset */
    unsigned char unmodeled_261[3];
    UwamonoPose *transform;                 /* +0x264 */
    unsigned char unmodeled_268[8];
    Matrix4 savedMatrix;                    /* +0x270: xglMatrixStackSave target on reset */
    UwamonoPose pose;                       /* +0x2B0: copied to *transform by func_001EECE0 */
    Matrix4 matrix;                         /* +0x2F0 */
    struct XenoActor *actor;                /* +0x330: released on a mode-1 reset */
    UwamonoState *motions;                  /* +0x334 */
    float form;                             /* +0x338: 1.0 in the ES form, else 5.0; also the effect's scale */
    short motion;                           /* +0x33C: 4 on reset */
    unsigned char unmodeled_33e[0x546 - 0x33E];
    short parent;                           /* +0x546: -1 when unset */
    unsigned char unmodeled_548[0x570 - 0x548];
    int flags;                              /* +0x570: 0xA on reset, 0x2 shown */
    int unmodeled_574;                      /* +0x574: 0 on reset */
    unsigned char unmodeled_578[8];
    float extent;                           /* +0x580: the actor's radius (tu072.c enepcStoreParams) */
    float height;                           /* +0x584 */
    float unmodeled_588;                    /* +0x588: 0 on reset */
    float unmodeled_58c;                    /* +0x58C: 1.0 on reset */
    signed char searchMode;                 /* +0x590 */
    unsigned char unmodeled_591;
    short elevatorMotion;                   /* +0x592: -1 on reset */
    int elevatorArg;                        /* +0x594: 0 on reset */
    unsigned char unmodeled_598[8];
    int unmodeled_5a0;                      /* +0x5A0: -1 on reset */
    int unmodeled_5a4;                      /* +0x5A4: -1 on reset */
    int unmodeled_5a8;                      /* +0x5A8: -1 on reset */
    unsigned char unmodeled_5ac[4];
    AlignedVector4 riderOffset;             /* +0x5B0: reset to (0, 0, 0, 1) */
    signed char talkMode;                   /* +0x5C0 */
    unsigned char unmodeled_5c1[0x618 - 0x5C1];
    int lookLink;                           /* +0x618: -1 on reset */
    short unmodeled_61c;                    /* +0x61C: -1 on reset */
    unsigned char unmodeled_61e[0x62C - 0x61E];
    signed char riderSide;                  /* +0x62C */
    unsigned char unmodeled_62d[3];
    int unmodeled_630;                      /* +0x630: -1 on reset */
    int unmodeled_634;                      /* +0x634: 0xFF on reset */
    void *effect;                           /* +0x638 */
    void *riders[2];                        /* +0x63C */
    int effectOwner;                        /* +0x644: -1 on reset */
    unsigned char unmodeled_648[0x870 - 0x648];
} UwamonoRecord;

/* UwamonoRecord.kind (the names of tu050.c's MapObjectRecord.kind). */
enum {
    MAP_KIND_FREE = 0,
    MAP_KIND_ACTOR = 1,
    MAP_KIND_MODEL = 2,
    MAP_KIND_FLAG = 3,
    MAP_KIND_ACTOR_STATIC = 4, /* an actor that is not moved by uwamonoUpdateRecord */
    MAP_KIND_PART = 5,         /* a part attached to the player (tu051.c) */
    MAP_KIND_EFFECT = 6,
    MAP_KIND_MOVER = 7,
    MAP_KIND_MODEL_REQUEST = 8 /* create only: uwamonoCreateRecord turns it into a model or frees it */
};

/* The game work block at gameWork as this file uses it: the state and map id
 * words (tu050.c MainWork.state, tu110.c SaveGameWork.mapId) and the three
 * 512-byte runs func_001EEF60 and func_001EEF98 clear. */
typedef struct SceneWork {
    unsigned char unmodeled_0000[0x14];
    int state;                          /* +0x0014: -1 after a scene load */
    int mapId;                          /* +0x0018 */
    unsigned char unmodeled_001c[0x1520 - 0x1C];
    unsigned char clearedRuns[3][0x200]; /* +0x1520 */
} SceneWork;

extern UwamonoRecord uwamonoRecords[];
extern int uwamonoCurrentIndex;
extern UwamonoState D_002BD600[];
extern SceneWork gameWork;
extern unsigned char D_003C6DC0[];      /* the scene file buffer */
extern char D_006895C0[];               /* "sc0000.sb": the digits are rewritten */
extern const char D_0069D1D0[];         /* "cf\\%s" */
extern const char D_006895D0[];         /* "cf\\xx\\%sl" */
extern int uwamonoEventIds[];
extern float D_0033EE90[];
extern struct EnepcParamSet *enepcParamSets; /* the enepc parameter presets (readyEnepcData) */
extern unsigned short *DataSpline;
/*
 * Declared as an incomplete array, not a scalar: cc1 -G8 would otherwise
 * classify a plain scalar extern as small data and emit a gp-relative load,
 * but the original reads this word through absolute hi/lo instructions.
 */
extern unsigned short *D_002B7BBC[];

void func_A9C938(void);
void scriptRegisterModules(void);
void sefSetCurrentOwner(int owner);
void resApplyCfFile(int id);
int sprintf(char *destination, const char *format, ...);
int xglCdReadFile(const char *name, void *buffer, int mode, int flags);
unsigned short xglGetLanguageCode(char *code);
int func_A9CEE8(unsigned char *scene, unsigned char *end);
void func_A9CAD0(unsigned char *scene);
void uwamonoResetRecords(int index, signed char mode);
void XenoActorRelease(struct XenoActor *actor);
void uwamonoClearMoveResetWord(int index);
void uwamonoClearRotateResetWord(int index);
void xglMatrixStackUnit(void);
void xglMatrixStackSave(Matrix4 matrix);
void uwamonoResetPoints(void);

#define MAP_RECORD_COUNT 0x100
#define UWAMONO_STATE_COUNT 32

/* Loads a uwamono scene: copies the pose of every model record to its
 * transform, registers the script modules, with mode 1 makes id the sef owner,
 * reads cf\scNNNN.sb and the language file cf\xx\scNNNNl (NNNN are the digits
 * of id), makes id the current map, shows the current record and resets the
 * records through uwamonoResetRecords(-1, 1). The scene parse failing is a
 * debugger trap (DEBUG_BREAK, class ee-assert-break). */
void func_001EECE0(int id, signed char mode)
{
    char path[0x20];
    char languagePath[0x20];
    char *sceneName = D_006895C0;
    int i;
    int slot;
    UwamonoRecord *record;
    int size;

    for (i = 0; i < MAP_RECORD_COUNT; i++) {
        record = &uwamonoRecords[i];
        if (record->kind == MAP_KIND_MODEL && record->transform != NULL) {
            *record->transform = record->pose;
        }
    }
    func_A9C938();
    scriptRegisterModules();
    if (mode == 1) {
        sefSetCurrentOwner(id);
        resApplyCfFile(id);
    }
    sceneName[2] = id / 1000 % 10 + '0';
    sceneName[3] = id / 100 % 10 + '0';
    sceneName[4] = id / 10 % 10 + '0';
    sceneName[5] = id % 10 + '0';
    sprintf(path, D_0069D1D0, sceneName);
    size = xglCdReadFile(path, D_003C6DC0, 0, 0);
    sprintf(languagePath, D_006895D0, sceneName);
    xglGetLanguageCode(languagePath + 3);
    xglCdReadFile(languagePath, D_003C6DC0 + size, 0, 0);
    if (!func_A9CEE8(D_003C6DC0, D_003C6DC0 + size)) {
        DEBUG_BREAK();
    }
    func_A9CAD0(D_003C6DC0);
    gameWork.mapId = id;
    uwamonoRecords[uwamonoCurrentIndex].flags |= 2;
    gameWork.state = -1;
    for (slot = 0; slot < 8; slot++) {
        uwamonoEventIds[slot] = -1;
    }
    uwamonoResetRecords(-1, 1);
}

#define CLEAR_RUN_LENGTH 512

/* Clears the second and third 512-byte runs of the work block. */
void func_001EEF60(void)
{
    int i;

    for (i = 0; i < CLEAR_RUN_LENGTH; i++) {
        gameWork.clearedRuns[1][i] = 0;
        gameWork.clearedRuns[2][i] = 0;
    }
}

/* Clears the first 512-byte run of the work block. */
void func_001EEF98(void)
{
    int i;

    for (i = 0; i < CLEAR_RUN_LENGTH; i++) {
        gameWork.clearedRuns[0][i] = 0;
    }
}

/* Resets uwamono records: one record (index >= 0) or all 256 (index -1). Mode 1
 * keeps the MAP_KIND_ACTOR_STATIC and MAP_KIND_PART records and releases each
 * reset record's actor through XenoActorRelease. With index -1 the 32
 * D_002BD600 state blocks are reset too, except the one whose owner is the kept
 * MAP_KIND_ACTOR_STATIC record, then uwamonoResetPoints runs and the D_0033EE90
 * radii and enepcParamSets are cleared. */
void uwamonoResetRecords(int index, signed char mode)
{
    int kept;
    int i;
    int j;
    int k;
    int first;
    int last;
    UwamonoRecord *record;
    UwamonoState *state;
    Vector4 *vector;

    /* No record kept yet: -100 rather than -1, which would match every free
     * state block (owner -1) below. */
    kept = -100;
    first = index;
    last = index + 1;
    if (index == -1) {
        first = 0;
        last = MAP_RECORD_COUNT;
    }
    DataSpline = D_002B7BBC[0];
    for (i = first; i < last; i++) {
        record = &uwamonoRecords[i];
        if (mode == 1) {
            if (record->kind == MAP_KIND_ACTOR_STATIC) {
                kept = i;
                continue;
            }
            if (record->kind == MAP_KIND_PART) {
                continue;
            }
        }
        record->kind = MAP_KIND_FREE;
        record->model = -1;
        record->parent = -1;
        record->tiltStep = -1;
        record->eventTimer = -1;
        record->transparencyStep = -1;
        record->scaleStep = -1;
        record->turnStep = -1;
        record->moveStep = -1;
        record->motion = 4;
        record->rotationOrder = 5;
        record->moveEndMode = -1;
        record->position.x = record->position.y = record->position.z = 0.0f;
        record->position.w = 1.0f;
        record->rotation.x = record->rotation.y = record->rotation.z = 0.0f;
        record->rotation.w = 1.0f;
        record->scale.x = record->scale.y = record->scale.z = 1.0f;
        record->scale.w = 1.0f;
        record->pivot.x = record->pivot.y = record->pivot.z = record->transparency = 0.0f;
        record->pivot.w = 1.0f;
        record->extent = record->height = record->unmodeled_588 = 0.0f;
        record->unmodeled_58c = 1.0f;
        record->flags = 0xA;
        record->unmodeled_634 = 0xFF;
        record->searchMode = 0;
        record->form = 1.0f;
        record->unmodeled_030.x = record->unmodeled_030.y = record->unmodeled_030.z = 0.0f;
        record->unmodeled_030.w = 1.0f;
        record->tilt.x = record->tilt.y = record->tilt.z = 0.0f;
        record->tilt.w = 1.0f;
        record->unmodeled_574 = 0;
        record->riderSide = 0;
        record->unmodeled_630 = -1;
        record->transform = NULL;
        record->talkMode = 0;
        record->lookLink = -1;
        record->unmodeled_61c = -1;
        if (mode == 1 && record->actor != NULL) {
            XenoActorRelease(record->actor);
        }
        record->actor = NULL;
        record->motions = NULL;
        record->elevatorMotion = -1;
        record->elevatorArg = 0;
        record->riders[0] = NULL;
        record->riders[1] = NULL;
        record->effect = NULL;
        record->effectOwner = -1;
        record->unmodeled_5a0 = -1;
        record->riderOffset.x = record->riderOffset.y = record->riderOffset.z = 0.0f;
        record->riderOffset.w = 1.0f;
        uwamonoClearMoveResetWord(i);
        uwamonoClearRotateResetWord(i);
        record->unmodeled_5a8 = -1;
        record->unmodeled_5a4 = -1;
        record->eventSlot = -1;
        xglMatrixStackUnit();
        xglMatrixStackSave(record->savedMatrix);
    }
    if (index != -1) {
        return;
    }
    for (i = 0; i < UWAMONO_STATE_COUNT; i++) {
        state = &D_002BD600[i];
        if (state->owner == kept) {
            continue;
        }
        state->owner = -1;
        state->index = i;
        state->unmodeled_0008 = -1;
        state->action = 0;
        state->unmodeled_000e = 0;
        state->unmodeled_0010.x = state->unmodeled_0010.y = state->unmodeled_0010.z = 0.0f;
        state->home.x = state->home.y = state->home.z = state->speed = 0.0f;
        state->floatParams48[1] = state->floatParams48[0] = state->angle = state->floatParams[3] =
            state->floatParams[2] = state->floatParams[1] = state->floatParams[0] = 0.0f;
        state->shortParams[0] = 0;
        state->shortParams[1] = 0;
        state->shortParams[2] = 0;
        state->shortParams[3] = 0;
        state->trailStep = 0;
        state->timer = 0;
        state->timerLimit = 0;
        state->unmodeled_005e = 0;
        state->mode = 0;
        state->unmodeled_0060 = 0;
        for (j = 0; j < 2; j++) {
            for (k = 0; k < 8; k++) {
                state->names[j][k] = 0;
            }
            state->unmodeled_0072[j] = 0;
        }
        state->unmodeled_0076 = 0;
        state->unmodeled_0077 = 0;
        state->unmodeled_0078 = 0;
        state->unmodeled_007a = 0;
        state->trailCount = 0;
        for (j = 0; j < 256; j++) {
            state->trail[j][0] = state->trail[j][1] = state->trail[j][2] = 0.0f;
        }
        state->unmodeled_401c = 1;
        for (j = 0; j < 256; j++) {
            state->points[j].coords[0] = state->points[j].coords[1] =
                state->points[j].coords[2] = 0.0f;
            state->points[j].coords[3] = 1.0f;
            for (k = 0; k < 4; k++) {
                state->pointLinks[j][k] = 0;
                state->pointLinkValues[j][k] = 0;
            }
        }
        state->pointCount = 0;
        state->routeNodeCount = 0;
        for (j = 0; j < 64; j++) {
            state->routeNodes[j].component[0] = state->routeNodes[j].component[1] =
                state->routeNodes[j].component[2] = 0.0f;
            state->routeNodes[j].component[3] = 1.0f;
            state->routeLinkCounts[j] = 0;
            state->routeParents[j] = 0;
            for (k = 0; k < 4; k++) {
                state->routeLinks[j][k] = 0;
            }
        }
        for (j = 0; j < 8; j++) {
            state->groups[j] = -1;
        }
        for (j = 0; j < 4; j++) {
            state->together[j] = -1;
        }
        state->unmodeled_30a0 = 0;
        state->unmodeled_30a2 = 0;
        state->unmodeled_30a4 = 0;
        /* The zero is a loop-local value: the original keeps it in a register
         * loaded just before the loop and walks the entries forwards. */
        vector = state->path;
        for (j = 0; j < 128; j++, vector++) {
            float zero = 0.0f;

            vector->x = zero;
        }
        state->unmodeled_3fc4[0] = 0;
        state->unmodeled_3fc4[1] = 0;
        state->unmodeled_3fc4[2] = 0;
        state->unmodeled_3fc4[3] = 0;
        state->unmodeled_3fc4[4] = 0;
        state->unmodeled_3fc4[5] = 0;
        state->unmodeled_3fc4[6] = 0;
        state->unmodeled_3fe0.x = state->unmodeled_3fe0.y = state->unmodeled_3fe0.z = 0.0f;
        state->unmodeled_3ff4 = -1;
        state->unmodeled_4000 = 0.0f;
        state->unmodeled_401e = 0;
        state->id = 0;
        state->groundId = -1;
        state->footId = -1;
        state->unmodeled_4024 = -1;
        state->flags = 0;
        state->pathStep = -1;
        state->unmodeled_3fc2 = 0;
        state->seIds[2] = -1;
        state->seIds[1] = -1;
        state->seIds[0] = -1;
        state->battle = 0;
        state->unmodeled_4020 = 99;
        for (j = 0; j < 16; j++) {
            state->walk[j] = j;
        }
        state->resourceId = -1;
        state->targetPriority = 0;
        state->targetSlot = -1;
        state->unmodeled_4030.x = state->unmodeled_4030.y = state->unmodeled_4030.z = 0.0f;
        state->effect = 0;
        state->unmodeled_406c = 0;
        state->unmodeled_4070 = 0;
    }
    uwamonoResetPoints();
    for (i = 0; i < 10; i++) {
        D_0033EE90[i] = 0.0f;
    }
    enepcParamSets = NULL;
}
