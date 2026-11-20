#ifndef INCLUDE_MAIN_GAME_DATA_H
#define INCLUDE_MAIN_GAME_DATA_H

#include "shared.h"
#include "main/pxy_packet_state.h"
#include "main/cur_matrix_asm.h"

/* Triangle wave record of the camera shake: value is the current amplitude, step the amount it decays
 * per cycle; period and phase are in frames, hold delays the decay, remaining counts the cycles left. */
typedef struct CameraWave {
    float value;                                /* +0x00 */
    float step;                                 /* +0x04 */
    unsigned short period;                      /* +0x08 */
    unsigned short phase;                       /* +0x0A */
    unsigned short hold;                        /* +0x0C */
    unsigned short remaining;                   /* +0x0E */
} CameraWave;

/* One of the four camera slots (0 look-at, 1 position and angles, 2 perspective, 3 roll): a timed
 * transition set by cameraSlotSetup. */
typedef struct CameraSlot {
    unsigned char unmodeled_0000[0x2];
    unsigned char blendMode;                    /* +0x02: stored by cameraSlotSetup when >= 0; slot 1 blends the position when 0, the angles otherwise */
    unsigned char easeMode;                     /* +0x03: stored by cameraSlotSetup when >= 0; the mode of cameraEase */
    float progress;                             /* +0x04 */
    float duration;                             /* +0x08: the frames of the transition as a float; a slot is active when nonzero */
    float fraction;                             /* +0x0C: the eased fraction of the transition, 1.0 when the frames were 0 */
} CameraSlot;

/* A camera range block (0x70 bytes): the anchor, the range mode and the staged vectors. The camera
 * records (cameraCfSlots) keep one each; the current camera is the one at gameWork+0x180. */
typedef struct CameraRange {
    int anchorIndex;                            /* +0x00: -1 when no anchor */
    short anchorSubIndex;                       /* +0x04: -1 when no anchor */
    unsigned char unmodeled_0006[0x2];
    unsigned char lockX;                        /* +0x08 */
    unsigned char lockY;                        /* +0x09 */
    unsigned char lockZ;                        /* +0x0A */
    unsigned char probeMode;                    /* +0x0B */
    unsigned char rangeMode;                    /* +0x0C */
    unsigned char unmodeled_000D[0x2];
    unsigned char rangeFlag;                    /* +0x0F */
    float northAngle;                           /* +0x10: the camera yaw the stick directions are relative to */
    unsigned char unmodeled_0014[0xC];
    Vector4 offset;                             /* +0x20: the look-at offset above the anchor */
    Vector4 angleLimits;                        /* +0x30: pitch min and max, yaw min and max */
    Vector4 cameraPosition;                       /* +0x40: the camera position, w is the perspective */
    Vector4 lookAt;                             /* +0x50: the look-at point, w is the roll */
    Vector4 angles;                             /* +0x60: x pitch, y yaw, w the distance to the look-at point */
} CameraRange;

/* The camera values a slot transition interpolates: position, look-at point and angles (the layout of
 * the current camera at gameWork+0x1C0). */
typedef struct CameraPose {
    Vector4 position;                           /* +0x00: w is the perspective */
    Vector4 lookAt;                             /* +0x10: w is the roll */
    Vector4 angles;                             /* +0x20: x pitch, y yaw, w distance */
} CameraPose;

/* The camera state of the game work block (gameWork+0xD0): the flags, the shake, the four slots, the
 * current camera and the values its transitions start from and end at. The offsets in the comments are
 * gameWork offsets. */
typedef struct GameWorkCamera {
    unsigned short flags;                       /* +0xD0: CAMERA_FLAG_* bits (game_camera.c) */
    unsigned char cameraType;                   /* +0xD2: the camera type of the active record */
    unsigned char cameraSubType;                /* +0xD3: cleared with typeState when the camera type changes */
    signed char recordIndex;                    /* +0xD4: the camera record in use */
    signed char surfaceGroup;                   /* +0xD5: the camera surface group, set by GameCameraChangeID */
    unsigned char allSlotsFlagA;                /* +0xD6 */
    unsigned char allSlotsFlagB;                /* +0xD7: also the anchor pending flag of the range update */
    unsigned char previousRecordIndex;          /* +0xD8 */
    unsigned char anchorLocked;                 /* +0xD9 */
    unsigned char previousCameraType;           /* +0xDA */
    unsigned char unmodeled_00DB[0x1];
    signed char trackIndex;                     /* +0xDC: set by cameraSetIndex */
    unsigned char unmodeled_00DD[0x3];
    unsigned long long typeState[2];            /* +0xE0: cleared (as one quadword) when the camera type changes */
    unsigned char shakeFlags;                   /* +0xF0 */
    unsigned char unmodeled_00F1[0xF];
    CameraWave waves[3];                        /* +0x100 */
    float sway[4];                              /* +0x130 */
    CameraSlot slot[4];                         /* +0x140: 0 look-at, 1 position and angles, 2 perspective, 3 roll */
    CameraRange range;                          /* +0x180: the current camera, laid out like a camera record range */
    CameraPose from;                            /* +0x1F0: the values a slot transition starts from */
    CameraPose to;                              /* +0x220: the values a slot transition ends at */
    Vector4 fromOffset;                         /* +0x250: the offset the position transition starts from */
    Vector4 anchorOffset;                       /* +0x260: the anchor offset vector */
    float anchorBase[3];                        /* +0x270 */
    unsigned char unmodeled_027C[0x4];
} GameWorkCamera;

