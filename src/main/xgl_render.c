#include "common.h"
#include "main/xgl_dma_handler.h"
#include "shared.h"
#include "main/movie_player.h"
#include "main/sound_driver.h"
#include "main/xgl_dma.h"
#include "main/xgl_render.h"
#include "main/gr_screen.h"
#include "main/xgl_camera.h"
#include "main/xgl_packet.h"
#include "main/xgl_pad.h"
#include "main/xgl_font.h"
#include "main/gifState.h"
#include "main/nml_model_set.h"
#include "main/xgl_studio.h"
#include "main/sef.h"

typedef unsigned int XglQuadword __attribute__((mode(TI)));

/* GIF tag fields. */
#define GIF_TAG_EOP 0x8000
#define GIF_TAG_PRE 0x4000
#define GIF_TAG_PRIM_SHIFT 15
#define GIF_TAG_NREG_SHIFT 28
#define GIF_REG_RGBAQ 1
#define GIF_REG_XYZ2 5
#define GIF_REG_AD 14
#define GS_PRIM_SPRITE 6
#define SCE_GS_XYOFFSET_1 0x18
#define SCE_GS_XYOFFSET_2 0x19
#define SCE_GS_PRMODECONT 0x1A
#define SCE_GS_TEXA 0x3B
#define SCE_GS_SCISSOR_1 0x40
#define SCE_GS_SCISSOR_2 0x41
#define SCE_GS_DIMX 0x44
#define SCE_GS_DTHE 0x45
#define SCE_GS_COLCLAMP 0x46
#define SCE_GS_TEST_1 0x47
#define SCE_GS_TEST_2 0x48
#define SCE_GS_FRAME_1 0x4C
#define SCE_GS_FRAME_2 0x4D
#define SCE_GS_ZBUF_1 0x4E
#define SCE_GS_ZBUF_2 0x4F
/* GS alpha 0x80 is fully opaque. */
#define GS_ALPHA_OPAQUE 0x80
#define XGL_DMA_CHANNEL_GIF 2


/* GIF_STAT and VIF1_STAT, polled by the VSync callback. */
#define GIF_STAT_ADDRESS 0x10003020
#define VIF1_STAT_ADDRESS 0x10003C00
#define GIF_STAT_APATH_MASK 0xC00 /* a GIF transfer path is active */
#define VIF1_STAT_VPS_MASK 3 /* the VIF1 pipeline is not idle */
/* EE hardware registers (GS privileged, DMAC, VIF). */
#define GS_DISPFB1 0x12000070
#define GS_DISPLAY1 0x12000080
#define GS_EXTBUF 0x120000B0
#define GS_EXTDATA 0x120000C0
#define GS_EXTWRITE 0x120000D0
#define GS_CSR 0x12001000
#define D1_CHCR 0x10009000
#define D2_CHCR 0x1000A000
#define D_ENABLEW 0x1000F590
#define VIF0_ERR 0x10003820
#define VIF1_ERR 0x10003C20
#define RENDER_COUNT_RESO 6
#define GS_COORD_CENTER 2048 /* primitive coordinate system origin */
/* ScanLineInterpolate modes. */
#define GS_PMODE_EN2 2 /* PMODE.EN2: xglRenderDispOn/Off toggle the display output */
/* Vsyncs per frame: the render thread waits for two, and a third means the frame ran late. */
#define RENDER_VSYNC_PER_FRAME 2
#define GS_ZTST_GEQUAL 2
#define GS_TEST_ZTE_GEQUAL 0x50000
#define GS_TEST_CLEAR_DEPTH 0x00032001 /* alpha test never passes but writes Z only */
#define GS_TEST_CLEAR_FRAME 0x00030003
/* Default clear colour: black with GS alpha 0x80. */
#define RENDER_CLEAR_COLOR_DEFAULT 0x80000000
/* Clear sprite GIF tag: PRE with a sprite PRIM, and the register list
 * A+D (state), RGBAQ, XYZ2, XYZ2 (two corners), plus a closing A+D for the clear packet. */
#define RENDER_CLEAR_GIF_REGS (GIF_REG_AD | (GIF_REG_RGBAQ << 4) | (GIF_REG_XYZ2 << 8) | (GIF_REG_XYZ2 << 12))
#define RENDER_CLEAR_QWORDS 6

/*
 * Packed GS registers of the draw-environment block below, laid out as
 * the GS manual defines them (bit positions from the first field).
 */
typedef struct GsFrame {
    u64 fbp : 9;
    u64 unmodeled_09 : 7;
    u64 fbw : 6;
    u64 unmodeled_22 : 2;
    u64 psm : 6;
    u64 unmodeled_30 : 2;
    u64 fbmsk : 32;
} GsFrame;

typedef struct GsZbuf {
    u64 zbp : 9;
    u64 unmodeled_09 : 15;
    u64 psm : 4;
    u64 unmodeled_28 : 4;
    u64 zmsk : 1;
    u64 unmodeled_33 : 31;
} GsZbuf;

typedef struct GsXyoffset {
    u64 ofx : 16;
    u64 unmodeled_16 : 16;
    u64 ofy : 16;
    u64 unmodeled_48 : 16;
} GsXyoffset;

typedef struct GsScissor {
    u64 scax0 : 11;
    u64 unmodeled_11 : 5;
    u64 scax1 : 11;
    u64 unmodeled_27 : 5;
    u64 scay0 : 11;
    u64 unmodeled_43 : 5;
    u64 scay1 : 11;
    u64 unmodeled_59 : 5;
} GsScissor;

typedef struct GsTest {
    u64 ate : 1;
    u64 atst : 3;
    u64 aref : 8;
    u64 afail : 2;
    u64 date : 1;
    u64 datm : 1;
    u64 zte : 1;
    u64 ztst : 2;
    u64 unmodeled_19 : 45;
} GsTest;

typedef struct GsTexa {
    u64 ta0 : 8;
    u64 unmodeled_08 : 7;
    u64 aem : 1;
    u64 unmodeled_16 : 16;
    u64 ta1 : 8;
    u64 unmodeled_40 : 24;
} GsTexa;

