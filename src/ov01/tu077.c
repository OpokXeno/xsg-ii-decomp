/*
 * OV01 original TU 77 (re-cut 2026-11-18: effect/menu files start at their Create)
 */

#include "common.h"
#include "shared.h"
#include "main/m_math_191.h"
#include "ov01/tu077.h"
#include "main/tu192.h"
#include "quadword.h"
#include "ov01/tu075.h"
#include "vu0_macro.h"
#include "main/xgl_2.h"
#include "main/tu187.h"
#include "main/tu197.h"
#include "main/m_math_193.h"
#include "main/tu196.h"

/* A VM object reference (the JNI jobject): a pointer to an object header
 * whose first word is its class. The original type aliased `void *`: the
 * sequence stores of ACT_createChr/ACT_createNPC (near_dir) and the JThread
 * object/script argument slots schedule only under void*'s alias set, not
 * under a char-pointer one. Field slots are reached by byte offset
 * (field->location.offset), so SceneByte views remain for that. */
typedef void *SceneObject;

typedef struct SchedulerState SchedulerState;

typedef float RgVector[4];

typedef struct JntMatrix JntMatrix;

/*
 * JNT_getStaticVal2/JNT_getVal2 index this type by joint slot
 * (`matrix_buffer + index*sizeof(JntMatrix)`, `sll ...,0x6`), so its size is
 * evidenced as 0x40 bytes. The joint tables are read as 4x4 floats by the
 * motion code and as a flat float[16] by JC_face and friends (the w-column
 * stores at [3], [7], [11]). The original accessed them through a union of
 * both views (alias set 0): JC_face/JC_setParent* only reproduce the store
 * order against the quadword copies that way. Unlike xgl_2.h's Matrix there
 * is no quadword member, since that would 16-align the records inside
 * JntWork (shifting every later field).
 */
struct JntMatrix {
    union {
        Matrix4 elements;
        float flat[16];
    };
};

typedef JntMatrix *JntMatrixBuffer;

typedef unsigned int JntMatrixSelect;

typedef struct JntHairIdSource {
    unsigned char unmodeled_00[4];
    unsigned short version;
    unsigned short channel_limit;
    unsigned short primary_id;   /* 0x08 */
    unsigned short secondary_id; /* 0x0A */
    unsigned short additional_primary_count;   /* 0x0C */
    unsigned short additional_secondary_count; /* 0x0E */
    int channel_offset;
} JntHairIdSource;

typedef struct JntChainSlot {
    struct FcvPackAttribute *attribute; /* 0x00 */
    unsigned char unmodeled_04[0x0c];
    Vector4 translation;           /* 0x10 */
    Vector4 scale;                 /* 0x20 */
    Vector4 rotation;              /* 0x30 */
} JntChainSlot;

typedef struct JntProductionOwner JntProductionOwner;

typedef struct JntTransformRecord {
    Vector4 translation;
    Vector4 rotation;
} JntTransformRecord;

typedef struct JntProducer JntProducer;

typedef struct JntInterruptElement JntInterruptElement;

struct JntInterruptElement {
    Vector4 scale;                   /* 0x00 */
    float rotation[3];               /* 0x10 */
    unsigned char unmodeled_1c[4];
    Vector4 translation;             /* 0x20 */
    unsigned short key;
    unsigned short interrupt_channel; /* 0x32, the channel that interrupts this element */
    void *attribute_data;
    unsigned char unmodeled_38[8];
};

struct JntWork {
    JntProducer *producer;         /* +0x000 */
    unsigned short *cursor;        /* +0x004: FCV2 packed-curve state */
    unsigned char unmodeled_008[0x10 - 0x08];
    unsigned int requested_attribute_key; /* 0x10 */
    unsigned char unmodeled_014[0x28 - 0x14];
    struct JntHairIdSource *hair_id_source;
    /*
     * JNT_getStaticVal2/JNT_getVal2 read this as the default matrix slot for
     * their CUR_MATRIX_Get call and unconditionally increment it once per
     * call (`lw/sw ...,44($.)`).
     */
    int current_index; /* 0x2C */
    struct JntStaticChannel *channels; /* +0x030 */
    void *model;
    /*
     * General flags word: JNT_setFlags writes it and JNT_getFlags reads it
     * (`sw $4,56($1)` / `lw $2,56($2)` off the 0x70000000 base). Callers
     * ACT_resetMatrix, ACT_updateMotionSub, ACT_modelDrawSub, ACT_setMotion
     * and ACT_setMotion2 read and write it around the joint update.
     */
    unsigned int flags;            /* +0x038 */
    unsigned short channel_index;  /* +0x03c */
    unsigned char unmodeled_03e[2];
    Vector4 root_translation;      /* +0x040 */
    Vector4 root_rotation;         /* +0x050 */
    Vector4 root_scale;            /* +0x060 */
    Vector4 channel_translation; /* 0x070 */
    Vector4 channel_rotation;    /* 0x080 */
    Vector4 channel_scale;       /* 0x090 */
    /* 0x0A0: 0x40-byte chain records; logical capacity is unrecovered. */
    JntChainSlot chain_slots[16];
    unsigned char unmodeled_4a0[0x4bc - 0x4a0];
    JntMatrixSelect matrix_select;
    unsigned short matrix_index;   /* +0x4c0 */
    short chain_base_index;        /* 0x4C2 */
    unsigned char unmodeled_4c4[0x4d0 - 0x4c4];
    Matrix4 transform_basis; /* 0x4D0: basis and chain origin */
    unsigned char unmodeled_510[0x720 - 0x510];
    JntMatrix root_matrix;
    JntMatrix interpolation_root;
    /*
     * JNT_getStaticVal2/JNT_getVal2 read this as the matrix table their
     * CUR_MATRIX_Set/Get calls index by joint slot (`lw ...,1952($.)`);
     * JNT_setMatrix/JNT_setMatrix2 write it through JNT_MATRIX_BUFFER_OFFSET
     * above (same 0x7A0 slot, kept as a raw offset there for the codegen
     * reason given on that macro).
     */
    JntMatrix *matrix_buffer; /* 0x7A0 */
    JntMatrixBuffer interp_matrix; /* 0x7A4, JNT_setInterpMatrix */
    JntTransformRecord (*static_value_records)[];    /* 0x7A8, JNT_setCurve arg0; read by JNT_getStaticVal */
    void *value_records;           /* 0x7AC, JNT_setCurve arg1; read by JNT_getVal */
    Matrix4 root_correction;       /* 0x7B0 */
    unsigned int attribute_key;
    unsigned int attribute_difference; /* 0x7F4 */
    unsigned char unmodeled_7f8[4];
    float current_frame;           /* +0x7fc */
    float interpolation; /* 0x800, JNT_setInterpMatrix */
    float chain_length;            /* 0x804 */
    struct JntChannelTag *current_value; /* +0x808: an attribute's tag words */
    char *attribute_name;          /* +0x80c */
    /*
     * JNT_animSetFlags writes the animation flags word at 0x810 and
     * JNT_setClipR writes the clipping radius right after it at 0x814;
     * ACT_updateMotionSub supplies both (JNT_animSetFlags also from
     * ACT_modelDrawSub).
     */
    unsigned int animation_flags;  /* +0x810 */
    float clip_radius; /* 0x814 */
    JntInterruptElement *elements;
    int channel_start_index;       /* +0x81c */
    JntProductionOwner *owner;     /* +0x820 */
    unsigned char unmodeled_824[0x0c];
    JntInterruptElement default_element; /* +0x830: default element storage */
};

typedef struct JntWork JntWork;

typedef struct JntStaticChannel JntStaticChannel;

typedef void *(*JntProductionConsumer)(JntWork *work,
                                      JntStaticChannel *channels,
                                      int argument);

/*
 * The joint producer embedded in Actor at +0x790 (Actor.jointProducer, the
 * only producer JNT_initProducer is given). Its tail words are the actor's
 * per-model caches that JNT_initProducer resets: ACT_getAccessories caches
 * JNT_getAccessories in accessory_joints, ACT_getMoveElement caches
 * JNT_getMoveElement in move_element_id (-1 = not looked up yet).
 */
struct JntProducer {
    int count;              /* 0x00 */
    int consumer_ids[8];
    JntProductionConsumer consumers[8]; /* 0x24, called by JNT_startProduction */
    int consumer_arguments[8];         /* 0x44 */
    int pending_output;     /* 0x64: work->flags after a production; ACT_updateMotionSub tests 0x10/0x40 */
    struct ActorAccessoryJointTable *accessory_joints; /* 0x68 */
    int move_element_id;    /* 0x6C */
    /* 0x70: entries in deferred_limits; JNT_startProduction clears it and
     * JNT_getVal2/JNT_getVal_HOGE2 append the shape binding descriptors that
     * follow a channel's rotation-limit header, which ACT_modelDrawSub walks. */
    int deferred_count;
    struct ActorShapeBinding *deferred_limits[8]; /* 0x74 */
};

typedef union JntRotationPayload {
    float values[3];
    int bits[3];
} JntRotationPayload;

struct JntStaticChannel {
    unsigned short type;           /* 0x00 */
    unsigned short interpolation_flags; /* 0x02 */
    int rotation_limits_offset;
    unsigned char unmodeled_08[6];
    unsigned short matrix_index;   /* 0x0E */
    Vector4 translation;           /* 0x10: quadword translation payload */
    JntRotationPayload rotate;     /* 0x20 */
    unsigned char unmodeled_2C[0x0C];
    float translation_direction;  /* 0x38: sign selects the chain direction */
    unsigned char unmodeled_3c[4];
};

/*
 * main/tu149's light editor (DB_lightWrite, updateLight) indexes these
 * vectors as float[4] rows (component offsets fold into the row's index, not
 * the load displacement), so each vector also has its RgVector row.
 */
typedef struct StudioLightEntry {
    union { Vector4 color; RgVector color_row; };
    union { Vector4 direction; RgVector direction_row; };
} StudioLightEntry;

/* The light block of a studio record, also the light record of an Actor
 * (+0x510). Whole-block copies (game.c GameStateSave/RestoreCameraLight) are
 * ld/sd, while the Studio records holding it are only 8-aligned (xgl_studio
 * .bss): the block is 8-aligned, which the doubleword view of its first vector
 * gives. */
typedef struct StudioLight {
    union {
        struct {
            union { Vector4 ambient_color; RgVector ambient_color_row; u64 ambient_color_words[2]; };
            StudioLightEntry lights[3];
        };
        Vector4 parameters[7];          /* the seven quadwords xglLightCalcMatrix loads: ambient, then three colour/direction pairs */
    };
    float normal_matrix[4][4];          /* +0x70: map_1.c's MapDrawLight view */
    float color_modifier_matrix[4][4];  /* +0xb0: map_1.c's MapDrawLight view */
} StudioLight;

/* Enemy parameter record at Actor +0xc0. Member offsets are relative to +0xc0
 * (actor offset = +0xc0 + member offset); the record ends at +0x228, the end of
 * the largest view (EnemyParamsInitView in enemy_system_init.h). Embedded by the
 * TU-local Actor views of the same bytes: ActorEnemyWorkView (enemy_1.c),
 * ActorEnemyDetectView (set_motion.c, hearing), EncountActor (set_motion.c) and
 * ActorPauseView (pause_menu_page_enemy.h, parameters). Views that keep their own
 * declaration: EnemyParamsInitView (ACT_createEnemy, move_kind stored as a
 * halfword) and EnemyParamsActionView (enemy_3.c, idle/reaction motion ids).
 * A member whose readers disagree on width or signedness is not a union here:
 * GCC 2.96 schedules union-member accesses differently (measured on enemy_1.c),
 * so the TU keeps its own EnemyParams*View declaration instead. */
