#include "common.h"
#include "shared.h"
#include "main/xgl_thread.h"
#include "main/xgl_font.h"
#include "main/xgl_camera.h"
#include "main/xgl_jpeg.h"
#include "main/tya_draw_gauge.h"

#include "main/pad.h"
#include "main/xgl_hdd.h"
#include "main/control_entry.h"
#include "ov02/title_hdd_install.h"
#include "main/xgl_packet.h"
#include "main/gr_screen.h"
#include "main/xgl_cd.h"
#include "main/se_bank_load.h"

/* Frames the screens ignore the pad after they appear. */
#define HDD_INPUT_DELAY 4
/* Position of the screen message and of the installer's status text. */
#define HDD_MESSAGE_X 32
#define HDD_MESSAGE_Y 16
#define HDD_STATUS_X 84
#define HDD_STATUS_Y 208
/* Progress gauge box (tyaDrawGauge) and the texts drawn over it while an install copies its images. */
#define HDD_GAUGE_X 168
#define HDD_GAUGE_Y 416
#define HDD_GAUGE_WIDTH 320
#define HDD_GAUGE_HEIGHT 12
#define HDD_HINT_X 32
#define HDD_HINT_Y 80
#define HDD_COUNTER_X 152
#define HDD_COUNTER_Y 414
#define HDD_PERCENT_X 168
#define HDD_PERCENT_Y 400
/* The install archive: a 0x40-byte header, then one jpeg for every fifth of the progress. */
#define HDD_ARCHIVE_HEADER_SIZE 0x40
#define HDD_PROGRESS_IMAGES 5
#define HDD_PROGRESS_PERCENT_PER_IMAGE 20
/* Failures xglHddInstall reports to its callback as a negative command (the values of src/main/xgl_hdd.c). */
#define HDD_INSTALL_FAIL_RESOURCE (-1)
#define HDD_INSTALL_FAIL_PARTITION_OPEN (-2)
#define HDD_INSTALL_FAIL_WRITE_HEADER (-3)
#define HDD_INSTALL_FAIL_EXTEND (-4)
#define HDD_INSTALL_FAIL_PARTITION_CLOSE (-5)
#define HDD_INSTALL_FAIL_FORMAT (-6)
#define HDD_INSTALL_FAIL_MOUNT (-7)
#define HDD_INSTALL_FAIL_DATA_OPEN (-8)
#define HDD_INSTALL_FAIL_DATA_WRITE (-9)
#define HDD_INSTALL_FAIL_DATA_CLOSE (-10)
#define HDD_INSTALL_FAIL_CHECK_OPEN (-11)
#define HDD_INSTALL_FAIL_CHECK_WRITE (-12)
#define HDD_INSTALL_FAIL_CHECK_CLOSE (-13)
#define HDD_INSTALL_FAIL_UNMOUNT (-14)
#define HDD_INSTALL_FAIL_REMOUNT (-15)
/* Sound ids: CIRCLE plays the decide sound, CROSS the cancel sound. */
#define HDD_SE_DECIDE 0x1000002
#define HDD_SE_CANCEL 0x1000008



/* The decoded jpeg is sent in two halves of HDD_TEXTURE_QWC quadwords. */
#define HDD_TEXTURE_QWC 0x7000
#define HDD_TEXTURE_HALF_SIZE (HDD_TEXTURE_QWC * 16)
/* sceVif1PkRef offset argument for each texture half (value of the original build). */
#define HDD_TEXTURE_VIF_OFFSET 0x51007000
/* Free-memory pointer is set this many bytes past the transfer buffer (the install's working area). */
#define HDD_WORK_AFTER_TRANSFER 0xE0000

struct HddInstallParams {
    u8 *image_data;
    u8 step;
    u8 unmodeled_05[3];
    u8 *transfer_buffer;
};