typedef struct GsDimx {
    u64 dm00 : 3;
    u64 unmodeled_03 : 1;
    u64 dm01 : 3;
    u64 unmodeled_07 : 1;
    u64 dm02 : 3;
    u64 unmodeled_11 : 1;
    u64 dm03 : 3;
    u64 unmodeled_15 : 1;
    u64 dm10 : 3;
    u64 unmodeled_19 : 1;
    u64 dm11 : 3;
    u64 unmodeled_23 : 1;
    u64 dm12 : 3;
    u64 unmodeled_27 : 1;
    u64 dm13 : 3;
    u64 unmodeled_31 : 1;
    u64 dm20 : 3;
    u64 unmodeled_35 : 1;
    u64 dm21 : 3;
    u64 unmodeled_39 : 1;
    u64 dm22 : 3;
    u64 unmodeled_43 : 1;
    u64 dm23 : 3;
    u64 unmodeled_47 : 1;
    u64 dm30 : 3;
    u64 unmodeled_51 : 1;
    u64 dm31 : 3;
    u64 unmodeled_55 : 1;
    u64 dm32 : 3;
    u64 unmodeled_59 : 1;
    u64 dm33 : 3;
    u64 unmodeled_63 : 1;
} GsDimx;

typedef struct GsPrmodecont {
    u64 ac : 1;
    u64 unmodeled_01 : 63;
} GsPrmodecont;

typedef struct GsColclamp {
    u64 clamp : 1;
    u64 unmodeled_01 : 63;
} GsColclamp;

typedef struct GsDthe {
    u64 dthe : 1;
    u64 unmodeled_01 : 63;
} GsDthe;

typedef struct GsGifRegisters {
    u64 regs0 : 4;
    u64 unmodeled_04 : 60;
} GsGifRegisters;

typedef struct GsGifTagControl {
    u64 nloop : 15;
    u64 eop : 1;
    u64 unmodeled_16 : 30;
    u64 pre : 1;
    u64 prim : 11;
    u64 flg : 2;
    u64 nreg : 4;
} GsGifTagControl;

/*
 * GS draw-environment block (0x004A8F00, size 0x100): a GIF A+D packet of
 * fifteen register writes, each an 8-byte value followed by its register
 * address, as sceGsPutDrawEnv sends it.
 */
typedef struct GsDrawEnv {
    GsGifTagControl gif_tag;
    GsGifRegisters gif_registers;
    GsFrame frame1;
    u64 frame1_address;
    GsZbuf zbuf1;
    u64 zbuf1_address;
    GsXyoffset xyoffset1;
    u64 xyoffset1_address;
    GsScissor scissor1;
    u64 scissor1_address;
    GsTest test1;
    u64 test1_address;
    GsFrame frame2;
    u64 frame2_address;
    GsZbuf zbuf2;
    u64 zbuf2_address;
    GsXyoffset xyoffset2;
    u64 xyoffset2_address;
    GsScissor scissor2;
    u64 scissor2_address;
    GsTest test2;
    u64 test2_address;
    GsPrmodecont prmodecont;
    u64 prmodecont_address;
    GsTexa texa;
    u64 texa_address;
    GsColclamp colclamp;
    u64 colclamp_address;
    GsDthe dthe;
    u64 dthe_address;
    GsDimx dimx;
    u64 dimx_address;
} GsDrawEnv;

typedef union GsDrawPacket {
    GsDrawEnv value;
    XglQuadword quadwords[16];
} GsDrawPacket;

extern GsDrawPacket DrawEnv;
extern void xglRenderSetReso(int index);
extern void SyncDCache(void *start, void *end);
int xglRenderVSyncCallback(int unused);
extern void sceGsPutDrawEnv(GsDrawEnv *env);
/* xgl_render's .sdata, 0x69E084-0x69E0AC, in address order. */
signed char pxyTickScaleDouble = 0;
int xglRenderGlobalFadeColor = 0; /* packed fade colour 0xAABBGGRR, written by xglRenderGlobalFadeSet */
int xglRenderCpUDispFlag = 0;     /* 0 by xglRenderCpUDispOff, 1 by xglRenderCpUDispOn */
int xglRenderClearSkip = 0;       /* 1 by xglRenderClearSkipOn, 0 by xglRenderClearSkipOff */
signed char xglRenderVSyncInterval = 0;
int xglRenderMoveCount = 0;
int LoopCount = 0;
/* The GS VSync interrupt callback increments this counter while
 * xglRenderSyncMove polls it on the render thread. */
volatile u32 VSyncCount = 0;
int ScanLineInterpolate = 0;
int xglCullingMapEmptyName = 0;
extern XglRenderReso asDispReso[RENDER_COUNT_RESO];
extern XglRenderVram asVramBase[RENDER_COUNT_RESO];
int xglRenderVSyncCallback(int unused);
void xglRenderDispEnvInit(void);
void xglRenderClearEnvInit(void);

void xglRenderCpUDispOff(void) {
    xglRenderCpUDispFlag = 0;
}

void xglRenderCpUDispOn(void) {
    xglRenderCpUDispFlag = 1;
}

extern s16 sRenderPsm;
extern u16 sRenderDisplayBase;
extern u16 sRenderFrameBase;
extern s16 cameraScreenWidth;
int FlushCache(int);

/* Swaps the draw and display frame bases and sends FRAME_1/FRAME_2 for the new draw buffer. */
void xglRenderDrawFlip(void)
{
    u16 frameBase;
    u64 *packet;

    packet = (u64 *) SCRATCHPAD_BASE;
    frameBase = sRenderDisplayBase;
    sRenderDisplayBase = sRenderFrameBase;
    sRenderFrameBase = frameBase;
    packet[0] = 0x1000000000008002ULL; /* GIFtag: NLOOP 2, EOP, one A+D register */
    packet[1] = GIF_REG_AD;
    packet[2] = (u64) sRenderDisplayBase | ((u64) (s16) (cameraScreenWidth / 64) << 16)
        | ((u64) sRenderPsm << 24);
    packet[3] = 0x4C; /* FRAME_1 */
    packet[4] = (u64) sRenderDisplayBase | ((u64) (s16) (cameraScreenWidth / 64) << 16)
        | ((u64) sRenderPsm << 24);
    packet[5] = 0x4D; /* FRAME_2 */
    /* sync: the GIF packet stores must reach memory before the DMA engine reads them */
    __asm__ volatile("sync" ::: "memory");
    FlushCache(0);
    xglDmaDirectNormal(2, (unsigned int) packet, 3);
}