typedef struct EnemyParams {
    float route_step_length;                /* +0x00 */
    float chase_distance;                   /* +0x04 */
    float move_speed;                       /* +0x08: pause menu label "movement speed" */
    float chase_speed;                      /* +0x0c */
    float sight_angle;                      /* +0x10 */
    float sight_distance;                   /* +0x14 */
    short stiff_duration;                   /* +0x18: pause menu stiffness row */
    short pause_interval;                   /* +0x1a */
    short threat_duration;                  /* +0x1c: pause menu threat row */
    short threat_interval;                  /* +0x1e */
    short turn_duration;                    /* +0x20 */
    short gravity_mode;                     /* +0x22 */
    float locator_distance;                 /* +0x24 */
    short wall_attribute;                   /* +0x28 */
    unsigned char move_kind;                /* +0x2a: Enemy_Init loads one byte */
    unsigned char unmodeled_2b;
    int preset_index;                       /* +0x2c: -1 when no AdrsEnemy preset */
    float *route_values;                    /* +0x30 */
    int route_value_count;                  /* +0x34 */
    Vector4 *homing_values;                 /* +0x38 */
    int homing_value_count;                 /* +0x3c */
    float body_radius;                      /* +0x40 */
    int ex_motion_resource;                 /* +0x44: -1 means no ACT_initExMotion */
    float value_108;                        /* +0x48 */
    int event_type;                         /* +0x4c */
    short monster_set_numbers[8];           /* +0x50: Check_Encount picks one, -1 is empty */
    int value_120;                          /* +0x60 */
    int locator_flags;                      /* +0x64 */
    float hearing_distance;                 /* +0x68: Enemy_FindByEar radius */
    short encounter_event_flags;            /* +0x6c: low byte copied to cfEvent */
    short light_level;                      /* +0x6e */
    short default_motion_table[7];          /* +0x70: Get_DefaultMotion */
    unsigned char unmodeled_7e[0x182];
    int value_2c0;                          /* +0x200 */
    int value_2c4;                          /* +0x204 */
    unsigned char unmodeled_208[0x18];
    short linked_actors[4];                 /* +0x220: Enemy_After_Battle */
} EnemyParams;

typedef struct ActorAnimSlot {
    unsigned int flags;
    float frame;
    float speed;
    float reverseLimit;
    float forwardLimit;
    unsigned short currentDataId;
    unsigned short nextDataId;
    unsigned short channelMask; /* +0x18 */
    unsigned short bufferIndex; /* +0x1a */
    unsigned char unmodeled_01c[0x22 - 0x1c];
    unsigned short animQueuedFlags;
} ActorAnimSlot;

typedef struct ActorAnimationState {
    ActorAnimSlot slot;
    float blendDuration; /* +0x24 */
    float blendEndFrame; /* +0x28 */
    void *animData;
    void *animUserData;
    union {
        void *animPack;                    /* +0x34: act_2.c clears it */
        struct FpkFcvHeader *fcvHeader;    /* +0x34: the FPK pack hair_test.c counts */
    };
    unsigned char unmodeled_38[0x48 - 0x38]; /* +0x38 */
    float blendWork[4]; /* +0x48 */
    unsigned char unmodeled_58[0x60 - 0x58]; /* +0x58 */
    unsigned short previousDataId; /* +0x60 */
} ActorAnimationState;

typedef struct ActorAccessoryJointTable {
    unsigned char unmodeled_00[0x20];
    int jointId[0x29];
} ActorAccessoryJointTable;

typedef struct ActorDrawEnv {
    float fogDistance[4];
    float fogColor[4];
    float lightPosition[3][4];
    float lightColor[3][4];
    unsigned int flags;
    unsigned int renderLevel;
    int pixelAlpha;
    int partAlphaCount;
    /*
     * Per-part pixel alpha, [n][0] alpha and [n][1] part id: a plain short
     * array, as Java_xeno_Chr_pixelAlphaParts__II's stores show (a struct
     * element moves the field offset out of the store displacements).
     */
    short partAlpha[4][2];
    float transparency;                                /* +0xa0: Chr peers' first filter float */
    float filter_param[3];                             /* +0xa4: the other three filter floats */
    float cameraCullDistance;
    unsigned char unmodeled_b4[4];
    unsigned long long alpha;
} ActorDrawEnv;

typedef struct ActorAttachment {
    short type;
    short joint;
    void **matrix;
} ActorAttachment;

typedef struct ActorModel {
    unsigned char unmodeled_00[0x54];
    void *matrixResource;
} ActorModel;

typedef struct ActorModelState {
    ActorModel model;
    Matrix4 *jointMatrices;
} ActorModelState;

/* The Java argument copy area of a Chr peer (Actor +0xc0..+0x4bf, ACTOR_ARGS):
 * a struct member, not a union, so its fields keep their own alias sets.
 * Comments give the Actor offset; enemy_2.c's Enemy_Command_Action and
 * Start_Enemy_Command keep &actor->args in a base register. */
typedef struct ActorJavaArgs {
    unsigned char unmodeled_00[0x18];
    u16 idle_motion_id;                  /* +0xd8: Enemy_ActionReady reads it as the idle motion */
    short action_frame;                  /* +0xda: Enemy_Command_Action clears it */
    u16 reaction_motion_id;              /* +0xdc: Enemy_ActionReady reads it as the reaction motion */
    short action_duration;               /* +0xde */
    unsigned char unmodeled_20[0x2c - 0x20];
    int command_code;                    /* +0xec: Enemy_Command_Code stores it before Enemy_Init */
    unsigned char unmodeled_30[0x4c - 0x30];
    int methodArg;                       /* +0x10c: DrillHitActor's enemy-broken method argument */
    unsigned char unmodeled_50[0x60 - 0x50];
    int command_count;                   /* +0x120: queued enemy commands */
    u32 locator_flags;                   /* +0x124 */
    unsigned char unmodeled_68[0x80 - 0x68];
    unsigned int command_type[16];       /* +0x140 */
    int command_parameter[5][16];        /* +0x180: five parameter rows */
    unsigned char unmodeled_200[0x400 - 0x200];
} ActorJavaArgs;

/*
 * What the menu's model actors (MenuModelUnitActorSet, ACT_create(-1)) keep
 * in the +0xc0 block instead of Java arguments: the transform the menu
 * controls move, the draw progress and the draw-type hook. The original
 * copies these vectors as two doublewords and keeps the
 * tskMenuModelTaskMain (1, 1, 1, 1) initializer constant only 8-aligned in
 * .rodata (+0x138), so the copy type is the 8-aligned DoublewordAlignedVector4.
 */
struct Actor;

typedef struct ActorMenuControl {
    AlignedVector4 offset;                             /* +0xc0 */
    AlignedVector4 rotation;                           /* +0xd0 */
    AlignedVector4 scale;                              /* +0xe0 */
    int enabled;                                       /* +0xf0 */
    unsigned char unmodeled_34[0xC];
    AlignedVector4 position;                           /* +0x100 */
    unsigned char unmodeled_50[0x10];
    float transparency;                                /* +0x120: menu draw progress */
    void (*drawTypeFunction)(struct Actor *actor);     /* +0x124 */
    int initialMotionId;                               /* +0x128: selected before opening the unit actor */
    unsigned char unmodeled_6C[4];
} ActorMenuControl;

