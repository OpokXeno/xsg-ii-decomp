#ifndef INCLUDE_OV01_OBJ_H
#define INCLUDE_OV01_OBJ_H

#include "shared.h"

/*
 * Slide block: the 0x80-byte subobject the slide steps work on. The callers pass it by address
 * (the menu panels at +0x10, the ov01 unit pose at +0x150, the unit_cmd.c record pairLow at +0x150).
 * position moves toward goal by velocity each frame (objStdMove also accelerates it); angle moves toward goalAngle by angleRate.
 * objStdInit clears it and sets position.w and the four scale values to 1.0.
 */
typedef struct SlideBlock {
    DoublewordAlignedVector4 position;  /* 0x00: x, y, z of the moved point; w is 1.0 after objStdInit */
    u8 unmodeled_10[0x14];
    f32 angle;                          /* 0x24: current angle, radians */
    u8 unmodeled_28[0x8];
    Vector4 scale;                      /* 0x30: all four values 1.0 after objStdInit */
    Vector4 velocity;                   /* 0x40: per-frame step, written by objHatoVec, advanced by objStdMove (w unused) */
    Vector4 acceleration;               /* 0x50: added to velocity by objStdMove (w unused) */
    DoublewordAlignedVector4 goal;      /* 0x60: destination; x and z are used by slideLimitGoalXZ */
    f32 goalAngle;                      /* 0x70: target angle */
    f32 angleRate;                      /* 0x74: radians per call, read by slideAngleStep */
    u8 unmodeled_78[0x8];
} SlideBlock;

/* Zeroes the block, then sets position.w and every scale value to 1.0. */
void objStdInit(SlideBlock *block);
/* Advances the block one step toward its goal on all three axes and returns 1 when all three have arrived. */
int objStdMove(SlideBlock *mover);
/* Sets the per-frame velocity from position to goal over `frames`; axes within 0.5 of the goal snap onto it. */
void objHatoVec(SlideBlock *block, int frames);
/* XZ distance from position to goal (no y term). */
f32 slideHorizontalDistance(SlideBlock *block);
/* Pulls goal toward position so that its XZ distance is `numerator`. */
void slideLimitGoalXZ(SlideBlock *block, f32 numerator);
/* The position jumps to the goal. */
void slideSnapPosToGoal(SlideBlock *block);
/* Steps angle toward goalAngle by angleRate, wrapping at 2*pi; returns 1 on arrival. */
int slideAngleStep(SlideBlock *block);
/* The angle jumps to goalAngle. */
void slideSnapAngleToGoal(SlideBlock *block);

#endif /* INCLUDE_OV01_OBJ_H */
