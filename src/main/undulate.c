#include "common.h"
#include "shared.h"
#include "main/undulate.h"
#include "main/game_data.h"
#include "main/game_camera.h"
#include "main/TiGlobalColl.h"
#include "main/elevator.h"
#include "main/xgl_2.h"

/* Result of UnduCheckSubHeightCheck: no hit, a hit nearer the origin height than the best so far, or the first hit. */
enum MapHitStatus {
    MAP_HIT_NONE = 0,
    MAP_HIT_NEARER = 1,
    MAP_HIT_FIRST = 2
};

/* EE scratchpad state of an unloaded probe. */
#define MAP_SCRATCHPAD_PROBE 0x70000000

/* elvActiveMask bits that clamp the probe z to elevator 0 (low and high side), and its half span. */
#define MAP_ELV_CLAMP_Z_LOW 0x80
#define MAP_ELV_CLAMP_Z_HIGH 0x40
#define MAP_ELEVATOR_HALF_SPAN 7.0f

/* Octant bits of mapHeadingXZ: z negative, x negative, and x/z swapped (|x| < |z|). */
#define MAP_OCTANT_Z_NEG 1
#define MAP_OCTANT_X_NEG 2
#define MAP_OCTANT_SWAPPED 4

/* keepOffWord modes of mapPushOutOfKeepOff: 0 pushes onto the open edge, 1 keeps the probe radius off it. */
#define MAP_KEEP_OFF_MODE_EDGE 0
#define MAP_KEEP_OFF_MODE_RADIUS 1

/*
 * Heading of the XZ vector (x, z) in radians: atan2(x, z) from a VU0 polynomial
 * over the smaller/larger ratio, moved into the quadrant by the signs. Quirk kept
 * from the original: a zero x returns pi for either sign of z (and 0 when z is zero too).
 */
f32 mapHeadingXZ(const Vector4 *direction)
{
    f32 deltaX;
    f32 deltaZ;
    f32 ratio;
    f32 angle;
    unsigned char octant;
    const float *coeffs;

    deltaX = direction->x;
    deltaZ = direction->z;
    if (deltaX == 0.0f) {
        if (deltaZ == 0.0f) {
            return 0.0f;
        }
        return 3.1415927f;
    }
    if (deltaZ == 0.0f) {
        if (deltaX < 0.0f) {
            return -1.5707964f;
        }
        return 1.5707964f;
    }
    octant = 0;
    if (deltaZ < 0.0f) {
        deltaZ = -deltaZ;
        octant = MAP_OCTANT_Z_NEG;
    }
    if (deltaX < 0.0f) {
        deltaX = -deltaX;
        octant |= MAP_OCTANT_X_NEG;
    }
    if (deltaX < deltaZ) {
        __asm__ __volatile__(
            "mfc1 $2, %0\n\t"
            "mfc1 $3, %1\n\t"
            "qmtc2 $2, $vf1\n\t"
            "qmtc2 $3, $vf2\n\t"
            : : "f"(deltaX), "f"(deltaZ) : "$2", "$3");
    } else {
        __asm__ __volatile__(
            "mfc1 $2, %0\n\t"
            "mfc1 $3, %1\n\t"
            "qmtc2 $2, $vf1\n\t"
            "qmtc2 $3, $vf2\n\t"
            : : "f"(deltaZ), "f"(deltaX) : "$2", "$3");
        octant |= MAP_OCTANT_SWAPPED;
    }
    coeffs = mapHeadingCoefficients;
    __asm__ __volatile__(
        "vdiv Q, $vf1x, $vf2x\n\t"
        "lqc2 $vf8, 0(%1)\n\t"
        "lqc2 $vf9, 16(%1)\n\t"
        "vwaitq\n\t"
        "vaddq.x $vf1x, $vf0x, Q\n\t"
        "vmul.x $vf2x, $vf1x, $vf1x\n\t"
        "vmulw.x $vf4x, $vf2x, $vf9w\n\t"
        "vaddz.x $vf5x, $vf4x, $vf9z\n\t"
        "vmul.x $vf4x, $vf5x, $vf2x\n\t"
        "vaddy.x $vf5x, $vf4x, $vf9y\n\t"
        "vmul.x $vf4x, $vf5x, $vf2x\n\t"
        "vadd.x $vf5x, $vf4x, $vf9x\n\t"
        "vmul.x $vf4x, $vf5x, $vf2x\n\t"
        "vaddw.x $vf5x, $vf4x, $vf8w\n\t"
        "vmul.x $vf4x, $vf5x, $vf2x\n\t"
        "vaddz.x $vf5x, $vf4x, $vf8z\n\t"
        "vmul.x $vf4x, $vf5x, $vf2x\n\t"
        "vaddy.x $vf5x, $vf4x, $vf8y\n\t"
        "vmul.x $vf4x, $vf5x, $vf2x\n\t"
        "vadd.x $vf5x, $vf4x, $vf8x\n\t"
        "vmul.x $vf4x, $vf5x, $vf1x\n\t"
        "qmfc2 $3, $vf4\n\t"
        "mtc1 $3, %0\n\t"
        : "=f"(angle) : "r"(coeffs));
    angle = (octant & MAP_OCTANT_SWAPPED) ? 1.5707964f - angle : angle;
    if (octant & MAP_OCTANT_Z_NEG) {
        angle = 3.1415927f - angle;
    }
    if (octant & MAP_OCTANT_X_NEG) {
        angle = -angle;
    }
    return angle;
}

void mapTriangleEdgePlanes(void) {
    __asm__ __volatile__(
        "vmula.xz ACC, vf19, vf20\n"
        "vmadda.xz ACC, vf16, vf21\n"
        "vmadd.xz vf4, vf17, vf23\n"
        "vmula.xz ACC, vf19, vf21\n"
        "vmadda.xz ACC, vf17, vf22\n"
        "vmadd.xz vf5, vf18, vf23\n"
        "vmula.xz ACC, vf19, vf22\n"
        "vmadda.xz ACC, vf18, vf20\n"
        "vmadd.xz vf6, vf16, vf23\n"
        "vsubz.x vf1, vf4, vf4z\n"
        "vsubz.x vf2, vf5, vf5z\n"
        "vsubz.x vf3, vf6, vf6z\n"
        "vaddx.y vf1, vf0, vf2x\n"
        "vaddx.z vf1, vf0, vf3x\n"
        : : : "memory");
}

/*
 * Stores the three edge-plane values of the current VU0 triangle (vf1 from
 * mapTriangleEdgePlanes) into edgePlanes. Hand-written asm in the original: edgePlanes
 * stays in $a0 across the call, which only touches VU0 registers.
 */
void mapStoreEdgePlanes(float edgePlanes[4])
{
    __asm__ __volatile__(
        "jal mapTriangleEdgePlanes\n\t"
        "sqc2 vf1, 0(%0)"
        : : "r"(edgePlanes) : "$31", "memory");
}

/* Clip flags of the point against the triangle planes; nonzero when the point is outside. */
int mapPointOutsideTriangle(const Vector4 *point)
{
    int clipFlags;
    int outside;

    __asm__ __volatile__(
        "lqc2 $vf19, 0(%0)\n\t"
        "vmr32.xyzw $vf1, $vf19\n\t"
        "vmr32.xyzw $vf23, $vf1\n\t"
        : : "r"(point) : "memory");
    mapTriangleEdgePlanes();
    __asm__ __volatile__(
        "vsub.xyzw $vf2, $vf0, $vf0\n\t"
        "vclipw.xyz $vf1, $vf2w\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "cfc2 %1, $vi18\n\t"
        "nop\n\t"
        "andi %0, %1, 0x15\n\t"
        : "=r"(outside), "=r"(clipFlags) : : "memory");
    return outside;
}

