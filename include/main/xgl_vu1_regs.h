#ifndef INCLUDE_MAIN_XGL_VU1_REGS_H
#define INCLUDE_MAIN_XGL_VU1_REGS_H

/* Functions of src/main/xgl_vu1_regs.c that other files call. */

/* Angle of the vector (x, y) in radians: atan of y / x, moved into the half-plane of the vector. */
float xglAtan2Angle(float x, float y);

#endif /* INCLUDE_MAIN_XGL_VU1_REGS_H */
