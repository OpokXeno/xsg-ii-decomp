#ifndef INCLUDE_MAIN_YTACHI_TEST_H
#define INCLUDE_MAIN_YTACHI_TEST_H

#include "shared.h"
#include "main/xenoActor.h"
#include "main/undulate.h"

/* Types and data of src/main/ytachi_test.c that other files use: the uwamono record table and its state blocks. */

#define MAP_RECORD_COUNT 0x100
#define UWAMONO_STATE_COUNT 32

/* The original reads and writes these vectors with alias set 0, as named components or as a float array: a union view of
 * a vector keeps the loads behind the stores before them (the field loop and the map object natives). */

/* The 0x40-byte pose block a record's transform pointer refers to; copied whole,
 * row 3 is the translation (init_uwamono_sys.c). */
typedef struct UwamonoTransform {
    Matrix4 matrix;
} UwamonoTransform;

/* The pose is copied whole as quadword-aligned rows (mapSceneScriptLoad) and as a 4-aligned matrix (NyuruMatrix). */
typedef union UwamonoPose {
    Matrix4 matrix;
    AlignedVector4 row[4];
    UwamonoTransform copy;
} UwamonoPose;

/* A point of UwamonoState: a path point or a route node (Homing_Add links the nodes). The reset writes it through
 * the union: that keeps the compiler from moving the stores past the stores that follow them (the link counts for
 * the nodes). Both kinds are copied as 4-aligned vectors (uwamonoDrawPointPath, uwamonoDrawStateLinks). */
typedef union UwamonoPathPoint {
    float coords[4];
    AlignedVector4 vector;
    Vector4 point;
} UwamonoPathPoint;

/* One 0x40C0-byte motion/state block of uwamonoStates, sizeof 0x40C0, 16-aligned
 * (uwamonoCreateRecord: n * 0x40C0); a record's motions pointer (+0x334) refers
 * to it. Field names follow src/main/enemy_1.c and set_motion.c; the unmodeled_
 * members are only known by their reset value. */