extern u16 sRenderFlipBase;

void xglRenderCopyDisp2Draw(void)
{
    GsAdPair *scratch;
    int count;

    scratch = (GsAdPair *) SCRATCHPAD_BASE;
    count = 0;
    count = 0;
    scratch[count].data = 0x1000000000008000ULL;
    scratch[count++].reg = GIF_REG_AD;
    scratch[count].data = 0;
    scratch[count++].reg = 0x3F;
    scratch[count].data = ((u64) (sRenderDisplayBase << 5) | 0x80000)
        | ((u64) sRenderPsm << 24)
        | ((u64) (sRenderFlipBase << 5) << 32)
        | ((u64) sRenderPsm << 56)
        | ((u64) 0x8000 << 36);
    scratch[count++].reg = 0x50;
    scratch[count].data = 0;
    scratch[count++].reg = 0x51;
    scratch[count].data = ((u64) 448 << 32) | 0x200;
    scratch[count++].reg = 0x52;
    scratch[count].data = 2;
    scratch[count++].reg = 0x53;
    scratch[0].data += count - 1;
    /* sync: the GIF packet stores must reach memory before the DMA engine reads them */
    __asm__ volatile("sync" ::: "memory");
    FlushCache(0);
    xglDmaDirectNormal(2, (unsigned int) scratch, count);
}

void xglRenderCopyDisp2Flip(void)
{
    GsAdPair *scratch;
    int count;

    scratch = (GsAdPair *) SCRATCHPAD_BASE;
    count = 0;
    count = 0;
    scratch[count].data = 0x1000000000008000ULL;
    scratch[count++].reg = GIF_REG_AD;
    scratch[count].data = 0;
    scratch[count++].reg = 0x3F;
    scratch[count].data = ((u64) (sRenderDisplayBase << 5) | 0x80000)
        | ((u64) sRenderPsm << 24)
        | ((u64) (sRenderFrameBase << 5) << 32)
        | ((u64) sRenderPsm << 56)
        | ((u64) 0x8000 << 36);
    scratch[count++].reg = 0x50;
    scratch[count].data = 0;
    scratch[count++].reg = 0x51;
    scratch[count].data = ((u64) 448 << 32) | 0x200;
    scratch[count++].reg = 0x52;
    scratch[count].data = 2;
    scratch[count++].reg = 0x53;
    scratch[0].data += count - 1;
    /* sync: the GIF packet stores must reach memory before the DMA engine reads them */
    __asm__ volatile("sync" ::: "memory");
    FlushCache(0);
    xglDmaDirectNormal(2, (unsigned int) scratch, count);
}

void xglRenderCopyDraw2Flip(void)
{
    GsAdPair *scratch;
    int count;

    scratch = (GsAdPair *) SCRATCHPAD_BASE;
    count = 0;
    count = 0;
    scratch[count].data = 0x1000000000008000ULL;
    scratch[count++].reg = GIF_REG_AD;
    scratch[count].data = 0;
    scratch[count++].reg = 0x3F;
    scratch[count].data = ((u64) (sRenderFlipBase << 5) | 0x80000)
        | ((u64) sRenderPsm << 24)
        | ((u64) (sRenderFrameBase << 5) << 32)
        | ((u64) sRenderPsm << 56)
        | ((u64) 0x8000 << 36);
    scratch[count++].reg = 0x50;
    scratch[count].data = 0;
    scratch[count++].reg = 0x51;
    scratch[count].data = ((u64) 448 << 32) | 0x200;
    scratch[count++].reg = 0x52;
    scratch[count].data = 2;
    scratch[count++].reg = 0x53;
    scratch[0].data += count - 1;
    /* sync: the GIF packet stores must reach memory before the DMA engine reads them */
    __asm__ volatile("sync" ::: "memory");
    FlushCache(0);
    xglDmaDirectNormal(2, (unsigned int) scratch, count);
}


void xglRenderDrawFlipPk(XglPacket *packet)
{
    GsAdPair *scratch;
    int count;

    scratch = (GsAdPair *) SCRATCHPAD_BASE;
    count = 0;
    scratch[count].data = 0x1000000000008000ULL;
    scratch[count++].reg = GIF_REG_AD;
    scratch[count].data = 0;
    scratch[count++].reg = 0x3F;
    scratch[count].data = (u64) ((sRender.draw_buffer_base << 5) | 0x80000)
        | ((u64) sRender.reso.frame_psm << 24)
        | ((u64) (sRender.vram.flip_base << 5) << 32)
        | ((u64) sRender.reso.frame_psm << 56)
        | 0x0008000000000000ULL;
    scratch[count++].reg = 0x50;
    scratch[count].data = 0;
    scratch[count++].reg = 0x51;
    scratch[count].data = ((u64) 448 << 32) | 0x200;
    scratch[count++].reg = 0x52;
    scratch[count].data = 2;
    scratch[count++].reg = 0x53;
    scratch[0].data += count - 1;
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectHLCode(packet, 0);
    sceVif1PkAddDirectDataN(packet, scratch, count);
    sceVif1PkCloseDirectHLCode(packet);
}

extern int xglRenderPathBusyFlag[];
extern int xglRenderVSyncTickCount[];

int xglRenderVSyncCallback(int unused)
{
    void ADXM_ExecVint(int);

    VSyncCount = VSyncCount + 1;
    xglRenderVSyncTickCount[0] = xglRenderVSyncTickCount[0] + 1;
    if (VSyncCount >= (u32) xglRenderVSyncInterval &&
        ((*(u32 *) GIF_STAT_ADDRESS & GIF_STAT_APATH_MASK) != 0 ||
         (*(u32 *) VIF1_STAT_ADDRESS & VIF1_STAT_VPS_MASK) != 0)) {
        xglRenderPathBusyFlag[0] = 1;
    } else {
        xglRenderPathBusyFlag[0] = 0;
    }
    ADXM_ExecVint(0);
    /* sync: orders the GIF/VIF status reads and the flag store before the VSync interrupt returns */
    __asm__ volatile("sync" ::: "memory");
    /* ei: re-enables interrupts for the VSync interrupt context this callback runs in */
    __asm__ volatile("ei");
    return 0;
}

