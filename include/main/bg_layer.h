#ifndef INCLUDE_MAIN_BG_LAYER_H
#define INCLUDE_MAIN_BG_LAYER_H

/* Background layers (src/main/bg_layer.c): the layer index shared by the per-layer tables and bgLayerObjects. */
enum {
    BG_BACK = 0,
    BG_MIDDLE = 1,
    BG_FRONT = 2,
    BG_MENUBG = 3,                  /* the menu background, loaded first; its state byte is index 3 */
    BG_OBJECT_COUNT = 3
};

/* Layer state byte: BG_LAYER_DONE marks a finished layer. */
#define BG_LAYER_DONE 0x80

/* Functions of src/main/bg_layer.c that other files call. */

void bgLayerInit(void);
void bgLayerLoadAll(void);
void bgLayerAdvance(int index);
void bgLayerResetState(void);
void bgLayerCameraSetup(void);
void bgLayerClearFlags(int index);
void bgLayerClearAllFlags(void);
void bgLayerRelease(void);

#endif /* INCLUDE_MAIN_BG_LAYER_H */