/* The installer keeps one transfer-buffer pointer in its own BSS. */
struct HddInstallParams *HddInstallParam;
/* DMA/VIF tags followed by the GS register data sent by HddInstallSync. */
u64 TestEnv_0_00A830A0[12] = {
    0, 0x5100000500000000, 0x1000000000000004, 0xE, 0, 0x50, 0, 0x51, 0x1C000000200, 0x52, 0, 0x53,
};
u64 TransEnv_1_00A83100[4] = { 0, 0x5100000100000000, 0x0800000000007000, 0 };
u64 FlushEnv_2_00A83120[6] = { 0x0000000000000000ULL, 0x5100000200000000ULL, 0x1000000000008001ULL, 0x000000000000000eULL, 0x0000000000000000ULL, 0x000000000000003fULL };
signed char hddSpinnerTable[8] = { 0x2d, 0x7f, 0x7c, 0x2f, 0, 0, 0, 0 };
char hddSpinnerFormat[8] = "%c";

/* The install percentage format: "%3d" and two font control bytes. */
static char hddPercentFormat[8] = "%3d\033\022";

/* The message of each failure screen, HDD_SCREEN_MESSAGE_FIRST to HDD_SCREEN_MESSAGE_LAST (defined below). */
extern const char hddNotAccessibleText[];
extern const char hddNotFormattedText[];
extern const char hddNoSpaceText[];
static const char *hddScreenMessages[4] = { hddNotAccessibleText, hddNotFormattedText, hddNoSpaceText, 0 };
/* Font control bytes passed to the existing font-script renderer. */
extern const char installScreenFontText[8];

void HddInstallSync(void)
{
    XglPacket *packet;
    u8 *transfer_buffer;

    packet = xglPacketGetCurrent();
    transfer_buffer = HddInstallParam->transfer_buffer;
    TestEnv_0_00A830A0[4] = ((u64)sRender.display_buffer_base << 37) | ((u64)0x8000 << 36);
    sceVif1PkCnt(packet, 0);
    sceVif1PkAddDataN(packet, TestEnv_0_00A830A0, 0x18);
    sceVif1PkRef(packet, TransEnv_1_00A83100, 2, 0, 0, 0);
    sceVif1PkRef(packet, transfer_buffer, HDD_TEXTURE_QWC, 0, HDD_TEXTURE_VIF_OFFSET, 0);
    sceVif1PkRef(packet, TransEnv_1_00A83100, 2, 0, 0, 0);
    sceVif1PkRef(packet, transfer_buffer + HDD_TEXTURE_HALF_SIZE, HDD_TEXTURE_QWC, 0, HDD_TEXTURE_VIF_OFFSET, 0);
    sceVif1PkRef(packet, FlushEnv_2_00A83120, 3, 0, 0, 0);
    xglSleep();
    xglFontPrintDirectOT(-1, installScreenFontText);
}

/* Pad ports (main/pad.h PadRecord) of the main data; port 0 is the one the installer polls. */
/* Prompt strings of the install and uninstall dialogs (splat D_ names). */
extern const char hddInstallStartPromptText[];
extern const char hddInstallDoneText[];
extern const char hddInstallFailedPromptText[];
extern const char hddUninstallStartPromptText[];
extern const char hddUninstallDoneText[];
extern const char hddUninstallFailedPromptText[];
static u8 hddSpinnerCount;
extern const char hddInstallImagePath[];
extern const char hddInstallAbortedText[];
extern const char hddUninstallAbortedText[];
/* WorkEnd is the global of src/main/xgl_cd.c: the free-memory pointer
 * the installer reads its CD file into and restores when it returns. */
extern const char hddInstallingText[];
extern const char hddAbortHintText[];
extern const char hddAbortConfirmText[];
extern const char hddInstallTraceFormat[];
extern const char hddInstallErrorFormat[];
extern const char hddStepInstallStartText[];
extern const char hddStepCreatePartitionText[];
extern const char hddStepWriteResourceText[];
extern const char hddStepCreateSubPartitionText[];
extern const char hddStepFormatPartitionText[];
extern const char hddStepMountPartitionText[];
extern const char hddStepStartInstallText[];
extern const char hddCheckFileCreateText[];
extern const char hddCheckFileWriteText[];
extern const char hddCheckFileCloseText[];
extern const char hddCheckFileUnmountText[];
extern const char hddCheckFileMountText[];
extern const char hddInstallCompleteText[];
int printf(const char *fmt, ...);
long long __udivdi3(long long percent, int count);

/* Installer status callback: prints the step text for `event` and draws the progress gauge for event 6. A nonzero
 * *param makes it sync and return that status. It does not wait on the pad; the wait is in
 * TitleHddInstallSubInstall. */