typedef struct UwamonoState {
    int owner;                              /* +0x0000: the record index, -1 when free */
    short index;                            /* +0x0004: the block's own index */
    unsigned char unmodeled_0006[2];
    int row;                                /* +0x0008: the key row of the record in the row flags, -1 on reset */
    signed char action;                     /* +0x000C: the current mode, set by Enemy_ActionReady (0 init, 1 wander ... 13) */
    unsigned char unmodeled_000d;
    short focusIndex;                       /* +0x000E: the record the slot aims at (Enemy_Found, Enemy_Chase) */
    AlignedVector4 target;                  /* +0x0010: the route point the slot moves toward; x, y, z zeroed on reset */
    /* A one-member union: enepcAlgorithm (runtime_motion_face.c) stores the area with alias set 0, so the state
     * pointer is reloaded after each store. */
    union {
        AlignedVector4 area;                /* +0x0020: centre of the area the slot keeps to; x, y, z zeroed on reset */
    };
    float areaSize;                         /* +0x0030: radius of the area */
    float wanderStep;                       /* +0x0034 */
    float leashRange;                       /* +0x0038 */
    float moveSpeed;                        /* +0x003C */
    float pursuitSpeed;                     /* +0x0040 */
    float fanWidth;                         /* +0x0044: radians */
    float fanLength;                        /* +0x0048 */
    float noticeRange;                      /* +0x004C */
    unsigned short wanderTime;              /* +0x0050 */
    short pickCountA;                       /* +0x0052: the upper bound of the first random pick */
    unsigned short waitTime;                /* +0x0054 */
    short pickCountB;                       /* +0x0056: the upper bound of the second random pick */
    short trailStep;                        /* +0x0058 */
    unsigned short timer;                   /* +0x005A: ticks since the current action started */
    short timerLimit;                       /* +0x005C */
    signed char alert;                      /* +0x005E: 99 at the start of every ACT_updateEnemy pass; Enemy_Chase tests it against 64, 88, 96 and 99 */
    unsigned char mode;                     /* +0x005F: the behaviour the slot was set up with (enepcSetParams), a UWAMONO_MODE_* value; Enemy_Pause and Enemy_Threat pick the next action from it */
    unsigned short cooldown;                /* +0x0060: ticks left in the cooldown mode */
    signed char pickRow[2][8];              /* +0x0062: the two random pick sets */
    short pickIndex[2];                     /* +0x0072 */
    unsigned char reactionKind;             /* +0x0076 */
    unsigned char reactionFlags;            /* +0x0077: bit 0 is set by Enemy_ActionReady's alert mode and cleared when the mode ends */
    short unmodeled_0078;
    short unmodeled_007a;
    /* A one-member union: Enemy_Chase and the other enemy handlers read the id with alias set 0. */
    union {
        int id;                             /* +0x007C */
    };
    short groundId;                         /* +0x0080: map block 0x100 entry of the actor's ground, -1 on reset */
    short footId;                           /* +0x0082: map block 0x700 entry of the footstep sound, -1 on reset */
    unsigned char unmodeled_0084[0xC];
    /* The reset walks the rows as floats; enemy_system_init.c's TM_Script_Spline_Add stores through the Vector4
     * view with alias set 0. */
    union {
        float trail[256][4];                /* +0x0090: x, y, z zeroed on reset */
        Vector4 trailPoints[256];
    };
    short trailCount;                       /* +0x1090 */
    unsigned char unmodeled_1092[0xE];
    UwamonoPathPoint points[256];                 /* +0x10A0: path points, reset to (0, 0, 0, 1) */
    short pointLinks[256][4];               /* +0x20A0 */
    short pointLinkValues[256][4];          /* +0x28A0 */
    short routeIndex;                       /* +0x30A0 */
    short routePrev;                        /* +0x30A2 */
    short motionCol;                        /* +0x30A4 */
    short pointCount;                       /* +0x30A6 */
    unsigned char unmodeled_30a8[8];
    UwamonoPathPoint routeNodes[64];               /* +0x30B0: reset to (0, 0, 0, 1) */
    short routeLinks[64][4];                /* +0x34B0: per node, the linked node ids */
    short routeLinkCounts[64];              /* +0x36B0 */
    short routeParents[64];                 /* +0x3730: the node Homing_Add linked from,
                                             * -1 when added without one; 0 on reset */
    short routeNodeCount;                   /* +0x37B0 */
    unsigned char unmodeled_37b2[0xE];
    Vector4 path[128];                      /* +0x37C0: only x zeroed on reset */
    short pathStep;                         /* +0x3FC0: the ring head, -1 on reset */
    short pathTail;                         /* +0x3FC2: the ring tail */
    int unmodeled_3fc4[3];                  /* +0x3FC4 */
    float stepFrame;                        /* +0x3FD0 */
    float soundCue;                         /* +0x3FD4 */
    int unmodeled_3fd8[2];                  /* +0x3FD8 */
    AlignedVector4 probe;                   /* +0x3FE0: x, y, z zeroed on reset */
    int flags;                              /* +0x3FF0 */
    short unmodeled_3ff4;                   /* -1 on reset */
    unsigned char unmodeled_3ff6[0xA];
    DoublewordAlignedVector4 homePoint;     /* +0x4000: the point the slot moves toward; x zeroed on reset */
    int seIds[3];                           /* +0x4010: -1 on reset */
    unsigned char unmodeled_401c;           /* 1 on reset */
    unsigned char unmodeled_401d;
    short noticeCooldown;                   /* +0x401E: frames before the slot notices again */
    int scanState;                          /* +0x4020: UWAMONO_SCAN_*, 99 on reset */
    int targetRecord;                       /* +0x4024: the record TM_Check_Before_Eyes found, -1 on reset */
    /* One-member unions: the original reads and writes these two fields with alias set 0 (Actor_LookAt_Set). */
    union {
        short targetPriority;               /* +0x4028: 3 keeps the current target */
    };
    union {
        short targetSlot;                   /* +0x402A: -1 when no target is held */
    };
    unsigned char unmodeled_402c[4];
    AlignedVector4 targetVector;            /* +0x4030: the point the held target is moved to; x, y, z zeroed on reset */
    short unmodeled_4040;
    short groups[8];                        /* +0x4042: -1 on reset */
    unsigned char unmodeled_4052[2];
    int together[4];                        /* +0x4054: -1 on reset */
    short battle;                           /* +0x4064 */
    unsigned char unmodeled_4066[2];
    void *effect;                           /* +0x4068: the effect record */
    int unmodeled_406c;
    short unmodeled_4070;
    unsigned char unmodeled_4072[2];
    int resourceId;                         /* +0x4074: -1 on reset */
    int walk[16];                           /* +0x4078: identity order on reset */
    unsigned char unmodeled_40b8[8];
} UwamonoState;