/* One entry of the elevator slope table. */
typedef struct GameWorkSlope {
    float x;
    float z;
    float push;
    float speed;
} GameWorkSlope;

/* The light sets of the scene, one per light set. */
typedef struct GameWorkLights {
    float direction[16][3][4];          /* +0x000: light types 1-3 */
    float color[16][3][4];              /* +0x300 */
    float ambient[16][4];               /* +0x600: light type 0 */
    float type4Direction[16][4];        /* +0x700: light type 4; w 0 disables it */
    float type4Value[16];               /* +0x800: light type 4, 0.2 by default */
} GameWorkLights;

/* One 256-bit map flag set. */
typedef struct MapBits {
    unsigned int bits[8];
} MapBits;

/* The resource record gameWork.active points at, handed to mapResourceLoadCallback. */
typedef struct MainActive {
    PxyHeader *object;          /* +0x00 */
    int jpegData;               /* +0x04 */
    unsigned char unmodeled_08[0x04];
    struct MapFixupBlock *block; /* +0x0C: the collision block mapLoadBlock registers */
    unsigned char *imageHead;   /* +0x10 */
    int imageOffset;            /* +0x14 */
    unsigned char unmodeled_18[0x08];
    int *jpegStream;            /* +0x20 */
    unsigned char *pending;     /* +0x24: decoded texture */
    short offsetY;              /* +0x28 */
    short offsetX;              /* +0x2A */
    unsigned short height;      /* +0x2C */
    unsigned short width;       /* +0x2E */
    ScreenVector4 view;         /* +0x30: the backdrop origin projected by mapBackdropDraw (x is the row, y the column) */
} MainActive;

struct SbImageHeader;

/* The game work block (0x1B30 bytes, one object owned by game_data.c): the scene state, the camera, the
 * player parameters, the light sets and the event flags. The offsets in the comments are gameWork
 * offsets. control, camera.flags and anchorIndex are plain words; a function that depends on reading
 * one of them with alias set 0 (after the stores before it) reads it through a local one-member union
 * view. */