typedef struct Actor {
    u32 flags;                                         /* +0x00 */
    void (*update)(struct Actor *actor);               /* +0x04 */
    void (*draw)(struct Actor *actor);                 /* +0x08 */
    u32 quadword_alignment_gap;                        /* +0x0c */
    AlignedVector4 position;                           /* +0x10 */
    Vector4 previous_position;                         /* +0x20 */
    Vector4 velocity;                                  /* +0x30 */
    Vector4 acceleration;                              /* +0x40 */
    AlignedVector4 rotation;                           /* +0x50 */
    AlignedVector4 scale;                              /* +0x60 */
    AlignedVector4 globalPosition;                     /* +0x70 */
    u8 number;                                         /* +0x80 */
    u8 signal;                                         /* +0x81 */
    u8 kind;                                           /* +0x82 */
    u8 humanKind;                                      /* +0x83 */
    short state84;                                     /* +0x84 */
    short inUseId;                                     /* +0x86 */
    short frame_counter;                              /* +0x88 */
    unsigned char unmodeled_8a[0x90 - 0x8a];
    u8 shadow_kind;                                    /* +0x90 */
    u8 shadowSize;                                     /* +0x91 */
    unsigned char unmodeled_92[0xa0 - 0x92];
    Vector4 shadow_offset;                             /* +0xa0: set_motion's Set_Shadow reads it */
    short last_step_distance;                          /* +0xb0: Sound_FootStep keeps the previous step here */
    unsigned char unmodeled_b2[0xc0 - 0xb2];
    /* The +0xc0 block is kind-specific: Chr peers keep their Java argument
     * copy here, the menu's model actors keep their transform control, and
     * enemies keep their parameter record. */
    union {
        ActorMenuControl menu;                         /* +0xc0 */
        ActorJavaArgs args;                            /* +0xc0 */
        EnemyParams enemy;                             /* +0xc0 */
    };
    SceneObject java_object;                           /* +0x4c0 */
    unsigned char unmodeled_4c4[0x4c8 - 0x4c4];
    /* +0x4c8 is the UnduParam record UnduParamInit fills; its attr_mask word
     * at +0x4d0 is status_flags, so the record is a plain byte array here. */
    unsigned char undulation[8];                       /* +0x4c8 */
    u16 status_flags;                                  /* +0x4d0 */
    unsigned char unmodeled_4d2[0x4dc - 0x4d2];
    float translate_y;                                 /* +0x4dc */
    struct LayoutHeader *data_header;                  /* +0x4e0 */
    unsigned char unmodeled_4e4[0x4e8 - 0x4e4];
    union {
        u64 attribute_flags;                           /* +0x4e8 */
        u32 collision_bits;                            /* +0x4e8: karakama_test.c reads the low word */
    };
    unsigned char unmodeled_4f0[0x510 - 0x4f0];
    StudioLight light;                                 /* +0x510 */
    union {
        Vector4 cleared600[2];                         /* +0x600 */
        u32 look_clear_600[8];                         /* +0x600: the same cleared block as u32s */
    };
    float look_point[3];                               /* +0x620 */
    float look_eye_speed;                              /* +0x62c */
    unsigned char unmodeled_630[0x640 - 0x630];
    union {
        float lookState[11];                           /* +0x640 */
        struct {
            u32 look_clear_640[8];                     /* +0x640 */
            float look_eye_angle[2];                   /* +0x660: lookState[8..9] */
            unsigned char unmodeled_668[4];
        };
    };
    float look_speed;                                  /* +0x66c */
    float shadowClipScale;                             /* +0x670 */
    short look_mode;                                   /* +0x674 */
    u16 windowId;                                      /* +0x676 */
    int look_eye_control;                              /* +0x678 */
    void *look_target;                                 /* +0x67c */
    u16 mapShadowPartIds[8];                           /* +0x680 */
    int mapShadowPartCount;                            /* +0x690 */
    signed char stick_x;                               /* +0x694 */
    signed char stick_y;                               /* +0x695 */
    short pauseCounter;                                /* +0x696 */
    int attachmentCount;                               /* +0x698 */
    int attachmentIndex;                               /* +0x69c */
    ActorAttachment attachments[8];                    /* +0x6a0 */
    signed char attachmentUsed[8];                     /* +0x6e0 */
    unsigned char unmodeled_6e8[0x6f0 - 0x6e8];
    ActorAnimationState animation;                     /* +0x6f0 */
    int transitionFrames;                              /* +0x754 */
    int holdFrames;                                    /* +0x758 */
    unsigned char unmodeled_75c[4];
    Vector4 rootTranslation;                           /* +0x760 */
    unsigned char unmodeled_770[0x790 - 0x770];
    JntProducer jointProducer;                         /* +0x790..+0x823 */
    void *jointMatrices;                               /* +0x824: ACT_updateMPack reads the same word as its linked actor */
    void *previousJointMatrices;                       /* +0x828 */
    void *staticValueRecords;                          /* +0x82c */
    void *valueRecords;                                /* +0x830 */
    unsigned char unmodeled_834[0x840 - 0x834];
    ActorModelState model[1];                          /* +0x840 */
    unsigned char unmodeled_89c[0x8d0 - 0x89c];
    void *modelResource;                               /* +0x8d0 */
    void *matrixResource;                              /* +0x8d4 */
    void *move;                                        /* +0x8d8 */
    void *animPackTables[8];                           /* +0x8dc */
    struct Actor *parent;                              /* +0x8fc */
    int childCount;                                    /* +0x900 */
    struct Actor *children[7];                         /* +0x904 */
    /* children[7..14] (Chr peers keep fifteen, and ACT_setParent does not cap
     * childCount at seven) would be the words that open drawEnv. A union here
     * was measured to reorder the scheduling of drawEnv's stores (chr.c), so the
     * record keeps drawEnv plain and the Chr loops index children as the original
     * does. */
    ActorDrawEnv drawEnv;                              /* +0x920 */
    float sortOffset;                                  /* +0x9e0 */
    float target_angle;                                /* +0x9e4 */
    float body_radius;                                 /* +0x9e8: 0.35f from ACT_init; collision radius and interaction range */
    short look_at_timer;                               /* +0x9ec */
    short look_at_target;                              /* +0x9ee */
    short flash_timer;                                 /* +0x9f0 */
    unsigned char unmodeled_9f2[0x9f4 - 0x9f2];
    /* script.c reads the same words as the JNI method names of the talk and
     * touch scripts (null when the actor has none). */
    union {
        int talk_message;                              /* +0x9f4 */
        const char *talk_method_name;
    };
    union {
        int touch_message;                             /* +0x9f8 */
        const char *touch_method_name;
    };
    unsigned char unmodeled_9fc[0xa00 - 0x9fc];
    AlignedVector4 saved_position;                     /* +0xa00: enemy_1.c's ACT_updateEnemy */
    SchedulerState *effects[8];                       /* +0xa10 */
    short effect_timer;                                /* +0xa30 */
    short effect_slot;                                 /* +0xa32 */
    unsigned char unmodeled_a34[0xa40 - 0xa34];
    u8 moved_hair_mode;                                /* +0xa40 */
    unsigned char unmodeled_a41[3];
    float moved_hair_delay;                            /* +0xa44 */
    unsigned char unmodeled_a48[8];
    Vector4 moved_hair_sway;                           /* +0xa50 */
    union {
        int statea60;                                  /* +0xa60 */
        union Matrix *previous_joint_matrices;         /* +0xa60: EXM reads it as the owner's previous motion matrices */
    };
    unsigned char unmodeled_a64[0xa70 - 0xa64];
} Actor;

/* Actor origin, raised origin, or an accessory coordinate selector. */
typedef enum MEfActorPoint {
    MEF_POINT_ACTOR = -1,
    MEF_POINT_ACTOR_RAISED = -2,
    MEF_POINT_ACCESSORY_COUNT = 0x28
} MEfActorPoint;


extern const char D_00A512F0[40];
extern const char D_00A51318[40];
extern const char D_00A51340[48];

extern Matrix4 D_68C670;
int sefActorMulLocalMatrix(Actor *actor, int input, Matrix4 *output, int apply);

float MMathMakeRandomPI(void);

float MMathMakeRandom2PI(void);

/* MMathApplyMatrix: rows are passed as float (*)[4] here, so tu194.h (Matrix4 *) is not included. */
Vector4 *MMathApplyMatrix(Vector4 *out, float (*matrix)[4], const Vector4 *vector);

typedef struct MEfPhaseBlock {
    float cornerAngles[4];  /* four float corner bearings, read as float[4] by MEfMake4Vertex2 */
} MEfPhaseBlock;

typedef struct MEfCornerTable {
    MEfPhaseBlock phases;                    /* +0x00 */
    unsigned char unmodeled_10[0x08];
    unsigned long long corner[2];            /* +0x18: the corner words MEfMakeCornerMatrix loads */
} MEfCornerTable;

extern const MEfCornerTable MEfVertex2CornerTable;



/* A packed RGBAQ colour quadword. */
typedef union MEfGsColor {
    Quadword whole;
    unsigned int rgba[4];
} MEfGsColor;

/* A packed GS vertex: the projected point, or its words (w carries ADC). */
typedef union MEfGsVertex {
    Quadword whole;
    AlignedVector4 vector;
    int words[4];
} MEfGsVertex;


/* GIF tag fields: EOP is bit 15 of the low word, PRE, PRIM and NREG start at bits 46, 47 and 60. */
#define MEF_GIF_TAG_PRE_SHIFT 46
#define MEF_GIF_TAG_PRIM_SHIFT 47
#define MEF_GIF_TAG_NREG_SHIFT 60

/* Height of the raised origin (MEF_POINT_ACTOR_RAISED) above the actor. */
#define MEF_ACTOR_RAISED_HEIGHT 1.5f

/* sqrt(2), the diagonal factor of a square's half side. */
#define MEF_SQRT2 1.41421356f

/* Frames a wait step runs before MSeObjRequestFree (MEfWaitStep). */
#define MEF_WAIT_FRAMES 31

/* Steps of a phase-flag sweep (MEfSetPhaseFlags): from step MEF_PHASE_STEPS on, the flags stay clear. */
#define MEF_PHASE_STEPS 30

/* Points on the path curve (mefPathCurve) and its key walk size in bytes (two walks of 25 Vector4). */
#define MEF_CURVE_POINTS 4
#define MEF_KEY_WALK_BYTES 0x320

/* Bits of MEfStepRecord.flags_history[0]: the phase is active, starting or ending. */
typedef enum MEfPhaseFlag {
    MEF_PHASE_ACTIVE = 0x01,
    MEF_PHASE_START = 0x10,
    MEF_PHASE_END = 0x20
} MEfPhaseFlag;


/* Writes a GIF tag for regCount registers and returns its first data entry. */
static inline MEfGsQword *OpenTag(MGsPacket *packet, int primEnable, int primitive, int regCount, u64 registers)
{
    MEfGsQword *entry = packet->current;

    entry->gif.tag = MEF_GIF_TAG_EOP | ((u64) primEnable << MEF_GIF_TAG_PRE_SHIFT) | ((u64) primitive << MEF_GIF_TAG_PRIM_SHIFT) | ((u64) regCount << MEF_GIF_TAG_NREG_SHIFT);
    entry->gif.registers = registers;
    packet->regCount = regCount;
    return entry + 1;
}

/* Writes one A+D register setting. */
static inline void SetRegister(MEfGsQword *entry, unsigned char address, u64 data)
{
    entry->ad.data = data;
    entry->ad.address = address;
}

/* Stores NLOOP into the open GIF tag and moves past its data; an empty tag stays open. */
static inline void CloseTag(MGsPacket *packet, int nloop)
{
    if (nloop > 0) {
        int entryCount = packet->regCount * nloop + 1;

        packet->current->nloop |= nloop;
        packet->current += entryCount;
        packet->count += entryCount;
    }
}

/* Colour templates (two RGBAQ quadwords each) and the per-strand edge vectors read by MEfDrawStepTrail. */
extern const MEfGsColor mefStepHeadColors[2];
extern const MEfGsColor mefStepTailColors[2];
extern const DoublewordAlignedVector4 D_00ABB1D0[4];
extern const DoublewordAlignedVector4 D_00ABB210[4];
extern const DoublewordAlignedVector4 D_00ABB270[4];
extern const DoublewordAlignedVector4 D_00ABB2B0[4];

/*
 * The object the step functions below receive. Offsets follow their loads and
 * stores: the phase-flags, update and advance callbacks at +0x08, +0x10 and
 * +0x14, and the step record at +0x20.
 */
typedef struct MEfStepRecord {
    unsigned char unmodeled_00[0x04];
    Actor *actor;                            /* +0x04 */
    u32 coordinate;                          /* +0x08 */
    unsigned char unmodeled_0c[0x20 - 0x0c];
    Vector4 vector;                          /* +0x20 */
    unsigned char unmodeled_30[0x40 - 0x30];
    MGsPacket packet;                        /* +0x40 (object +0x60): MGsGPInit / MGsGPTerm */
    short step;                              /* +0x54 (object +0x74) */
    unsigned char unmodeled_56[0x60 - 0x56];
    Vector4 origin;                          /* +0x60: matrix row 3 with w = 1 */
    int counter;                             /* +0x70 (object +0x90) */
    unsigned char unmodeled_74[0xb0 - 0x74];
    float angle;                             /* +0xb0 */
    unsigned short flags_history[25];        /* +0xb4 (object +0xd4): [0] is the flags halfword, [1..24] its history */
    unsigned char unmodeled_e6[0xf0 - 0xe6];
    union {
        Vector4 path[4][25];
        RgVector path_rows[4][25];
    };                     /* +0xf0: rows 0 and 1 the two key walks, rows 2 and 3 the sampled curve */
} MEfStepRecord;

typedef struct MEfStepObject {
    unsigned char unmodeled_00[0x08];
    void (*flags_callback)();
    unsigned char unmodeled_0c[0x04];
    void (*update_callback)();
    void (*advance_callback)();
    unsigned char unmodeled_18[0x20 - 0x18];
    MEfStepRecord record;                    /* +0x20 */
} MEfStepObject;

/*
 * Record of the path step (the same object +0x20 slot as MEfStepRecord).
 * The path step keeps a matrix at +0x70 where the wait step keeps its
 * counter, so it has its own record type.
 */
typedef struct MEfPathRecord {
    unsigned char unmodeled_00[0x04];
    Actor *actor;                            /* +0x04 */
    u32 coordinate;                          /* +0x08 */
    unsigned char unmodeled_0c[0x20 - 0x0c];
    Vector4 offset;                          /* +0x20: MMathApplyMatrix input */
    unsigned char unmodeled_30[0x40 - 0x30];
    MGsPacket packet;                        /* +0x40: MGsGPInit / MGsGPTerm */
    short step;                              /* +0x54 */
    unsigned char unmodeled_56[0x60 - 0x56];
    Vector4 origin;                          /* +0x60: matrix row 3 with w = 1 */
    Matrix4 matrix;                          /* +0x70 */
    float angle;                             /* +0xb0 */
    unsigned short history[19];              /* +0xb4 */
    unsigned char unmodeled_da[0xe0 - 0xda];
    Vector4 track[4][19];                    /* +0xe0: rows 0-1 sampled points, rows 2-3 their angles */
} MEfPathRecord;