/* Text of the install screen, in the order the code draws it. */
const char installScreenFontText[8] = "\017D\000\200\015\003";
/* JP: インストール中です。“ＰｌａｙＳｔａｔｉｏｎ２”本体の\n\n電源を切らないでください。
 * EN: Installing. Do not turn off the power of the "PlayStation 2"\n\nconsole. */
const char hddInstallingText[96] = "\031\001\245\244\245\363\245\271\245\310\241\274\245\353\303\346\244\307\244\271\241\243\241\310\243\320\243\354\243\341\243\371\243\323\243\364\243\341\243\364\243\351\243\357\243\356\010\210\000\010\243\262\241\311\313\334\302\316\244\316\n\n\305\305\270\273\244\362\300\332\244\351\244\312\244\244\244\307\244\257\244\300\244\265\244\244\241\243";
const char hddStepInstallStartText[16] = "Install Start";
const char hddStepCreatePartitionText[24] = "Create Partition";
const char hddStepWriteResourceText[16] = "Write Resource";
const char hddStepCreateSubPartitionText[32] = "Create Sub Partition %1d";
const char hddStepFormatPartitionText[24] = "Format Partition";
const char hddStepMountPartitionText[16] = "Mount Partition";
const char hddStepStartInstallText[16] = "Start Install";
/* JP: ×ボタン：中断
 * EN: (X) button: Abort */
const char hddAbortHintText[32] = "\031\000\014  \200\241\337\014\200\200\200\245\334\245\277\245\363\241\247\303\346\303\307";
/* JP: インストールはまだ完了していません。\n\n中断しますか？\n\n○ボタン：中断／×ボタン：継続
 * EN: The installation is not finished yet.\n\nAbort it?\n\n(O) button: Abort / (X) button: Continue */
const char hddAbortConfirmText[104] = "\245\244\245\363\245\271\245\310\241\274\245\353\244\317\244\336\244\300\264\260\316\273\244\267\244\306\244\244\244\336\244\273\244\363\241\243\n\n\303\346\303\307\244\267\244\336\244\271\244\253\241\251\n\n\014\200  \241\373\014\200\200\200\245\334\245\277\245\363\241\247\303\346\303\307\241\277\014  \200\241\337\014\200\200\200\245\334\245\277\245\363\241\247\267\321\302\263";
const char hddCheckFileCreateText[24] = "CheckFile Create";
const char hddCheckFileWriteText[16] = "CheckFile Write";
const char hddCheckFileCloseText[16] = "CheckFile Close";
const char hddCheckFileUnmountText[24] = "CheckFile Unmount";
const char hddCheckFileMountText[16] = "CheckFile Mount";
const char hddInstallCompleteText[24] = "Install Complete";
const char hddInstallTraceFormat[16] = "%d:%d(%08x)\n";
const char hddInstallErrorFormat[24] = "Error %2d/%8x";