/* The actor of a record and its motion state block: the pair at +0x330 of UwamonoRecord, which the slot loops of
 * the enemy code address as one unit (&uwamonoRecords[i].body). */
typedef struct UwamonoBody {
    struct XenoActor *actor;                /* +0x00: released on a mode-1 reset */
    UwamonoState *motions;                  /* +0x04 */
} UwamonoBody;

/* One 0x870-byte record of uwamonoRecords, the map object table of the uwamono system. The labels
 * uwamonoSlotTable (+0x330) and uwamonoStateSlots (+0x334) are the actor and motions fields of the body of record 0. */
typedef struct UwamonoRecord {
    short id;                               /* +0x000: the key uwamonoFindIndexByShortKey searches */
    unsigned char kind;                     /* +0x002: MAP_KIND_* */
    unsigned char unmodeled_003;
    /* A one-member union: uwamonoInitRecord (runtime_menu.c) reads the key with alias set 0, after the stores
     * before it. */
    union {
        int key;                            /* +0x004: bit number in the map flag sets */
    };
    int type;                               /* +0x008: extra-object type and model index (mapObjectResources[type + 2]): 0-3 traps "ExTrp",
                                             * 4-6 treasure boxes "ExTbx", 7-9 save points "ExSav", 10-11 dummies, 12 and up items "ExItm";
                                             * -1 has no model */
    float pendingValue;                     /* +0x00C: stored by uwamonoSetPendingValue while uwamonoDrawDisabled is 0 */
    /* A union of the vector and its float array: the field loop (game_data.c) writes them as arrays, which gives the
     * address arithmetic and the alias set 0 stores of the original. */
    union {
        AlignedVector4 position;            /* +0x010 */
        float positionCoords[4];
    };
    /* A one-member union: the MAP_KIND_MOVER case of DispMapObject (game_map_object.c) stores the position into it with
     * alias set 0, which keeps the loads after the store behind it. */
    union {
        Vector4 previous;                   /* +0x020 */
    };
    AlignedVector4 velocity;                /* +0x030: reset to (0, 0, 0, 1); y is the fall speed CheckUwamonoHeight writes */
    union {
        AlignedVector4 rotation;            /* +0x040: angles; y is the heading */
        float rotationCoords[4];
    };
    /* A union of the vector and its float array: uwamonoStoreCurrent (game_data.c) stores the form's scale through the
     * array, which keeps the address arithmetic and the store order of the original. */
    union {
        AlignedVector4 scale;               /* +0x050: x, y, z set by the player's form */
        float scaleCoords[4];
    };
    AlignedVector4 pivot;                   /* +0x060 */
    float transparency;                     /* +0x070: percent, 0 opaque */
    unsigned char unmodeled_074[0xC];
    AlignedVector4 moveFrom;                /* +0x080: end points of the running move */
    AlignedVector4 moveTo;                  /* +0x090 */
    /* Unions of the vector and its float array: the objx natives (uwamono_objx.c) address the angles by index. */
    union {
        AlignedVector4 turnFrom;            /* +0x0A0: end angles of the running turn; a sector record keeps its angle in x */
        float turnFromCoords[4];
    };
    union {
        AlignedVector4 turnTo;              /* +0x0B0: a sector record keeps its width in x, a circle record its radius */
        float turnToCoords[4];
    };
    Vector4 scaleFrom;                      /* +0x0C0 */
    Vector4 scaleTo;                        /* +0x0D0 */
    float transparencyFrom;                 /* +0x0E0 */
    float transparencyTo;                   /* +0x0E4 */
    unsigned char unmodeled_0e8[8];
    Vector4Elements startA[3];                   /* +0x0F0: light blend start values (uwamonoLightBlendStep) */
    Vector4Elements followA[3];                  /* +0x120: the light direction the blend runs to */
    Vector4Elements startB[3];                   /* +0x150: a union view (alias set 0), so uwamonoStartAreaBlend's cursor stores stay behind these copies */
    Vector4Elements followB[3];                  /* +0x180 */
    Vector4Elements startD;                      /* +0x1B0 */
    Vector4Elements followD;                     /* +0x1C0 */
    float blendStart;                       /* +0x1D0 */
    float blendEnd;                         /* +0x1D4 */
    unsigned char unmodeled_1d8[8];
    Vector4Elements startC;                      /* +0x1E0 */
    Vector4Elements followC;                     /* +0x1F0 */
    AlignedVector4 tilt;                    /* +0x200 */
    short moveEndMode;                      /* +0x210: mode for XenoActorCheckMode when the move ends, -1 for none */
    short moveStep;                         /* +0x212 */
    short moveCount;                        /* +0x214 */
    short turnStep;                         /* +0x216 */
    short turnCount;                        /* +0x218 */
    short scaleStep;                        /* +0x21A */
    short scaleCount;                       /* +0x21C */
    short transparencyStep;                 /* +0x21E: -1 on reset */
    short transparencyCount;                /* +0x220 */
    short cursorA[3];                       /* +0x222 */
    short countA[3];                        /* +0x228 */
    short cursorB[3];                       /* +0x22E */
    short countB[3];                        /* +0x234 */
    short cursorD;                          /* +0x23A */
    short countD;                           /* +0x23C */
    short cursorE;                          /* +0x23E */
    short totalE;                           /* +0x240 */
    short cursorC;                          /* +0x242 */
    short countC;                           /* +0x244 */
    unsigned char unmodeled_246[2];
    int lightArea;                          /* +0x248: the map light set the blend runs to, -1 on reset */
    short eventTimer;                       /* +0x24C */
    unsigned char unmodeled_24e[2];
    int event;                              /* +0x250: 0 state change, 1 player model change, -1 none */
    int eventState;                         /* +0x254 */
    int eventStateArg;                      /* +0x258 */
    short tiltStep;                         /* +0x25C */
    short tiltCount;                        /* +0x25E */
    signed char rotationOrder;              /* +0x260: the order the rotation angles are applied in, 5 on reset */
    unsigned char unmodeled_261[3];
    /* A one-member union: HitCheckBoxCorner and NyuruMatrix (init_uwamono_sys.c) load the pointer with alias set 0. */
    union {
        UwamonoPose *transform;             /* +0x264: the table row the model draws with */
    };
    unsigned char unmodeled_268[8];
    UwamonoPose savedMatrix;                /* +0x270: xglMatrixStackSave target on reset */
    UwamonoPose pose;                       /* +0x2B0: copied to *transform by mapSceneScriptLoad */
    Matrix4 matrix;                         /* +0x2F0: the placement matrix, its last row the position */
    UwamonoBody body;                       /* +0x330: the actor and its motion state */
    float form;                             /* +0x338: UWAMONO_FORM_ES while riding an ES, else 5.0; also the effect's scale */
    short motion;                           /* +0x33C: 4 on reset; 1 once the form is chosen */
    unsigned char unmodeled_33e[2];
    float moveX[32];                        /* +0x340: move curve, uwamonoAddMoveStep */
    float moveY[32];                        /* +0x3C0 */
    short moveStepCount;                    /* +0x440 */
    unsigned char unmodeled_442[2];
    float turnX[32];                        /* +0x444: rotate curve, uwamonoAddRotateStep */
    float turnY[32];                        /* +0x4C4 */
    short turnStepCount;                    /* +0x544 */
    short parent;                           /* +0x546: index of the record this one follows, stands on or is attached to, -1 when unset */
    short attached;                         /* +0x548 */
    unsigned char unmodeled_54a[6];
    union {
        Vector4 savedPosition;              /* +0x550: offset of the record from its parent */
        float savedPositionCoords[4];
    };
    union {
        Vector4 savedRotation;              /* +0x560 */
        float savedRotationCoords[4];
    };
    int flags;                              /* +0x570: 0xA on reset, 0x2 shown */
    int landedFlags;                        /* +0x574: CheckUwamonoHeight; 0 on reset */
    unsigned char unmodeled_578[8];
    /* A one-member union: TM_Check_Before_Eyes reads the size with alias set 0. */
    union {
        AlignedVector4 extent;              /* +0x580: x is the form's size (the actor's radius), y the height; z 0 and w 1.0 on reset */
    };
    signed char searchMode;                 /* +0x590: 0 or 1 by object type; 0 inactive, 1 and 2 dispatch in CrossPointUwamonoAt */
    signed char searchShape;                /* +0x591: selects the search test and the collision test; setLocater's type */
    short elevatorMotion;                   /* +0x592: -1 on reset */
    int elevatorArg;                        /* +0x594: setElevator argument 3; 0 on reset */
    unsigned char searchFlag;               /* +0x598 */
    unsigned char unmodeled_599[3];
    int level;                              /* +0x59C: the jump height step */
    int effectId;                           /* +0x5A0: effect or motion started when searched, -1 on reset */
    /* One-member unions: uwamonoStartPairMove and uwamonoTriggerRecord (uwamono_record.c) read the partner indices with
     * alias set 0, so every use of one reloads it after the stores before it. */
    union {
        int linkA;                          /* +0x5A4: partner records set by the kowaremono native, -1 on reset */
    };
    union {
        int linkB;                          /* +0x5A8 */
    };
    unsigned char unmodeled_5ac[4];
    /* A one-member union: the rider placement of DispMapObject (game_map_object.c) reads it with alias set 0. */
    union {
        AlignedVector4 focusOffset;         /* +0x5B0: from the position to the point that is marked, aimed at and ridden; reset to (0, 0, 0, 1) */
    };
    signed char motionKind;                 /* +0x5C0: 1 = moves between two points, else rotates */
    signed char searchState;                /* +0x5C1 */
    signed char busy;                       /* +0x5C2: set while the door motion runs */
    unsigned char unmodeled_5c3;
    short timer;                            /* +0x5C4 */
    unsigned char unmodeled_5c6[0x5D0 - 0x5C6];
    Vector4Elements closedPosition;         /* +0x5D0: the two states of a door; uwamonoStartMove takes the pair as an array */
    Vector4Elements openPosition;                   /* +0x5E0 */
    Vector4 closedRotation;                 /* +0x5F0 */
    Vector4 openRotation;                   /* +0x600 */
    float speed;                            /* +0x610 */
    float swingLength;                      /* +0x614 */
    int lookLink;                           /* +0x618: index of the record this one looks at or is linked to, -1 when none */
    short eventRow;                         /* +0x61C: the event index of a kind 8 record, -1 on reset */
    unsigned char unmodeled_61e[2];
    int triggerSound;                       /* +0x620: seVoiceStartDefault id played when the record is triggered, -1 none */
    int moveSound;                          /* +0x624: played when the pair move starts */
    unsigned char unmodeled_628[4];
    signed char state;                      /* +0x62C: 0 out of reach, 1 selected, 2 in reach; the marker handle shown */
    unsigned char unmodeled_62d[3];
    int unmodeled_630;                      /* +0x630: -1 on reset */
    int cellMask;                           /* +0x634: map cells the record belongs to, 0xFF on reset */
    void *handles[3];                       /* +0x638: [0] the sef effect, [1] and [2] the marker handles; released with the record */
    int releaseNextIndex;                   /* +0x644: the record released after this one, -1 for none */
    /* +0x648..+0x870: the XenoActor state a record with an actor keeps while the actor pool is rebuilt (Game_Data_Push
     * saves it, Game_Data_Pop puts it back); the fields have the names and types of the XenoActor ones. The saved light
     * set overlays the UnduCheck block of a record without an actor. */
    MapCollisionObject savedProbe;       /* +0x648: the actor's collision object (XenoActor +0x740..+0x780), copied whole */
    unsigned char unmodeled_688[8];
    union {
        XenoLight savedLight;               /* +0x690 */
        struct {
            unsigned char unmodeled_690[0x740 - 0x690];
            MapCollisionObject collision;   /* +0x740: the object UnduCheck takes (UnduParamInit resets it); its flags are the hit flags */
            unsigned char unmodeled_780[0x810 - 0x780];
        };
    };
    /* One-member unions: Game_Data_Push stores the two blocks with alias set 0, which keeps the loads of the actor
     * fields after them behind the stores. */
    union {
        DoublewordAlignedVector4 savedColor;      /* +0x810 */
    };
    union {
        DoublewordAlignedVector4 savedColorBlock; /* +0x820 */
    };
    float savedFog[4];                      /* +0x830: fogA..fogD */
    int savedDrawMode;                      /* +0x840 */
    int savedDrawFlags;                     /* +0x844 */
    float savedDrawParam;                   /* +0x848 */
    float savedModeValue4;                  /* +0x84C */
    u32 savedFlags;                         /* +0x850 */
    float savedFrame;                       /* +0x854 */
    int savedMode;                          /* +0x858 */
    int savedSlot;                          /* +0x85C */
    int savedRequestedMode;                 /* +0x860 */
    float savedMotionSpeed;                 /* +0x864: motion[0].speed */
    int savedMotionRange[2];                /* +0x868: motion[0].range */
} UwamonoRecord;