/* mefPathCurve: ov01RecordInit clears 0xb0 bytes and stores the count at +0. */
typedef struct MEfPathCurve {
    int count;                               /* +0x00 */
    unsigned char unmodeled_04[0x10 - 0x04];
    Vector4 points[4];                       /* +0x10 */
    unsigned char unmodeled_50[0xb0 - 0x50];
} MEfPathCurve;

typedef union MEfPathCurveObject {
    MEfPathCurve curve;
    Vector4 vectors[512];
} MEfPathCurveObject;

extern MEfPathCurveObject mefPathCurve;
unsigned short xglSRand(void);
/* ov01RecordInit: the definition is tu076.c; this file keeps its own view (void * record). */
void ov01RecordInit(void *record, int value);
void *memcpy(void *, const void *, unsigned int);

void func_00AABDF0(void);
void MEfWaitUpdate(void);
void MEfWaitStep(MEfStepObject *object);

void *memset(void *s, int c, unsigned int n);
void MEfStartFromActor(MEfStepRecord *record);
void MEfSampleHermitePath(MEfStepRecord *record);
void MEfSetPhaseFlags(MEfStepObject *object);
void MEfAdvanceStep(MEfStepObject *object);
void MEfDrawStepTrail(MEfStepObject *object);
extern Vector4 MMathUnitVector;
/* MMathCalcHermiteKey(Prm): the tu076.c definitions take other argument types, so this file keeps its own view. */
void MMathCalcHermiteKeyPrm(void *base, int index);
void MMathCalcHermiteKey(Vector4 *destination, void *base, int index, float parameter);

int ov01LastValueGet(void);

void MEfStartPathFromActor(MEfPathRecord *record);
void MEfSamplePathTrack(MEfPathRecord *record);

/* Vu0Sin/Vu0Cos with the COP1 hazard nop written into the block (the original schedules nothing into that slot). */
#define MEfPathSin(result, angle) \
    __asm__ __volatile__( \
        "mfc1 $8, %1\n" \
        "qmtc2 $8, vf4\n" \
        "vcallms 0x20\n" \
        "qmfc2.i $8, vf1\n" \
        "mtc1 $8, %0\n" \
        "nop\n" \
        : "=f" (result) \
        : "f" (angle) \
        : "$8")

#define MEfPathCos(result, angle) \
    __asm__ __volatile__( \
        "mfc1 $8, %1\n" \
        "qmtc2 $8, vf4\n" \
        "vcallms 0xe8\n" \
        "qmfc2.i $8, vf1\n" \
        "mtc1 $8, %0\n" \
        "nop\n" \
        : "=f" (result) \
        : "f" (angle) \
        : "$8")

#define QuadwordCopyT0Dst(destination, source) \
    __asm__ __volatile__( \
        "lq $8, 0(%6)\n" \
        "sq $8, 0(%5)\n" \
        : "=m" (((float *) (destination))[0]), "=m" (((float *) (destination))[1]), "=m" (((float *) (destination))[2]), "=m" (((float *) (destination))[3]), "=m" (((float *) (destination))[3]) \
        : "r" (destination), "r" (source) \
        : "$8", "memory")

Matrix4 *MSeGetActorMatrix(Matrix4 *destination, Actor *actor, u32 coordinate)
{
    __asm__ __volatile__(
        "lqc2 $vf1, 32(%1)\n"
        "lqc2 $vf2, 16(%1)\n"
        "lqc2 $vf3, 0(%1)\n"
        "sqc2 $vf0, 48(%0)\n"
        "sqc2 $vf1, 32(%0)\n"
        "sqc2 $vf2, 16(%0)\n"
        "sqc2 $vf3, 0(%0)\n"
        :
        : "r" (destination), "r" (D_68C670)
        : "memory");
    if (actor == 0) {
        MOutputDebugStringWarn("MSeGetActorMatrix: actor is NULL");
        return destination;
    }
    switch (coordinate) {
    case MEF_POINT_ACTOR:
        MMathRotateMatrixY(destination, destination, actor->rotation.y);
        __asm__ __volatile__(
            "lqc2 $vf1, 0(%1)\n"
            "vmove.w $vf1w, $vf0w\n"
            "sqc2 $vf1, 48(%0)\n"
            :
            : "r" (destination), "r" (&actor->position)
            : "memory");
        return destination;
    case MEF_POINT_ACTOR_RAISED:
        MMathRotateMatrixY(destination, destination, actor->rotation.y);
        __asm__ __volatile__(
            "lqc2 $vf1, 0(%1)\n"
            "vmove.w $vf1w, $vf0w\n"
            "sqc2 $vf1, 48(%0)\n"
            :
            : "r" (destination), "r" (&actor->position)
            : "memory");
        (*destination)[3][1] = MEF_ACTOR_RAISED_HEIGHT;
        break;
    default:
        if (sefActorMulLocalMatrix(actor, coordinate, destination, 1) == 0) {
            MMathRotateMatrixY(destination, destination, actor->rotation.y);
            __asm__ __volatile__(
                "lqc2 $vf1, 0(%1)\n"
                "vmove.w $vf1w, $vf0w\n"
                "sqc2 $vf1, 48(%0)\n"
                :
                : "r" (destination), "r" (&actor->position)
                : "memory");
            MOutputDebugStringWarn("MSeGetActorMatrix: Invalid point (%d)", coordinate);
        }
        break;
    }
    return destination;
}

/*
 * MEfGetActorCoord: the translation row of MSeGetActorMatrix, with w = 1.
 */
Vector4 *MEfGetActorCoord(Vector4 *destination, Actor *actor, u32 coordinate)
{
    Matrix4 actorMatrix;

    MSeGetActorMatrix(&actorMatrix, actor, coordinate);
    __asm__ __volatile__(
        "lqc2 $vf1,48(%1)\n\t"
        "vmove.w $vf1w,$vf0w\n\t"
        "sqc2 $vf1,0(%0)\n\t"
        :
        : "r"(destination), "r"(actorMatrix)
        : "memory"
    );
    return destination;
}

Vector4 *MEfMakeRandomVector(Vector4 *destination)
{
    destination->x = MMathMakeRandomPI();
    destination->y = MMathMakeRandomPI();
    destination->z = MMathMakeRandomPI();
    destination->w = 1.0f;
    return destination;
}

Vector4 *MEfScatterXY(Vector4 *destination, Vector4 *target, Vector4 *source, float spread)
{
    Vector4 angles;
    Matrix4 rotation;
    Vector4 cosSin[2];  /* [0].x = cosine, [1].x = sine: the VU0 0xe8 and 0x20 microprogram results */
    float randomAngle;

    MMathCalcAngle(&angles, source, target);
    MMathRotateMatrixYX(&rotation, 0, &angles);
    __asm__ __volatile__(
        "lqc2 $vf1, 0(%1)\n"
        "vmove.w $vf1w, $vf0w\n"
        "sqc2 $vf1, 48(%0)\n"
        :
        : "r" (&rotation), "r" (source)
        : "memory");
    randomAngle = MMathMakeRandom2PI();
    spread *= MMathMakeRandom();
    __asm__ __volatile__(
        "sqc2 $vf0, 0(%0)\n"
        :
        : "r" (&angles)
        : "memory");
    __asm__ __volatile__(
        "mfc1 $8, %1\n"
        "qmtc2 $8, $vf4\n"
        "vcallms 0xe8\n"
        "qmfc2.i $8, $vf1\n"
        "mtc1 $8, %0\n"
        "nop\n"
        : "=f" (cosSin[0].x)
        : "f" (randomAngle)
        : "$8");
    angles.x = spread * cosSin[0].x;
    __asm__ __volatile__(
        "mfc1 $8, %1\n"
        "qmtc2 $8, $vf4\n"
        "vcallms 0x20\n"
        "qmfc2.i $8, $vf1\n"
        "mtc1 $8, %0\n"
        "nop\n"
        : "=f" (cosSin[1].x)
        : "f" (randomAngle)
        : "$8");
    angles.y = spread * cosSin[1].x;
    MMathApplyMatrix(destination, rotation, &angles);
    return destination;
}

int MEfMake4Vertex(Vector4 *destination, const Vector4 *source, float halfSize)
{
    MEfCameraParams *camera;
    Matrix4 basis;
    Vector4 offset;
    Vector4 *vertex;
    int rejected;
    int i;

    camera = &MEfCamParams;
    rejected = 0;
    __asm__ __volatile__(
        "lq $8, 0(%4)\n"
        "lq $9, 16(%4)\n"
        "lq $10, 32(%4)\n"
        "lq $11, 48(%4)\n"
        "sq $8, %0\n"
        "sq $9, %1\n"
        "sq $10, %2\n"
        "sq $11, %3\n"
        : "=m" (basis[0][0]), "=m" (basis[1][0]), "=m" (basis[2][0]), "=m" (basis[3][0])
        : "r" (&camera->rotationMatrix)
        : "$8", "$9", "$10", "$11", "memory");
    __asm__ __volatile__(
        "lqc2 $vf1, 0(%1)\n"
        "vmove.w $vf1w, $vf0w\n"
        "sqc2 $vf1, %0\n"
        : "=m" (basis[3][0])
        : "r" (source)
        : "memory");
    vertex = destination;
    for (i = 0; i < 4; i++) {
        __asm__ __volatile__(
            "sqc2 $vf0, 0(%0)\n"
            :
            : "r" (&offset)
            : "memory");
        offset.x = (i & 2) ? -halfSize : halfSize;
        offset.y = (i & 1) ? -halfSize : halfSize;
        MMathApplyMatrix(&offset, basis, &offset);
        if (MMathRotTransPersClip(vertex, camera->camera, &camera->viewMatrix, &offset)) {
            rejected++;
        }
        vertex++;
    }
    return rejected;
}