int HddInstallCB(int command, int offset, void *param)
{
    int *status = param;
    TyaGaugeArgs gauge;
    u64 print_args[2];
    int progress_max;
    int step;

    if (*status) {
        HddInstallSync();
        return *status;
    }

    xglFontPrint(HDD_MESSAGE_X, HDD_MESSAGE_Y, -1, hddInstallingText);
    switch (command) {
    case 0:
        xglFontDebugPrintf(HDD_STATUS_X, HDD_STATUS_Y, hddStepInstallStartText);
        break;
    case 1:
        xglFontDebugPrintf(HDD_STATUS_X, HDD_STATUS_Y, hddStepCreatePartitionText);
        break;
    case 2:
        xglFontDebugPrintf(HDD_STATUS_X, HDD_STATUS_Y, hddStepWriteResourceText);
        break;
    case 3:
        xglFontDebugPrintf(HDD_STATUS_X, HDD_STATUS_Y, hddStepCreateSubPartitionText, offset);
        break;
    case 4:
        xglFontDebugPrintf(HDD_STATUS_X, HDD_STATUS_Y, hddStepFormatPartitionText);
        break;
    case 5:
        xglFontDebugPrintf(HDD_STATUS_X, HDD_STATUS_Y, hddStepMountPartitionText);
        break;
    case 6:
        if (offset == 0) {
            xglFontDebugPrintf(HDD_STATUS_X, HDD_STATUS_Y, hddStepStartInstallText);
            break;
        }
        gauge.x = HDD_GAUGE_X;
        gauge.y = HDD_GAUGE_Y;
        gauge.vertex_attribute = -1;
        gauge.width = HDD_GAUGE_WIDTH;
        gauge.current = offset;
        gauge.filled_color[0] = 0x80;
        gauge.filled_color[2] = 0x80;
        gauge.filled_color[3] = 0x80;
        gauge.unfilled_color[0] = 0x40;
        gauge.unfilled_color[1] = 0x40;
        gauge.unfilled_color[2] = 0x40;
        gauge.unfilled_color[3] = 0x80;
        gauge.height = HDD_GAUGE_HEIGHT;
        gauge.filled_color[1] = 0xc0;
        progress_max = offset >> 16;
        gauge.maximum = progress_max;
        tyaDrawGauge(&gauge);
        xglFontPrint(HDD_HINT_X, HDD_HINT_Y, -1, hddAbortHintText);
        print_args[0] = (u32)hddSpinnerFormat;
        print_args[1] = (u32)hddSpinnerTable[(hddSpinnerCount >> 3) & 3];
        xglFontPrintf(HDD_COUNTER_X, HDD_COUNTER_Y, -1, print_args);
        hddSpinnerCount += 1;
        print_args[0] = (u32)hddPercentFormat;
        print_args[1] = (u16)offset * 100 / progress_max;
        xglFontPrintf(HDD_PERCENT_X, HDD_PERCENT_Y, -1, print_args);
        step = __udivdi3(print_args[1], HDD_PROGRESS_PERCENT_PER_IMAGE);
        if (step != HddInstallParam->step && step < HDD_PROGRESS_IMAGES) {
            JpegDecodeControl jpeg;
            u8 *image_data;

            image_data = HddInstallParam->image_data;
            memset(&jpeg, 0, sizeof(jpeg));
            HddInstallParam->step = step;
            jpeg.source = image_data + ((u32 *)image_data)[step] + HDD_ARCHIVE_HEADER_SIZE;
            jpeg.destination = HddInstallParam->transfer_buffer;
            xglJpegDecode(&jpeg);
        }
        if ((PadData[0].pressed & PAD_CROSS) == 0) {
            break;
        }
        seVoiceStartDefault(HDD_SE_CANCEL);
        HddInstallSync();
        while (1) {
            u16 pressed;

            xglFontPrint(HDD_MESSAGE_X, HDD_MESSAGE_Y, -1, hddAbortConfirmText);
            pressed = PadData[0].pressed;
            if ((pressed & PAD_CIRCLE) != 0) {
                seVoiceStartDefault(HDD_SE_DECIDE);
                *status = 1;
                return 1;
            }
            if ((pressed & PAD_CROSS) != 0) {
                seVoiceStartDefault(HDD_SE_CANCEL);
                if (xglHddCheck() >= 0) {
                    HddInstallSync();
                    return 0;
                }
                *status = 2;
                return 2;
            }
            HddInstallSync();
        }
    case 7:
        switch (offset) {
        case 0:
            xglFontDebugPrintf(HDD_STATUS_X, HDD_STATUS_Y, hddCheckFileCreateText);
            break;
        case 1:
            xglFontDebugPrintf(HDD_STATUS_X, HDD_STATUS_Y, hddCheckFileWriteText);
            break;
        case 2:
            xglFontDebugPrintf(HDD_STATUS_X, HDD_STATUS_Y, hddCheckFileCloseText);
            break;
        case 3:
            xglFontDebugPrintf(HDD_STATUS_X, HDD_STATUS_Y, hddCheckFileUnmountText);
            break;
        case 4:
            xglFontDebugPrintf(HDD_STATUS_X, HDD_STATUS_Y, hddCheckFileMountText);
            break;
        }
        break;
    case 8:
        xglFontDebugPrintf(HDD_STATUS_X, HDD_STATUS_Y, hddInstallCompleteText);
        break;
    case HDD_INSTALL_FAIL_RESOURCE:
    case HDD_INSTALL_FAIL_PARTITION_OPEN:
    case HDD_INSTALL_FAIL_WRITE_HEADER:
    case HDD_INSTALL_FAIL_EXTEND:
    case HDD_INSTALL_FAIL_PARTITION_CLOSE:
    case HDD_INSTALL_FAIL_FORMAT:
    case HDD_INSTALL_FAIL_MOUNT:
    case HDD_INSTALL_FAIL_DATA_OPEN:
    case HDD_INSTALL_FAIL_DATA_WRITE:
    case HDD_INSTALL_FAIL_DATA_CLOSE:
    case HDD_INSTALL_FAIL_CHECK_OPEN:
    case HDD_INSTALL_FAIL_CHECK_WRITE:
    case HDD_INSTALL_FAIL_CHECK_CLOSE:
    case HDD_INSTALL_FAIL_UNMOUNT:
    case HDD_INSTALL_FAIL_REMOUNT:
        printf(hddInstallTraceFormat, command, offset, offset);
        xglFontDebugPrintf(HDD_STATUS_X, HDD_STATUS_Y, hddInstallErrorFormat, command, offset);
        break;
    }
    HddInstallSync();
    return 0;
}

