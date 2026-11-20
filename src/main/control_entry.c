#include "common.h"
#include "main/xgl_mc.h"
#include "shared.h"
#include "main/xgl_thread.h"
#include "main/xgl_render.h"
#include "main/xgl_studio.h"
#include "main/movie_player.h"
#include "main/se_bank_load.h"
#include "main/sound_driver.h"
#include "main/xgl_dma_handler.h"
#include "main/xgl_cd.h"
#include "main/xgl_menu.h"
#include "main/xgl_task.h"
#include "main/xgl_pad.h"
#include "main/xgl_dma.h"
#include "main/control_entry.h"
#include "main/xgl_font.h"
#include "ov02/logo.h"
#include "ov02/boot_display.h"
#include "ov02/title.h"
#include "main/xgl_packet.h"

#include "main/xgl_2.h"
/* First address of the work heap WorkEnd tracks. */
#define WORK_HEAP_START 0xB00000

/* sceCdMmode media: DVD. */
#define SCECdDVD 2

/* Packet buffer of xglPacketInit: base, size and rate. */
#define PACKET_BUFFER_BASE 0xC00000
#define PACKET_BUFFER_SIZE 0x400000
#define PACKET_RATE 50.0f


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
    xglPacketInit(PACKET_BUFFER_BASE, PACKET_BUFFER_SIZE, PACKET_RATE);
    xglRenderInit();
    xglFontInitial();
    xglMovieInit();
    xglMenuInitial();
}


/* ov02 (the title overlay): an empty function, the logo screen and the title screen. */

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
    BootDisplay();
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

const char iopRebootImagePath[32] = "cdrom0:\\IOP\\IOPRP271.IMG;1";

const char adxiModuleName[16] = "cri_adxi";

const char adxiModuleArgs[16] = "spucore=1";

const char korgModuleName[16] = "korgivag";

const char korgModuleArgs[48] = "vag=256\000key=256\000prg=64\000sbf=128\000seq=64\000ply=16";

char sio2manModuleName[8] = "sio2man";

char mcmanModuleName[8] = "mcman";

char mcservModuleName[8] = "mcserv";

char padmanModuleName[8] = "padman";

char cdvdstmModuleName[8] = "cdvdstm";

char libsdModuleName[8] = "libsd";

const char soundEffectBankName[8] = "SYS_SE";

u8 *WorkEnd = 0;

int mainParamArgc = 0;

char **mainParamArgv = 0;

signed char uwamonoDrawDisabled = 1;