int MEfMake4Vertex2(Vector4 *destination, const Vector4 *source, float halfSize, float angleBase)
{
    MEfCameraParams *camera;
    MEfPhaseBlock phaseCopy;
    Matrix4 basis;
    Vector4 offset;
    Vector4 cosSin[2];  /* [0].x = cosine, [1].x = sine: the VU0 0xe8 and 0x20 microprogram results */
    float cornerAngle;
    int rejected;
    int i;
    Vector4 *vertex;
    Matrix4 *viewMatrix;

    camera = &MEfCamParams;
    rejected = 0;
    phaseCopy = MEfVertex2CornerTable.phases;
    QuadwordCopyMatrixT0(basis, camera->rotationMatrix);
    __asm__ __volatile__(
        "lqc2 $vf1, 0(%5)\n"
        "vmove.w $vf1w, $vf0w\n"
        "sqc2 $vf1, 48(%4)\n"
        : "=m" (basis[3][0]), "=m" (basis[3][1]), "=m" (basis[3][2]), "=m" (basis[3][3])
        : "r" (basis), "r" (source)
        : "memory");
    vertex = destination;
    viewMatrix = &camera->viewMatrix;
    /* The corners sit on the diagonal: their radius is the half side times sqrt(2). */
    halfSize *= MEF_SQRT2;
    for (i = 0; i <= 3; i++) {
        cornerAngle = angleBase + ((float *)&phaseCopy)[i];
        __asm__ __volatile__(
            "sqc2 $vf0, 0(%0)\n"
            :
            : "r" (&offset)
            : "memory");
        __asm__ __volatile__(
            "mfc1 $8, %1\n"
            "qmtc2 $8, $vf4\n"
            "vcallms 0xe8\n"
            "qmfc2.i $8, $vf1\n"
            "mtc1 $8, %0\n"
            "nop\n"
            : "=f" (cosSin[0].x)
            : "f" (cornerAngle)
            : "$8");
        offset.x = halfSize * cosSin[0].x;
        __asm__ __volatile__(
            "mfc1 $8, %1\n"
            "qmtc2 $8, $vf4\n"
            "vcallms 0x20\n"
            "qmfc2.i $8, $vf1\n"
            "mtc1 $8, %0\n"
            "nop\n"
            : "=f" (cosSin[1].x)
            : "f" (cornerAngle)
            : "$8");
        offset.y = halfSize * cosSin[1].x;
        MMathApplyMatrix(&offset, basis, &offset);
        if (MMathRotTransPersClip(vertex, camera->camera, viewMatrix, &offset)) {
            rejected++;
        }
        vertex++;
    }
    return rejected;
}

int MEfCreateWait(MEfStepObject *object)
{
    object->flags_callback = func_00AABDF0;
    object->update_callback = MEfWaitUpdate;
    object->record.counter = 0;
    object->advance_callback = MEfWaitStep;
    return 1;
}

void func_00AABDF0(void)
{
}

Matrix4 *MEfMakeCornerMatrix(Matrix4 *destination)
{
    unsigned long long *cornerWords = MEfVertex2CornerTable.corner;
    DoublewordVector4 translation;

    translation.words[0] = cornerWords[0];
    translation.words[1] = cornerWords[1];
    __asm__ __volatile__(
        "lqc2 $vf1, 0(%1)\n"
        "vmove.w $vf1w, $vf0w\n"
        "lqc2 $vf2, 32(%2)\n"
        "lqc2 $vf3, 16(%2)\n"
        "lqc2 $vf4, 0(%2)\n"
        "sqc2 $vf1, 48(%0)\n"
        "sqc2 $vf2, 32(%0)\n"
        "sqc2 $vf3, 16(%0)\n"
        "sqc2 $vf4, 0(%0)\n"
        :
        : "r" (destination), "r" (&translation), "r" (D_68C670)
        : "memory");
    return destination;
}

void MEfWaitUpdate(void)
{
    Matrix4 scratch;
    DoublewordVector4 cornerVector;
    unsigned long long *followingCornerWords;
    int count;

    followingCornerWords = MEfVertex2CornerTable.corner + 2;
    cornerVector.words[0] = followingCornerWords[0];
    cornerVector.words[1] = followingCornerWords[1];
    for (count = 0x14050; count != 0; count--) {
        func_00272EC8(scratch, &cornerVector.vector);
    }
}

void MEfWaitStep(MEfStepObject *object)
{
    MEfStepRecord *record;

    record = &object->record;
    record->counter = record->counter + 1;
    if (record->counter >= MEF_WAIT_FRAMES) {
        MSeObjRequestFree(object);
    }
}

int MEfCreateHermitePath(MEfStepObject *object)
{
    MEfStepRecord *record;
    Vector4 *keyWalks;

    record = &object->record;
    keyWalks = object->record.path[0];
    record->step = 0;
    MEfStartFromActor(record);
    memset(record->flags_history, 0, sizeof(record->flags_history));
    memset(keyWalks, 0, MEF_KEY_WALK_BYTES);
    __asm__ __volatile__(
        "lq $8, 0(%4)\n"
        "sq $8, 0(%5)\n"
        : "=m" (keyWalks->x), "=m" (keyWalks->y), "=m" (keyWalks->z), "=m" (keyWalks->w)
        : "r" (&object->record.origin), "r" (keyWalks)
        : "$8", "memory");
    QuadwordCopyT0(object->record.path[1], &object->record.origin);
    MEfSampleHermitePath(record);
    MGsGPInit(&object->record.packet, 0, 0);
    object->flags_callback = MEfSetPhaseFlags;
    object->update_callback = MEfDrawStepTrail;
    object->advance_callback = MEfAdvanceStep;
    return 1;
}

void MEfStartFromActor(MEfStepRecord *record)
{
    /* Corner offset tables, defined (const) at the end of this file. */
    extern float D_00ABB190[4];
    extern float D_00ABB1A0[4];
    Vector4 *curveKeys;
    Matrix4 matrix;
    Vector4 corner;
    Vector4 lanes[2];
    float angle;
    float randomScale;
    float angleStep;
    int i;

    curveKeys = mefPathCurve.vectors;
    MSeGetActorMatrix(&matrix, record->actor, record->coordinate);
    MMathApplyMatrix(&corner, matrix, &record->vector);
    __asm__ __volatile__(
        "lqc2 $vf1, 0(%1)\n"
        "vmove.w $vf1w, $vf0w\n"
        "sqc2 $vf1, 48(%0)\n"
        :
        : "r" (matrix), "r" (&corner)
        : "memory");
    QuadwordCopyMatrixT0((Matrix4 *)&record->counter, matrix);
    __asm__ __volatile__(
        "lqc2 $vf1, 48(%2)\n"
        "vmove.w $vf1w, $vf0w\n"
        "sqc2 $vf1, 0(%1)\n"
        : "=m" (record->origin)
        : "r" (&record->origin), "r" (matrix)
        : "memory");
    do {
        angle = MMathMakeRandom2PI();
    } while (!(angle < 3.83972430229187f) && !(5.585053443908691f < angle));
    record->angle = angle;
    angleStep = 0.10471976f;
    if ((xglSRand() & 1) == 0) {
        angleStep = -0.10471976f;
    }
    randomScale = MMathMakeRandom() + 0.05f;
    ov01RecordInit(curveKeys, MEF_CURVE_POINTS);
    for (i = 0; i < MEF_CURVE_POINTS; i++) {
        __asm__ __volatile__(
            "sqc2 $vf0, 0(%0)\n"
            :
            : "r" (&corner), "r" (&corner)
            : "memory");
        __asm__ __volatile__(
            "mfc1 $8, %1\n"
            "qmtc2 $8, $vf4\n"
            "vcallms 0xe8\n"
            "qmfc2.i $8, $vf1\n"
            "mtc1 $8, %0\n"
            "nop\n"
            : "=f" (lanes[0].x)
            : "f" (angle)
            : "$8");
        corner.x = lanes[0].x * D_00ABB190[i] * randomScale;
        __asm__ __volatile__(
            "mfc1 $8, %1\n"
            "qmtc2 $8, $vf4\n"
            "vcallms 0x20\n"
            "qmfc2.i $8, $vf1\n"
            "mtc1 $8, %0\n"
            "nop\n"
            : "=f" (lanes[1].x)
            : "f" (angle)
            : "$8");
        corner.y = lanes[1].x * D_00ABB190[i] * randomScale;
        corner.z = D_00ABB1A0[i];
        angle += angleStep;
        MMathApplyMatrix(&curveKeys[i + 1], matrix, &corner);
    }
}

void MEfSampleHermitePath(MEfStepRecord *record)
{
    Vector4 *curveKeys;
    MEfStepRecord *rec;
    Matrix4 matrix;
    Vector4 hermitePoint;
    Vector4 corner;
    Vector4 lanes[2];
    float angle;
    float sineValue;
    float cosineValue;
    float weightSpeed;
    float weight;
    float negWeight;
    int i;
    Vector4 *recordCell;
    int segment;
    int segmentStep;
    int k;

    curveKeys = mefPathCurve.vectors;
    rec = record;
    weight = 0.0f;
    weightSpeed = 0.5f;
    angle = MMathMakeRandom2PI();
    __asm__ __volatile__(
        "mfc1 $8, %1\n"
        "qmtc2 $8, $vf4\n"
        "vcallms 0x20\n"
        "qmfc2.i $8, $vf1\n"
        "mtc1 $8, %0\n"
        "nop\n"
        : "=f" (sineValue)
        : "f" (angle)
        : "$8");
    lanes[0].x = sineValue;
    __asm__ __volatile__(
        "mfc1 $8, %1\n"
        "qmtc2 $8, $vf4\n"
        "vcallms 0xe8\n"
        "qmfc2.i $8, $vf1\n"
        "mtc1 $8, %0\n"
        "nop\n"
        : "=f" (cosineValue)
        : "f" (angle)
        : "$8");
    lanes[1].x = cosineValue;
    QuadwordCopyMatrixT0(matrix, (Matrix4 *)&record->counter);
    __asm__ __volatile__(
        "lqc2 $vf1, 0(%1)\n"
        "vmove.w $vf1w, $vf0w\n"
        "sqc2 $vf1, 48(%0)\n"
        :
        : "r" (matrix), "r" (&MMathUnitVector)
        : "memory");

    for (i = 0; i < 25; i++) {
        recordCell = (Vector4 *)rec + i;
        segment = i / 8;
        segmentStep = i - segment * 8;
        if (segment == 3) {
            segment = 2;
            segmentStep = 8;
        }
        MMathCalcHermiteKeyPrm(curveKeys, segment);
        MMathCalcHermiteKey(&hermitePoint, curveKeys, segment, (float)segmentStep * 0.125f);
        __asm__ __volatile__(
            "sqc2 $vf0, 0(%4)\n"
            : "=m" (corner.x), "=m" (corner.y), "=m" (corner.z), "=m" (corner.w)
            : "r" (&corner)
            : "memory");
        corner.x = cosineValue * weight;
        corner.y = sineValue * weight;
        MMathApplyMatrix(&corner, matrix, &corner);
        __asm__ __volatile__(
            "lqc2 $vf1, 0(%4)\n"
            "lqc2 $vf2, 0(%5)\n"
            "vadd.xyz $vf1xyz, $vf1xyz, $vf2xyz\n"
            "sqc2 $vf1, 0(%5)\n"
            : "=m" (corner.x), "=m" (corner.y), "=m" (corner.z), "=m" (corner.w)
            : "r" (&hermitePoint), "r" (&corner)
            : "memory");
        __asm__ __volatile__(
            "lq $8, 0(%3)\n"
            "sq $8, 0(%4)\n"
            : "=m" (recordCell[15].y), "=m" (recordCell[15].z), "=m" (recordCell[15].w)
            : "r" (&corner), "r" (&recordCell[15])
            : "$8", "memory");
        __asm__ __volatile__(
            "sqc2 $vf0, 0(%4)\n"
            : "=m" (corner.x), "=m" (corner.y), "=m" (corner.z), "=m" (corner.w)
            : "r" (&corner)
            : "memory");
        negWeight = -weight;
        corner.x = cosineValue * negWeight;
        corner.y = sineValue * negWeight;
        MMathApplyMatrix(&corner, matrix, &corner);
        __asm__ __volatile__(
            "lqc2 $vf1, 0(%4)\n"
            "lqc2 $vf2, 0(%5)\n"
            "vadd.xyz $vf1xyz, $vf1xyz, $vf2xyz\n"
            "sqc2 $vf1, 0(%5)\n"
            : "=m" (corner.x), "=m" (corner.y), "=m" (corner.z), "=m" (corner.w)
            : "r" (&hermitePoint), "r" (&corner)
            : "memory");
        __asm__ __volatile__(
            "lq $8, 0(%4)\n"
            "sq $8, 0(%5)\n"
            : "=m" (recordCell[40].x), "=m" (recordCell[40].y), "=m" (recordCell[40].z), "=m" (recordCell[40].w)
            : "r" (&corner), "r" (&recordCell[40])
            : "$8", "memory");
        if (i >= 11) {
            weightSpeed *= 0.85f;
            weight += weightSpeed;
        }
    }

    for (k = 0; k < 2; k++) {
        for (i = 0; i < 24; i++) {
            MMathCalcAngle(&rec->path[k + 2][i], &rec->path[k][i], &rec->path[k][i + 1]);
            rec->path_rows[k + 2][i][2] = rec->angle;
        }
        QuadwordCopyT0(&rec->path[k + 2][24], &rec->path[k + 2][23]);
    }
}