/* Distance from the point origin to the plane through point with normal normal. */
f32 mapPlaneDistance(const Vector4 *origin, const Vector4 *point, const Vector4 *normal) {
    f32 result;

    __asm__ __volatile__("lqc2 vf2, 0(%0)" : : "r"(origin) : "memory");
    __asm__ __volatile__("lqc2 vf3, 0(%0)" : : "r"(point) : "memory");
    __asm__ __volatile__("lqc2 vf4, 0(%0)" : : "r"(normal) : "memory");
    __asm__ __volatile__("vsub.xz vf1, vf3, vf2" : : : "memory");
    __asm__ __volatile__("vmul.xz vf5, vf1, vf4" : : : "memory");
    __asm__ __volatile__("vaddz.x vf1, vf5, vf5z" : : : "memory");
    __asm__ __volatile__("vaddy.x vf5, vf1, vf3y" : : : "memory");
    __asm__ __volatile__("qmfc2.ni %0, vf5" : "=r"(result) : : "memory");
    return result;
}

static void *mapCurrentBlock;
extern unsigned char elvActiveMask;
extern unsigned char elvNoOutMask;
extern int elvKeepOffFlags[];
int printf(const char *format, ...);

void UnduInit(MapFixupBlock *block)
{
    unsigned int *entryWords;
    volatile MapFixupEntry *entry;
    volatile MapFixupGroup **groupTable;
    int groupIndex;
    int entryIndex;

    mapCurrentBlock = block;
    if (block == 0) {
        return;
    }
    if (block->flags & MAP_BLOCK_FIXED_UP) {
        return;
    }
    switch (block->flags) {
    default:
        /* JP: ====未知のバージョンのIDデータです====
         * EN: ====This ID data is of an unknown version==== */
        printf("\075\075\075\075\314\244\303\316\244\316\245\320\241\274\245\270\245\347\245\363\244\316\111\104\245\307\241\274\245\277\244\307\244\271\075\075\075\075");
        mapCurrentBlock = 0;
        break;
    case MAP_BLOCK_VALID:
        groupTable = (volatile MapFixupGroup **) &block->firstGroup;
        for (groupIndex = 0; groupIndex < block->groupCount; groupIndex++) {
            ((unsigned int *) groupTable)[groupIndex] += (unsigned int) block;
            entryWords = (unsigned int *) &groupTable[groupIndex]->firstEntry;
            for (entryIndex = 0; entryIndex < groupTable[groupIndex]->entryCount; entryIndex++) {
                unsigned int *groupSlot = (unsigned int *) &groupTable[groupIndex];

                entry = (MapFixupEntry *) (entryWords[entryIndex] + (unsigned int) block);
                entryWords[entryIndex] = (unsigned int) entry;
                if ((groupTable[groupIndex]->typeWord & 0xFF00) == MAP_GROUP_KEEP_OFF) {
                    if (elvKeepOffFlags[entry->keepId] != 0) {
                        elvKeepOffFlags[entry->keepId] = (int) entry | 1;
                    }
                }
                if (((MapFixupGroup *) *groupSlot)->relocated == 0) {
                    volatile MapFixupEntryWords *target = (MapFixupEntryWords *) entryWords[entryIndex];

                    target->pointer04 += (unsigned int) block;
                    target->pointer08 += (unsigned int) block;
                    target->pointer0C += (unsigned int) block;
                }
            }
        }
        break;
    }
    block->flags |= MAP_BLOCK_FIXED_UP;
}

/* Loads the current block: clears the elevator masks, sets every keep-off flag to 1, then stores each entry of the 0x0B groups in its keep-off flag. */
void mapLoadBlock(MapFixupBlock *block)
{
    MapFixupBlock *current;
    MapFixupGroup **groupSlot;
    MapFixupGroup *group;
    MapFixupEntry **entrySlot;
    int groupIndex;
    int entryIndex;
    int flagIndex;

    mapRegisterBlock((MapBlockHeader *)block);
    elvActiveMask = 0;
    elvNoOutMask = 0;
    for (flagIndex = 0; flagIndex < MAP_KEEP_OFF_COUNT; flagIndex++) {
        elvKeepOffFlags[flagIndex] = 1;
    }
    UnduInit(block);
    current = mapCurrentBlock;
    if (current == 0) {
        return;
    }
    if (current->flags != MAP_BLOCK_LOADED) {
        return;
    }
    groupSlot = (MapFixupGroup **) &current->firstGroup;
    for (groupIndex = 0; groupIndex < current->groupCount; groupIndex++) {
        entrySlot = (MapFixupEntry **) &groupSlot[groupIndex]->firstEntry;
        for (entryIndex = 0; entryIndex < groupSlot[groupIndex]->entryCount; entryIndex++) {
            if ((groupSlot[groupIndex]->typeWord & 0xFF00) == MAP_GROUP_KEEP_OFF) {
                elvKeepOffFlags[entrySlot[entryIndex]->keepId] = (int) entrySlot[entryIndex] | 1;
            }
        }
    }
}

/* Entry of the current block's group whose type word is groupType; keepId < 0 selects the group's first entry, otherwise the entry whose keep id is keepId. */
MapFixupEntry *UnduDataGetHeader(int groupType, int keepId)
{
    MapFixupBlock *block = mapCurrentBlock;
    MapFixupGroup **groupSlot;
    MapFixupGroup *group;
    MapFixupEntry **entrySlot;
    MapFixupEntry *entry;
    int groupIndex;
    int entryIndex;

    if (block == 0) {
        return 0;
    }
    if (!(block->flags & MAP_BLOCK_FIXED_UP)) {
        UnduInit(block);
    }
    if (block->flags != MAP_BLOCK_LOADED) {
        return 0;
    }
    groupSlot = (MapFixupGroup **) &block->firstGroup;
    for (groupIndex = 0; groupIndex < block->groupCount; groupIndex++) {
        group = *groupSlot++;
        if (group->typeWord != groupType) {
            continue;
        }
        entrySlot = (MapFixupEntry **) &group->firstEntry;
        if (keepId < 0) {
            return *entrySlot;
        }
        for (entryIndex = 0; entryIndex < group->entryCount; entryIndex++, entrySlot++) {
            /* Both arms are the same lookup in the original: the relocated byte is read and the arms merge late. */
            if (group->relocated != 0) {
                if ((*entrySlot)->keepId == keepId) {
                    return *entrySlot;
                }
            } else {
                if ((*entrySlot)->keepId == keepId) {
                    return *entrySlot;
                }
            }
        }
    }
    return 0;
}

/* Resets a probe request to the default cross-check, height limit, scratchpad and unit masks. */
void UnduParamInit(MapCollisionObject *request)
{
    request->record = 0;
    request->previousRecord = 0;
    request->flags = 0;
    request->bits = 0;
    request->count = 16;
    request->loopCount = 16;
    request->missCount = 0;
    request->probeEnabled = 0xFF;
    request->heightTolerance = 1.0f;
    request->distance = MAP_HEIGHT_NONE;
    request->block = 0;
    request->recordSet = (MapRecordSet *) MAP_SCRATCHPAD_PROBE;
    request->kind = 0;
    request->radius = 0.5f;
    request->keyMask = 0;
    request->keyValue = 1;
}

/*
 * Closest point of the edge edgeStart-edgeEnd to point: writes the offset from
 * that closest point to point into offset (the asm computes point - closest) and
 * returns its length. The parameter t is clamped to
 * [0, 1] with VU0 clip flags; when it was clamped (clip bits 0x108) the length
 * is recomputed from the unclamped edge offset. Hand-written VU0 asm.
 */
