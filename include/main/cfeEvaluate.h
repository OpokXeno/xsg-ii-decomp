#ifndef INCLUDE_MAIN_CFEEVALUATE_H
#define INCLUDE_MAIN_CFEEVALUATE_H

#include "shared.h"
#include "main/cfe_list.h"
#include "main/xgl_1.h"
#include "main/xgl_2.h"
#include "main/xgl_light.h"

/* One 0x80-byte command of cfeCommands: an effect (its id built from a page index and a page offset,
 * cfeCommandSetOwnerFlags), where and how it is spawned, and the tracks that drive those values. */
typedef struct CfeEvalCommand {
    int flag;                   /* +0x00 */
    int page_index;             /* +0x04 */
    int page_offset;            /* +0x08 */
    unsigned char unmodeled_0c[4];
    Vector4 position;           /* +0x10 */
    Vector4 rotation;           /* +0x20 */
    float scale[3];             /* +0x30 */
    unsigned char unmodeled_3c[4];
    int frames;                 /* +0x40 */
    int action;                 /* +0x44: what cfeCommandEvaluate does, 0 to 4 */
    int owner;                  /* +0x48: entry of cfeEntries that owns the effect, or negative */
    CfeTrackNode *track[12];    /* +0x4C: CFE_COMMAND_TRACK_* (cfe.stKyEf sets them by this index) */
    unsigned char unmodeled_7c[4];
} CfeEvalCommand;

/* The tracks of a command (CfeEvalCommand.track). */
enum CfeCommandTrack {
    CFE_COMMAND_TRACK_POSITION,         /* x, y, z */
    CFE_COMMAND_TRACK_ROTATION = 3,     /* x, y, z */
    CFE_COMMAND_TRACK_SCALE = 6,        /* x, y, z */
    CFE_COMMAND_TRACK_ACTION = 9,
    CFE_COMMAND_TRACK_FRAMES,
    CFE_COMMAND_TRACK_OWNER
};

/* Command word layout: page index above bit 12, a bank bit selected by the flag. */
#define CFE_PAGE_SHIFT 0xC
#define CFE_BANK_FLAGGED 0x20000000
#define CFE_BANK_PLAIN 0x10000000
#define CFE_OWNER_FLAGS 0x80

/* One 0x18-byte slot of the table at cfeEvalSlots (the screen effects): three values and the tracks they are
 * sampled from (CFE_SLOT_SOURCE_*; cfe.stKyScrEff sets them by this index). */
typedef struct CfeEvalSlot {
    int result_int;
    int sample_int;
    float sample_float;
    CfeTrackNode *source[3];    /* +0x0C */
} CfeEvalSlot;

/* CfeEvalSlot.source: the track of result_int, sample_float and sample_int. */
enum CfeSlotSource {
    CFE_SLOT_SOURCE_RESULT_INT,
    CFE_SLOT_SOURCE_SAMPLE_FLOAT,
    CFE_SLOT_SOURCE_SAMPLE_INT
};

/* Global block at cfeEvalGlobal (cfeGlobalEvaluate): the fog switch, two vectors and eight source handles. */
typedef struct CfeEvalGlobal {
    int fogEnabled;             /* sampled from source[7]; cfeCmdStepFrame sets the fog when it is set */
    unsigned char unmodeled_04[0x0C];
    Vector4 evaluated[2];
    CfeTrackNode *source[8];
} CfeEvalGlobal;

/* The vector of the cfe records and entries, with its components by name or by index. The original
 * reaches it through a union: after every store the record pointer is loaded again (alias set 0), and
 * cfeEvalEntriesInit addresses each component by index (every store has the whole offset in its address,
 * so the loop walks one pointer at the last component). */

/* Record written by cfeRecordEvaluate: three vectors of results, the starting position and angles and nine source
 * handles. */