void MEfSetPhaseFlags(MEfStepObject *object)
{
    MEfStepRecord *record;
    short step;

    record = &object->record;
    if (ov01LastValueGet() == 0) {
        step = record->step;
        record->flags_history[0] = 0U;
        if (step < MEF_PHASE_STEPS) {
            record->flags_history[0] = MEF_PHASE_ACTIVE;
            if (step == 0) {
                record->flags_history[0] = MEF_PHASE_START | MEF_PHASE_ACTIVE;
            }
            if (step == MEF_PHASE_STEPS - 1) {
                record->flags_history[0] |= MEF_PHASE_END;
            }
        }
    }
}

/* Copies a vector through $8; the four lanes of the destination are the outputs. */
#define QuadwordCopyT0Lanes(destination, source) \
    __asm__ __volatile__( \
        "lq $8, 0(%5)\n" \
        "sq $8, 0(%4)\n" \
        : "=m" ((destination)->x), "=m" ((destination)->y), "=m" ((destination)->z), "=m" ((destination)->w) \
        : "r" (destination), "r" (source) \
        : "$8")

/* Zeroes a vector from vf0; the four lanes of the destination are the outputs. */
#define Vu0SetOriginLanes(destination) \
    __asm__ __volatile__( \
        "sqc2 vf0, 0(%4)\n" \
        : "=m" ((destination)->x), "=m" ((destination)->y), "=m" ((destination)->z), "=m" ((destination)->w) \
        : "r" (destination))

/* Copies a point into the translation row of a matrix and sets its w to 1. */
#define Vu0MMathSetTranslation(matrix, source) \
    __asm__ __volatile__( \
        "lqc2 vf1, 0(%5)\n" \
        "vmove.w vf1, vf0\n" \
        "sqc2 vf1, 48(%4)\n" \
        : "=m" ((matrix)[3][0]), "=m" ((matrix)[3][1]), "=m" ((matrix)[3][2]), "=m" ((matrix)[3][3]) \
        : "r" (matrix), "r" (source))

/*
 * Draws the stepped trail as GS triangle strips. Each visible step gives a pair of
 * vertices: the strand's two edge vectors rotated by the step's angles, moved to the
 * step's point and projected; a clipped vertex restarts the strip (ADC on the next two).
 */
void MEfDrawStepTrail(MEfStepObject *object)
{
    AlignedVector4 edge[2];
    AlignedVector4 corner;
    Matrix4 matrix;
    MEfGsVertex top;
    MEfGsVertex bottom;
    MEfGsColor colour[2];
    MEfStepRecord *record;
    struct XglCamera *camera;
    Matrix4 *viewMatrix;
    MEfGsQword *cursor;
    MEfGsQword *stepQuads;
    int stripLength;
    int strand;
    int row;
    MGsPacket *packet;
    MEfGsQword *header;
    int cullLeft;
    int step;
    int sample;
    int fade;

    packet = &object->record.packet;
    camera = MEfCamParams.camera;
    viewMatrix = &MEfCamParams.viewMatrix;
    MGsGPInit(packet, 0, 0);
    header = packet->current;
    header[0].gif.tag = MEF_GS_SETUP_TAG;
    header[0].gif.registers = MEF_GS_SETUP_REGS;
    packet->regCount = 1;
    cursor = header + 1;
    SetRegister(cursor, MEF_GS_TEST_1, MEF_GS_TEST_1_ATE_ATST_GREATER_ZGEQUAL);
    SetRegister(&header[2], MEF_GS_ZBUF_1, MEF_GS_ZBUF_1_NO_DEPTH_WRITE);
    SetRegister(&header[3], MEF_GS_ALPHA_1, MEF_GS_ALPHA_1_ADDITIVE);
    CloseTag(packet, 3);
    record = &object->record;

    /* The head of the trail: one strip per strand, fading with the step. */
    for (strand = 0; strand < 4; strand++) {
        stripLength = 0;
        cullLeft = 0;
        cursor = OpenTag(&record->packet, 1, MEF_GS_STRIP_PRIM, 4, MEF_GS_STRIP_REGS);
        for (step = 0; step < 12; step++) {
            if ((short) record->flags_history[step] == 0) {
                continue;
            }
            fade = 11 - step;
            colour[0].whole = mefStepHeadColors[0].whole;
            colour[1].whole = mefStepHeadColors[1].whole;
            colour[0].rgba[1] += fade * 8;
            colour[1].rgba[1] += fade;
            colour[0].rgba[2] -= fade * 8;
            colour[1].rgba[2] -= fade * 8;
            if ((record->flags_history[step] & (MEF_PHASE_START | MEF_PHASE_END)) != 0) {
                colour[0].rgba[3] = colour[1].rgba[3] = 0;
                Vu0SetOriginLanes(&edge[0]);
                Vu0SetOriginLanes(&edge[1]);
            } else if (step == 11) {
                colour[0].rgba[3] = colour[1].rgba[3] = 0;
                QuadwordCopyT0Lanes(&edge[0], &D_00ABB270[strand]);
                QuadwordCopyT0Lanes(&edge[1], &D_00ABB2B0[strand]);
            } else {
                QuadwordCopyT0Lanes(&edge[0], &D_00ABB1D0[strand]);
                QuadwordCopyT0Lanes(&edge[1], &D_00ABB210[strand]);
            }
            MMathRotateMatrixYXZ(&matrix, 0, &record->path[2][step]);
            Vu0MMathSetTranslation(matrix, &record->path[0][step]);
            MMathApplyMatrix(&corner, matrix, &edge[0]);
            if (MMathRotTransPersClip(&top.vector, camera, viewMatrix, &corner) != 0) {
                cullLeft = 2;
            } else if (cullLeft != 0) {
                cullLeft--;
                top.words[3] = MEF_GS_XYZ_ADC;
            }
            MMathApplyMatrix(&corner, matrix, &edge[1]);
            if (MMathRotTransPersClip(&bottom.vector, camera, viewMatrix, &corner) != 0) {
                cullLeft = 2;
            } else if (cullLeft != 0) {
                cullLeft--;
                bottom.words[3] = MEF_GS_XYZ_ADC;
            }
            cursor[stripLength * 4 + 0].whole = colour[0].whole;
            cursor[stripLength * 4 + 1].whole = top.whole;
            cursor[stripLength * 4 + 2].whole = colour[1].whole;
            cursor[stripLength * 4 + 3].whole = bottom.whole;
            stripLength++;
        }
        CloseTag(&record->packet, stripLength);
    }

    /* The tail: both rows of the path, with four samples between the points of steps 22 and 23. */
    for (row = 0; row < 2; row++) {
        for (strand = 0; strand < 4; strand++) {
            cursor = OpenTag(&record->packet, 1, MEF_GS_STRIP_PRIM, 4, MEF_GS_STRIP_REGS);
            cullLeft = 0;
            stripLength = 0;
            for (step = 10; step < 25; step++) {
                if ((short) record->flags_history[step] == 0) {
                    continue;
                }
                colour[1].whole = mefStepTailColors[1].whole;
                colour[0].whole = mefStepTailColors[0].whole;
                colour[1].rgba[1] -= step - 10;
                if ((record->flags_history[step] & (MEF_PHASE_START | MEF_PHASE_END)) != 0) {
                    colour[0].rgba[3] = colour[1].rgba[3] = 0;
                    Vu0SetOriginLanes(&edge[0]);
                    Vu0SetOriginLanes(&edge[1]);
                } else if (step == 10) {
                    colour[0].rgba[3] = colour[1].rgba[3] = 0;
                } else {
                    QuadwordCopyT0Lanes(&edge[0], &D_00ABB270[strand]);
                    QuadwordCopyT0Lanes(&edge[1], &D_00ABB2B0[strand]);
                }
                MMathRotateMatrixYXZ(&matrix, 0, &record->path[2 + row][step]);
                /* The samples of one step are written from its first pair while the cursor advances. */
                stepQuads = cursor;
                for (sample = 0; sample < 4 && (sample == 0 || (step == 22 || step == 23)); sample++) {
                    MMathVectorInterpolation(&corner, &record->path[row][step], &record->path[row][step + 1], (float) sample * 0.25f);
                    Vu0MMathSetTranslation(matrix, &corner);
                    MMathApplyMatrix(&corner, matrix, &edge[0]);
                    if (MMathRotTransPersClip(&top.vector, camera, viewMatrix, &corner) != 0) {
                        cullLeft = 2;
                    } else if (cullLeft != 0) {
                        cullLeft--;
                        top.words[3] = MEF_GS_XYZ_ADC;
                    }
                    MMathApplyMatrix(&corner, matrix, &edge[1]);
                    if (MMathRotTransPersClip(&bottom.vector, camera, viewMatrix, &corner) != 0) {
                        cullLeft = 2;
                    } else if (cullLeft != 0) {
                        cullLeft--;
                        bottom.words[3] = MEF_GS_XYZ_ADC;
                    }
                    stepQuads[sample * 4 + 0].whole = colour[0].whole;
                    stepQuads[sample * 4 + 1].whole = top.whole;
                    stepQuads[sample * 4 + 2].whole = colour[1].whole;
                    stepQuads[sample * 4 + 3].whole = bottom.whole;
                    cursor += 4;
                    stripLength++;
                }
            }
            CloseTag(&record->packet, stripLength);
        }
    }
    MGsGPTerm(&record->packet);
}

void MEfAdvanceStep(MEfStepObject *object)
{
    MEfStepRecord *record;
    unsigned short *shiftSlot;
    int shiftsLeft;
    int next_step;

    record = &object->record;
    if (ov01LastValueGet() != 0) {
        return;
    }
    shiftsLeft = 24;
    shiftSlot = &record->flags_history[24];
    do {
        shiftsLeft -= 1;
        *shiftSlot = shiftSlot[-1];
        shiftSlot -= 1;
    } while (shiftsLeft > 0);
    next_step = (unsigned short) record->step + 1;
    record->step = next_step;
    if ((short) next_step >= 0x37) {
        MSeObjRequestFree(object);
    }
}

