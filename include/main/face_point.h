#ifndef INCLUDE_MAIN_FACE_POINT_H
#define INCLUDE_MAIN_FACE_POINT_H

#include "shared.h"
#include "main/pxy_packet_state.h"

struct XglCamera;


/* Functions of src/main/face_point.c that other files call. */

void xglCullingIgnore(void);
void xglCullingIgnoreOff(void);
int xglCullingExist(void);
void xglCullingMapInit(void);
/* Selects the culling map named by the pack header (its name at +0x10, with its prefix). */
void xglCullingMapSet(const PxyHeader *source);
int nmlModelCalcClipCulling(struct XglCamera *camera, const Vector4 *point);

#endif /* INCLUDE_MAIN_FACE_POINT_H */