/* TitleHddInstall install arm: start prompt, then the install and its completion or failure prompt. */
/* Messages and paths of the install and uninstall screens. */
/* JP: ゼノサーガのインストールを開始します。\n\nよろしいですか？\n\n○ボタン：開始／×ボタン：キャンセル
 * EN: Starting the Xenosaga installation.\n\nIs this okay?\n\n(O) button: Start / (X) button: Cancel */
const char hddInstallStartPromptText[112] = "\245\274\245\316\245\265\241\274\245\254\244\316\245\244\245\363\245\271\245\310\241\274\245\353\244\362\263\253\273\317\244\267\244\336\244\271\241\243\n\n\244\350\244\355\244\267\244\244\244\307\244\271\244\253\241\251\n\n\014\200  \241\373\014\200\200\200\245\334\245\277\245\363\241\247\263\253\273\317\241\277\014  \200\241\337\014\200\200\200\245\334\245\277\245\363\241\247\245\255\245\343\245\363\245\273\245\353";
/* JP: インストールが正常に終了しました。\n\n○ボタン：タイトルに戻る
 * EN: The installation finished successfully.\n\n(O) button: Return to title */
const char hddInstallDoneText[72] = "\245\244\245\363\245\271\245\310\241\274\245\353\244\254\300\265\276\357\244\313\275\252\316\273\244\267\244\336\244\267\244\277\241\243\n\n\014\200  \241\373\014\200\200\200\245\334\245\277\245\363\241\247\245\277\245\244\245\310\245\353\244\313\314\341\244\353";
/* JP: インストールに失敗しました。\n\nインストールを中止しますか？\n\n○ボタン：はい／×ボタン：いいえ
 * EN: The installation failed.\n\nCancel the installation?\n\n(O) button: Yes / (X) button: No */
const char hddInstallFailedPromptText[112] = "\245\244\245\363\245\271\245\310\241\274\245\353\244\313\274\272\307\324\244\267\244\336\244\267\244\277\241\243\n\n\245\244\245\363\245\271\245\310\241\274\245\353\244\362\303\346\273\337\244\267\244\336\244\271\244\253\241\251\n\n\014\200  \241\373\014\200\200\200\245\334\245\277\245\363\241\247\244\317\244\244\241\277\014  \200\241\337\014\200\200\200\245\334\245\277\245\363\241\247\244\244\244\244\244\250";
/* JP: ゼノサーガのアンインストールを開始します。\n\nよろしいですか？\n\n○ボタン：開始／×ボタン：キャンセル
 * EN: Starting the Xenosaga uninstallation.\n\nIs this okay?\n\n(O) button: Start / (X) button: Cancel */
const char hddUninstallStartPromptText[120] = "\245\274\245\316\245\265\241\274\245\254\244\316\245\242\245\363\245\244\245\363\245\271\245\310\241\274\245\353\244\362\263\253\273\317\244\267\244\336\244\271\241\243\n\n\244\350\244\355\244\267\244\244\244\307\244\271\244\253\241\251\n\n\014\200  \241\373\014\200\200\200\245\334\245\277\245\363\241\247\263\253\273\317\241\277\014  \200\241\337\014\200\200\200\245\334\245\277\245\363\241\247\245\255\245\343\245\363\245\273\245\353";
/* JP: アンインストールが正常に終了しました。\n\n○ボタン：タイトルに戻る
 * EN: The uninstallation finished successfully.\n\n(O) button: Return to title */