float mapEdgeClosestPoint(Vector4 *offset, const Vector4 *point, const Vector4 *edgeStart, const Vector4 *edgeEnd)
{
    float distance;

    __asm__ __volatile__(
        "lqc2 $vf7, 0(%1)\n\t"
        "lqc2 $vf8, 0(%2)\n\t"
        "lqc2 $vf9, 0(%3)\n\t"
        "vsub.xz $vf1, $vf7, $vf8\n\t"
        "vsub.xz $vf2, $vf9, $vf8\n\t"
        "vmul.xz $vf3, $vf1, $vf2\n\t"
        "vmul.xz $vf4, $vf2, $vf2\n\t"
        "vaddz.x $vf5, $vf3, $vf3z\n\t"
        "vaddz.x $vf6, $vf4, $vf4z\n\t"
        "vdiv Q, vf5x, vf6x\n\t"
        "vaddw.y $vf2, $vf0, $vf0w\n\t"
        "vsub.w $vf3, $vf0, $vf0w\n\t"
        "vmove.y $vf6, $vf0\n\t"
        "vwaitq\n\t"
        "vmulq.xyz vf5, vf2, Q\n\t"
        "vclipw.xyz vf5, vf0w\n\t"
        "vclipw.xyz vf5, vf3w\n\t"
        "vsub.xz $vf6, $vf1, $vf5\n\t"
        "sqc2 $vf6, 0(%4)\n\t"
        "vmul.xz $vf3, $vf6, $vf6\n\t"
        "vaddz.x $vf4, $vf3, $vf3z\n\t"
        "vsqrt Q, vf4x\n\t"
        "vwaitq\n\t"
        "cfc2 $8, $vi22\n\t"
        "cfc2 $2, $vi18\n\t"
        "nop\n\t"
        "andi $3, $2, 0x108\n\t"
        "beq $0, $3, 1f\n\t"
        "vmul.xz $vf3, $vf1, $vf1\n\t"
        "vaddz.x $vf4, $vf3, $vf3z\n\t"
        "vsqrt Q, vf4x\n\t"
        "sqc2 $vf1, 0(%4)\n\t"
        "vwaitq\n\t"
        "cfc2 $8, $vi22\n"
        "1:\n\t"
        "nop\n\t"
        "mtc1 $8, %0"
        : "=f"(distance)
        : "r"(point), "r"(edgeStart), "r"(edgeEnd), "r"(offset)
        : "$2", "$3", "$8", "memory");
    return distance;
}


/*
 * Keeps the probe point off the open edges (neighbour -1) of the keep-off groups
 * in elvKeepOffFlags. First clamps z to the range of elevator 0 when elvActiveMask
 * has MAP_ELV_CLAMP_Z_LOW or MAP_ELV_CLAMP_Z_HIGH. Then, for each group whose Y range holds the point, a point
 * inside one of its triangles is moved past the nearest open edge (mode 1 by the
 * probe radius, mode 0 onto the edge); a point outside is pushed away from every
 * open edge nearer than the radius (mode 1 only; mode 0 has a zero limit).
 */
void mapPushOutOfKeepOff(MapRecordSet *probe, Vector4 *point)
{
    Vector4 elevatorPosition;
    Vector4 *vertices;
    Vector4 *triangleVertex[3];
    Vector4 *ringVertex[4];
    MapRecordGroup *group;
    MapRecord *candidate;
    MapRecord *edgeRecord;
    int insideTriangle;
    int keepOffWord;
    int keepOffIndex;
    int recordIndex;
    int edgeRecordIndex;
    int edgeIndex;
    f32 pushLimit;
    f32 elevatorLimit;
    f32 edgeDistance;
    f32 pushScale;

    if (elvActiveMask & (MAP_ELV_CLAMP_Z_LOW | MAP_ELV_CLAMP_Z_HIGH)) {
        elvGetPosition(0, &elevatorPosition, 0);
        if (elvActiveMask & MAP_ELV_CLAMP_Z_LOW) {
            elevatorLimit = (elevatorPosition.z - MAP_ELEVATOR_HALF_SPAN) + probe->radius;
            if (point->z < elevatorLimit) {
                point->z = elevatorLimit;
            }
        }
        if (elvActiveMask & MAP_ELV_CLAMP_Z_HIGH) {
            elevatorLimit = (elevatorPosition.z + MAP_ELEVATOR_HALF_SPAN) - probe->radius;
            if (elevatorLimit < point->z) {
                point->z = elevatorLimit;
            }
        }
    }
    for (keepOffIndex = 0; keepOffIndex < MAP_KEEP_OFF_COUNT; keepOffIndex++) {
        keepOffWord = elvKeepOffFlags[keepOffIndex];
        group = (MapRecordGroup *) (keepOffWord & ~3);
        keepOffWord &= 3;
        if (group == 0) {
            continue;
        }
        if (!(point->y < group->vertices[0].y) && !(group->vertices[0].w < point->y)) {
            insideTriangle = 0;
            candidate = group->records;
            vertices = group->vertices;
            for (recordIndex = 0; recordIndex < group->recordCount; recordIndex++) {
                triangleVertex[0] = vertices + candidate->vertexIndex[0];
                triangleVertex[1] = vertices + candidate->vertexIndex[1];
                triangleVertex[2] = vertices + candidate->vertexIndex[2];
                __asm__ __volatile__(
                    "lqc2 $vf16, 0(%0)\n\t"
                    "lqc2 $vf17, 0(%1)\n\t"
                    "lqc2 $vf18, 0(%2)\n\t"
                    "vmr32.xyzw $vf1, $vf16\n\t"
                    "vmr32.xyzw $vf2, $vf17\n\t"
                    "vmr32.xyzw $vf3, $vf18\n\t"
                    "vmr32.xyzw $vf20, $vf1\n\t"
                    "vmr32.xyzw $vf21, $vf2\n\t"
                    "vmr32.xyzw $vf22, $vf3\n\t"
                    : : "r"(triangleVertex[0]), "r"(triangleVertex[1]), "r"(triangleVertex[2]) : "memory");
                if (!mapPointOutsideTriangle(point)) {
                    insideTriangle = 1;
                    break;
                }
                candidate++;
            }
            edgeRecord = group->records;
            probe->correction.vector.x = 0.0f;
            probe->correction.vector.w = 100.0f;
            probe->correction.vector.z = 0.0f;
            switch (keepOffWord) {
            default:
            case MAP_KEEP_OFF_MODE_EDGE:
                pushLimit = probe->correction.vector.x;
                break;
            case MAP_KEEP_OFF_MODE_RADIUS:
                pushLimit = probe->radius;
                break;
            }
            for (edgeRecordIndex = 0; edgeRecordIndex < group->recordCount; edgeRecordIndex++, edgeRecord++) {
                ringVertex[0] = vertices + edgeRecord->vertexIndex[0];
                ringVertex[3] = ringVertex[0];
                ringVertex[1] = vertices + edgeRecord->vertexIndex[1];
                ringVertex[2] = vertices + edgeRecord->vertexIndex[2];
                for (edgeIndex = 0; edgeIndex < 3; edgeIndex++) {
                    if (edgeRecord->neighbour[edgeIndex] == -1) {
                        edgeDistance = mapEdgeClosestPoint(&probe->edgeOffset, point, ringVertex[edgeIndex], ringVertex[edgeIndex + 1]);
                        if (insideTriangle) {
                            if (edgeDistance < probe->correction.vector.w) {
                                probe->correction.vector.x = probe->edgeOffset.x * (-1.0f - (pushLimit / edgeDistance));
                                probe->correction.vector.z = probe->edgeOffset.z * (-1.0f - (pushLimit / edgeDistance));
                                probe->correction.vector.w = edgeDistance;
                            }
                        } else if (edgeDistance < pushLimit) {
                            pushScale = (pushLimit / edgeDistance) - 1.0f;
                            probe->correction.vector.x += probe->edgeOffset.x * pushScale;
                            probe->correction.vector.z += probe->edgeOffset.z * pushScale;
                        }
                    }
                }
            }
            point->x += probe->correction.vector.x;
            point->z += probe->correction.vector.z;
        }
    }
}

