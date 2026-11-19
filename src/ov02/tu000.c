#include "common.h"
#include "shared.h"
#include "main/control_entry.h"
#include "main/xgl_font.h"
#include "main/xgl_jpeg.h"

/* Memory-card status and request codes read by logoMcCheck. */
#define LOGO_MC_STATUS_READY 1
#define LOGO_MC_STATUS_UNFORMATTED 2
#define LOGO_MC_STATUS_ERROR 0x80
#define LOGO_MC_STATUS_BUSY 0xff
#define LOGO_MC_REQUEST_FREE_SPACE 1
#define LOGO_MC_REQUEST_DIRECTORY 2
/* Free space in KB the save data needs: the capacity text reads "requires 34KB of free space". */
#define LOGO_SAVE_MIN_FREE_KB 34
/* Position of the card messages and the number of frames the card state settles for. */
#define LOGO_MESSAGE_X 16
#define LOGO_MESSAGE_Y 64
#define LOGO_CARD_SETTLE_FRAMES 8
/* Sound ids: CIRCLE plays the decide sound, CROSS the cancel sound. */
#define LOGO_SE_DECIDE 0x1000002
#define LOGO_SE_CANCEL 0x1000008
#define LOGO_CLEAR_COLOR_BLACK 0x80000000
/* credit.bin is loaded at LOGO_CREDIT_FILE_BASE, its first words are the band table; decoded bands go to
 * LOGO_CREDIT_BAND_BASE, one LOGO_CREDIT_BAND_STRIDE apart. */
#define LOGO_CREDIT_FILE_BASE 0x01000000
#define LOGO_CREDIT_BAND_BASE 0x01100000
#define LOGO_CREDIT_BAND_STRIDE 0x100000
#define LOGO_CREDIT_BAND_ROWS 128
#define LOGO_CREDIT_HALF_QWC 0x7000
/* sceVif1PkRef offset argument for each credit half (value of the original build). */
#define LOGO_CREDIT_HALF_VIF_OFFSET 0x51007000
#define LOGO_CREDIT_HALF_SIZE (LOGO_CREDIT_HALF_QWC * 16)

int xglMcMain(void);
/* Defined here: deviceCheckFontModeText, TestEnv_1_00A0F690, namcoLogoPath, msiLogoPath (data order is section order). */
extern const char deviceCheckFontModeText[8];

int device_check_sync(void)
{
    int status;

    status = xglMcMain();
    xglSleep();
    xglFontPrintDirectOT(-1, deviceCheckFontModeText);
    return status;
}

/* Card-check gate of the logo screen: always returns 1 in this build, so the card check always runs. */
int func_00A80040(void) {
    return 1;
}

#include "main/pad.h"
#include "main/tu012.h"
#include "main/tu021.h"

extern u8 *WorkEnd;
extern PadRecord PadData[2];
extern const char logoSaveSpaceHeaderText[];
extern const char logoNoCardText[];
extern const char logoNoSpaceSlot1Text[];
extern const char logoNoSpaceSlot2Text[];
extern const char logoNoSpaceBothText[];
extern const char logoStartGamePromptText[];
extern const char logoInsertCardText[];

/* Two-halfword view of the device_check_sync result: slot0 is the first port's card state. */
typedef union McSlotState {
    int value;
    struct {
        short slot0;
        short slot1;
    } slot;
} McSlotState;

/* Memory-card check of the logo screen: queries both ports, waits for the slots to settle,
 * then shows the save-space warning until CIRCLE or CROSS is pressed or the card state changes. */
