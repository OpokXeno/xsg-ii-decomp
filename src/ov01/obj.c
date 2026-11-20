#include "common.h"
#include "shared.h"
#include "ov01/obj.h"

#define STD_MOVE_ARRIVE_EPSILON 1e-6f   /* objStdMove treats a remaining distance this small as arrived */


/* objStdMove: advances the mover one step toward its target on all three axes and returns 1 when all three
 * have arrived. */
int objStdMove(SlideBlock *mover)
{
    int arrived;

    arrived = 0;
    {
        float velocity;
        float position;
        float distance;
        float magnitude;

        velocity = mover->velocity.x + mover->acceleration.x;
        position = mover->position.x + velocity;
        mover->velocity.x = velocity;
        mover->position.x = position;
        if (position == mover->goal.x) {
            arrived++;
        } else if ((velocity > 0.0f && mover->goal.x < position)
                   || (velocity < 0.0f && position < mover->goal.x)
                   || ((distance = mover->goal.x - position) < 0.0f
                           ? (magnitude = -distance, magnitude <= STD_MOVE_ARRIVE_EPSILON)
                           : (magnitude = distance, magnitude <= STD_MOVE_ARRIVE_EPSILON))) {
            mover->position.x = mover->goal.x;
            arrived++;
        }
    }

    {
        float velocity;
        float position;
        float distance;
        float magnitude;

        velocity = mover->velocity.y + mover->acceleration.y;
        position = mover->position.y + velocity;
        mover->velocity.y = velocity;
        mover->position.y = position;
        if (position == mover->goal.y) {
            arrived++;
        } else if ((velocity > 0.0f && mover->goal.y < position)
                   || (velocity < 0.0f && position < mover->goal.y)
                   || ((distance = mover->goal.y - position) < 0.0f
                           ? (magnitude = -distance, magnitude <= STD_MOVE_ARRIVE_EPSILON)
                           : (magnitude = distance, magnitude <= STD_MOVE_ARRIVE_EPSILON))) {
            mover->position.y = mover->goal.y;
            arrived++;
        }
    }

    {
        float velocity;
        float position;
        float distance;
        float magnitude;

        velocity = mover->velocity.z + mover->acceleration.z;
        position = mover->position.z + velocity;
        mover->velocity.z = velocity;
        mover->position.z = position;
        if (position == mover->goal.z) {
            arrived++;
        } else if ((velocity > 0.0f && mover->goal.z < position)
                   || (velocity < 0.0f && position < mover->goal.z)
                   || ((distance = mover->goal.z - position) < 0.0f
                           ? (magnitude = -distance, magnitude <= STD_MOVE_ARRIVE_EPSILON)
                           : (magnitude = distance, magnitude <= STD_MOVE_ARRIVE_EPSILON))) {
            mover->position.z = mover->goal.z;
            arrived++;
        }
    }
    return arrived == 3;
}

#define SLIDE_ANGLE_FULL_TURN 6.2831855f /* 2*pi, the wrap point of SlideBlock.angle */
#define SLIDE_ARRIVE_EPSILON 1e-6f       /* an angle or gap this small counts as arrived */

/* objHatoVec: per-frame step from position to goal over `frames`; axes within 0.5 of goal snap onto it. */
void objHatoVec(SlideBlock *block, int frames) {
    f32 snapRadius = 0.5f;
    f32 dyAfterX;
    f32 dy;
    f32 dz;
    f32 goalX = block->goal.x;
    f32 goalY = block->goal.y;
    f32 dx = goalX - block->position.x;
    dy = goalY - block->position.y;

    block->velocity.x = dx / frames;
    block->velocity.y = dy / frames;
    block->velocity.z = (block->goal.z - block->position.z) / frames;

    if ((dx < 0.0f ? -dx : dx) < snapRadius) {
        block->position.x = goalX;
    }
    dyAfterX = goalY - block->position.y;
    if ((dyAfterX < 0.0f ? -dyAfterX : dyAfterX) < snapRadius) {
        block->position.y = goalY;
    }
    dz = block->goal.z - block->position.z;
    if ((dz < 0.0f ? -dz : dz) < snapRadius) {
        block->position.z = block->goal.z;
    }
}

/* slideHorizontalDistance: XZ distance from position to goal (no y term). */
f32 slideHorizontalDistance(SlideBlock *block) {
    f32 dx;
    f32 dz;

    dx = block->goal.x - block->position.x;
    if (dx < 0.0f) {
        dx = -dx;
    }
    dz = block->goal.z - block->position.z;
    if (dz < 0.0f) {
        dz = -dz;
    }
    return __builtin_sqrtf(dx * dx + dz * dz);
}

/* slideLimitGoalXZ: pulls goal toward position so its XZ distance is `numerator`. */
void slideLimitGoalXZ(SlideBlock *block, f32 numerator) {
    f32 fraction;
    f32 positionZ;

    fraction = numerator / slideHorizontalDistance(block);
    block->goal.x = block->position.x + (block->goal.x - block->position.x) * fraction;
    positionZ = block->position.z;
    block->goal.z = positionZ + (block->goal.z - positionZ) * fraction;
}

/* slideSnapPosToGoal: the position jumps to the goal (16 bytes copied). */
void slideSnapPosToGoal(SlideBlock *block) {
    block->position = block->goal;
}

/* slideAngleStep: steps angle toward goalAngle by angleRate, wrapping at 2*pi; snaps and returns 1 on arrival. */
int slideAngleStep(SlideBlock *block) {
    f32 angle;
    f32 offset;      /* a scratch: the current angle, then its gap to goalAngle, then the stepped angle */
    f32 rate;
    f32 goalDistance;
    int arrived;

    arrived = 0;
    offset = block->angle;
    angle = offset;
    if (offset == block->goalAngle) {
        block->angle = block->goalAngle;
        arrived = 1;
    } else {
        offset = offset - block->goalAngle;
        if ((offset < 0.0f ? (offset = -offset) : offset) <= SLIDE_ARRIVE_EPSILON) {
            block->angle = block->goalAngle;
            return 1;
        }
        rate = block->angleRate;
        block->angle = angle + rate;
        if (0.0f < rate && SLIDE_ANGLE_FULL_TURN <= block->angle) {
            block->angle = block->angle - SLIDE_ANGLE_FULL_TURN;
        }
        offset = block->angle;
        if (0.0f > rate && offset < 0.0f) {
            block->angle = SLIDE_ANGLE_FULL_TURN + offset;
            offset = block->angle;
        }
        goalDistance = offset - block->goalAngle;
        if (goalDistance < 0.0f) {
            goalDistance = -goalDistance;
        }
        if (goalDistance <= (rate < 0.0f ? -rate : rate)
            || (offset = offset - block->goalAngle, (offset < 0.0f ? (offset = -offset) : offset) <= SLIDE_ARRIVE_EPSILON)) {
            block->angle = block->goalAngle;
            arrived = 1;
        }
    }
    return arrived;
}

/* slideSnapAngleToGoal: the angle jumps to goalAngle. */
void slideSnapAngleToGoal(SlideBlock *block) {
    block->angle = block->goalAngle;
}
