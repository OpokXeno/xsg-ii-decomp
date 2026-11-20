#ifndef INCLUDE_MAIN_XGL_STUDIO_H
#define INCLUDE_MAIN_XGL_STUDIO_H

#include "shared.h"

/* Functions of src/main/xgl_studio.c that other files call. */

struct XglCamera;

typedef float RgVector[4];

/*
 * The light editor of Xenosaga I (DB_lightWrite, updateLight) indexes these
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


/* Set while a movie plays: xglStudioUpdate skips the camera and studio moves. */
extern int StudioEntrySkip;

void xglStudioInit(void);
void xglStudioMainCameraInit(void);
void xglStudioChange(int studio_index);
void xglStudioGetCamera(struct XglCamera **camera, int camera_index);
void xglStudioMove(void);
struct XglCamera *xglStudioSelectGetActiveCamera(int studio_index);
void xglStudioUpdate(void);
void studioSceneFlagSet(void);
void studioSceneFlagClear(void);
void xglStudioSetActiveIndex(int studio_index);
struct XglCamera *xglStudioGetActiveCamera(void);
struct XglCamera *xglStudioGetCamera2(int camera_index);
StudioLight *xglStudioGetLight2(void);
void xglStudioFlushActiveCamera(void);
void xglStudioDisableCameraUpdate(void);
void xglStudioEnableCameraUpdate(void);
void xglStudioMainCameraMove(void);
void xglStudioSetup(void);

#endif /* INCLUDE_MAIN_XGL_STUDIO_H */