/* Plane distance of the probe origin to a record, or MAP_HEIGHT_NONE when the origin is outside the triangle. */
f32 UnduCheckSub(const MapRecordSet *set, const MapRecord *record)
{
    const Vector4 *origin = &set->origin;
    const Vector4 *vertices = set->vertices;
    const Vector4 *vertex0 = vertices + record->vertexIndex[0];
    const Vector4 *vertex1 = vertices + record->vertexIndex[1];
    const Vector4 *vertex2 = vertices + record->vertexIndex[2];
    __asm__ __volatile__("lqc2 vf16, 0(%0)" : : "r"(vertex0) : "memory");
    __asm__ __volatile__("lqc2 vf17, 0(%0)" : : "r"(vertex1) : "memory");
    __asm__ __volatile__("lqc2 vf18, 0(%0)" : : "r"(vertex2) : "memory");
    __asm__ __volatile__("vmr32.xyzw vf1, vf16" : : : "memory");
    __asm__ __volatile__("vmr32.xyzw vf2, vf17" : : : "memory");
    __asm__ __volatile__("vmr32.xyzw vf3, vf18" : : : "memory");
    __asm__ __volatile__("vmr32.xyzw vf20, vf1" : : : "memory");
    __asm__ __volatile__("vmr32.xyzw vf21, vf2" : : : "memory");
    __asm__ __volatile__("vmr32.xyzw vf22, vf3" : : : "memory");
    if (mapPointOutsideTriangle(origin) == 0) {
        return mapPlaneDistance(origin, vertex0, set->planes + record->planeIndex);
    }
    return MAP_HEIGHT_NONE;
}

/* Keeps a hit when it is nearer the origin height than the best hit so far; returns MAP_HIT_NONE, MAP_HIT_NEARER or MAP_HIT_FIRST. */
int UnduCheckSubHeightCheck(MapRecordSet *set, MapRecord *record, float height)
{
    int hitStatus;
    float hitGap;
    float collisionGap;

    hitStatus = MAP_HIT_NONE;
    if (set->collisionHeight == MAP_HEIGHT_NONE) {
        hitStatus = MAP_HIT_FIRST;
    } else {
        hitGap = set->origin.y - height;
        if (hitGap < 0.0f) {
            hitGap = -hitGap;
        }
        collisionGap = set->origin.y - set->collisionHeight;
        if (collisionGap < 0.0f) {
            collisionGap = -collisionGap;
        }
        if (hitGap < collisionGap) {
            hitStatus = MAP_HIT_NEARER;
        }
    }
    switch (hitStatus) {
    case MAP_HIT_NEARER:
        if (set->origin.y + set->heightTolerance < height) {
            break;
        }
    case MAP_HIT_FIRST:
        set->collisionHeight = height;
        set->collisionRecord = record;
        break;
    default:
        break;
    }
    return hitStatus;
}


/* Scans the record table and passes each hit to UnduCheckSubHeightCheck unless its kind is 4. */
void UnduCheckSubCheckAll(MapRecordSet *owner)
{
    MapRecord *record;
    int recordIndex;
    float value;
    int kind;
    MapRecordSet *set = owner;

    record = set->records;
    if (mapScanRegisteredNode(owner) == 0) {
        for (recordIndex = 0; recordIndex < set->recordCount; recordIndex++) {
            value = UnduCheckSub(set, record);
            if (value != MAP_HEIGHT_NONE) {
                kind = record->kind;
                if (set->kindTable != 0) {
                    kind = set->kindTable[kind].kindCode;
                }
                if (kind != 4) {
                    UnduCheckSubHeightCheck(set, record, value);
                }
            }
            record++;
        }
    }
}

/* Element info block: flags at +0x08, kind bits at +0x0F, a limit word at +0x20. */
typedef struct ElvInfo {
    unsigned char unmodeled_00[8];
    unsigned short flags;               /* +0x08 */
    unsigned char unmodeled_0A[5];
    unsigned char kindBits;             /* +0x0F */
    unsigned char unmodeled_10[16];
    unsigned int limit;                 /* +0x20 */
} ElvInfo;

/* Key table entry: the key word is the first of its two words. */
typedef struct ElvKeyEntry {
    unsigned int key;
    unsigned int unmodeled_04;
} ElvKeyEntry;

/* Element state: info pointer, key table, radius, last key, key mask and value, and a position vector at +0xD0. */
typedef struct ElvElement {
    ElvInfo *info;                      /* +0x00 */
    unsigned char unmodeled_04[16];
    ElvKeyEntry *keyTable;              /* +0x14 */
    unsigned char unmodeled_18[28];
    float radius;                       /* +0x34 */
    unsigned char unmodeled_38[32];
    unsigned int lastKey;               /* +0x58 */
    unsigned char unmodeled_5C[4];
    unsigned int keyMask;               /* +0x60 */
    unsigned char unmodeled_64[4];
    unsigned int keyValue;              /* +0x68 */
    unsigned char unmodeled_6C[100];
    Vector4 position;                   /* +0xD0 */
} ElvElement;

/* Tests an element against the probe key: the key table or the key slot gives the key, a masked compare returns 1 at once, and otherwise the info block and position band decide. */
int CheckWallAttr(ElvElement *element, unsigned short *keySlot)
{
    unsigned int recordKey;
    unsigned int keyKindBits;

    if (element->keyTable != 0) {
        recordKey = element->keyTable[*keySlot].key;
    } else {
        recordKey = *keySlot;
    }
    element->lastKey = recordKey;
    if (((recordKey & element->keyMask) ^ element->keyValue) == 0) {
        return 1;
    }
    if (!(element->info->flags & 0x800) && (recordKey & 0xFFFFFF0F) == 3) {
        /* the unsigned short cast is kept: without it the code changes */
        keyKindBits = (unsigned short) element->lastKey & 0xF0;

        if (keyKindBits == 0 || keyKindBits == (element->info->kindBits << 4)) {
            return 1;
        }
    }
    if (element->info->limit == 512) {
        if (element->lastKey == 0) {
            if ((elvNoOutMask >> (element->lastKey & 7)) & 1) {
                return 1;
            }
        }
    } else if ((element->lastKey & 0xFFFFFF00) == 512) {
        if (((elvActiveMask >> (element->lastKey & 7)) & 1) == 0) {
            return 1;
        }
        elvGetPosition(element->lastKey & 7, &element->position, 0);
        if (element->radius < element->position.y - 0.5f || element->position.y + 0.5f < element->radius) {
            return 1;
        }
    }
    return 0;
}