void xglRenderSyncInit(void)
{
    sceGsSyncVCallback(xglRenderVSyncCallback);
    VSyncCount = 0;
}

void xglRenderSyncMove(void)
{
    extern u16 sRenderDispfbBase;
    extern u16 sRenderPrevDispfbBase;
    extern int xglRenderFrameDelta[];
    void mwPlyExecServer(void);
    void sceGsResetPath(void);
    void sceGsSyncV(int);
    int sceGsSyncPath(int, int);
    int sceGsResetGraph(int, int, int, int);
    int pollCount;
    int pathBusy;
    u64 *packet;

    /* VSyncCount is advanced by the VSync interrupt, so every read of it is volatile. */
    if ((u32) xglRenderVSyncInterval < VSyncCount) {
        xglRenderFrameDelta[0] = 1;
    } else {
        xglRenderFrameDelta[0] = 0;
    }

    if (pxyTickScaleDouble == 1) {
        mwPlyExecServer();
        sceGsSyncPath(0, 0);
        return;
    }

    pathBusy = 0;
    pollCount = 0;
    while (pollCount < 0x80) {
        pathBusy = sceGsSyncPath(1, 0);
        if (pathBusy == 0 && VSyncCount >= (u32) xglRenderVSyncInterval) {
            break;
        }
        mwPlyExecServer();
        pollCount++;
    }

    if (pathBusy != 0) {
        *(u32 *) D_ENABLEW = 0x10000;
        *(u32 *) D1_CHCR = 0;
        *(u32 *) D2_CHCR = 0;
        *(u32 *) D_ENABLEW = 0;
        sceGsResetPath();
        sceGsResetGraph(1, 0, 0, 0);
        sceGsSyncPath(0, 0);

        packet = (u64 *) SCRATCHPAD_BASE;
        packet[0] = 0x1000000000008006ULL;
        packet[1] = GIF_REG_AD;
        packet[2] = 0;
        packet[3] = 0x3F;
        packet[4] = ((u64) (sRenderDispfbBase << 5) | 0x80000) | ((u64) sRenderPsm << 24)
            | ((u64) (sRenderPrevDispfbBase << 5) << 32) | ((u64) sRenderPsm << 56) | ((u64) 0x8000 << 36);
        packet[5] = 0x50;
        packet[6] = 0;
        packet[7] = 0x51;
        packet[8] = (0x1C0ULL << 32) | 0x200;
        packet[9] = 0x52;
        packet[10] = 2;
        packet[11] = 0x53;
        packet[12] = 0;
        packet[13] = 0x3F;

        /* sync: the GIF packet stores must reach memory before the DMA engine reads them */
        __asm__ volatile("sync" ::: "memory");
        FlushCache(0);
        xglDmaDirectNormal(2, (unsigned int) packet, 7);
        sceGsSyncPath(0, 0);
        sceGsSyncV(0);
    }

    VSyncCount = 0;
}

static void xglRenderDrawEnvInit(void)
{
    DrawEnv.value.gif_tag.nloop = 15;
    DrawEnv.value.gif_tag.eop = 1;
    DrawEnv.value.gif_tag.pre = 0;
    DrawEnv.value.gif_tag.prim = 0;
    DrawEnv.value.gif_tag.flg = 0;
    DrawEnv.value.gif_tag.nreg = 1;
    DrawEnv.value.gif_registers.regs0 = GIF_REG_AD;
    DrawEnv.value.frame1.fbp = sRender.vram.height;
    DrawEnv.value.frame1.fbw = sRender.reso.frame_width / 64;
    DrawEnv.value.frame1.psm = sRender.reso.frame_psm;
    DrawEnv.value.frame1.fbmsk = 0;
    DrawEnv.value.frame1_address = SCE_GS_FRAME_1;
    DrawEnv.value.zbuf1.zbp = sRender.vram.depth_base;
    DrawEnv.value.zbuf1.psm = sRender.reso.depth_psm;
    DrawEnv.value.zbuf1.zmsk = 0;
    DrawEnv.value.zbuf1_address = SCE_GS_ZBUF_1;
    DrawEnv.value.xyoffset1.ofx = (GS_COORD_CENTER - (sRender.reso.frame_width >> 1)) << 4;
    DrawEnv.value.xyoffset1.ofy = (GS_COORD_CENTER - (sRender.reso.frame_height >> 1)) << 4;
    DrawEnv.value.xyoffset1_address = SCE_GS_XYOFFSET_1;
    DrawEnv.value.scissor1.scax0 = 0;
    DrawEnv.value.scissor1.scax1 = sRender.reso.frame_width - 1;
    DrawEnv.value.scissor1.scay0 = 0;
    DrawEnv.value.scissor1.scay1 = sRender.reso.frame_height - 1;
    DrawEnv.value.scissor1_address = SCE_GS_SCISSOR_1;
    DrawEnv.value.test1.ate = 0;
    DrawEnv.value.test1.atst = 0;
    DrawEnv.value.test1.aref = 0;
    DrawEnv.value.test1.afail = 0;
    DrawEnv.value.test1.date = 0;
    DrawEnv.value.test1.datm = 0;
    DrawEnv.value.test1.zte = 1;
    DrawEnv.value.test1.ztst = GS_ZTST_GEQUAL;
    DrawEnv.value.test1_address = SCE_GS_TEST_1;
    DrawEnv.value.frame2.fbp = sRender.vram.height;
    DrawEnv.value.frame2.fbw = sRender.reso.frame_width / 64;
    DrawEnv.value.frame2.psm = sRender.reso.frame_psm;
    DrawEnv.value.frame2.fbmsk = 0;
    DrawEnv.value.frame2_address = SCE_GS_FRAME_2;
    DrawEnv.value.zbuf2.zbp = sRender.vram.depth_base;
    DrawEnv.value.zbuf2.psm = sRender.reso.depth_psm;
    DrawEnv.value.zbuf2.zmsk = 0;
    DrawEnv.value.zbuf2_address = SCE_GS_ZBUF_2;
    DrawEnv.value.xyoffset2.ofx = (GS_COORD_CENTER - (sRender.reso.frame_width >> 1)) << 4;
    DrawEnv.value.xyoffset2.ofy = (GS_COORD_CENTER - (sRender.reso.frame_height >> 1)) << 4;
    DrawEnv.value.xyoffset2_address = SCE_GS_XYOFFSET_2;
    DrawEnv.value.scissor2.scax0 = 0;
    DrawEnv.value.scissor2.scax1 = sRender.reso.frame_width - 1;
    DrawEnv.value.scissor2.scay0 = 0;
    DrawEnv.value.scissor2.scay1 = sRender.reso.frame_height - 1;
    DrawEnv.value.scissor2_address = SCE_GS_SCISSOR_2;
    DrawEnv.value.test2.ate = 0;
    DrawEnv.value.test2.atst = 0;
    DrawEnv.value.test2.aref = 0;
    DrawEnv.value.test2.afail = 0;
    DrawEnv.value.test2.date = 0;
    DrawEnv.value.test2.datm = 0;
    DrawEnv.value.test2.zte = 1;
    DrawEnv.value.test2.ztst = GS_ZTST_GEQUAL;
    DrawEnv.value.test2_address = SCE_GS_TEST_2;
    DrawEnv.value.prmodecont.ac = 1;
    DrawEnv.value.prmodecont_address = SCE_GS_PRMODECONT;
    DrawEnv.value.texa.ta0 = GS_ALPHA_OPAQUE;
    DrawEnv.value.texa.aem = 0;
    DrawEnv.value.texa.ta1 = GS_ALPHA_OPAQUE;
    DrawEnv.value.texa_address = SCE_GS_TEXA;
    DrawEnv.value.colclamp.clamp = 1;
    DrawEnv.value.colclamp_address = SCE_GS_COLCLAMP;
    DrawEnv.value.dthe.dthe = 0;
    DrawEnv.value.dthe_address = SCE_GS_DTHE;
    DrawEnv.value.dimx.dm00 = 4;
    DrawEnv.value.dimx.dm01 = 2;
    DrawEnv.value.dimx.dm02 = 5;
    DrawEnv.value.dimx.dm03 = 3;
    DrawEnv.value.dimx.dm10 = 0;
    DrawEnv.value.dimx.dm11 = 6;
    DrawEnv.value.dimx.dm12 = 1;
    DrawEnv.value.dimx.dm13 = 7;
    DrawEnv.value.dimx.dm20 = 5;
    DrawEnv.value.dimx.dm21 = 3;
    DrawEnv.value.dimx.dm22 = 4;
    DrawEnv.value.dimx.dm23 = 2;
    DrawEnv.value.dimx.dm30 = 1;
    DrawEnv.value.dimx.dm31 = 7;
    DrawEnv.value.dimx.dm32 = 0;
    DrawEnv.value.dimx.dm33 = 6;
    DrawEnv.value.dimx_address = SCE_GS_DIMX;
}