const char hddUninstallDoneText[80] = "\245\242\245\363\245\244\245\363\245\271\245\310\241\274\245\353\244\254\300\265\276\357\244\313\275\252\316\273\244\267\244\336\244\267\244\277\241\243\n\n\014\200  \241\373\014\200\200\200\245\334\245\277\245\363\241\247\245\277\245\244\245\310\245\353\244\313\314\341\244\353";
/* JP: アンインストールに失敗しました。\n\nアンインストールを中止しますか？\n\n○ボタン：はい／×ボタン：いいえ
 * EN: The uninstallation failed.\n\nCancel the uninstallation?\n\n(O) button: Yes / (X) button: No */
const char hddUninstallFailedPromptText[120] = "\245\242\245\363\245\244\245\363\245\271\245\310\241\274\245\353\244\313\274\272\307\324\244\267\244\336\244\267\244\277\241\243\n\n\245\242\245\363\245\244\245\363\245\271\245\310\241\274\245\353\244\362\303\346\273\337\244\267\244\336\244\271\244\253\241\251\n\n\014\200  \241\373\014\200\200\200\245\334\245\277\245\363\241\247\244\317\244\244\241\277\014  \200\241\337\014\200\200\200\245\334\245\277\245\363\241\247\244\244\244\244\244\250";
const char hddInstallImagePath[24] = "data\\yajima\\hddi.bin";
/* JP: ハードディスクドライブに、ゼノサーガの\n\nインストールに必要な空き容量が足りません。\n\n（空き容量として１７９２ＭＢ以上必要です。）\n\n\n\n他のアプリケーションをアンインストールすると、\n\nインストールできるようになる可能性があります。
 * EN: The hard disk drive does not have enough free space\n\nto install Xenosaga.\n\n(At least 1792 MB of free space is required.)\n\n\n\nUninstalling other applications\n\nmay make the installation possible. */
const char hddNoSpaceText[240] = "\245\317\241\274\245\311\245\307\245\243\245\271\245\257\245\311\245\351\245\244\245\326\244\313\241\242\245\274\245\316\245\265\241\274\245\254\244\316\n\n\245\244\245\363\245\271\245\310\241\274\245\353\244\313\311\254\315\327\244\312\266\365\244\255\315\306\316\314\244\254\302\255\244\352\244\336\244\273\244\363\241\243\n\n\010\210\377\370\241\312\266\365\244\255\315\306\316\314\244\310\244\267\244\306\031\001\243\261\243\267\243\271\243\262\243\315\243\302\031\000\260\312\276\345\311\254\315\327\244\307\244\271\241\243\010\210\377\370\241\313\n\n\n\n\302\276\244\316\245\242\245\327\245\352\245\261\241\274\245\267\245\347\245\363\244\362\245\242\245\363\245\244\245\363\245\271\245\310\241\274\245\353\244\271\244\353\244\310\241\242\n\n\245\244\245\363\245\271\245\310\241\274\245\353\244\307\244\255\244\353\244\350\244\246\244\313\244\312\244\353\262\304\307\275\300\255\244\254\244\242\244\352\244\336\244\271\241\243";
/* JP: ハードディスクドライブがフォーマットされて\n\nいません。
 * EN: The hard disk drive is not\n\nformatted. */
const char hddNotFormattedText[56] = "\245\317\241\274\245\311\245\307\245\243\245\271\245\257\245\311\245\351\245\244\245\326\244\254\245\325\245\251\241\274\245\336\245\303\245\310\244\265\244\354\244\306\n\n\244\244\244\336\244\273\244\363\241\243";
/* JP: ハードディスクドライブにアクセスできません。
 * EN: Cannot access the hard disk drive. */
const char hddNotAccessibleText[48] = "\245\317\241\274\245\311\245\307\245\243\245\271\245\257\245\311\245\351\245\244\245\326\244\313\245\242\245\257\245\273\245\271\244\307\244\255\244\336\244\273\244\363\241\243";
/* JP: \n\n\n\nインストールを中止します。\n\n○ボタン：タイトルに戻る
 * EN: \n\n\n\nThe installation is canceled.\n\n(O) button: Return to title */