/* UwamonoRecord.kind (the kinds of a map object record). */
enum {
    MAP_KIND_FREE = 0,
    MAP_KIND_ACTOR = 1,
    MAP_KIND_MODEL = 2,
    MAP_KIND_FLAG = 3,
    MAP_KIND_ACTOR_STATIC = 4, /* an actor that is not moved by ACT_updateEnemy */
    MAP_KIND_PART = 5,         /* a part attached to the player (game_camera.c) */
    MAP_KIND_EFFECT = 6,
    MAP_KIND_MOVER = 7,
    MAP_KIND_MODEL_REQUEST = 8 /* create only: uwamonoCreateRecord turns it into a model or frees it */
};

/* Named values of the map object (uwamono) enemy code. */
enum UwamonoLimit {
    UWAMONO_RING_SIZE = 128,
    UWAMONO_SPLINE_STEPS = 32    /* GetBSplineLoop steps per route point */
};

enum UwamonoScanState {
    UWAMONO_SCAN_RECORD = 0,     /* a ready record overlaps the probed point */
    UWAMONO_SCAN_NO_GROUND = 64, /* the map found no ground under the point */
    UWAMONO_SCAN_GROUND = 99     /* the map found ground under the point */
};

/*
 * The mode of a slot (UwamonoState.action) and the action it is entered with are the
 * same value: Enemy_ActionReady stores its argument in it; UwamonoState.mode holds the same kind of value. The names
 * follow the handler each value dispatches to in ACT_updateEnemy.
 */