/* The path step object: the same callback slots as MEfStepObject, with the path record at +0x20. */
typedef struct MEfPathObject {
    unsigned char unmodeled_00[0x08];
    void (*flags_callback)();
    unsigned char unmodeled_0c[0x04];
    void (*update_callback)();
    void (*advance_callback)();
    unsigned char unmodeled_18[0x20 - 0x18];
    MEfPathRecord record;                    /* +0x20 */
} MEfPathObject;

struct MEfShortStepObject;
void MEfSetPathPhaseFlags(MEfStepObject *object);
void MEfDrawPathStepTrail(MEfPathObject *object);
void MEfAdvanceStepShort(struct MEfShortStepObject *record);

/*
 * Creator of the path step: clears the step and the history, takes the actor
 * frame, clears both sampled rows and starts each at the frame origin, samples
 * the path, then installs the flags, draw and advance callbacks. Returns 1.
 */
int MEfCreatePathStep(MEfPathObject *object)
{
    MEfPathRecord *record;
    Vector4 *trackRows;

    record = &object->record;
    trackRows = object->record.track[0];
    record->step = 0;
    MEfStartPathFromActor(record);
    memset(record->history, 0, sizeof(record->history));
    memset(trackRows, 0, sizeof(record->track[0]) * 2);
    __asm__ __volatile__(
        "lq $8, 0(%4)\n"
        "sq $8, 0(%5)\n"
        : "=m" (trackRows->x), "=m" (trackRows->y), "=m" (trackRows->z), "=m" (trackRows->w)
        : "r" (&object->record.origin), "r" (trackRows)
        : "$8", "memory");
    QuadwordCopyT0(object->record.track[1], &object->record.origin);
    MEfSamplePathTrack(record);
    MGsGPInit(&record->packet, 0, 0);
    object->flags_callback = MEfSetPathPhaseFlags;
    object->update_callback = MEfDrawPathStepTrail;
    object->advance_callback = MEfAdvanceStepShort;
    return 1;
}

/* Path step set-up: samples the actor's matrix, moves the offset point into it, and fills the four curve points on a circle around that point. */
void MEfStartPathFromActor(MEfPathRecord *record)
{
    /* Ring offset tables, defined (const) at the end of this file. */
    extern float D_00ABB2F0[4];
    extern float D_00ABB300[4];
    unsigned short randomBits;
    int i;
    Matrix4 matrix;
    Vector4 point;
    Vector4 cosine;
    Vector4 sine;
    float angleStep;
    Vector4 *from;
    Vector4 *to;
    float angle;
    float scale;
    Vector4 *out;
    MEfPathCurve *curve = &mefPathCurve.curve;

    MSeGetActorMatrix(&matrix, record->actor, record->coordinate);
    MMathApplyMatrix(&point, matrix, &record->offset);
    __asm__("lqc2 $vf1,0(%1)\n\t"
            "vmove.w $vf1w,$vf0w\n\t"
            "sqc2 $vf1,%0"
            : "=m"(matrix[3])
            : "r"(&point)
            : "memory");
    QuadwordCopyMatrixT0(record->matrix, matrix);
    __asm__("lqc2 $vf1,%3\n\t"
            "vmove.w $vf1w,$vf0w\n\t"
            "sqc2 $vf1,0(%2)"
            : "=m"(record->origin), "=m"(record->origin.w)
            : "r"(&record->origin), "m"(matrix[3][0])
            : "memory");
    angle = MMathMakeRandom2PI();
    angleStep = 0.10471976f;
    record->angle = angle;
    randomBits = xglSRand();
    if ((randomBits & 1) == 0) {
        angleStep = -0.10471976f;
    }
    scale = MMathMakeRandom() + 0.05f;
    ov01RecordInit(curve, MEF_CURVE_POINTS);
    for (i = 0; i < MEF_CURVE_POINTS; i++) {
        out = &curve->points[i];
        __asm__ __volatile__("sqc2 vf0, 0(%4)\n"
                             : "=m"(point), "=m"(point.w), "=m"(*out), "=m"(out->w)
                             : "r"(&point)
                             : "memory");
        MEfPathCos(cosine.x, angle);
        point.x = cosine.x * D_00ABB2F0[i] * scale;
        MEfPathSin(sine.x, angle);
        point.y = sine.x * D_00ABB2F0[i] * scale;
        point.z = D_00ABB300[i];
        MMathApplyMatrix(out, matrix, &point);
        angle = angle + angleStep;
    }
}

void MEfSamplePathTrack(MEfPathRecord *record)
{
    Matrix4 matrix;
    Vector4 point;
    Vector4 entry;
    Vector4 sine;
    Vector4 cosine;
    float angle;
    float sineValue;
    float cosineValue;
    float distance;
    float spacing;
    Vector4 *from;
    Vector4 *to;
    float sixth;
    float damp;
    int perSegment;
    int i;
    MEfPathCurve *curve;
    int segment;
    int sampleInSegment;
    int row;
    MEfPathRecord *rec;

    rec = record;
    curve = &mefPathCurve.curve;
    angle = MMathMakeRandom2PI();
    distance = 0.0f;
    spacing = 2.0f;
    Vu0Sin(sineValue, angle);
    sine.x = sineValue;
    Vu0Cos(cosineValue, angle);
    cosine.x = cosineValue;
    QuadwordCopyMatrixT0(matrix, rec->matrix);
    __asm__ __volatile__("lui $2,%%hi(MMathUnitVector)\n\t"
            "addiu $2,$2,%%lo(MMathUnitVector)\n\t"
            "lqc2 $vf1,0($2)\n\t"
            "vmove.w $vf1w,$vf0w\n\t"
            "sqc2 $vf1,%0"
            : "=m"(matrix[3])
            :
            : "$2", "memory");
    sixth = 1.0f / 6.0f;
    damp = 0.85f;
    for (i = 0, perSegment = 6; i < 19; i++) {
        segment = i / perSegment;
        sampleInSegment = i % perSegment;
        if (segment == 3) {
            segment = 2;
            sampleInSegment = 6;
        }
        MMathCalcHermiteKeyPrm(curve, segment);
        MMathCalcHermiteKey(&point, curve, segment, (float) sampleInSegment * sixth);
        __asm__ __volatile__("sqc2 $vf0,0(%4)" : "=m"(entry.x), "=m"(entry.y), "=m"(entry.z), "=m"(entry.w) : "sampleInSegment"(&entry) : "memory");
        entry.x = cosineValue * distance;
        entry.y = sineValue * distance;
        MMathApplyMatrix(&entry, matrix, &entry);
        __asm__ __volatile__("lqc2 $vf1,0(%4)\n\t"
                "lqc2 $vf2,0(%5)\n\t"
                "vadd.xyz $vf1xyz,$vf1xyz,$vf2xyz\n\t"
                "sqc2 $vf1,0(%5)"
                : "=m"(entry.x), "=m"(entry.y), "=m"(entry.z), "=m"(entry.w) : "sampleInSegment"(&point), "sampleInSegment"(&entry) : "memory");
        QuadwordCopyT0Dst(&rec->track[0][i], &entry);
        __asm__ __volatile__("sqc2 $vf0,0(%4)" : "=m"(entry.x), "=m"(entry.y), "=m"(entry.z), "=m"(entry.w) : "sampleInSegment"(&entry) : "memory");
        entry.x = cosineValue * -distance;
        entry.y = sineValue * -distance;
        MMathApplyMatrix(&entry, matrix, &entry);
        __asm__ __volatile__("lqc2 $vf1,0(%4)\n\t"
                "lqc2 $vf2,0(%5)\n\t"
                "vadd.xyz $vf1xyz,$vf1xyz,$vf2xyz\n\t"
                "sqc2 $vf1,0(%5)"
                : "=m"(entry.x), "=m"(entry.y), "=m"(entry.z), "=m"(entry.w) : "sampleInSegment"(&point), "sampleInSegment"(&entry) : "memory");
        QuadwordCopyT0Dst(&rec->track[1][i], &entry);
        if (i >= 9) {
            spacing = spacing * damp;
            distance = distance + spacing;
        }
    }
    for (row = 0; row < 2; row++) {
        for (i = 0; i < 18; i++) {
            MMathCalcAngle(&rec->track[row + 2][i], &rec->track[row][i], &rec->track[row][i + 1]);
            ((float *)&rec->track[row + 2][i])[2] = rec->angle;
        }
        QuadwordCopyT0(&rec->track[row + 2][18], &rec->track[row + 2][17]);
    }
}

void MEfSetPathPhaseFlags(MEfStepObject *object)
{
    MEfStepRecord *record;
    short step;

    record = &object->record;
    if (ov01LastValueGet() == 0) {
        step = record->step;
        record->flags_history[0] = 0U;
        if (step < MEF_PHASE_STEPS) {
            record->flags_history[0] = MEF_PHASE_ACTIVE;
            if (step == 0) {
                record->flags_history[0] = MEF_PHASE_START | MEF_PHASE_ACTIVE;
            }
            if (step == MEF_PHASE_STEPS - 1) {
                record->flags_history[0] |= MEF_PHASE_END;
            }
        }
    }
}

/* Colour templates and per-strand edge vectors of the path trail (the layout of MEfDrawStepTrail's tables). */
extern const MEfGsColor mefPathHeadColors[2];
extern const MEfGsColor mefPathTailColors[2];
extern const DoublewordAlignedVector4 D_00ABB330[4];
extern const DoublewordAlignedVector4 D_00ABB370[4];
extern const DoublewordAlignedVector4 D_00ABB3D0[4];
extern const DoublewordAlignedVector4 D_00ABB410[4];