void logoMcCheck(void)
{
    McSlotState screen_state[4];
    int slot_status[2];
    McRequestParam directory_request;
    McRequestParam *request;
    int free_space[2];
    McSlotState wait_state[4];
    const char *warning;
    const char *capacity_warning;
    int warning_mask;
    int i;

    for (i = LOGO_CARD_SETTLE_FRAMES - 1; i >= 0; i--) {
        screen_state[0].value = device_check_sync();
    }

    slot_status[0] = 0;
    slot_status[1] = 0;
    warning = 0;
    free_space[0] = 0;
    free_space[1] = 0;
    request = &directory_request;

    for (;;) {
        xglMcRequest(0, LOGO_MC_REQUEST_FREE_SPACE, 0, &slot_status[0]);
        xglMcRequest(1, LOGO_MC_REQUEST_FREE_SPACE, 0, &slot_status[1]);
        directory_request.file = -1;
        directory_request.data = WorkEnd;
        xglMcRequest(0, LOGO_MC_REQUEST_DIRECTORY, request, &free_space[0]);
        xglMcRequest(1, LOGO_MC_REQUEST_DIRECTORY, request, &free_space[1]);

        do {
            screen_state[0].value = device_check_sync();
        } while (screen_state[0].slot.slot0 == LOGO_MC_STATUS_BUSY || screen_state[0].slot.slot0 == LOGO_MC_STATUS_ERROR ||
                 screen_state[0].slot.slot1 == LOGO_MC_STATUS_BUSY || screen_state[0].slot.slot1 == LOGO_MC_STATUS_ERROR);

        for (i = LOGO_CARD_SETTLE_FRAMES - 1; i >= 0; i--) {
            screen_state[0].value = device_check_sync();
        }

        capacity_warning = logoSaveSpaceHeaderText;
        if (screen_state[0].slot.slot0 != LOGO_MC_STATUS_READY && screen_state[0].slot.slot0 != LOGO_MC_STATUS_UNFORMATTED &&
            screen_state[0].slot.slot1 != LOGO_MC_STATUS_READY && screen_state[0].slot.slot1 != LOGO_MC_STATUS_UNFORMATTED) {
            warning = logoNoCardText;
        } else {
            warning_mask = 0;
            do {
                if (screen_state[0].slot.slot0 == LOGO_MC_STATUS_UNFORMATTED || screen_state[0].slot.slot1 == LOGO_MC_STATUS_UNFORMATTED) {
                    break;
                }
                if (screen_state[0].slot.slot0 == LOGO_MC_STATUS_READY) {
                    if (free_space[0] <= 0 && slot_status[0] < LOGO_SAVE_MIN_FREE_KB) {
                        warning_mask = 1;
                    } else {
                        break;
                    }
                }
                if (screen_state[0].slot.slot1 == LOGO_MC_STATUS_READY) {
                    if (free_space[1] <= 0 && slot_status[1] < LOGO_SAVE_MIN_FREE_KB) {
                        warning_mask = warning_mask | 2;
                    } else {
                        warning_mask = 0;
                    }
                }
            } while (0);
            switch (warning_mask) {
            case 0:
                warning = 0;
                break;
            case 1:
                warning = logoNoSpaceSlot1Text;
                break;
            case 2:
                warning = logoNoSpaceSlot2Text;
                break;
            case 3:
                warning = logoNoSpaceBothText;
                break;
            default:
                break;
            }
        }

        while (warning != 0) {
            xglFontPrint(LOGO_MESSAGE_X, LOGO_MESSAGE_Y, -1, warning);
            if (capacity_warning != 0) {
                xglFontPrintDirectOT(-1, capacity_warning);
            }
            xglFontPrintDirectOT(-1, logoStartGamePromptText);

            if (PadData[0].pressed & PAD_CIRCLE) {
                seVoiceStartDefault(LOGO_SE_DECIDE);
                warning = 0;
                break;
            }

            if (PadData[0].pressed & PAD_CROSS) {
                seVoiceStartDefault(LOGO_SE_CANCEL);
                do {
                    wait_state[0].value = device_check_sync();
                    xglFontPrint(LOGO_MESSAGE_X, LOGO_MESSAGE_Y, -1, logoInsertCardText);
                    if (PadData[0].pressed & PAD_CROSS) {
                        seVoiceStartDefault(LOGO_SE_DECIDE);
                        break;
                    }
                } while (screen_state[0].value == wait_state[0].value);
                break;
            }

            wait_state[0].value = device_check_sync();
            if (screen_state[0].value != wait_state[0].value) {
                break;
            }
        }

        if (warning == 0) {
            break;
        }
    }

    device_check_sync();
}

#include "main/xgl_render.h"

extern const char D_00A83438[];
void xglRenderClearColor(u32 color);
/* Credit roll: loads credit.bin and draws it in 0x80-row bands. */
void logoShowCredits(void);

/* Logo screen entry: clears the frame, runs the card check, then the credit roll. */
void LogoFirst(void)
{
    xglRenderClearFrame();
    xglRenderClearColor(LOGO_CLEAR_COLOR_BLACK);
    xglSleep();
    if (func_00A80040() != 0) {
        logoMcCheck();
    }
    xglFontPrintDirectOT(-1, D_00A83438);
    logoShowCredits();
}