enum UwamonoMode {
    UWAMONO_MODE_INIT = 0,          /* Enemy_Init, then the next action */
    UWAMONO_MODE_WANDER = 1,        /* Enemy_Pause: random walk in the area */
    UWAMONO_MODE_WAIT = 2,          /* Enemy_Threat */
    UWAMONO_MODE_ANCHOR = 3,        /* Enemy_Route */
    UWAMONO_MODE_AIM = 4,           /* Enemy_Found */
    UWAMONO_MODE_PURSUE = 5,        /* Enemy_Chase */
    UWAMONO_MODE_COOLDOWN = 6,      /* Enemy_Stiff */
    UWAMONO_MODE_SCRIPT = 8,        /* Enemy_Unique */
    UWAMONO_MODE_ALERT = 9,         /* Enemy_Damage_Explosion; entered with the cooldown and effect set up */
    UWAMONO_MODE_ROUTE = 10,        /* Enemy_Escape */
    UWAMONO_MODE_SPLINE = 11,       /* Enemy_Route_Chase */
    UWAMONO_MODE_START = 12,        /* entered by Enemy_Init; moves on to a wander or a route */
    UWAMONO_MODE_SPLINE_START = 13  /* moves on to the spline mode */
};

extern UwamonoRecord uwamonoRecords[];