/* Draws the path trail the way MEfDrawStepTrail draws the stepped one, over its 19-step record. */
void MEfDrawPathStepTrail(MEfPathObject *object)
{
    AlignedVector4 edge[2];
    AlignedVector4 corner;
    Matrix4 matrix;
    MEfGsVertex top;
    MEfGsVertex bottom;
    MEfGsColor colour[2];
    MEfPathRecord *record;
    struct XglCamera *camera;
    Matrix4 *viewMatrix;
    MEfGsQword *cursor;
    MEfGsQword *stepQuads;
    int stripLength;
    int strand;
    int row;
    MGsPacket *packet;
    MEfGsQword *header;
    int cullLeft;
    int step;
    int sample;
    int fade;

    packet = &object->record.packet;
    camera = MEfCamParams.camera;
    viewMatrix = &MEfCamParams.viewMatrix;
    MGsGPInit(packet, 0, 0);
    header = packet->current;
    header[0].gif.tag = MEF_GS_SETUP_TAG;
    header[0].gif.registers = MEF_GS_SETUP_REGS;
    packet->regCount = 1;
    cursor = header + 1;
    SetRegister(cursor, MEF_GS_TEST_1, MEF_GS_TEST_1_ATE_ATST_GREATER_ZGEQUAL);
    SetRegister(&header[2], MEF_GS_ZBUF_1, MEF_GS_ZBUF_1_NO_DEPTH_WRITE);
    SetRegister(&header[3], MEF_GS_ALPHA_1, MEF_GS_ALPHA_1_ADDITIVE);
    CloseTag(packet, 3);
    record = &object->record;

    /* The head of the trail: one strip per strand, fading with the step. */
    for (strand = 0; strand < 4; strand++) {
        stripLength = 0;
        cullLeft = 0;
        cursor = OpenTag(&record->packet, 1, MEF_GS_STRIP_PRIM, 4, MEF_GS_STRIP_REGS);
        for (step = 0; step < 10; step++) {
            if ((short) record->history[step] == 0) {
                continue;
            }
            fade = 9 - step;
            colour[0].whole = mefPathHeadColors[0].whole;
            colour[1].whole = mefPathHeadColors[1].whole;
            colour[0].rgba[1] += fade * 8;
            colour[1].rgba[1] += fade;
            colour[0].rgba[2] -= fade * 8;
            colour[1].rgba[2] -= fade * 8;
            if ((record->history[step] & (MEF_PHASE_START | MEF_PHASE_END)) != 0) {
                colour[0].rgba[3] = colour[1].rgba[3] = 0;
                Vu0SetOriginLanes(&edge[0]);
                Vu0SetOriginLanes(&edge[1]);
            } else if (step == 9) {
                colour[0].rgba[3] = colour[1].rgba[3] = 0;
                QuadwordCopyT0Lanes(&edge[0], &D_00ABB3D0[strand]);
                QuadwordCopyT0Lanes(&edge[1], &D_00ABB410[strand]);
            } else {
                QuadwordCopyT0Lanes(&edge[0], &D_00ABB330[strand]);
                QuadwordCopyT0Lanes(&edge[1], &D_00ABB370[strand]);
            }
            MMathRotateMatrixYXZ(&matrix, 0, &record->track[2][step]);
            Vu0MMathSetTranslation(matrix, &record->track[0][step]);
            MMathApplyMatrix(&corner, matrix, &edge[0]);
            if (MMathRotTransPersClip(&top.vector, camera, viewMatrix, &corner) != 0) {
                cullLeft = 2;
            } else if (cullLeft != 0) {
                cullLeft--;
                top.words[3] = MEF_GS_XYZ_ADC;
            }
            MMathApplyMatrix(&corner, matrix, &edge[1]);
            if (MMathRotTransPersClip(&bottom.vector, camera, viewMatrix, &corner) != 0) {
                cullLeft = 2;
            } else if (cullLeft != 0) {
                cullLeft--;
                bottom.words[3] = MEF_GS_XYZ_ADC;
            }
            cursor[stripLength * 4 + 0].whole = colour[0].whole;
            cursor[stripLength * 4 + 1].whole = top.whole;
            cursor[stripLength * 4 + 2].whole = colour[1].whole;
            cursor[stripLength * 4 + 3].whole = bottom.whole;
            stripLength++;
        }
        CloseTag(&record->packet, stripLength);
    }

    /* The tail: both rows of the path, with four samples between the points of steps 16 and 17. */
    for (row = 0; row < 2; row++) {
        for (strand = 0; strand < 4; strand++) {
            cursor = OpenTag(&record->packet, 1, MEF_GS_STRIP_PRIM, 4, MEF_GS_STRIP_REGS);
            cullLeft = 0;
            stripLength = 0;
            for (step = 8; step < 19; step++) {
                if ((short) record->history[step] == 0) {
                    continue;
                }
                colour[1].whole = mefPathTailColors[1].whole;
                colour[0].whole = mefPathTailColors[0].whole;
                colour[1].rgba[1] -= step - 8;
                if ((record->history[step] & (MEF_PHASE_START | MEF_PHASE_END)) != 0) {
                    colour[0].rgba[3] = colour[1].rgba[3] = 0;
                    Vu0SetOriginLanes(&edge[0]);
                    Vu0SetOriginLanes(&edge[1]);
                } else if (step == 8) {
                    colour[0].rgba[3] = colour[1].rgba[3] = 0;
                } else {
                    QuadwordCopyT0Lanes(&edge[0], &D_00ABB3D0[strand]);
                    QuadwordCopyT0Lanes(&edge[1], &D_00ABB410[strand]);
                }
                MMathRotateMatrixYXZ(&matrix, 0, &record->track[2 + row][step]);
                /* The samples of one step are written from its first pair while the cursor advances. */
                stepQuads = cursor;
                for (sample = 0; sample < 4 && (sample == 0 || (step == 16 || step == 17)); sample++) {
                    MMathVectorInterpolation(&corner, &record->track[row][step], &record->track[row][step + 1], (float) sample * 0.25f);
                    Vu0MMathSetTranslation(matrix, &corner);
                    MMathApplyMatrix(&corner, matrix, &edge[0]);
                    if (MMathRotTransPersClip(&top.vector, camera, viewMatrix, &corner) != 0) {
                        cullLeft = 2;
                    } else if (cullLeft != 0) {
                        cullLeft--;
                        top.words[3] = MEF_GS_XYZ_ADC;
                    }
                    MMathApplyMatrix(&corner, matrix, &edge[1]);
                    if (MMathRotTransPersClip(&bottom.vector, camera, viewMatrix, &corner) != 0) {
                        cullLeft = 2;
                    } else if (cullLeft != 0) {
                        cullLeft--;
                        bottom.words[3] = MEF_GS_XYZ_ADC;
                    }
                    stepQuads[sample * 4 + 0].whole = colour[0].whole;
                    stepQuads[sample * 4 + 1].whole = top.whole;
                    stepQuads[sample * 4 + 2].whole = colour[1].whole;
                    stepQuads[sample * 4 + 3].whole = bottom.whole;
                    cursor += 4;
                    stripLength++;
                }
            }
            CloseTag(&record->packet, stripLength);
        }
    }
    MGsGPTerm(&record->packet);
}


/*
 * The block at +0x20 of the record MEfAdvanceStepShort updates; its step counter sits
 * at +0x54 of the block (+0x74 of the record).
 */
typedef struct MEfShortStepBlock {
    unsigned char unmodeled_00[0x54];
    unsigned short step;                               /* +0x54 */
    unsigned char unmodeled_56[0xb4 - 0x56];
} MEfShortStepBlock;

/*
 * The record MEfAdvanceStepShort updates: a 19-entry history at +0xd4 that the
 * function shifts one entry toward its end.
 */
typedef struct MEfShortStepObject {
    unsigned char unmodeled_00[0x20];
    MEfShortStepBlock block;                                /* +0x20 */
    unsigned short history[19];                        /* +0xd4 */
} MEfShortStepObject;


/*
 * Returns unless ov01LastValueGet reports 0. Otherwise shifts history[0..17] up
 * by one entry, advances the step counter, and calls MSeObjRequestFree once the
 * incremented step reaches 0x31.
 */
void MEfAdvanceStepShort(MEfShortStepObject *record)
{
  int shiftIndex;
  short next_step;
  MEfShortStepBlock *block;

  block = &record->block;
  if (ov01LastValueGet() != 0) {
    return;
  }
  shiftIndex = 18;
  do {
    record->history[shiftIndex] = record->history[shiftIndex - 1];
    shiftIndex -= 1;
  } while (shiftIndex > 0);
  next_step = (short) (block->step + 1);
  block->step = (unsigned short) (block->step + 1);
  if (next_step >= 0x31) {
    MSeObjRequestFree(record);
  }
}

/* Read-only tables of the step and path trails (original .rodata), in address order. */
/* Corner table of the vertex-2 corners: four corner bearings, then the two corner words. */
const MEfCornerTable MEfVertex2CornerTable = {
    { { 0.0f, 1.5707964f, 4.712389f, 3.1415927f } },
    { 0 },
    { 0x400000003f800000ULL, 0x3f80000040400000ULL },
};

const float D_00ABB180[4] = { -4.0f, 3.0f, -2.0f, 1.0f };
const float D_00ABB190[4] = { 0.0f, 5.0f, 7.0f, 8.0f };
const float D_00ABB1A0[4] = { 0.0f, 1.6f, 8.0f, 20.0f };
const MEfGsColor mefStepHeadColors[2] = { { rgba: { 0x10, 0x30, 0xa0, 0xff } }, { rgba: { 0x00, 0x10, 0x80, 0xff } } };
const DoublewordAlignedVector4 D_00ABB1D0[4] = {
    { -0.005f, 0.0f, 0.0f, 1.0f },
    { 0.0f, -0.005f, 0.0f, 1.0f },
    { 0.005f, 0.0f, 0.0f, 1.0f },
    { 0.0f, 0.005f, 0.0f, 1.0f }
};
const DoublewordAlignedVector4 D_00ABB210[4] = {
    { 0.06f, 0.0f, 0.0f, 1.0f },
    { 0.0f, 0.06f, 0.0f, 1.0f },
    { -0.06f, 0.0f, 0.0f, 1.0f },
    { 0.0f, -0.06f, 0.0f, 1.0f }
};
const MEfGsColor mefStepTailColors[2] = { { rgba: { 0x10, 0x30, 0xa0, 0xff } }, { rgba: { 0x00, 0x10, 0x80, 0xff } } };
const DoublewordAlignedVector4 D_00ABB270[4] = {
    { -0.003f, 0.0f, 0.0f, 1.0f },
    { 0.0f, -0.003f, 0.0f, 1.0f },
    { 0.003f, 0.0f, 0.0f, 1.0f },
    { 0.0f, 0.003f, 0.0f, 1.0f }
};
const DoublewordAlignedVector4 D_00ABB2B0[4] = {
    { 0.03f, 0.0f, 0.0f, 1.0f },
    { 0.0f, 0.03f, 0.0f, 1.0f },
    { -0.03f, 0.0f, 0.0f, 1.0f },
    { 0.0f, -0.03f, 0.0f, 1.0f }
};
const float D_00ABB2F0[4] = { 0.0f, 30.0f, 42.0f, 48.0f };
const float D_00ABB300[4] = { 0.0f, 30.4f, 152.0f, 380.0f };
const MEfGsColor mefPathHeadColors[2] = { { rgba: { 0x10, 0x30, 0xa0, 0xff } }, { rgba: { 0x00, 0x10, 0x80, 0xff } } };
const DoublewordAlignedVector4 D_00ABB330[4] = {
    { -0.03f, 0.0f, 0.0f, 1.0f },
    { 0.0f, -0.03f, 0.0f, 1.0f },
    { 0.03f, 0.0f, 0.0f, 1.0f },
    { 0.0f, 0.03f, 0.0f, 1.0f }
};
const DoublewordAlignedVector4 D_00ABB370[4] = {
    { 0.32f, 0.0f, 0.0f, 1.0f },
    { 0.0f, 0.32f, 0.0f, 1.0f },
    { -0.32f, 0.0f, 0.0f, 1.0f },
    { 0.0f, -0.32f, 0.0f, 1.0f }
};
const MEfGsColor mefPathTailColors[2] = { { rgba: { 0x10, 0x30, 0xa0, 0xff } }, { rgba: { 0x00, 0x10, 0x80, 0xff } } };
const DoublewordAlignedVector4 D_00ABB3D0[4] = {
    { -0.015f, 0.0f, 0.0f, 1.0f },
    { 0.0f, -0.015f, 0.0f, 1.0f },
    { 0.015f, 0.0f, 0.0f, 1.0f },
    { 0.0f, 0.015f, 0.0f, 1.0f }
};
const DoublewordAlignedVector4 D_00ABB410[4] = {
    { 0.17f, 0.0f, 0.0f, 1.0f },
    { 0.0f, 0.17f, 0.0f, 1.0f },
    { -0.17f, 0.0f, 0.0f, 1.0f },
    { 0.0f, -0.17f, 0.0f, 1.0f }
};

/* mefPathCurve: 8192 bytes of zero storage, shared with the other path TUs. */
MEfPathCurveObject mefPathCurve = { 0 };