void xglRenderDrawEnvMove(void)
{
    DrawEnv.value.frame1.fbp = sRender.vram.height;
    DrawEnv.value.frame2.fbp = sRender.vram.height;
    DrawEnv.value.zbuf1.zbp = sRender.vram.depth_base;
    DrawEnv.value.zbuf2.zbp = sRender.vram.depth_base;
    SyncDCache(&DrawEnv.value, &DrawEnv.value + 1);
    sceGsPutDrawEnv(&DrawEnv.value);
}

void sceGsSetDefDispEnv(GsDispEnv *env, s16 psm, s16 width, s16 height, int dx, int dy);
/*
 * dispEnvDispfb1..dispEnvExtwrite are fields of DispEnv (0x00645740, GsDispEnv above):
 * dispfb1 (+0x10), display1 (+0x18), dispfb2 (+0x28), display2 (+0x30),
 * extbuf (+0x38), extdata (+0x40), extwrite (+0x48). dispEnvTailWork lies just past
 * the 0x50 bytes of GsDispEnv.
 */
extern GsDispfbRegister dispEnvDispfb1;
extern GsDisplayRegister dispEnvDisplay1;
extern GsDispfbRegister dispEnvDispfb2;
extern GsDisplayRegister dispEnvDisplay2;
extern u64 dispEnvExtbuf;
extern u64 dispEnvExtdata;
extern u64 dispEnvExtwrite;
extern u64 dispEnvTailWork[6];
extern s16 cameraScreenWidth;
extern s16 cameraScreenHeight;

void xglRenderDispEnvInit(void) {
    sceGsSetDefDispEnv(&DispEnv, sRenderPsm, cameraScreenWidth, cameraScreenHeight, 0, 0);
    DispEnv.flags &= ~2;
    DispEnv.flags &= ~0x40;
    dispEnvTailWork[0] = dispEnvDisplay1.value;
    dispEnvDispfb2.value = dispEnvDispfb1.value;
    dispEnvDisplay2.value = dispEnvDisplay1.value;
    dispEnvExtbuf = 0;
    dispEnvExtdata = 0;
    dispEnvExtwrite = 0;
}

extern u64 dispEnvCacheEnd[];
extern u16 sRenderDispfbBase;
void sceGsPutDispEnv(GsDispEnv *env);

void xglRenderDispEnvMove(void)
{
    dispEnvDispfb2.field.fbp = sRenderDispfbBase;
    dispEnvDispfb1.field.fbp = sRenderDispfbBase;
    switch (ScanLineInterpolate) {
    case 0:
        DispEnv.pmode.en1 = 0;
        break;
    case 1:
        dispEnvDispfb2.field.fbp = sRenderDispfbBase;
        DispEnv.pmode.en1 = DispEnv.pmode.en2;
        DispEnv.pmode.alp = 0x80;
        dispEnvDisplay2.field.dx = dispEnvDisplay1.field.dx;
        dispEnvDisplay2.field.dy = dispEnvDisplay1.field.dy - 1;
        break;
    case 2:
        break;
    }
    SyncDCache(&DispEnv, &dispEnvCacheEnd);
    sceGsPutDispEnv(&DispEnv);
    *(volatile u64 *) GS_DISPFB1 = dispEnvDispfb2.value;
    *(volatile u64 *) GS_DISPLAY1 = dispEnvDisplay2.value;
    *(volatile u64 *) GS_EXTBUF = dispEnvExtbuf;
    *(volatile u64 *) GS_EXTDATA = dispEnvExtdata;
    *(volatile u64 *) GS_EXTWRITE = dispEnvExtwrite;
    *(volatile u64 *) GS_CSR = 16;
}