/* The labels uwamonoSlotTable (uwamonoRecords + 0x330) and uwamonoStateSlots (+ 0x334) name the actor and the motions of the
 * first record; the code that reaches them through these symbols (a different relocation than uwamonoRecords) indexes them
 * with the 0x870-byte record stride. */
typedef struct UwamonoSlot {
    UwamonoBody body;                       /* +0x00 */
    unsigned char unmodeled_08[sizeof(UwamonoRecord) - 8];
} UwamonoSlot;

typedef struct UwamonoStateSlot {
    UwamonoState *motions;                  /* +0x00 */
    unsigned char unmodeled_04[sizeof(UwamonoRecord) - 4];
} UwamonoStateSlot;

extern UwamonoSlot uwamonoSlotTable[];
extern UwamonoStateSlot uwamonoStateSlots[];

/* One 16-byte row of the key table uwamonoKeyRowTable (enemyorder.dat; uwamonoCreateRecord searches it by its fourth argument). */
typedef struct UwamonoKeyRow {
    short key;                  /* +0x0 */
    unsigned char gateIfClear;  /* +0x2: read while control bit 11 is clear */
    unsigned char gateIfSet;    /* +0x3: read while control bit 11 is set */
    int value;                  /* +0x4 */
    unsigned char bytes[8];     /* +0x8 */
} UwamonoKeyRow;

