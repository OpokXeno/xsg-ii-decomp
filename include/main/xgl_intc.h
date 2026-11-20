#ifndef INCLUDE_MAIN_XGL_INTC_H
#define INCLUDE_MAIN_XGL_INTC_H

/* The INTC handlers of src/main/xgl_intc.c, one per cause, registered through xglIntcRecords
 * (main/xgl_dma_handler.h). */

int xglIntcHandlerGs(int cause);
int xglIntcHandlerVblankStart(int cause);
int xglIntcHandlerVblankEnd(int cause);
int xglIntcHandlerVif0(int cause);
int xglIntcHandlerVif1(int cause);
int xglIntcHandlerVu0(int cause);
int xglIntcHandlerVu1(int cause);
int xglIntcHandlerIpu(int cause);
int xglIntcHandlerTimer0(int cause);
int xglIntcHandlerTimer1(int cause);

#endif /* INCLUDE_MAIN_XGL_INTC_H */