void xglRenderClearDepth(void)
{
    ClearEnv.value.target = GS_TEST_CLEAR_DEPTH;
}

void xglRenderClearFrame(void)
{
    ClearEnv.value.target = GS_TEST_CLEAR_FRAME;
}

void xglRenderClearColor(u32 color)
{
    ClearEnv.value.color_r = color & 0xff;
    ClearEnv.value.color_g = (color >> 8) & 0xff;
    ClearEnv.value.color_b = (color >> 16) & 0xff;
    ClearEnv.value.color_a = (color >> 24) & 0xff;
}

void xglRenderClearEnvInit(void)
{
    ClearEnv.value.gif_tag_low = 0x8001;
    ClearEnv.value.gif_tag_high = 0x50034000;
    ClearEnv.value.gif_registers_low = RENDER_CLEAR_GIF_REGS | (GIF_REG_AD << 16);
    ClearEnv.value.gif_registers_high = 0;
    xglRenderClearFrame();
    ClearEnv.value.target_address = SCE_GS_TEST_1;
    xglRenderClearColor(RENDER_CLEAR_COLOR_DEFAULT);
    ClearEnv.value.left = (GS_COORD_CENTER - (s16) (cameraScreenWidth >> 1)) << 4;
    ClearEnv.value.top = (GS_COORD_CENTER - (s16) (cameraScreenHeight >> 1)) << 4;
    ClearEnv.value.right = (GS_COORD_CENTER + (s16) (cameraScreenWidth >> 1)) << 4;
    ClearEnv.value.bottom = (GS_COORD_CENTER + (s16) (cameraScreenHeight >> 1)) << 4;
    ClearEnv.value.restore_target = GS_TEST_ZTE_GEQUAL;
    ClearEnv.value.restore_target_address = SCE_GS_TEST_1;
}

void xglRenderClearEnvMove(void)
{
    SyncDCache(&ClearEnv.value, &ClearEnv.value + 1);
    xglDmaDirectNormal(XGL_DMA_CHANNEL_GIF, (u32) &ClearEnv.value, RENDER_CLEAR_QWORDS);
}

void xglRenderSetReso(int resolution)
{
    sRender.reso = asDispReso[resolution];
    sRender.vram = asVramBase[resolution];
    sRender.display_buffer_base = sRender.vram.width;
    sRender.draw_buffer_base = sRender.vram.height;
    xglRenderDrawEnvInit();
    xglRenderDispEnvInit();
    xglRenderClearEnvInit();
    xglStudioInit();
    xglStudioMainCameraInit();
}

extern s16 sRenderPrevDispfbBase;
extern int xglRenderStatus[];
extern int xglRenderFrameDelta[];
extern int xglRenderFrameCount[];

void xglRenderSwapBase(void)
{
    u16 previous_display_base;

    previous_display_base = sRenderDisplayBase;
    sRenderPrevDispfbBase = sRenderDispfbBase;
    sRenderDispfbBase = sRenderFlipBase;
    sRenderDisplayBase = sRenderFlipBase;
    sRenderFlipBase = previous_display_base;
    xglRenderStatus[0] = 0;
    xglRenderFrameCount[0] += xglRenderFrameDelta[0];
}

void xglRenderDispOff(void)
{
    DispEnv.flags &= ~(u64) GS_PMODE_EN2;
}

void xglRenderDispOn(void)
{
    DispEnv.flags |= GS_PMODE_EN2;
}

/* xglRenderClearSkip: written 1 by xglRenderClearSkipOn and 0 by xglRenderClearSkipOff; referenced by xglRenderInit and xglRenderMove. */

void xglRenderClearSkipOn(void) {
    xglRenderClearSkip = 1;
}

void xglRenderClearSkipOff(void) {
    xglRenderClearSkip = 0;
}

extern int sceGsResetGraph(int, int, int, int);
extern int xglRenderSkippedFrameCount[];
extern unsigned char pxyGlyphDigitTable[252];
void sceGsResetPath(void);
void sceGsSyncV(int);

void xglRenderInit(void)
{
    int i;

    sceGsResetPath();
    *(volatile u32 *) VIF0_ERR = 0;
    *(volatile u32 *) VIF1_ERR = 0;
    sceGsSyncV(0);
    sceGsResetGraph(0, 1, 2, 0);
    xglRenderSyncInit();
    xglRenderSetReso(1);
    xglRenderMoveCount = 0;
    LoopCount = 0;
    xglRenderVSyncInterval = 2;
    ScanLineInterpolate = 1;
    xglRenderStatus[0] = 0;
    xglRenderFrameCount[0] = 0;
    xglRenderFrameDelta[0] = 0;
    xglRenderPathBusyFlag[0] = 0;
    xglRenderVSyncTickCount[0] = 0;
    xglRenderSkippedFrameCount[0] = 0;
    pxyGlyphDigitTable[0] = 0;
    for (i = 0; i < 17; i++) {
        sRender.final_packet_callback[i] = 0;
    }
    xglRenderGlobalFadeInit();
    nmlModelConstruct();
    xglRenderCpUDispOff();
    xglIntcEnable(5);
    xglRenderClearSkip = 0;
}


void xglRenderFinalPacket(void)
{
    XglPacket *packet;
    int i;

    packet = xglPacketGetCurrent();
    for (i = 0; i < 17; i++) {
        if (sRender.final_packet_callback[i] != 0) {
            sRender.final_packet_callback[i](packet);
        }
    }
}

/* xglRenderGlobalFadeCallback: cleared by xglRenderGlobalFadeInit; referenced by xglRenderGlobalFade (the fade draw). */
extern int xglRenderGlobalFadeCallback[];
/* xglRenderGlobalFadeColor: packed fade colour, one byte per channel as 0xAABBGGRR; written by xglRenderGlobalFadeSet. */