typedef struct CfeEvalRecord {
    Vector4Elements evaluated[3]; /* +0x00: position (w = 1), angles, scale */
    Vector4Elements initial[2];   /* +0x30: the position and angles the record starts from (cfeMapInitRecords) */
    CfeTrackNode *source[9];    /* +0x50 */
    unsigned char unmodeled_74[0x80 - 0x74];
} CfeEvalRecord;

/* The model pack of an object as the group and object commands read it (xenoActor.c: PxyHeader). */
typedef struct CfePackHeader {
    unsigned char unmodeled_00[0x60];
    int geometryCount;          /* +0x60 */
    unsigned char unmodeled_64[4];
    int matrixOffset;           /* +0x68: bytes from the pack to its geometry matrices */
    unsigned char unmodeled_6c[0xD0 - 0x6C];
    int subPackOffset;          /* +0xD0: bytes from the pack to its part pack, 0 if none */
} CfePackHeader;

/* A map of cfeGroups, the original's maps (cfe.ldMp; 0xC0 bytes, quadword aligned): an object, the part table of
 * its pack with the part matrices and one evaluation record per part, and the group's own values with their tracks. */
typedef struct CfeMap {
    struct XenoActor *object;   /* +0x00 */
    CfePackHeader *parts;       /* +0x04: the part pack of the object's pack (one record per geometry) */
    Matrix4 *matrices;          /* +0x08: one per part */
    CfeEvalRecord *records;     /* +0x0C: one per part */
    unsigned char unmodeled_10[0x50 - 0x10];
    AlignedVector4 position;    /* +0x50: from tracks 0..2 */
    int light;                  /* +0x60: from track 4 */
    CfeTrackNode *tracks[5];    /* +0x64: position x, y, z, hidden, light */
    unsigned char unmodeled_78[0xB8 - 0x78];
    /* +0xB8: the collision set that is current while the group is shown. A pointer: its load is scheduled
     * ahead of an int store in cfeGroupTableEvaluate. */
    void *collision;
    unsigned char unmodeled_bc[0xC0 - 0xBC];
} CfeMap;

/* A row of cfeDirections, the original's lights (0x40 bytes): the angles of a light direction, the light-set
 * vectors it drives (cfeLightRowsInit points them into cfeLightSet; row 0 has only the ambient colour) and
 * the tracks they are sampled from. */
typedef struct CfeLight {
    float angles[3];            /* +0x00: X and Y rotation of the direction */
    unsigned char unmodeled_0c[4];
    float *direction;           /* +0x10: rotated from angles */
    float *color;               /* +0x14 */
    float *extra;               /* +0x18 */
    CfeTrackNode *track[9];     /* +0x1C: CFE_LIGHT_TRACK_* (cfe.stKyLgt sets them by this index) */
} CfeLight;

/* The tracks of a light (CfeLight.track). */
enum CfeLightTrack {
    CFE_LIGHT_TRACK_EXTRA,              /* x, y, z */
    CFE_LIGHT_TRACK_ANGLE = 3,          /* x, y, z */
    CFE_LIGHT_TRACK_COLOR = 6           /* r, g, b */
};

/* Table entry (stride 0x230) of cfeEntryEvaluate: three vectors, a result word and ten source handles. */
typedef struct CfeEvalEntry {
    /* The movie work record (XS1 MovieInfo). The movie plays from memory: source is the file, read into a
     * GameResBufferAlloc block, and imageAddress are two more blocks of 16 * imageQwords + 0x20 bytes.
     * bufferIndex is nonzero while imageAddress[0] holds the frame to show, else [1]. */
    XglMovieWork movie;         /* +0x000 */
    char name[0x40];            /* +0x0D0: the movie file name */
    Vector4Elements evaluated[3]; /* +0x110: position, angles, scale */
    AlignedVector4 corners[4];  /* +0x140: the quad the entry is drawn with (cfeEvalEntriesInit) */
    unsigned char unmodeled_180[0x80];
    int rewind;                 /* +0x200: sampled from source[9]; cfeEntryTableUpdate rewinds the movie when set */
    CfeTrackNode *source[10];
    unsigned char unmodeled_22c[4];
} CfeEvalEntry;