const char hddInstallAbortedText[72] = "\n\n\n\n\245\244\245\363\245\271\245\310\241\274\245\353\244\362\303\346\273\337\244\267\244\336\244\271\241\243\n\n\014\200  \241\373\014\200\200\200\245\334\245\277\245\363\241\247\245\277\245\244\245\310\245\353\244\313\314\341\244\353";
/* JP: \n\n\n\nアンインストールを中止します。\n\n○ボタン：タイトルに戻る
 * EN: \n\n\n\nThe uninstallation is canceled.\n\n(O) button: Return to title */
const char hddUninstallAbortedText[72] = "\n\n\n\n\245\242\245\363\245\244\245\363\245\271\245\310\241\274\245\353\244\362\303\346\273\337\244\267\244\336\244\271\241\243\n\n\014\200  \241\373\014\200\200\200\245\334\245\277\245\363\241\247\245\277\245\244\245\310\245\353\244\313\314\341\244\353";

void TitleHddInstallSubInstall(void)
{
    int installStatus;
    int result;
    int frames;
    u16 pad;

    for (;;) {
        installStatus = 0;
        frames = HDD_INPUT_DELAY;
        for (;;) {
            xglFontPrint(HDD_MESSAGE_X, HDD_MESSAGE_Y, -1, hddInstallStartPromptText);
            HddInstallSync();
            if (frames > 0) {
                frames--;
                continue;
            }
            pad = PadData[0].pressed;
            if (pad & PAD_CIRCLE) {
                break;
            }
            if (pad & PAD_CROSS) {
                seVoiceStartDefault(HDD_SE_CANCEL);
                return;
            }
        }
        seVoiceStartDefault(HDD_SE_DECIDE);
        result = xglHddCheck();
        if (result == 0) {
            result = xglHddInstall(HddInstallCB, &installStatus);
        }
        if (result == 1) {
            HddInstallSync();
            return;
        }
        if (result == 0) {
            xglHddActivate(1);
            result = xglHddActivate(-1);
        } else {
            result = 0;
        }
        HddInstallSync();
        frames = HDD_INPUT_DELAY;
        if (result == 1) {
            for (;;) {
                xglFontPrint(HDD_MESSAGE_X, HDD_MESSAGE_Y, -1, hddInstallDoneText);
                HddInstallSync();
                if (frames > 0) {
                    frames--;
                    continue;
                }
                pad = PadData[0].pressed;
                if (pad & PAD_CIRCLE) {
                    seVoiceStartDefault(HDD_SE_DECIDE);
                    return;
                }
            }
        }
        for (;;) {
            xglFontPrint(HDD_MESSAGE_X, HDD_MESSAGE_Y, -1, hddInstallFailedPromptText);
            HddInstallSync();
            if (frames > 0) {
                frames--;
                continue;
            }
            pad = PadData[0].pressed;
            if (pad & PAD_CIRCLE) {
                seVoiceStartDefault(HDD_SE_DECIDE);
                return;
            }
            if (pad & PAD_CROSS) {
                break;
            }
        }
        seVoiceStartDefault(HDD_SE_CANCEL);
        HddInstallSync();
    }
}

/* TitleHddInstall uninstall arm: same prompt flow as the install arm, without the install callback. */
void TitleHddInstallSubUninstall(void)
{
    int result;
    int frames;
    u16 pad;

    for (;;) {
        frames = HDD_INPUT_DELAY;
        for (;;) {
            xglFontPrint(HDD_MESSAGE_X, HDD_MESSAGE_Y, -1, hddUninstallStartPromptText);
            HddInstallSync();
            if (frames > 0) {
                frames--;
                continue;
            }
            pad = PadData[0].pressed;
            if (pad & PAD_CIRCLE) {
                break;
            }
            if (pad & PAD_CROSS) {
                seVoiceStartDefault(HDD_SE_CANCEL);
                return;
            }
        }
        seVoiceStartDefault(HDD_SE_DECIDE);
        result = xglHddCheck2();
        if (result == 0) {
            result = xglHddUninstall();
        }
        frames = HDD_INPUT_DELAY;
        HddInstallSync();
        if (result == 0) {
            for (;;) {
                xglFontPrint(HDD_MESSAGE_X, HDD_MESSAGE_Y, -1, hddUninstallDoneText);
                HddInstallSync();
                if (frames > 0) {
                    frames--;
                    continue;
                }
                pad = PadData[0].pressed;
                if (pad & PAD_CIRCLE) {
                    seVoiceStartDefault(HDD_SE_DECIDE);
                    return;
                }
            }
        }
        for (;;) {
            xglFontPrint(HDD_MESSAGE_X, HDD_MESSAGE_Y, -1, hddUninstallFailedPromptText);
            HddInstallSync();
            if (frames > 0) {
                frames--;
                continue;
            }
            pad = PadData[0].pressed;
            if (pad & PAD_CIRCLE) {
                seVoiceStartDefault(HDD_SE_DECIDE);
                xglHddCheck();
                return;
            }
            if (pad & PAD_CROSS) {
                break;
            }
        }
        seVoiceStartDefault(HDD_SE_CANCEL);
        HddInstallSync();
    }
}