void xglRenderGlobalFadeInit(void) {
    xglRenderGlobalFadeColor = 0;
    xglRenderGlobalFadeCallback[0] = 0;
}

int xglRenderGlobalFadeGet(void) {
    return xglRenderGlobalFadeColor;
}

void xglRenderGlobalFadeSetPacked(int value)
{
    xglRenderGlobalFadeColor = value;
}

void xglRenderGlobalFadeSet(float r, float g, float b, float a)
{
    if (r < 0.0f) {
        r = 0.0f;
    }
    if (g < 0.0f) {
        g = 0.0f;
    }
    if (b < 0.0f) {
        b = 0.0f;
    }
    if (a < 0.0f) {
        a = 0.0f;
    }
    if (1.0f < r) {
        r = 1.0f;
    }
    if (1.0f < g) {
        g = 1.0f;
    }
    if (1.0f < b) {
        b = 1.0f;
    }
    if (1.0f < a) {
        a = 1.0f;
    }
    xglRenderGlobalFadeColor = ((int) (r * 255.0f) & 0xff) + (((int) (g * 255.0f) & 0xff) << 8) + (((int) (b * 255.0f) & 0xff) << 16) + (((int) (a * 128.0f) & 0xff) << 24);
}

void xglRenderGlobalFade(void)
{
    int alpha;
    XglPacket *packet;

    alpha = (int) (((unsigned int) xglRenderGlobalFadeColor >> 24) & 0xff);
    packet = xglPacketGetCurrent();
    if (xglRenderGlobalFadeCallback[0] != 0) {
        ((XglRenderFadeCallback) xglRenderGlobalFadeCallback[0])(packet);
    }
    if (alpha != 0 && xglRenderGlobalFadeColor != 0) {
        GsAdPair *scratch;
        int count;
        int i;

        scratch = (GsAdPair *) SCRATCHPAD_BASE;
        count = 0;
        scratch[count].data = 0x1000000000008000ULL;
        scratch[count++].reg = GIF_REG_AD;
        scratch[count].data = ((u64) alpha << 32) | 0x64;
        scratch[count++].reg = 0x42;
        scratch[count].data = 0x30000;
        scratch[count++].reg = SCE_GS_TEST_1;
        scratch[count].data = ((u64) ((unsigned int) xglRenderGlobalFadeColor & 0xff))
            | ((u64) (((unsigned int) xglRenderGlobalFadeColor >> 8) & 0xff) << 8)
            | ((u64) (((unsigned int) xglRenderGlobalFadeColor >> 16) & 0xff) << 16)
            | (0xFE00ULL << 46);
        scratch[count++].reg = GIF_REG_RGBAQ;
        scratch[count].data = 0x46;
        scratch[count++].reg = 0;
        scratch[count].data = 0x72007000ULL;
        scratch[count++].reg = 4;
        scratch[count].data = 0x8E009000ULL;
        scratch[count++].reg = 4;
        for (i = 0; i < 80; i++) {
            scratch[count].data = 0x68;
            scratch[count++].reg = 0x42;
        }
        scratch[0].data += count - 1;
        sceVif1PkCnt(packet, 0);
        sceVif1PkOpenDirectHLCode(packet, 0);
        sceVif1PkAddDirectDataN(packet, scratch, count);
        sceVif1PkCloseDirectHLCode(packet);
    } else {
        GsAdPair *scratch;
        int count;
        int i;

        scratch = (GsAdPair *) SCRATCHPAD_BASE;
        count = 0;
        scratch[count].data = 0x1000000000008000ULL;
        scratch[count++].reg = GIF_REG_AD;
        scratch[count].data = 0x68;
        scratch[count++].reg = 0x42;
        scratch[count].data = 0x30000;
        scratch[count++].reg = SCE_GS_TEST_1;
        scratch[count].data = 0x3F80000080808500ULL;
        scratch[count++].reg = GIF_REG_RGBAQ;
        scratch[count].data = 0x46;
        scratch[count++].reg = 0;
        scratch[count].data = 0x72007000ULL;
        scratch[count++].reg = 4;
        scratch[count].data = 0x8E009000ULL;
        scratch[count++].reg = 4;
        for (i = 0; i < 80; i++) {
            scratch[count].data = 0x68;
            scratch[count++].reg = 0x42;
        }
        scratch[0].data += count - 1;
        sceVif1PkCnt(packet, 0);
        sceVif1PkOpenDirectHLCode(packet, 0);
        sceVif1PkAddDirectDataN(packet, scratch, count);
        sceVif1PkCloseDirectHLCode(packet);
    }
}

void xglRenderFinalPacket(void);
void xglRenderSyncMove(void);
void xglRenderSwapBase(void);
void xglRenderDrawEnvMove(void);
void xglRenderDispEnvMove(void);
void xglRenderClearEnvMove(void);

void xglRenderMove(void)
{
            extern int xglRenderMoveCount;
    extern int LoopCount;

    xglRenderFrameBeginFlushEIrq();
    if (pxyGlyphDigitTable[0] == 0) {
        sefDrawEffect();
    }
    if (pxyGlyphDigitTable[0] == 0) {
        pxyFlush();
    } else {
        nmlModelFlushClear();
    }
    gifStateUpdateDrawHook();
    if (pxyGlyphDigitTable[0] == 0) {
        xglRenderFinalPacket();
    }
    xglFontFlush();
    if (pxyGlyphDigitTable[0] == 0) {
        xglRenderGlobalFade();
    }
    if (ScanLineInterpolate == 2 && pxyGlyphDigitTable[0] == 0) {
        xglPacketInterpolate();
    }
    xglRenderFrameEndFlushEIrq();
    xglPacketMove();
    xglRenderSyncMove();
    xglRenderMoveCount++;
    LoopCount++;
    sceGsResetGraph(1, 0, 0, 0);
    xglRenderSwapBase();
    xglRenderDrawEnvMove();
    xglRenderDispEnvMove();
    if (xglRenderClearSkip == 0) {
        xglRenderClearEnvMove();
    }
    xglPacketSend();
    xglRenderSkippedFrameCount[0] = xglRenderVSyncTickCount[0] / 2 - 1;
    if (xglRenderSkippedFrameCount[0] < 0) {
        xglRenderSkippedFrameCount[0] = 0;
    }
    xglRenderVSyncTickCount[0] = 0;
    pxyBeginFrame();
}