/* Walks the triangle record from point from toward point to; returns an exit edge code or the next record. */
int mapStepRecord(MapRecordSet *set, MapRecord *record, Vector4 *from, Vector4 *to)
{
    int i;
    f32 zero;
    int nextVertex;
    int recordKind;
    int neighbour;
    MapRecord *neighbourRecord;
    MapRecordKind *kindTable;
    Vector4 *vertices;
    Vector4 *vertex0;
    Vector4 *vertex1;
    Vector4 *vertex2;
    Vector4 *edgeStart;
    Vector4 *edgeEnd;

    vertices = set->vertices;
    vertex0 = vertices + record->vertexIndex[0];
    vertex1 = vertices + record->vertexIndex[1];
    vertex2 = vertices + record->vertexIndex[2];
    set->triangleVertex[0] = vertex0;
    set->triangleVertex[1] = vertex1;
    set->triangleVertex[2] = vertex2;
    __asm__ __volatile__("lqc2 vf16, 0(%0)" : : "r"(vertex0) : "memory");
    __asm__ __volatile__("lqc2 vf17, 0(%0)" : : "r"(vertex1) : "memory");
    __asm__ __volatile__("lqc2 vf18, 0(%0)" : : "r"(vertex2) : "memory");
    __asm__ __volatile__("vmr32.xyzw vf1, vf16" : : : "memory");
    __asm__ __volatile__("vmr32.xyzw vf2, vf17" : : : "memory");
    __asm__ __volatile__("vmr32.xyzw vf3, vf18" : : : "memory");
    __asm__ __volatile__("vmr32.xyzw vf20, vf1" : : : "memory");
    __asm__ __volatile__("vmr32.xyzw vf21, vf2" : : : "memory");
    __asm__ __volatile__("vmr32.xyzw vf22, vf3" : : : "memory");
    __asm__ __volatile__("lqc2 vf19, 0(%0)" : : "r"(to) : "memory");
    __asm__ __volatile__("vmr32.xyzw vf1, vf19" : : : "memory");
    __asm__ __volatile__("vmr32.xyzw vf23, vf1" : : : "memory");
    mapStoreEdgePlanes(set->planeA);
    if (set->planeA[0] <= 0.0f)
    {
        if (set->planeA[1] <= 0.0f)
        {
            if (set->planeA[2] <= 0.0f)
            {
                return 4;
            }
        }
    }
    __asm__ __volatile__("lqc2 vf19, 0(%0)" : : "r"(from) : "memory");
    __asm__ __volatile__("vmr32.xyzw vf1, vf19" : : : "memory");
    __asm__ __volatile__("vmr32.xyzw vf23, vf1" : : : "memory");
    mapStoreEdgePlanes(set->planeB);
    __asm__ __volatile__("lqc2 vf25, 0(%0)" : : "r"(from) : "memory");
    __asm__ __volatile__("lqc2 vf26, 0(%0)" : : "r"(to) : "memory");
    __asm__ __volatile__("vmr32.xyzw vf1, vf25" : : : "memory");
    __asm__ __volatile__("vmr32.xyzw vf2, vf26" : : : "memory");
    __asm__ __volatile__("vmr32.xyzw vf28, vf1" : : : "memory");
    __asm__ __volatile__("vmr32.xyzw vf29, vf2" : : : "memory");
    zero = 0.0f;
    for (i = 0; i < 3; i++)
    {
        if (!(set->planeB[i] <= zero && zero <= set->planeA[i] && (set->planeB[i] != zero || set->planeA[i] != zero)))
        {
            continue;
        }
        {
            f32 sideStart;
            f32 sideEnd;
            nextVertex = i + 1;
            if (nextVertex >= 3) {
                nextVertex = 0;
            }
            edgeStart = set->triangleVertex[i];
            edgeEnd = set->triangleVertex[nextVertex];
            __asm__ __volatile__(
                "lqc2 vf3, 0(%2)\n\t"
                "lqc2 vf24, 0(%3)\n\t"
                "vmr32.xyzw vf1, vf3\n\t"
                "vmr32.xyzw vf2, vf24\n\t"
                "vmr32.xyzw vf4, vf1\n\t"
                "vmr32.xyzw vf27, vf2\n\t"
                "vmula.xz ACC, vf3, vf28\n\t"
                "vmadda.xz ACC, vf25, vf29\n\t"
                "vmadd.xz vf2, vf26, vf4\n\t"
                "vmula.xz ACC, vf24, vf28\n\t"
                "vmadda.xz ACC, vf25, vf29\n\t"
                "vmadd.xz vf4, vf26, vf27\n\t"
                "vsubz.x vf1, vf2, vf2z\n\t"
                "vsubz.x vf3, vf4, vf4z\n\t"
                "qmfc2 $3, vf1\n\t"
                "qmfc2 $5, vf3\n\t"
                "mtc1 $3, %0\n\t"
                "mtc1 $5, %1\n\t"
                "nop\n\t"
                : "=f"(sideStart), "=f"(sideEnd) : "r"(edgeStart), "r"(edgeEnd) : "memory");
            if ((sideStart <= zero && zero <= sideEnd) || (zero <= sideStart && sideEnd <= zero))
            {
                neighbour = record->neighbour[i];
                if (neighbour == -1)
                {
                    recordKind = record->kind;
                    kindTable = set->kindTable;
                    if (kindTable != 0)
                    {
                        recordKind = kindTable[recordKind].kindCode;
                    }
                    set->hitKind = recordKind;
                    return i + 1;
                }
                else
                {
                    neighbourRecord = set->records + neighbour;
                    if (CheckWallAttr((ElvElement *) set, (unsigned short *) neighbourRecord) != 0)
                    {
                        return i + 257;
                    }
                    return (int) neighbourRecord;
                }
            }
        }
    }
    return 0;
}

/* -0.0f: the slope marker for an edge parallel to the x axis. */
extern float mapHorizontalSlope[];

#define FLOAT_BITS(x) (*(int *)&(x))


typedef union MapRecordUnit { MapRecord fields; float word[4]; } MapRecordUnit;

