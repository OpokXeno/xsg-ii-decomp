#ifndef INCLUDE_MAIN_XGL_PACKET_UNPACK_H
#define INCLUDE_MAIN_XGL_PACKET_UNPACK_H

#include "main/xgl_camera.h"
#include "shared.h"

/* Functions of src/main/xgl_packet_unpack.c that other files call: the water sends of the VU1 unpack packets. */

/* Sends one water frame for the camera: the view matrix times data, the matrices, the parameter rows and a pair. */
void tiWaterSendFrame(XglCamera *camera, Matrix4 data);
/* Sends the camera position and the direction from it to the translation row (row 3) of transform. */
void tiWaterSendDirection(XglCamera *camera, Matrix4 transform);
/* Sends the pair (x, y) at VU address 0x3DA. */
void tiWaterSendItemPair(float x, float y);

#endif /* INCLUDE_MAIN_XGL_PACKET_UNPACK_H */
