#ifndef INCLUDE_MAIN_NML_MODEL_SET_H
#define INCLUDE_MAIN_NML_MODEL_SET_H

#include "shared.h"
#include "main/pxy_packet_state.h"
#include "main/xgl_light.h"

/* Functions of src/main/nml_model_set.c that other files call. */

union Matrix; /* main/xgl_2.h */

/* A plane or angle row as pxySetClipPlaneMasked reads it: words 0..2 are compared as integers. */
typedef union PxyRowBits {
    Vector4Elements row;
    u32 words[4];
} PxyRowBits;

/* The rotation of pxySetClipPlaneWithAngle: its two floats become the z and w of the angle row. */
typedef struct PxyAngle {
    float rotX;
    float rotY;
} PxyAngle;

void nmlModelConstruct(void);
void pxyRequestTextureCopyPass(void);
void pxySkipOverlayDraw(void);
void pxySetHideMask(void *value);
void pxySetSecondPartMask(void *value);
void nmlModelSetParent(PxyHeader *value);
void nmlModelSetStencilAlpha(long long value);
int pxyGlowPendingGet(void);
void nmlModelSetDrawMatrix(Matrix4 matrix);
void nmlModelSetStencil(int enabled);
void nmlModelDirectSend(int key, void *data, int qwc);
void nmlModelDirectSendFlagged(int key, void *data, int qwc, int flag);
void nmlModelDirectSendFull(int key, void *data, int qwc, int flag, int state);
void pxySetDefaultLight(XglLightSet *light);
void nmlModelSetGlobalFogDist(float fogNear, float fogFar, float fogMin, float fogMax);
void nmlModelSetGlobalFogCol(Vector4 *value);
void xglDirectionFromAngles(Vector4 *direction, const Vector4 *rotation);
void pxySetMode4Flag(void);
void pxyRequestOverlayPass(void);
void nmlModelFlushClear(void);
void nmlModelSetDropPass(void);
void nmlModelSetFogCol(Vector4 *position);
void nmlModelSetFogDist(float fogNear, float fogFar, float fogMin, float fogMax);
void nmlModelSetHumanModel(void);
void nmlModelSetRenderLevel(int flags);
void nmlModelSetScale(const Vector4 *src);
void nmlModelSetShadowMapEntry(void);
void nmlModelSetSortOffset(float value);
void nmlModelSetTexMapCircle(void);
void nmlModelSetTexProreal(void);
void nmlModelSetTexture(TxyBlock *block);
void nmlModelSetTransparency(float value);
void nmlModelSetZwrite(int enabled);
void pxyAbortModel(void);
void pxyAddClipPlanePair(PxyRowBits *plane, PxyRowBits *angle);
void pxyBeginFrame(void);
int pxyFindEntryIndex(PxyHeader *header, const char *name);
void pxyFlush(void);
void *pxyGetEntry(PxyHeader *header, int index);
int pxyIsModelHidden(void);
void pxyLatchActorState(int value);
void pxyRequestForcedVisibility(void);
void pxyResetLightToDefault(void);
float pxyInterpolateKeys(int count, Vector4Elements *keyframes, float time, float weight);
void pxySetAnimTime(float value);
void pxySetCameraEnable(int index, int value);
void pxySetClipPlaneMasked(PxyRowBits *plane, PxyRowBits *angle, int count, float x, float y, float z, u32 *mask);
void pxySetClipPlaneWithAngle(Vector4Elements *plane, Vector4Elements *angle, PxyAngle *rotation);
void pxySetColorIndexSource(u8 *colorIndex);
void pxySetColorScale(Vector4 *value);
void pxySetDrawCallback(void (*callback)(int));
void pxySetFocus(float first, float second);
void pxySetFogWhite(void);
void pxySetFreezeAnimation(void);
void pxySetJointMatrices(union Matrix *joints, union Matrix *global);
void pxySetLightCopy(XglLightSet *light);
void pxySetLightPtr(XglLightSet *light);
void pxySetMode3Flag(int enabled);
void pxySetModelPosition(Vector4 *position);
void pxySetMotionGlobalMatrices(union Matrix *global);
void pxySetPartMatrices(union Matrix *matrices);
void pxySetShadowCasterMode(int value);
void pxySetShadowFirstPass(void);
void pxySetShadowHeightRange(float first, float second);
void pxySetShadowMode(int mode, float a, float b, float c);
void pxySetShadowUploadFlag(void);
void pxySetSourceDrawPass(void);
void pxySetTexMapDropAll(void);
int pxySubmitModel(PxyHeader *header);
void xglRenderFrameBeginFlushEIrq(void);
void xglRenderFrameEndFlushEIrq(void);

#endif /* INCLUDE_MAIN_NML_MODEL_SET_H */