struct XenoActor;

/* One sampled flag of a node (8 bytes, one per geometry of the actor's pack): the value and the track it is
 * sampled from. The key commands address the track as track[column]; the script passes column 0. */
typedef struct CfeFlagRecord {
    int value;
    CfeTrackNode *track[1];
} CfeFlagRecord;

/* The tracks of an actor entry from +0x130 (CfeActor.track): the script command cfe.stKyAt sets key lists by this
 * index. */
enum CfeActorTrack {
    CFE_ACTOR_TRACK_POSITION,           /* x, y, z of the XenoActor */
    CFE_ACTOR_TRACK_ROTATION = 3,       /* x, y, z */
    CFE_ACTOR_TRACK_SCALE = 6,          /* x, y, z */
    CFE_ACTOR_TRACK_MODE = 9,
    CFE_ACTOR_TRACK_SELECTOR,
    CFE_ACTOR_TRACK_SLOT,
    CFE_ACTOR_TRACK_CHANNEL,
    CFE_ACTOR_TRACK_PLAY_START,
    CFE_ACTOR_TRACK_PLAY_END,
    CFE_ACTOR_TRACK_PARENT_ACT,
    CFE_ACTOR_TRACK_PARENT_MAT,
    CFE_ACTOR_TRACK_SPEED_A,
    CFE_ACTOR_TRACK_SPEED_B,
    CFE_ACTOR_TRACK_ALPHA,
    CFE_ACTOR_TRACK_DAMP_OFFSET,
    CFE_ACTOR_TRACK_FIRST_WORD,
    CFE_ACTOR_TRACK_DRAW_MODE,
    CFE_ACTOR_TRACK_TRANSLUCENT,
    CFE_ACTOR_TRACK_FORCE_MODE,
    CFE_ACTOR_TRACK_COUNT
};

/* The tracks of an actor entry from +0x194 (CfeActor.headTrack), set by cfe.stKyAtHd. */
enum CfeActorHeadTrack {
    CFE_ACTOR_HEAD_SHOWN,
    CFE_ACTOR_HEAD_POSITION,            /* x, y, z of the entry position */
    CFE_ACTOR_HEAD_COUNT = 4
};

/* One 0x1B0-byte entry of cfeEntries, the original's actor table (actor[numActor].pAct in its asserts): the
 * XenoActor it drives, the values it last set and the tracks they come from. */
typedef struct CfeActor {
    unsigned char unmodeled_000[0xC0];
    struct XenoActor *actor;    /* +0x0C0: pAct */
    unsigned char unmodeled_0c4[0xD0 - 0xC4];
    AlignedVector4 vectorD0;    /* +0x0D0: cfeActorInitialVector (cfeActorTableInit) */
    /* +0x0E0: the sampled offset, then in front of the actor. Reached through a union in the original:
     * cfeActorEvaluate loads the actor pointer again after the store to position.x (alias set 0). */
    union {
        AlignedVector4 position;
    };
    int flags;                  /* +0x0F0: CFE_ACTOR_SHOWN */
    int mode;                   /* +0x0F4 */
    int selector;               /* +0x0F8 */
    int slot;                   /* +0x0FC: the mode slot */
    float channelValue;         /* +0x100: the channel value of the actor's first motion state */
    int playStart;              /* +0x104: the play range of that motion state */
    int playEnd;                /* +0x108 */
    int parentActId;            /* +0x10C: parentActID, the cfeEntries entry whose actor this one hangs on; -1 none */
    int parentMatId;            /* +0x110: parentMatID, the joint of that actor */
    float speedA;               /* +0x114: of the actor's motion state A */
    float speedB;               /* +0x118: of motion state B */
    int dampOffset;             /* +0x11C: passed to cfeDampDirOffset every frame */
    int firstWord;              /* +0x120 */
    int drawMode;               /* +0x124 */
    int translucent;            /* +0x128 */
    int forceMode;              /* +0x12C: nonzero sets the mode again every time */
    CfeTrackNode *track[CFE_ACTOR_TRACK_COUNT];          /* +0x130 */
    CfeTrackNode *headTrack[CFE_ACTOR_HEAD_COUNT];       /* +0x194 */
    struct CfeFlagRecord *records; /* +0x1A4: the flag records of the actor's list */
    unsigned char unmodeled_1a8[0x1B0 - 0x1A8];
} CfeActor;

