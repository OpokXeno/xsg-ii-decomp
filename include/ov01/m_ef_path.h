#ifndef INCLUDE_OV01_M_EF_PATH_H
#define INCLUDE_OV01_M_EF_PATH_H

#include "shared.h"
#include "ov01/m_gs.h"

/* Functions of src/ov01/m_ef_path.c that other files call. */

/* Points of the path curve (a Hermite path): the effects use up to MEF_PATH_CURVE_POINTS = 5 of them as the start,
 * the middle, the end offset and the end of the curve (the smoke path uses all five as keys). */
#define MEF_PATH_CURVE_POINTS 8

/*
 * The path curve record that MEfHermitePathInit clears (0xB0 bytes) and stores the point count in: the count,
 * the points and the two tangents MMathCalcHermiteKeyPrm computes from them. Every effect uses this head; the
 * smoke and actor paths keep more behind it (m_ef.h).
 */
typedef struct MEfPathCurve {
    int count;                                       /* +0x00 */
    unsigned char unmodeled_04[0x10 - 0x04];
    AlignedVector4 points[MEF_PATH_CURVE_POINTS];    /* +0x10 */
    AlignedVector4 tangentStart;                     /* +0x90 */
    AlignedVector4 tangentEnd;                       /* +0xA0 */
} MEfPathCurve;

/* Clears the 0xB0-byte curve and stores count in it. */
void MEfHermitePathInit(MEfPathCurve *curve, int count);

/* Key choice of a Hermite path: the previous, start, end and next points of key index. */
void MMathCalcHermiteKeyPrm(MEfPathCurve *curve, int index);

/* Hermite sample at parameter on the segment from key point index to the next one. */
void MMathCalcHermiteKey(Vector4 *destination, MEfPathCurve *curve, int index, float parameter);

/* Draws the marker of a path end (a flat shape turning once a second) at point. */
void MEfDrawPathEndMarker(MGsPacket *packet, const Vector4 *point, float scale);

#endif /* INCLUDE_OV01_M_EF_PATH_H */
