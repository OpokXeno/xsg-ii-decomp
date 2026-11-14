#include "common.h"
#include "shared.h"
#include "main/TiGlobalColl.h"
#include "main/cfePlugIn.h"
#include "main/tu022.h"
#include "main/tu146.h"
#include "main/xgl_studio.h"

int GameResBufferAlloc(int);
void cfeNodePoolSet(void *);

void cfeNodePoolInit(void) {
    cfeNodePoolSet(GameResBufferAlloc(0x48000));
    cfeNodePoolClear();
}

INCLUDE_ASM("asm/main/nonmatchings/tu141", cfeEvalRecordsInit);

void *memset(void *, int, unsigned int);
int xglCameraSetPadPort(void *, int);
void *xglStudioGetCamera2(int);
void xglStudioInit(void);
void xglStudioMainCameraInit(void);
extern void *D_006384C0;

/* Studio setup: clears the 0x70-byte block at D_006384C0, starts xglStudio and the main camera,
   then keeps the object xglStudioGetCamera2 creates there (its word 1 is set to 2). */
void cfeStudioInit(void) {
    memset(&D_006384C0, 0, 0x70);
    xglStudioInit();
    xglStudioChange(0);
    xglStudioMainCameraInit();
    D_006384C0 = xglStudioGetCamera2(0);
    ((int *)D_006384C0)[1] = 2;
    xglCameraSetPadPort(D_006384C0, 1);
}

extern int cfeCommandCount[];
extern unsigned char cfeCommands[];

void cfeCommandTableInit(void) {
    int *entry;
    int i;

    cfeCommandCount[0] = 0;
    memset(cfeCommands, 0, 0x2000);
    entry = (int *)(cfeCommands + 0x1FC8);
    for (i = 0; i < 64; i++) {
        *entry = -1;
        entry -= 0x20;
    }
}

INCLUDE_ASM("asm/main/nonmatchings/tu141", func_0024EB48);

INCLUDE_ASM("asm/main/nonmatchings/tu141", func_0024ECA0);

extern int cfeGroupCount;
extern unsigned char cfeGroups[];

void cfeGroupTableInit(void) {
    memset(cfeGroups, 0, 0x300);
    cfeGroupCount = 0;
}

extern int cfePlugInRecordCount;

void cfePlugInRecordCountReset(void) {
    cfePlugInRecordCount = 0;
}

INCLUDE_ASM("asm/main/nonmatchings/tu141", cfeEvalGlobalInit);

INCLUDE_ASM("asm/main/nonmatchings/tu141", cfeEvalEntriesInit);

/* Resets the three CfeEvalSlot entries of cfeEvalSlots (cfeEvaluate.h): result 0, sample 0x80, sample_float 1.0. */
extern unsigned char cfeEvalSlots[];

void cfeEvalSlotsInit(void) {
    u8 *slot;
    int i;

    do { memset(cfeEvalSlots, 0, 0x48); slot = cfeEvalSlots; } while (0);
    for (i = 0; i < 3; i++) {
        ((int *)slot)[0] = 0;
        ((int *)slot)[1] = 0x80;
        ((f32 *)slot)[2] = 1.0f;
        slot += 0x18;
    }
}


/* Empties the text table: count cfeSndRecordCount = 0 and the first 0x50-byte text record of cfeSndRecords
   (cfePlugIn appends records of that size and bumps the count). */
void cfePlugInTextReset(void) {
    int *field;

    cfeSndRecordCount = 0;
    memset(cfeSndRecords, 0, sizeof(CfeSndRecord));
    field = &cfeSndRecords[0].duration;
    field[-1] = 0x7F;
    field[0] = 0;
}

int cfeEvalRecordsInit(void);
int func_0024ECA0(void);
int cfeEvalGlobalInit(void);
int cfeEvalEntriesInit(void);

/* Module init: calls two helpers, then each table initializer of this file in order. */
void cfeInit(void) {
    func_001A4010();
    tiGlobalCollSetHandle(0);
    cfeNodePoolInit();
    cfeGroupTableInit();
    cfeEvalRecordsInit();
    cfeStudioInit();
    cfeCommandTableInit();
    func_0024ECA0();
    cfePlugInRecordCountReset();
    cfeEvalGlobalInit();
    cfeEvalEntriesInit();
    cfeEvalSlotsInit();
    cfePlugInTextReset();
}

int sceLseek(int, int, int);
int sceOpen(const char *, int, ...);

/* Opens `path` for writing, creating it (O_WRONLY | O_CREAT = 0x202, mode 0666 = 0x1B6). */
int cfeFileOpenWrite(const char *path) {
    int fd;

    fd = sceOpen(path, 0x202, 0x1B6);
    if (fd >= 0) {
        sceLseek(fd, 0, 0);
    }
    return fd;
}

void func_0024F090(const char *path) {
    sceOpen(path, 1);
}

int sceWrite(int, const void *, int);
int sceRead(int, void *, int);

/* Thin wrappers over the SDK file calls. */
int cfeFileWrite(int fd, const void *buf, int size) {
    return sceWrite(fd, buf, size);
}

int cfeFileRead(int fd, void *buf, int size) {
    return sceRead(fd, buf, size);
}
