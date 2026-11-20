#include "common.h"
#include "main/xenoActor.h"
#include "main/xgl_timer0.h"
#include "debug_break.h"
#include "shared.h"
#include "main/game_data.h"
#include "main/ytachi_test.h"
#include "main/tya_draw_gauge.h"
#include "main/nml_model_set.h"

extern unsigned char cameraCfSlots[];
extern unsigned char cameraSlotTable[];
extern unsigned short uwamonoAttachIdTable[];
extern unsigned int uwamonoKeyRangeTable[];
extern float uwamonoSlopeFactorTable[];
extern unsigned int uwamonoSlopeDirectionTable[];
extern unsigned char uwamonoProbeTemplate[];

/* One GS vertex: X and Y in 12.4 fixed point, then the attribute word. */
typedef struct TyaGaugeVertex {
    unsigned int x;
    unsigned int y;
    unsigned int attribute;
    unsigned int unmodeled_0C;
} TyaGaugeVertex;

/* DMA/VIF/GIF packet of the two gauge sprites, uploaded as 9 quadwords. */
typedef struct TyaGaugePacket {
    unsigned int dma_vif_tag[4];
    unsigned int vif_gif_setup[4];
    unsigned int gif_control[4];
    unsigned int filled_color[4];
    TyaGaugeVertex filled_vertices[2];
    unsigned int unfilled_color[4];
    TyaGaugeVertex unfilled_vertices[2];
} TyaGaugePacket;

#define TYA_GAUGE_COORDINATE_SCALE 0x10
#define TYA_GAUGE_X_ORIGIN 0x6FF8
#define TYA_GAUGE_Y_ORIGIN 0x71F7

extern TyaGaugePacket tyaGaugePacket;


/* Draws a horizontal gauge filled to current / maximum of its width. */
void tyaDrawGauge(TyaGaugeArgs *gauge)
{
    int left_x;
    int top_y;
    int right_x;
    int bottom_y;
    int fill_width;
    int fill_x;

    left_x = (gauge->x * TYA_GAUGE_COORDINATE_SCALE) + TYA_GAUGE_X_ORIGIN;
    top_y = (gauge->y * TYA_GAUGE_COORDINATE_SCALE) + TYA_GAUGE_Y_ORIGIN;
    right_x = left_x + (gauge->width * TYA_GAUGE_COORDINATE_SCALE);
    bottom_y = top_y + (gauge->height * TYA_GAUGE_COORDINATE_SCALE);
    fill_width = (gauge->current * gauge->width) / gauge->maximum;
    if (fill_width < 0) {
        fill_width = 0;
    }
    if (gauge->width < fill_width) {
        fill_width = gauge->width;
    }
    fill_x = left_x + (fill_width * TYA_GAUGE_COORDINATE_SCALE);

    tyaGaugePacket.filled_color[0] = gauge->filled_color[0];
    tyaGaugePacket.filled_color[1] = gauge->filled_color[1];
    tyaGaugePacket.filled_color[2] = gauge->filled_color[2];
    tyaGaugePacket.filled_color[3] = gauge->filled_color[3];
    tyaGaugePacket.filled_vertices[0].x = left_x;
    tyaGaugePacket.filled_vertices[0].y = top_y;
    tyaGaugePacket.filled_vertices[0].attribute = gauge->vertex_attribute;
    tyaGaugePacket.filled_vertices[1].x = fill_x;
    tyaGaugePacket.filled_vertices[1].y = bottom_y;
    tyaGaugePacket.filled_vertices[1].attribute = gauge->vertex_attribute;

    tyaGaugePacket.unfilled_color[0] = gauge->unfilled_color[0];
    tyaGaugePacket.unfilled_color[1] = gauge->unfilled_color[1];
    tyaGaugePacket.unfilled_color[2] = gauge->unfilled_color[2];
    tyaGaugePacket.unfilled_color[3] = gauge->unfilled_color[3];
    tyaGaugePacket.unfilled_vertices[0].x = fill_x;
    tyaGaugePacket.unfilled_vertices[0].y = top_y;
    tyaGaugePacket.unfilled_vertices[0].attribute = gauge->vertex_attribute;
    tyaGaugePacket.unfilled_vertices[1].x = right_x;
    tyaGaugePacket.unfilled_vertices[1].y = bottom_y;
    tyaGaugePacket.unfilled_vertices[1].attribute = gauge->vertex_attribute;

    nmlModelDirectSend(4, &tyaGaugePacket, 9);
}

/* .data of this TU after the uwamono tables: address order. */
TyaGaugePacket tyaGaugePacket = {
    { 0, 0, 0, 0x51000008u },
    { 32769, 0x70034000u, 0x551551Eu, 0 },
    { 0x70000u, 0, 71, 0 },
    { 128, 192, 128, 128 },
    { { 0, 0, 0xFFFFFFFFu, 0 }, { 0, 0, 0xFFFFFFFFu, 0 } },
    { 64, 64, 64, 128 },
    { { 0, 0, 0xFFFFFFFFu, 0 }, { 0, 0, 0xFFFFFFFFu, 0 } }
};