void xglRenderAndMovieInit(void) {
    xglRenderInit();
    moviePlayerLibraryInit();
}

void xglRenderMove(void);

void xglRenderFrame(void) {
    xglRenderMove();
    soundApplySettings();
    xglPadRead();
}

/* xgl_render's .data, 0x645580-0x645874, in address order. */
XglRenderReso asDispReso[RENDER_COUNT_RESO] = {
    { 0x31, 0x0, 640, 448 },
    { 0x31, 0x0, 512, 448 },
    { 0x31, 0x0, 448, 448 },
    { 0x31, 0x0, 384, 448 },
    { 0x31, 0x0, 320, 448 },
    { 0x31, 0x0, 256, 448 }
};
XglRenderVram asVramBase[RENDER_COUNT_RESO] = {
    { 0x0, { 0x8C, 0x118, 0x1A4 }, 0x8C, 0x118, 0x1A4, { 0x230, 0x2BC }, 0x1A4, { 0x1C4, 0x1E4 } },
    { 0x0, { 0x70, 0xE0, 0x150 }, 0x70, 0xE0, 0x150, { 0x1C0, 0x230 }, 0x1C0, { 0x1E0, 0x200 } },
    { 0x0, { 0x62, 0xC4, 0x126 }, 0x62, 0xC4, 0x126, { 0x188, 0x1EA }, 0x188, { 0x1A8, 0x1C8 } },
    { 0x0, { 0x54, 0xA8, 0xFC }, 0x54, 0xA8, 0xFC, { 0x150, 0x1A4 }, 0x1A4, { 0x1C4, 0x1E4 } },
    { 0x0, { 0x46, 0x8C, 0xD2 }, 0x46, 0x8C, 0xD2, { 0x118, 0x15E }, 0x1A4, { 0x1C4, 0x1E4 } },
    { 0x0, { 0x38, 0x70, 0xA8 }, 0x38, 0x70, 0xA8, { 0xE0, 0x118 }, 0x150, { 0x170, 0x190 } }
};
GsDrawPacket DrawEnv = { 0 };
GsDispEnv DispEnv = { 0 };
/* Six doublewords after DispEnv (0x645790-0x6457C0); dispEnvCacheEnd is the second. */
u64 dispEnvTailWork[6] = { 0 };
GsClearPacket ClearEnv = { 0 };
XglRenderState sRender = { 0 };

/* .data of this file after its own (0x645874-0x6459A0): hooks and render state read by several files. */
void (*pxyEntryGroupHooks[])(int) = { 0 }; /* called with the entry group */

void (*pxyFrameCopyHooks[])(XglPacket *) = { 0 };

void (*msgFrameHooks[])(void) = { 0 };

void (*gifStateEffectHook[])(XglPacket *) = { 0 };

void (*pauseDrawHook[])(XglPacket *) = { 0 };

int xglRenderGlobalFadeCallback[] = { 0 };

int xglRenderStatus[] = { 0 };

int xglRenderFrameDelta[] = { 0 };

int xglRenderFrameCount[] = { 0 };

int xglRenderPathBusyFlag[] = { 0 };

int xglRenderVSyncTickCount[] = { 0 };

int xglRenderSkippedFrameCount[] = { 0 };

unsigned char pxyGlyphDigitTable[252] = {
    0x00, 0x00, 0x00, 0x00, 0x07, 0x01, 0x02, 0x01, 0x06, 0x02, 0x07, 0x04,
    0x07, 0x05, 0x06, 0x05, 0x02, 0x04, 0x01, 0x02, 0x01, 0x01, 0x02, 0xfe,
    0xfe, 0x05, 0x02, 0x01, 0x06, 0x05, 0x02, 0xff, 0x00, 0x00, 0x00, 0x00,
    0x07, 0x02, 0x02, 0x03, 0x01, 0x03, 0x07, 0x03, 0x01, 0xff, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x07, 0x01, 0x02, 0x02, 0x01, 0x04, 0x01, 0x05,
    0x02, 0x05, 0x03, 0x01, 0x07, 0x05, 0x07, 0x01, 0x07, 0xff, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x07, 0x01, 0x02, 0x02, 0x01, 0x04, 0x01, 0x05,
    0x02, 0x05, 0x03, 0x04, 0x04, 0x02, 0x04, 0x04, 0x04, 0x05, 0x05, 0x05,
    0x06, 0x04, 0x07, 0x02, 0x07, 0x01, 0x06, 0x02, 0x07, 0xff, 0x00, 0x00,
    0x07, 0x04, 0x07, 0x04, 0x01, 0x01, 0x06, 0x05, 0x06, 0x01, 0x06, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x07, 0x05, 0x01, 0x01, 0x01, 0x01, 0x04, 0x04,
    0x04, 0x05, 0x05, 0x05, 0x06, 0x04, 0x07, 0x02, 0x07, 0x01, 0x06, 0x02,
    0x07, 0xff, 0x00, 0x00, 0x07, 0x05, 0x02, 0x04, 0x01, 0x02, 0x01, 0x01,
    0x02, 0x01, 0x06, 0x02, 0x07, 0x04, 0x07, 0x05, 0x06, 0x05, 0x05, 0x04,
    0x04, 0x01, 0x04, 0xff, 0x07, 0x01, 0x01, 0x05, 0x01, 0x02, 0x07, 0x05,
    0x01, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x02, 0x01, 0x04,
    0x01, 0x05, 0x02, 0x05, 0x03, 0x04, 0x04, 0x02, 0x04, 0x01, 0x05, 0x01,
    0x06, 0x02, 0x07, 0x04, 0x07, 0x05, 0x06, 0x05, 0x05, 0x04, 0x04, 0x02,
    0x04, 0x01, 0x03, 0x02, 0x03, 0x02, 0x01, 0xff, 0x00, 0x00, 0x00, 0x00,
    0x07, 0x01, 0x06, 0x02, 0x07, 0x04, 0x07, 0x05, 0x06, 0x05, 0x02, 0x04,
    0x01, 0x02, 0x01, 0x01, 0x02, 0x01, 0x03, 0x02, 0x04, 0x05, 0x04, 0xff,
};
