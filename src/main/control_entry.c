#include "common.h"
#include "shared.h"
#include "main/control_entry.h"
#include "main/xgl_render.h"
#include "main/xgl_studio.h"
#include "main/tu021.h"
#include "main/tu022.h"
#include "main/tu024.h"
#include "main/xgl_cd.h"

/* First address of the work heap WorkEnd tracks. */
#define WORK_HEAP_START 0xB00000

/* sceCdMmode media: DVD. */
#define SCECdDVD 2

/* Packet buffer of xglPacketSystemInit: base, size and rate. */
#define PACKET_BUFFER_BASE 0xC00000
#define PACKET_BUFFER_SIZE 0x400000
#define PACKET_RATE 50.0f

extern u8 *WorkEnd;

extern const char iopRebootImagePath[];
extern char sio2manModuleName[];
extern char mcmanModuleName[];
extern char mcservModuleName[];
extern char padmanModuleName[];
extern char cdvdstmModuleName[];
extern char libsdModuleName[];
extern const char adxiModuleName[];
extern const char adxiModuleArgs[];
extern const char korgModuleName[];
extern const char korgModuleArgs[];

/* SCE SDK entry points that bring up the IOP and its file system. */
void sceSifInitRpc(unsigned int mode);
int sceCdInit(int init_mode);
int sceSifRebootIop(const char *image);
int sceSifSyncIop(void);
int sceSifInitIopHeap(void);
int sceSifLoadFileReset(void);
int sceCdMmode(int media);
int sceFsReset(void);

void xglPadInitial(void);
void xglMcInitial(void);
void *xglTaskInitial(void *manager, int capacity, int reset_gs);
void xglDmaInitial(void);
void xglGeometryInit(void);
void xglPacketSystemInit(u32 buffer, u32 size, float rate);
void xglRenderInit(void);
void xglFontInitial(void);
void xglMovieInit(void);
void xglMenuInitial(void);

/*
 * Reboots the IOP with IOPRP271, loads the SIF/CD/memory-card/pad/sound IOP
 * modules (XS2 adds cdvdstm, the CRI ADX driver and the KORG VAG driver and
 * drops XS1's ssd/rssd), then initialises every xgl subsystem.
 */
void InitializeSystem(void) {
    sceSifInitRpc(0);
    sceCdInit(0);
    while (sceSifRebootIop(iopRebootImagePath) == 0) {
    }
    while (sceSifSyncIop() == 0) {
    }
    sceSifInitRpc(0);
    sceSifInitIopHeap();
    sceSifLoadFileReset();
    sceCdInit(0);
    sceCdMmode(SCECdDVD);
    sceFsReset();
    xglCdSifLoadModule(sio2manModuleName, 0);
    xglCdSifLoadModule(mcmanModuleName, 0);
    xglCdSifLoadModule(mcservModuleName, 0);
    xglCdSifLoadModule(padmanModuleName, 0);
    xglCdSifLoadModule(cdvdstmModuleName, 0);
    xglCdSifLoadModule(libsdModuleName, 0);
    xglCdSifLoadModule(adxiModuleName, adxiModuleArgs);
    xglCdSifLoadModule(korgModuleName, korgModuleArgs);
    WorkEnd = (u8 *)WORK_HEAP_START;
    xglCdInitial();
    soundDriverInit();
    xglPadInitial();
    xglMcInitial();
    xglTaskInitial(0, 0, 0);
    xglDmaInitial();
    xglIntrInit();
    xglGeometryInit();
    xglPacketSystemInit(PACKET_BUFFER_BASE, PACKET_BUFFER_SIZE, PACKET_RATE);
    xglRenderInit();
    xglFontInitial();
    xglMovieInit();
    xglMenuInitial();
}

void xglRenderAndMovieInit(void);
void xglStudioSetup(void);
void xglRenderDispOn(void);

/* ov02 (the title overlay): an empty function, the logo screen and the title screen. */
void func_00A80790(void);
void LogoFirst(void);
unsigned int Title(void);

extern int mainParamArgc;
extern char **mainParamArgv;
extern const char soundEffectBankName[];

/* Title's result: the low 24 bits are the entry address of the chosen screen, the top byte its overlay. */
#define TITLE_ENTRY_MASK 0xFFFFFF
#define TITLE_OVERLAY_SHIFT 24

/* Sound-bank slot and size of the system sound effects. */
#define SYSTEM_SE_SLOT 1
#define SYSTEM_SE_SIZE 0x5000

/*
 * Entry point: stores argc and argv in mainParamArgc and mainParamArgv, runs
 * InitializeSystem and the device, render and studio setup, loads the system
 * sound effects, shows the boot logo, then repeatedly runs the ov02 title
 * screen: while it returns 0 or its own entry address it reloads ov02 and
 * calls it again; otherwise the low 24 bits are the chosen screen's entry
 * address and the top byte is the overlay to load before jumping to it.
 * Never returns.
 *
 * The compiler emits the call to __main (running static/global constructors)
 * at entry to any function literally named main, as in XS1.
 */
int main(int argc, char **argv) {
    unsigned int selection;
    unsigned int (*entry)(void);

    mainParamArgc = argc;
    mainParamArgv = argv;
    func_00A80790();
    InitializeSystem();
    xglCdOpenDevice();
    xglRenderAndMovieInit();
    xglRenderFrame();
    xglCdControlThread();
    xglStudioSetup();
    xglStudioUpdate();
    xglRenderDispOn();
    seBankLoad(SYSTEM_SE_SLOT, soundEffectBankName, SYSTEM_SE_SIZE);
    xglCdLoadOverlay(OVERLAY_TITLE);
    LogoFirst();

    for (;;) {
        xglCdLoadOverlay(OVERLAY_TITLE);
        selection = Title();
        if (selection == 0) {
            continue;
        }
        entry = (unsigned int (*)(void))(selection & TITLE_ENTRY_MASK);
        if (entry == Title) {
            continue;
        }
        xglCdLoadOverlay(selection >> TITLE_OVERLAY_SHIFT);
        entry();
    }
}


void xglSleep(void) {
    xglRenderFrame();
    xglCdControlThread();
    xglStudioUpdate();
}

/*
 * EE Timer 0.  T0_COUNT is the free-running counter and T0_MODE its control
 * word; bit 7 (0x80) of T0_MODE is CUE, which starts the count.
 */
#define T0_COUNT ((volatile u32 *)0x10000000)
#define T0_MODE ((volatile u32 *)0x10000010)
#define T0_MODE_CUE 0x80

void xglTimer0Reset(int mode) {
    *T0_COUNT = 0;
    *T0_MODE = mode + T0_MODE_CUE;
}
