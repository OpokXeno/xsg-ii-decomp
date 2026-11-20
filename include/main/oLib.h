#ifndef INCLUDE_MAIN_OLIB_H
#define INCLUDE_MAIN_OLIB_H

/* Functions of src/main/oLib.c that other files call. */

/* Cosine and sine of an angle in degrees, from 10-degree tables built on first use. */
float xglCosDeg(int degrees);
float xglSinDeg(int degrees);

/* Nonzero while the per-frame record updates are suspended (record_render.c reads it through a one-member view). */
extern int recordUpdatePaused;

#endif /* INCLUDE_MAIN_OLIB_H */