/* CfeActor.flags: the entry's actor follows its position. */
#define CFE_ACTOR_SHOWN 0x1

/* The tracks of the studio camera (CfeCamera.track): cfe.stKyCm sets them by this index. */
enum CfeCameraTrack {
    CFE_CAMERA_TRACK_POSITION,          /* x, y, z of travel.position */
    CFE_CAMERA_TRACK_ROTATION = 3,      /* x, y, z of travel.rotation */
    CFE_CAMERA_TRACK_FOV = 6,           /* travel.fov */
    CFE_CAMERA_TRACK_SHAKE_ON,
    CFE_CAMERA_TRACK_SHAKE_FRAMES,
    CFE_CAMERA_TRACK_SHAKE_AMOUNT,
    CFE_CAMERA_TRACK_COUNT
};

/* The shake offsets of the camera: position x, y, z, then rotation x, y, z. */
typedef struct CfeCameraOffset {
    f32 position[3];
    f32 rotation[3];
} CfeCameraOffset;

/* The studio camera block cfeCamera (0x70 bytes, cleared by cfeStudioInit): the camera, its key tracks and the
 * camera shake. */
typedef struct CfeCamera {
    struct XglCamera *camera;   /* +0x00 */
    CfeTrackNode *track[CFE_CAMERA_TRACK_COUNT]; /* +0x04 */
    int flags;                  /* +0x2C: CFE_CAMERA_HELD leaves the camera alone */
    int shakeOn;                /* +0x30 */
    int shakeFrames;            /* +0x34: frames from one shake offset to the next */
    int shakeAmount;            /* +0x38: the largest offset in 1/1000 units (rotation 1/10000) */
    int shakeCounter;           /* +0x3C: frames left to the next offset; -1 starts a new one */
    CfeCameraOffset from;       /* +0x40 */
    CfeCameraOffset to;         /* +0x58 */
} CfeCamera;

/* cfeCamera as the original reaches it: through a union member, so its accesses carry alias set 0 (the camera
 * pointer and the counter are read again after every camera store). */
typedef union CfeStudioView {
    CfeCamera eval;
} CfeStudioView;

/* CfeCamera.flags: stops the evaluation. */
#define CFE_CAMERA_HELD 0x1

/* The cut-scene tables (the original's actors, maps, effects, lights, fog, monitors and screen effects) and
 * the light set the lights point into. */
extern CfeActor cfeEntries[];
extern int cfeEntryCount;
extern CfeMap cfeGroups[];
extern int cfeGroupCount;
extern CfeEvalCommand cfeCommands[];
extern int cfeCommandCount;
extern CfeLight cfeDirections[4];
extern CfeEvalGlobal cfeEvalGlobal;
extern CfeEvalEntry cfeEvalEntries[];
extern int cfeEvalEntryCount;
extern CfeEvalSlot cfeEvalSlots[3];
extern XglLightSet cfeLightSet;
extern CfeStudioView cfeCamera;

/* Functions of src/main/cfeEvaluate.c that cfePlugIn.c calls. */
int cfeActorTableEvaluate(int sample_param);
void cfeGroupTableEvaluate(int time);
void cfeCameraEvaluate(int time, int paused);
void cfeCommandTableEvaluate(int time);
void cfeLightTableEvaluate(int time);
void cfeGlobalEvaluate(int sample_param);
int cfeEntryTableEvaluate(int sample_param);
void cfeSlotTableEvaluate(int sample_param);
void cfeSndTableEvaluate(int target);
void cfeEntryTableUpdate(int noPlay);

#endif /* INCLUDE_MAIN_CFEEVALUATE_H */