void UnduCheckSubCrossCheck(MapRecordSet *set, MapCollisionObject *object, MapRecord *record)
{
    Vector4 edgeVector;
    Vector4 *startPtr;
    Vector4 *endPtr;
    MapRecord *current;
    Vector4 *edgeStart;
    Vector4 *edgeEnd;
    Vector4 *crossStart;
    Vector4 *crossEnd;
    Vector4 *motionVec;
    float edgeAngle;
    float slideLength;
    float motionAngle;
    float moveX;
    float moveZ;
    float edgeVecX;
    float edgeVecZ;
    float moveSlope;
    float edgeSlope;
    float angleDelta;
    float hitX;
    float hitZ;
    float offsetX;
    float offsetZ;
    float motionLength;
    float reversedDelta;
    float wallDistance;
    float reachFraction;
    float startX;
    float startZ;
    float crossStartX;
    float crossStartZ;
    int exitCode;
    int stepResult;
    int nextIndex;

    endPtr = &set->origin;
    startPtr = &set->probeStart;
    current = record;
    set->blockCount = object->count;
    if (set->blockCount != 0)
    {
        for (;;)
        {
            set->triangleVertex[2] = 0;
            set->triangleVertex[1] = 0;
            set->triangleVertex[0] = 0;
            exitCode = 0;
            set->loopCount = object->loopCount;
            if (set->loopCount != 0)
            {
                do
                {
                    stepResult = mapStepRecord(set, current, startPtr, endPtr);
                    if (stepResult < 512)
                    {
                        if (stepResult == 4)
                        {
                            UnduCheckSubHeightCheck(set, current, mapPlaneDistance(endPtr, set->triangleVertex[0], set->planes + current->planeIndex));
                        }
                        exitCode = stepResult;
                        break;
                    }
                    current = (MapRecord *)stepResult;
                    set->loopCount--;
                } while (set->loopCount != 0);
            }
            exitCode = (exitCode < 512) ? exitCode : 0;
            if (set->hitKind == 4)
            {
                object->bits |= 0x10;
                set->collisionRecord = 0;
                set->collisionHeight = -1000.0f;
                UnduCheckSubCheckAll(set);
                if (set->collisionRecord != 0)
                {
                    break;
                }
            }
            if (exitCode == 0)
            {
                break;
            }
            if (exitCode == 4)
            {
                break;
            }
            set->mask = exitCode;
            /* the u8 truncation of exitCode is kept: a typed form changes the code */
            exitCode = (unsigned char)exitCode;
            switch (object->flags & 7)
            {
            case 2:
            {
                slideLength = 0.0f;
                motionAngle = 0.0f;
                motionVec = set->motion;
                if (motionVec != 0)
                {
                    motionAngle = mapHeadingXZ(motionVec);
                    slideLength = __builtin_sqrtf(motionVec->x * motionVec->x + motionVec->z * motionVec->z);
                }
                nextIndex = (exitCode < 3) ? exitCode : 0;
                edgeStart = set->triangleVertex[exitCode - 1];
                edgeEnd = set->triangleVertex[nextIndex];
                edgeVector.x = edgeEnd->x - edgeStart->x;
                edgeVector.z = edgeEnd->z - edgeStart->z;
                edgeAngle = mapHeadingXZ(&edgeVector);
                angleDelta = edgeAngle - motionAngle;
                if (angleDelta < -1.5707964f)
                {
                    angleDelta += 6.2831855f;
                    while (angleDelta < -1.5707964f)
                    {
                        angleDelta += 6.2831855f;
                    }
                }
                reversedDelta = angleDelta - 3.1415927f;
                if (angleDelta < 0.0f)
                {
                    angleDelta = -angleDelta;
                }
                if (reversedDelta < 0.0f)
                {
                    reversedDelta = -reversedDelta;
                }
                if (reversedDelta < angleDelta)
                {
                    edgeAngle += 3.1415927f;
                    angleDelta = reversedDelta;
                }
                slideLength *= xglCos(angleDelta);
                if (set->motion != 0)
                {
                    endPtr->x = set->probeStart.x + xglSin(edgeAngle) * slideLength;
                    endPtr->z = set->probeStart.z + xglCos(edgeAngle) * slideLength;
                }
                current = record;
                exitCode = 0;
            }
                break;
            case 3:
            {
                int nextVertexIndex;

                nextVertexIndex = (exitCode < 3) ? exitCode : 0;
                crossStart = set->triangleVertex[exitCode - 1];
                crossEnd = set->triangleVertex[nextVertexIndex];
                crossStartX = crossStart->x;
                crossStartZ = crossStart->z;
                startX = startPtr->x;
                startZ = startPtr->z;
                moveX = endPtr->x - startX;
                moveZ = endPtr->z - startZ;
                if (-0.01f < moveX && moveX < 0.01f)
                {
                    moveSlope = 0.0f;
                }
                else if (-0.01f < moveZ && moveZ < 0.01f)
                {
                    moveSlope = mapHorizontalSlope[0];
                }
                else
                {
                    moveSlope = moveZ / moveX;
                }
                edgeVecX = crossEnd->x - crossStartX;
                edgeVecZ = crossEnd->z - crossStartZ;
                if (-0.01f < edgeVecX && edgeVecX < 0.01f)
                {
                    edgeSlope = 0.0f;
                }
                else if (-0.01f < edgeVecZ && edgeVecZ < 0.01f)
                {
                    edgeSlope = mapHorizontalSlope[0];
                }
                else
                {
                    edgeSlope = edgeVecZ / edgeVecX;
                }
                if (FLOAT_BITS(moveSlope) == 0)
                {
                    if (FLOAT_BITS(edgeSlope) == 0)
                    {
                        float rangeHi;

                        hitZ = endPtr->z;
                        rangeHi = crossStartZ;
                        hitX = crossStartX;
                        if (crossStartZ < endPtr->z)
                        {
                            rangeHi = endPtr->z;
                            hitZ = crossStartZ;
                        }
                        if (crossEnd->z < rangeHi)
                        {
                            rangeHi = crossEnd->z;
                        }
                        if (rangeHi < hitZ)
                        {
                            rangeHi = hitZ;
                        }
                        hitZ = rangeHi;
                    }
                    else
                    {
                        hitX = startPtr->x;
                        hitZ = edgeSlope * (hitX - crossStartX) + crossStartZ;
                    }
                }
                else if (FLOAT_BITS(edgeSlope) == 0)
                {
                    hitX = crossStartX;
                    hitZ = moveSlope * (hitX - startPtr->x) + startPtr->z;
                }
                else if (FLOAT_BITS(moveSlope) == 0x80000000)
                {
                    if (FLOAT_BITS(edgeSlope) == 0x80000000)
                    {
                        float rangeLo;
                        float rangeHi;

                        rangeLo = endPtr->x;
                        rangeHi = crossStartX;
                        if (crossStartX < endPtr->x)
                        {
                            rangeHi = endPtr->x;
                            rangeLo = crossStartX;
                        }
                        if (crossEnd->x < rangeHi)
                        {
                            rangeHi = crossEnd->x;
                        }
                        if (rangeHi < rangeLo)
                        {
                            rangeHi = rangeLo;
                        }
                        hitX = rangeHi;
                        hitZ = crossStartZ;
                    }
                    else
                    {
                        hitX = (startPtr->z - crossStartZ) / edgeSlope + crossStartX;
                        hitZ = startPtr->z;
                    }
                }
                else if (FLOAT_BITS(edgeSlope) == 0x80000000)
                {
                    hitX = (crossStartZ - startPtr->z) / moveSlope + startPtr->x;
                    hitZ = crossStartZ;
                }
                else
                {
                    hitX = ((edgeSlope * crossStartX - crossStartZ) - moveSlope * startPtr->x + startPtr->z) / (edgeSlope - moveSlope);
                    hitZ = moveSlope * (hitX - startPtr->x) + startPtr->z;
                }
                offsetX = hitX - startPtr->x;
                offsetZ = hitZ - startPtr->z;
                wallDistance = __builtin_sqrtf(offsetX * offsetX + offsetZ * offsetZ);
                offsetX = endPtr->x - startPtr->x;
                offsetZ = endPtr->z - startPtr->z;
                wallDistance -= 0.001f;
                motionLength = __builtin_sqrtf(offsetX * offsetX + offsetZ * offsetZ);
                reachFraction = wallDistance / motionLength;
                hitX = startPtr->x + offsetX * reachFraction;
                hitZ = startPtr->z + offsetZ * reachFraction;
                set->origin.z = hitZ;
                set->origin.x = hitX;
                /* the MapRecordUnit view is kept: the plain current->planeIndex form changes the code */
                set->origin.y = set->collisionHeight = mapPlaneDistance(endPtr, set->triangleVertex[0], set->planes + ((MapRecordUnit *) current)->fields.planeIndex);
                set->collisionRecord = current;
                exitCode = 4;
            }
            break;
            case 1:
            default:
            {
                exitCode = 4;
            }
            break;
            }
            if (exitCode == 4)
            {
                break;
            }
            if (--set->blockCount == 0)
            {
                break;
            }
        }
    }
    if (set->blockCount == 0)
    {
        set->collisionRecord = record;
        set->collisionHeight = object->distance;
        set->origin.x = set->probeStart.x;
        set->origin.y = set->probeStart.y;
        set->origin.z = set->probeStart.z;
    }
}

int mapWalkSegment(MapRecordSet *set, MapRecord *record, Vector4 *from, Vector4 *to)
{
    int exitCode;
    int stepResult;

    exitCode = 0;
    set->triangleVertex[2] = 0;
    set->triangleVertex[1] = 0;
    set->triangleVertex[0] = 0;
    set->recordCursor = record;
    set->loopCount = set->object->loopCount;
    while (set->loopCount != 0)
    {
        stepResult = mapStepRecord(set, record, from, to);
        record = (MapRecord *)stepResult;
        if (stepResult < 512)
        {
            exitCode = stepResult;
            break;
        }
        set->recordCursor = record;
        set->loopCount--;
    }
    return exitCode < 512 ? exitCode : 0;
}