#include "main/sdv_033.h"

extern u16 sRenderDisplayBase;
extern u64 D_00A82850[];
extern u64 D_00A828B0[];
extern u64 D_00A828D0[];
extern u32 D_00A82900[];
extern XglPacket *xglPacketGetCurrent(void);
extern void sceVif1PkCnt(XglPacket *packet, int count);
extern void sceVif1PkAddDataN(XglPacket *packet, const void *data, int count);
extern void sceVif1PkRef(XglPacket *packet, const void *environment, int count,
                         int mode, int offset, int flags);

/* Draws one credit band: its two 0x7000-QWC halves, then the blank rows left below rowsDrawn. */
void logoDrawCreditBand(u8 *creditBand, int rowsDrawn)
{
    XglPacket *packet;

    packet = xglPacketGetCurrent();
    if (creditBand != 0) {
        D_00A82850[4] = ((u64)sRenderDisplayBase << 37) | ((u64)0x8000 << 36);
        sceVif1PkCnt(packet, 0);
        sceVif1PkAddDataN(packet, D_00A82850, 0x18);
        sceVif1PkRef(packet, D_00A828B0, 2, 0, 0, 0);
        sceVif1PkRef(packet, creditBand, LOGO_CREDIT_HALF_QWC, 0, LOGO_CREDIT_HALF_VIF_OFFSET, 0);
        sceVif1PkRef(packet, D_00A828B0, 2, 0, 0, 0);
        sceVif1PkRef(packet, creditBand + LOGO_CREDIT_HALF_SIZE, LOGO_CREDIT_HALF_QWC, 0, LOGO_CREDIT_HALF_VIF_OFFSET, 0);
        sceVif1PkRef(packet, D_00A828D0, 3, 0, 0, 0);
    }
    if (rowsDrawn < LOGO_CREDIT_BAND_ROWS) {
        D_00A82900[19] = LOGO_CREDIT_BAND_ROWS - rowsDrawn;
        sceVif1PkCnt(packet, 0);
        sceVif1PkAddDataN(packet, D_00A82900, 0x1C);
    }
}

extern const char logoCreditPath[];
void xglRenderClearDepth(void);
int xglCdReadFile(const char *name, void *buffer, int mode, int flags);
void xglSleep(void);

/* The decode request (JpegDecodeControl, main/xgl_jpeg.h): only source and destination are set here; the rest is zeroed by memset. */
/* Credit roll: loads credit.bin and draws it in 0x80-row bands. */
void logoShowCredits(void)
{
    u32 *credit_table;
    u32 file_size;
    int band_count;
    u32 band_dest;
    u32 jpeg_base;
    JpegDecodeControl jpeg_request;
    int band_index;
    void *band;
    int phase;
    int row;
    int drawn;
    int align_mask;

    credit_table = (u32 *)LOGO_CREDIT_FILE_BASE;
    band_dest = LOGO_CREDIT_BAND_BASE;
    xglRenderClearDepth();
    xglCdReadFile(logoCreditPath, (void *)LOGO_CREDIT_FILE_BASE, 0, 1);
    file_size = credit_table[0];
    credit_table[0] = 0;
    band_count = file_size >> 2;
    jpeg_base = ((file_size + 0x43) & ~0x3F) + LOGO_CREDIT_FILE_BASE;
    if (band_count != 0) {
        align_mask = -64;
        band_index = band_count;
        do {
            band_index -= 1;
            memset(&jpeg_request, 0, sizeof(jpeg_request));
            jpeg_request.source = (u8 *)(jpeg_base + ((*credit_table + 0x3F) & align_mask));
            jpeg_request.destination = (u8 *)band_dest;
            xglJpegDecode(&jpeg_request);
            *credit_table = band_dest;
            band_dest += LOGO_CREDIT_BAND_STRIDE;
            credit_table++;
        } while (band_index != 0);
    }
    band_index = 0;
    if (band_count != 0) {
        do {
        band = (void *)((u32 *)LOGO_CREDIT_FILE_BASE)[band_index];
        phase = 0;
        row = 0;
        do {
            switch (phase) {
            case 0:
                row = 0;
                phase = 1;
                /* fall through */
            case 1:
                drawn = row;
                row += 16;
                logoDrawCreditBand(band, drawn);
                if (row < LOGO_CREDIT_BAND_ROWS) {
                    break;
                }
                row = 30;
                phase = 2;
                /* fall through */
            case 2:
                logoDrawCreditBand(band, LOGO_CREDIT_BAND_ROWS);
                if (row > 0) {
                    row -= 1;
                    break;
                }
                row = LOGO_CREDIT_BAND_ROWS;
                phase = 3;
                /* fall through */
            case 3:
                drawn = row;
                row -= 16;
                logoDrawCreditBand(band, drawn);
                if (row <= 0) {
                    phase = 4;
                }
                break;
            }
            xglSleep();
        } while (phase != 4);
        band_index++;
        } while (band_index < band_count);
    }
    logoDrawCreditBand(0, 0);
    xglSleep();
    logoDrawCreditBand(0, 0);
    xglSleep();
}