typedef struct GameWork {
    unsigned char *workEnd;                     /* +0x00: WorkEnd, saved when the field loop starts */
    unsigned char unmodeled_0004[0x2];
    unsigned short loadStage;                   /* +0x06: 1 once the save is read, 2 once the party models are loaded */
    int control;                                /* +0x08: CONTROL_* bits */
    unsigned char inputDelay;                   /* +0x0C: frames until the pause (16 after a toggle, 2 after a ladder exit) or the movie skip (32, 64) is accepted again */
    unsigned char unmodeled_000D[0x3];
    int sceneFlags;                             /* +0x10: bit 0 is set when an encounter starts */
    int nextMap;                                /* +0x14: the requested next map, -1 when none */
    int mapId;                                  /* +0x18: the current map */
    int entryKey;                               /* +0x1C: -1 none, -2 the party position, else an entry key */
    int resourceId;                             /* +0x20: the file id of the map model (game_map_object.c sets it; game_over.c also writes chapter + 1) */
    unsigned char unmodeled_0024[0x1];
    unsigned char elevatorMask;                 /* +0x25: the copy of elvActiveMask (elevator.c) saved with the scene */
    unsigned char unmodeled_0026[0x2];
    MainActive *active;                  /* +0x28: the resource record being loaded */
    unsigned char *resBuffer;                   /* +0x2C: GameResBufferAlloc result, the loaded image */
    GameWorkSlope slopes[8];                    /* +0x30: the elevator slope table */
    int mapLoaded;                              /* +0xB0 */
    int keepOffMask;                            /* +0xB4: bit n set when elvKeepOffFlags[n] is */
    struct SbImageHeader *imageEnd;             /* +0xB8: the SBL library image behind the loaded script image */
    unsigned char unmodeled_00BC[0x4];
    union { Vector4 mapColor; };                /* +0xC0: setColor, reset to 1.0 at each scene start; a one-member union: the original accesses it with alias set 0 */
    GameWorkCamera camera;                      /* +0xD0 */
    unsigned char unmodeled_0280[0x10];
    unsigned char sceneFlag;                    /* +0x290: cleared at each scene start */
    unsigned char unmodeled_0291[0x3F];
    float frameScale;                           /* +0x2D0: elapsed frames, scales the per-frame movement */
    float frameScaleSaved;                      /* +0x2D4: copy of frameScale made by uwamonoLoadCurrent */
    float playerParams[5][2];                   /* +0x2D8: setPlayer arguments 4 to 8 (percent) in [i][0]; [i][1] is the saved copy */
    int climbFrames;                            /* +0x300: setPlayer argument 9, the ladder-step divisor */
    int climbFramesSaved;                       /* +0x304 */
    float formScale;                            /* +0x308: the current record scale.x */
    float formSize;                             /* +0x30C: the current record extent.x */
    int replayStatus;                           /* +0x310: ground status recorded with the input */
    int replayStickX;                           /* +0x314 */
    int replayStickY;                           /* +0x318 */
    short encounterTarget;                      /* +0x31C */
    unsigned char unmodeled_031E[0x2];
    short encounterReaction;                    /* +0x320: -1 none */
    unsigned char unmodeled_0322[0x2];
    int encounterFlags;                         /* +0x324 */
    unsigned char unmodeled_0328[0x4];
    int encounterRecord;                        /* +0x32C: record index of the current encounter */
    short fadeTimer;                            /* +0x330: frames left of the rider fade */
    unsigned char vibrationStart;              /* +0x332: the fade\'s start value (the output of its first step) */
    unsigned char vibrationEnd;               /* +0x333: the fade\'s end value (the output settles near it) */
    int vibrationHold;                          /* +0x334: hold countdown of the fade */
    int vibrationFramesLeft;                    /* +0x338: steps left in the fade */
    int vibrationFramesTotal;                   /* +0x33C: steps the fade was started with */
    unsigned char unmodeled_0340[0x4];
    void *releaseHandle;                        /* +0x344: effect handle deleted when a battle ends (game_data.c) */
    DoublewordAlignedVector4 fogDist;           /* +0x348: setFog near, far, min, max */
    unsigned char unmodeled_0358[0x8];
    Vector4 fogColor;                           /* +0x360: setFog color */
    int shadowCasterMode;                       /* +0x370: setItai argument 2 (default 1), the mode of pxyDrawShadowCaster */
    float gravity;                              /* +0x374: setPlayer argument 10 (ten-thousandths) */
    float gravitySaved;                         /* +0x378 */
    float bounceDivisor;                        /* +0x37C: setPlayer argument 11 (percent) */
    float bounceDivisorSaved;                   /* +0x380 */
    GameWorkLights lights;                      /* +0x384 */
    unsigned char unmodeled_0BC4[0xC0];
    GameWorkLights savedLights;                 /* +0xC84 */
    unsigned char unmodeled_14C4[0x1C];
    MapBits savedMapBits[2];                    /* +0x14E0 */
    signed char eventFlags[512];                /* +0x1520: one flag per kind 8 event index */
    signed char rowFlags[2][512];               /* +0x1720: key row flags, [0] while control bit 11 is clear, [1] while set */
    signed char panelMode;                      /* +0x1B20: the kind of the last panel command */
    unsigned char unmodeled_1B21[0x3];
    int panelValue;                             /* +0x1B24: counted down per frame and shown as a timecode; ov01's
                                                 * battleEndChk ends a time-limited battle at 0 */
    unsigned char unmodeled_1B28[0x8];
} GameWork;

/* Bits of gameWork.control. */
#define CONTROL_PAUSED 0x1
#define CONTROL_SCRIPTED 0x8000
#define CONTROL_REPLAYED 0x10000
/* the SQUARE attack on a map object is running (game_camera.c) */
#define CONTROL_MAP_ATTACK 0x20000
/* set when a map resource loads and cleared two frames later (DispMapObject); readers treat it as "the map just loaded" */
#define CONTROL_MAP_LOADED 0x20
/* Set while a cut scene runs (cfePlugIn.c sets it at the start, clears it at the end). */
#define CONTROL_CFE_SCENE 0x200
#define CONTROL_CFE_NO_FADE 0x100000 /* set with it: events then start no fade-out (runtime.c) */
#define CONTROL_SOFT_RESET 0x10000000 /* the soft-reset pad combo was pressed; Title will not start a game while it is set */

extern GameWork gameWork;

void Game_Data_Push(void);
void Game_Data_Pop(void);
void uwamonoLoadCurrent(void);
void uwamonoStoreCurrent(void);
/* The field loop; title.c returns its address as the next overlay entry. */
void Game(void);

/* A 16-byte row of the pose table at uwamonoEntries: a pose with its angle and its key. */
typedef struct UwamonoEntry {
    float x;
    float y;
    float z;
    short angle;
    unsigned short key;
} UwamonoEntry;

extern UwamonoEntry *uwamonoEntries;

/* The loaded map: the object block, its texture data, the model pack (read for its entry matrices) and the map group
 * (UnduDataGetHeader 0x300) whose recordCount is the row count of uwamonoEntries. */
extern PxyHeader *mainObject;
extern int mapTextureData;
extern PxyHeader *mapObjectTable;
extern struct MapFixupEntry *uwamonoEntryCountPtr;

#endif /* INCLUDE_MAIN_GAME_DATA_H */