f32 UnduCheckResultCheck(MapRecordSet *set, MapCollisionObject *object, Vector4 *normal)
{
    unsigned short flags;
    int kind;

    flags = object->flags & 0xefff;
    object->flags = flags;
    object->bits = object->bits | (set->mask & 3);
    object->distance = set->collisionHeight;
    if (set->collisionRecord != 0)
    {
        object->previousRecord = object->record;
        object->record = set->collisionRecord;
        if (set->kindTable != 0)
        {
            kind = set->kindTable[set->collisionRecord->kind].kindCode;
        }
        else
        {
            kind = set->collisionRecord->kind;
        }
        object->kind = kind;
        if ((kind & ~0xff) == 0x200)
        {
            elvGetPosition(kind & 0xf, &set->elv, 0);
            set->collisionHeight = set->elv.y;
        }
    }
    else
    {
        object->record = 0;
        object->kind = 0;
        if ((flags & 7) != 1)
        {
            return set->collisionHeight;
        }
    }
    if (set->motion != 0)
    {
        normal->x = set->origin.x;
        normal->y = set->origin.y;
        normal->z = set->origin.z;
    }
    return set->collisionHeight;
}

/* Binds the object to its record set (or the scratchpad one) and stores the probe bounds; returns the set, or 0 when no block is available. */
MapRecordSet *mapSetupCollisionObject(Vector4 *position, const int *delta, MapCollisionObject *object)
{
    /* The probe start is read through the union view: a plain Vector4 read changes the scheduling of this function. */
    DoublewordVector4 *start = (DoublewordVector4 *) position;
    unsigned short recordCount;
    MapRecordSet *set;
    MapRecordGroup *block;

    set = object->recordSet;
    if ((*(unsigned char *) mapCurrentBlock & 0x80) == 0) {
        UnduInit(mapCurrentBlock);
    }
    block = object->block;
    if (set == 0) {
        set = (MapRecordSet *)0x70000000;
    }
    if (block == 0) {
        if (mapCurrentBlock != 0) {
            if ((*(unsigned char *) mapCurrentBlock & 0x7f) == 3) {
                block = UnduDataGetHeader(256, -1);
                if (block == 0) {
                    return 0;
                }
            } else {
                return 0;
            }
        } else {
            return 0;
        }
    }
    set->vertices = block->vertices;
    recordCount = block->recordCount;
    set->recordCount = recordCount;
    set->heightTolerance = object->heightTolerance;
    set->collisionHeight = -1000.0f;
    set->planes = block->planes;
    set->records = block->records;
    set->kindTable = block->kindTable;
    set->object = object;
    set->collisionRecord = 0;
    object->bits = 0;
    set->probeStart.x = start->vector.x;
    set->probeStart.y = start->vector.y;
    set->probeStart.z = start->vector.z;
    set->mask = 0;
    set->unmodeled_53 = 0;
    /* The loop note keeps the delta store ahead of the object copies in the scheduler. */
    do {
        set->motion = (Vector4 *) delta;
        set->keyMask = object->keyMask;
        set->radius = object->radius;
        set->keyValue = object->keyValue;
    } while (0);
    if (delta != 0 && (delta[0] == 0x7fffffff || delta[1] == 0x7fffffff || delta[2] == 0x7fffffff)) {
        set->motion = 0;
    }
    return set;
}


/* Moves the position to the centroid of the record triangle nearest to it in the object's record set. */
void mapSnapToNearestTriangle(Vector4 *position, MapCollisionObject *object)
{
    MapRecordSet *set = object->recordSet;
    const MapRecord *record;
    int i;
    float startX = position->x;
    float startZ;

    set->probeStart.x = startX;
    set->origin.x = startX;
    startZ = position->z;
    record = set->records;
    set->origin.w = 1000000.0f;
    set->probeStart.z = startZ;
    set->origin.z = startZ;
    for (i = 0; i < set->recordCount; i++) {
        const Vector4 *vertex0 = set->vertices + record->vertexIndex[0];
        const Vector4 *vertex1 = set->vertices + record->vertexIndex[1];
        const Vector4 *vertex2 = set->vertices + record->vertexIndex[2];
        float offsetX = (vertex0->x + vertex1->x + vertex2->x) / 3.0f - set->probeStart.x;
        float offsetZ = (vertex0->z + vertex1->z + vertex2->z) / 3.0f - set->probeStart.z;
        float distSq = offsetX * offsetX + offsetZ * offsetZ;
        if (distSq < set->origin.w) {
            set->origin.w = distSq;
            set->origin.x = offsetX;
            set->origin.z = offsetZ;
        }
        record++;
    }
    position->x = set->origin.x + set->probeStart.x;
    position->z = set->origin.z + set->probeStart.z;
}

void mapBindAndSnapObject(Vector4 *position, MapCollisionObject *object) {
    if (mapSetupCollisionObject(position, 0, object) != 0) {
        mapSnapToNearestTriangle(position, object);
    }
}

typedef unsigned int Quadword __attribute__((mode(TI)));


typedef union MapCollisionObjectUnit { MapCollisionObject fields; float word[2]; } MapCollisionObjectUnit;