/* Empty function (no body in the original build). */
void func_00A80790(void) {

}

/* .data of this TU (address order). */
u64 D_00A82848[1] = { 0 };
u64 D_00A82850[12] = { 0x0000000000000000ULL, 0x5100000500000000ULL, 0x1000000000000004ULL, 0x000000000000000eULL, 0x0000000000000000ULL, 0x0000000000000050ULL, 0x0000000000000000ULL, 0x0000000000000051ULL, 0x000001c000000200ULL, 0x0000000000000052ULL, 0x0000000000000000ULL, 0x0000000000000053ULL };
u64 D_00A828B0[4] = { 0x0000000000000000ULL, 0x5100000100000000ULL, 0x0800000000007000ULL, 0x0000000000000000ULL };
u64 D_00A828D0[6] = { 0x0000000000000000ULL, 0x5100000200000000ULL, 0x1000000000008001ULL, 0x000000000000000eULL, 0x0000000000000000ULL, 0x000000000000003fULL };
u32 D_00A82900[42] = { 0x00000000, 0x00000000, 0x00000000, 0x51000006, 0x00008001, 0x50234000, 0x000551EE, 0x00000000, 0x00030000, 0x00000000, 0x00000047, 0x00000000, 0x00000044, 0x00000000, 0x00000042, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000080, 0x00006FF8, 0x000071F7, 0x00000000, 0x00000000, 0x00008FF8, 0x00008DF7, 0x00000000, 0x00000000, 0x00000000, 0x631EDA80, 0x01AAAB80, 0x011EECD8, 0x0123C600, 0x0123CF78, 0x01251E30, 0x01AAAD50, 0x0123E978, 0x00000000, 0x63A81EF0, 0x63A82600, 0x63A828E4, 0x00000000 };

/* .rodata of this TU (address order). */

const char deviceCheckFontModeText[8] = "\031\003";
const char logoSaveSpaceHeaderText[72] = "\n\n\031\003Xenosaga Episode II requires 34KB of free space to\nsave data.";
const char logoNoCardText[96] = "\031\003No memory card (8MB) (for PlayStation\244\2452)\nin MEMORY CARD slot 1 and MEMORY CARD slot 2.";
const char logoNoSpaceSlot1Text[88] = "\031\003Insufficient space on\nmemory card (8MB) (for PlayStation\244\2452)\nin MEMORY CARD slot 1.";
const char logoNoSpaceSlot2Text[88] = "\031\003Insufficient space on\nmemory card (8MB) (for PlayStation\244\2452)\nin MEMORY CARD slot 2.";
const char logoNoSpaceBothText[112] = "\031\003Insufficient space on\nmemory card (8MB) (for PlayStation\244\2452)\nin MEMORY CARD slot 1 and MEMORY CARD slot 2.";
const char logoStartGamePromptText[72] = "\n\n\031\003Start game?\n\n\014\200  \031\000\241\373\031\003\014\200\200\200 button: Yes/\014  \200\031\000\241\337\031\003\014\200\200\200 button: No";
const char logoInsertCardText[168] = "\031\003Please insert a memory card (8MB) (for PlayStation\244\2452)\nwith at least 34KB of free space into\nMEMORY CARD slot 1 or MEMORY CARD slot 2.\n\n\014  \200\241\337\014\200\200\200 button: Cancel";
const char D_00A83438[8] = "\031";
const char logoCreditPath[16] = "credit.bin";
