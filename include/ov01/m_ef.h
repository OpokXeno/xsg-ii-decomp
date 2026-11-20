#ifndef INCLUDE_OV01_M_EF_H
#define INCLUDE_OV01_M_EF_H

#include "shared.h"
#include "main/sv_get_image_item.h"
#include "ov01/m_gs.h"
#include "main/xenoActor.h"
#include "ov01/m_ef_path.h"
#include "main/nml_model_set.h"

/* A packed RGBAQ colour quadword of a GS packet, by channel. */
typedef union MEfGsColor {
    unsigned int rgba[4];
    Quadword whole;
} MEfGsColor;

/* A packed GS vertex: the projected point, or its words (w carries ADC). */
typedef union MEfGsVertex {
    Quadword whole;
    AlignedVector4 vector;
    int words[4];
} MEfGsVertex;

/* Sampled keys that the actor path stores behind the cleared record. */
#define MEF_PATH_KEY_COUNT 13

/* The smoke path (m_ef_create_smp_01.c): two control points and the actor frame behind the head. */
typedef struct MEfSmokePathCurve {
    MEfPathCurve head;                               /* +0x00 */
    AlignedVector4 startControl;                     /* +0xB0 */
    union {
        AlignedVector4 endControl;                   /* +0xC0 */
        float endAxes[4];                            /* the smoke path pushes y sideways through this view */
    };
    Matrix4 actorMatrix;                             /* +0xD0: actor frame of the effect 29 path */
} MEfSmokePathCurve;

/* The actor path (m_ef_actor_path.c): its sampled keys behind the head. */
typedef struct MEfKeyedPathCurve {
    MEfPathCurve head;                               /* +0x00 */
    Vector4 keyPosition[MEF_PATH_KEY_COUNT];         /* +0xB0 */
} MEfKeyedPathCurve;

/* mefPathCurve is 0x2000 bytes of zero storage shared by all path effects (m_ef.c): each effect reads it
 * through its own view. */
typedef union MEfPathCurveObject {
    MEfPathCurve curve;
    MEfSmokePathCurve smoke;
    MEfKeyedPathCurve keyed;
    Vector4 vectors[512];
} MEfPathCurveObject;

extern MEfPathCurveObject mefPathCurve;

/* Functions of src/ov01/m_ef.c that the later effect files call. */



Matrix4 *MSeGetActorMatrix(Matrix4 *destination, struct XenoActor *actor, u32 coordinate);
Vector4 *MEfGetActorCoord(Vector4 *destination, struct XenoActor *actor, u32 coordinate);
Vector4 *MEfMakeRandomVector(Vector4 *destination);
Vector4 *MEfScatterXY(Vector4 *destination, Vector4 *target, Vector4 *source, float spread);
int MEfMake4Vertex(Vector4 *destination, const Vector4 *source, float halfSize);
int MEfMake4Vertex2(Vector4 *destination, const Vector4 *source, float halfSize, float angleBase);

#endif /* INCLUDE_OV01_M_EF_H */