int mapPushOutByRadius(MapRecordSet *set, MapCollisionObject *object)
{
    MapRecord *record;
    float angleStep;
    float distance;
    int i;
    int sampleCount;
    MapRecord *objectRecord;
    Vector4 *probeStartPtr;
    int vertexIndex0;
    int vertexIndex1;
    int vertexIndex2;

    if (1.0f < set->radius) {
        sampleCount = F2I((set->radius * 2.0f) * 3.1415927f * 3.0f) / 4 * 4;
    } else {
        sampleCount = F2I((set->radius * 2.0f) * 3.1415927f * 10.0f) / 4 * 4;
    }
    angleStep = 6.2831855f / I2F(sampleCount);
    record = set->collisionRecord;
    if (!record) {
        return 0;
    }
    *(Quadword *) &set->savedProbeStart = *(Quadword *) &set->probeStart;
    *(Quadword *) &set->savedOrigin = *(Quadword *) &set->origin;
    set->probeStart.x = set->origin.x;
    set->probeStart.y = set->origin.y;
    set->probeStart.z = set->origin.z;
    set->deltaX = 0.0f;
    set->deltaZ = 0.0f;
    set->hitFlag = 0.0f;
    for (i = 0; sampleCount > i; i++) {
        float sampleAngle;
        Vector4 *edgeStart;
        Vector4 *edgeEnd;
        int exitCode;
        int nextVertex;
        float edgeVecX;
        float edgeVecZ;
        float originOffsetX;
        float originOffsetZ;
        float lenSq;
        float projection;
        float offsetX;
        float offsetZ;
        float absX;
        float absZ;
        Vector4 *vertex0;
        Vector4 *vertex1;
        Vector4 *vertex2;
        Vector4 *savedOriginPtr;

        sampleAngle = I2F(i) * angleStep;
        set->origin.x = set->probeStart.x + xglCos(sampleAngle) * set->radius;
        set->origin.y = set->probeStart.y;
        set->origin.z = set->probeStart.z + xglSin(sampleAngle) * set->radius;
        /* the MapRecordUnit view is kept: the plain record->vertexIndex form changes the code */
        vertexIndex0 = ((MapRecordUnit *) record)->fields.vertexIndex[0];
        vertexIndex1 = ((MapRecordUnit *) record)->fields.vertexIndex[1];
        vertexIndex2 = ((MapRecordUnit *) record)->fields.vertexIndex[2];
        vertex2 = set->vertices + vertexIndex2;
        vertex1 = set->vertices + vertexIndex1;
        vertex0 = set->vertices + vertexIndex0;
        __asm__ __volatile__(
            "lqc2 vf16, 0(%0)\n"
            "lqc2 vf17, 0(%1)\n"
            "lqc2 vf18, 0(%2)\n"
            "vmr32.xyzw vf1, vf16\n"
            "vmr32.xyzw vf2, vf17\n"
            "vmr32.xyzw vf3, vf18\n"
            "vmr32.xyzw vf20, vf1\n"
            "vmr32.xyzw vf21, vf2\n"
            "vmr32.xyzw vf22, vf3\n"
            : : "r"(vertex0), "r"(vertex1), "r"(vertex2) : "memory");
        if (mapPointOutsideTriangle(&set->origin) == 0) {
            continue;
        }
        exitCode = mapWalkSegment(set, record, &set->probeStart, &set->origin) & 0xff;
        if ((unsigned int)(exitCode - 1) >= 3) {
            continue;
        }
        nextVertex = (exitCode < 3) ? exitCode : 0;
        edgeEnd = set->triangleVertex[nextVertex];
        edgeStart = set->triangleVertex[exitCode - 1];
        edgeVecX = edgeEnd->x - edgeStart->x;
        set->edgeDx = edgeVecX;
        edgeVecZ = edgeEnd->z - edgeStart->z;
        set->edgeDz = edgeVecZ;
        lenSq = edgeVecX * edgeVecX + edgeVecZ * edgeVecZ;
        originOffsetX = set->origin.x - edgeStart->x;
        set->edgeSx = originOffsetX;
        originOffsetZ = set->origin.z - edgeStart->z;
        set->edgeSz = originOffsetZ;
        projection = (originOffsetX * edgeVecX + originOffsetZ * edgeVecZ) / lenSq;
        offsetX = originOffsetX - projection * edgeVecX;
        offsetZ = originOffsetZ - projection * edgeVecZ;
        absX = __builtin_fabsf(offsetX);
        absZ = __builtin_fabsf(offsetZ);
        if (absX >= 0.001f || absZ >= 0.001f) {
            set->probeStart.x -= offsetX;
            set->probeStart.z -= offsetZ;
            savedOriginPtr = &set->savedOrigin;
            probeStartPtr = &set->probeStart;
            mapWalkSegment(set, record, savedOriginPtr, probeStartPtr);
            record = set->recordCursor;
            set->deltaX = -(set->probeStart.x - set->savedOrigin.x);
            set->deltaZ = -(set->probeStart.z - set->savedOrigin.z);
        }
    }
    set->collisionRecord = 0;
    set->collisionHeight = -1000.0f;
    if (record != 0) {
        set->origin.x = set->savedOrigin.x - set->deltaX;
        set->origin.y = set->savedOrigin.y;
        set->origin.z = set->savedOrigin.z - set->deltaZ;
        /* the MapCollisionObjectUnit view is kept: the plain object->record form changes the code */
        objectRecord = ((MapCollisionObjectUnit *) object)->fields.record;
        set->probeStart = set->savedProbeStart;
        if (objectRecord != 0) {
            record = objectRecord;
        } else {
            record = object->previousRecord;
        }
        distance = UnduCheckSub(set, record);
        if (-1000.0f != distance) {
            UnduCheckSubHeightCheck(set, record, distance);
        } else if (set->motion == 0) {
            UnduCheckSubCheckAll(set);
        } else {
            UnduCheckSubCrossCheck(set, object, record);
        }
    }
    return set->hitFlag != 0.0f;
}


/* Moves the probe: without a block it offsets the position by the bounds; otherwise it probes the record set and returns the hit distance. */
f32 UnduCheck(Vector4 *position, const Vector4 *delta, MapCollisionObject *object)
{
    MapRecordSet *set;
    MapRecord *record;
    float distance;
    float recordDistance;
    float endX;
    float endY;
    float endZ;
    int pushOutCount;

    /* The failure block precedes the test in the original layout (b start; failed: ...; start:); it is the shared exit of three
     * paths (no block, no record set) and has no structured form: an if/else or early returns put it after the main path. */
    goto start;
failed:
    object->distance = 0.0f;
    object->record = 0;
    object->kind = 0;
    return object->distance;
start:
    {
        if (mapCurrentBlock == 0) {
            if (delta != 0) {
                position->x += delta->x;
                position->y += delta->y;
                position->z += delta->z;
            }
            goto failed;
        }
        set = mapSetupCollisionObject(position, (const int *)delta, object);
        if (set == 0) {
            goto failed;
        }
        if ((object->flags & 0x200) != 0) {
            if (object->record == 0) {
                if (object->missCount >= 61) {
                    mapSnapToNearestTriangle(position, object);
                    object->previousRecord = 0;
                } else {
                    object->missCount++;
                }
            } else {
                object->missCount = 0;
            }
        }
        if (object->record == 0 || (*(int *)&object->flags & 0x5000) != 0 || (gameWork.control & CONTROL_MAP_LOADED) != 0 || (object->kind & ~0xff) == 0x200) {
            set->origin.x = set->probeStart.x;
            set->origin.y = set->probeStart.y;
            set->origin.z = set->probeStart.z;
            UnduCheckSubCheckAll(set);
            distance = UnduCheckResultCheck(set, object, position);
            if (distance == -1000.0f) {
                return distance;
            }
            if (set->motion == 0) {
                if ((object->flags & 0x100) != 0 && set->collisionRecord != 0) {
                    return set->collisionHeight;
                }
                return distance;
            } else {
                set->collisionRecord = 0;
                set->collisionHeight = -1000.0f;
            }
        }
        if (object->record != 0) {
            record = object->record;
        } else {
            record = object->previousRecord;
        }
        endX = set->probeStart.x;
        endY = set->probeStart.y;
        endZ = set->probeStart.z;
        if (set->motion != 0) {
            endX += set->motion->x;
            endY += set->motion->y;
            endZ += set->motion->z;
        }
        set->origin.x = endX;
        set->origin.y = endY;
        set->origin.z = endZ;
        recordDistance = UnduCheckSub(set, record);
        if (recordDistance != -1000.0f) {
            UnduCheckSubHeightCheck(set, record, recordDistance);
        } else if (set->motion == 0) {
            UnduCheckSubCheckAll(set);
        } else {
            UnduCheckSubCrossCheck(set, object, record);
        }
        if ((object->flags & 0x100) != 0) {
            pushOutCount = 0;
            if (set->collisionRecord != 0) {
                while (pushOutCount < 32 && mapPushOutByRadius(set, object) != 0) {
                    pushOutCount++;
                }
            }
        }
        if ((set->object->flags & 0x800) == 0) {
            f32 hitDistance = UnduCheckResultCheck(set, object, position);
            mapPushOutOfKeepOff(set, position);
            return hitDistance;
        }
        return UnduCheckResultCheck(set, object, position);
    }
}

/* Runs the move probe on the scratchpad probe state at 0x70000000. */
void mapProbeScratchMove(Vector4 *position) {
    MapCollisionObject *probe = (MapCollisionObject *) MAP_SCRATCHPAD_PROBE;

    UnduParamInit(probe);
    probe->recordSet = (MapRecordSet *) (MAP_SCRATCHPAD_PROBE + 0x40);
    UnduCheck(position, 0, probe);
}

void uwamonoPlayerUpdateDummy(void) {

}

int mapDummy(void) {
    return 0;
}

float mapHorizontalSlope[1] = { -0.0f };