/* HDD install dispatcher called from Title: shows the install or uninstall arm, or the message screens. */
void TitleHddInstall(int command)
{
    struct HddInstallParams params;
    JpegDecodeControl request;
    u8 *saved_work_end;
    u8 *buffer;
    u8 *transfer_buffer;
    int size;

    saved_work_end = WorkEnd;
    if (command == HDD_SCREEN_INSTALL) {
        if (xglHddCheck()) {
            command = HDD_SCREEN_MESSAGE_FIRST;
        }
    } else if (command == HDD_SCREEN_UNINSTALL) {
        if (xglHddCheck2()) {
            xglHddActivate(0);
            command = HDD_SCREEN_REACTIVATE;
        }
    }

    buffer = WorkEnd;
    size = xglCdReadFile(hddInstallImagePath, buffer, 0, CD_NOTIFY_DUMMY);
    transfer_buffer = (u8 *)(((u32)buffer + size + 0x3f) & ~0x3f);
    WorkEnd = &transfer_buffer[HDD_WORK_AFTER_TRANSFER];

    params.image_data = buffer;
    params.step = 0;
    params.transfer_buffer = transfer_buffer;
    memset(&request, 0, sizeof(request));
    request.source = &buffer[HDD_ARCHIVE_HEADER_SIZE];
    request.destination = transfer_buffer;
    xglJpegDecode(&request);
    HddInstallParam = &params;
    HddInstallSync();
    xglFontPrintDirectOT(-1, installScreenFontText);

    switch (command) {
    case HDD_SCREEN_INSTALL:
        TitleHddInstallSubInstall();
        break;
    case HDD_SCREEN_UNINSTALL:
        TitleHddInstallSubUninstall();
        break;
    case HDD_SCREEN_MESSAGE_LAST:
    case HDD_SCREEN_MESSAGE_FIRST + 1:
    case HDD_SCREEN_MESSAGE_FIRST:
        {
            PadRecord *pad = &PadData[0];
            int frames = HDD_INPUT_DELAY;

            while (1) {
                xglFontPrint(HDD_MESSAGE_X, HDD_MESSAGE_Y, -1, hddScreenMessages[command - HDD_SCREEN_MESSAGE_FIRST]);
                xglFontPrintDirectOT(-1, hddInstallAbortedText);
                HddInstallSync();
                if (frames > 0) {
                    frames--;
                    continue;
                }
                if ((pad->pressed & PAD_CIRCLE) != 0) {
                    break;
                }
            }
            seVoiceStartDefault(HDD_SE_DECIDE);
        }
        break;
    case HDD_SCREEN_REACTIVATE:
        {
            PadRecord *pad = &PadData[0];
            int frames = HDD_INPUT_DELAY;

            while (1) {
                xglFontPrint(HDD_MESSAGE_X, HDD_MESSAGE_Y, -1, hddNotAccessibleText);
                xglFontPrintDirectOT(-1, hddUninstallAbortedText);
                HddInstallSync();
                if (frames > 0) {
                    frames--;
                    continue;
                }
                if ((pad->pressed & PAD_CIRCLE) != 0) {
                    break;
                }
            }
            seVoiceStartDefault(HDD_SE_DECIDE);
            xglHddActivate(0x103);
            xglHddCheck();
        }
        break;
    }

    HddInstallSync();
    WorkEnd = saved_work_end;
}
