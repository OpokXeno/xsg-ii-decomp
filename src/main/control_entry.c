#include "common.h"
#include "main/control_entry.h"
#include "main/xgl_render.h"
#include "main/xgl_studio.h"

INCLUDE_ASM("asm/main/nonmatchings/control_entry", InitializeSystem);

void InitializeSystem(void);
void xglCdOpenDevice(void);
void xglRenderAndMovieInit(void);
void xglCdControlThread(void);
void xglStudioSetup(void);
void xglRenderDispOn(void);
void xglCdLoadOverlay(int overlay);
int seBankLoad(int slot, const char *name, int option);

/* ov02 (the title overlay): an empty function, the logo screen and the title screen. */
void func_00A80790(void);
void LogoFirst(void);
unsigned int Title(void);

extern int mainParamArgc;
extern char **mainParamArgv;
extern const char soundEffectBankName[];

/* Overlay numbers for xglCdLoadOverlay. */
#define OVERLAY_TITLE 2

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

#include "shared.h"

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