/* One 8-byte row of the event table uwamonoEventTable (treasurebox.dat): type selects what the record hands out, id is its
 * argument (1 hit action, 2 item, 3 data box category 4, 4 data box category 3) and count how many. */
typedef struct UwamonoEventRow {
    short unmodeled_00;
    short type;                 /* +0x2 */
    short id;                   /* +0x4 */
    short count;                /* +0x6 */
} UwamonoEventRow;

extern UwamonoKeyRow *uwamonoKeyRowTable;
extern UwamonoEventRow *uwamonoEventTable;
extern struct EnepcParamSet *enepcParamSets;
/* The look-at points are enepcLookAtPointTable (float[9][4]); game_camera.c and runtime_motion_face.c each declare their own
 * view of it (Vector4 rows, float rows: the float view gives the addressing of enepcLookAt), so it has no declaration here.
 * enepcLookAtReachTable holds the reach of each point (0 = unused). */
extern float enepcLookAtReachTable[12];

/* Resets the records (every one when index is -1); mode 1 also releases the actors. */
void uwamonoResetRecords(int index, signed char mode);
void gameRowFlagsClear(void);
void gameEventFlagsClear(void);
void mapSceneScriptLoad(int id, signed char mode);
void YtachiTest(void);
extern int uwamonoCurrentIndex;
extern UwamonoState uwamonoStates[];

/* Data of src/main/tya_draw_gauge.c that other files read (needs shared.h for Vector4). */

#define UWAMONO_POINT_COUNT 16

/* Point table entry, read by uwamono_record.c. */
typedef struct UwamonoPoint {
    Vector4 position;                   /* +0x00: copied as one 16-byte block */
    short key;                          /* +0x10 */
    unsigned char unmodeled_12[2];
    float value;                        /* +0x14 */
    unsigned char unmodeled_18[8];
} UwamonoPoint;

extern UwamonoPoint uwamonoPoints[UWAMONO_POINT_COUNT];

#endif /* INCLUDE_MAIN_YTACHI_TEST_H */
