#include "common.h"
#include "shared.h"
#include "main/ysegf.h"
#include "main/xgl_thread.h"
#include "main/gifState.h"
#include "main/game_camera.h"
#include "main/xgl_cd_arena.h"
#include "main/bg_layer.h"
#include "main/font_obj.h"
#include "main/menu_draw.h"
#include "main/xenoActor.h"
#include "main/xgl_cd.h"
#include "main/yg2path.h"
#include "main/cfeFileControl.h"
#include "main/yg2_text.h"
#include "main/pad.h"
#include "main/xgl_camera.h"
#include "main/xgl_studio.h"
#include "main/se_bank_load.h"
#include "main/game_data.h"
#include "main/assert_fail.h"
#include "main/xgl_font.h"
#include "main/test_129.h"
#include "main/xgl_packet.h"
#include "main/render_sprite.h"
#include "main/nml_model_set.h"
#include "main/res_get_path.h"

#include "main/xgl_2.h"
/* Page shown by yg2PathDraw; yg2PathMode selects it and yg2PathRun dispatches the pad input on it. */
enum YgPathPage {
    YG2PATH_PAGE_MENU = 0,
    YG2PATH_PAGE_GRID = 1,
    YG2PATH_PAGE_MESSAGE = 2,
    YG2PATH_PAGE_PROFILE = 3
};

/* Quest status: the low nibble of a questFlags byte (YG2PATH_STATUS_MASK); each value selects the message
   text of the matching YgQuestRecord field: message, messageSuccess, messageFailed. */
enum YgPathStatus {
    YG2PATH_STATUS_IN_PROGRESS = 1,
    YG2PATH_STATUS_SUCCESS = 2,
    YG2PATH_STATUS_FAILED = 3
};

/* Sound ids passed to seVoiceStartDefault: cursor moves, confirm opens a page or record, cancel backs out. */
enum YgPathSound {
    YG2PATH_SE_CURSOR = 0x1000001,
    YG2PATH_SE_CONFIRM = 0x1000002,
    YG2PATH_SE_CANCEL = 0x1000008
};

#define YG2PATH_PATH_COUNT 0x24              /* questFlags entries: quest paths */
#define YG2PATH_LAST_PATH 0x23
#define YG2PATH_CITIZEN_COUNT 0x32           /* citizenFlags entries */
#define YG2PATH_COLUMNS 6                    /* paths per row of the grid */
#define YG2PATH_CELL_PITCH 44                /* grid cell width and height */
#define YG2PATH_CELL_ROW_SHIFT 22            /* odd grid rows are shifted right by this */
#define YG2PATH_STATUS_MASK 0xF              /* low nibble of a questFlags byte: availability and status */
#define YG2PATH_FLAG_NEW 0x10                /* questFlags bit that is cleared when the path detail opens */
#define YG2PATH_SCROLL_FULL 0x80             /* scroll position at which a page is fully open */
#define YG2PATH_SCROLL_STEP 0xF              /* scroll added per frame while a page opens */
#define YG2PATH_QUEST_BUFFER_SIZE 0x195D8
#define YG2PATH_CITIZEN_BUFFER_SIZE 0x29FE
#define YG2PATH_FILE_TEMP_SIZE 0x30000

/* One quest message record (0xB46 bytes per path): the text fields of QuestMes.txt. */
typedef struct YgQuestRecord {
    char subject[0x1F];
    char location[0x11];
    char client[0x11];
    char message[0x101];
    char messageSuccess[0x101];
    char messageFailed[0x101];
    char reportSuccess[0x401];
    char reportFailed[0x401];
} YgQuestRecord;

/* One citizen record (0xD7 bytes per citizen): the text fields of Citizen.txt. */
typedef struct YgCitizenRecord {
    char name[0x11];
    char sex[0x7];
    char age[0x11];
    char hobby[0x17];
    char profile[0x97];
} YgCitizenRecord;

void *memset(void *destination, int value, unsigned int count);

/* Partial view of sRender: a callback pointer at +0x48 and the word that
   follows it at +0x4C; the prefix before +0x48 is not modeled here. */
typedef struct YgPathRenderState {
    u8 unmodeled_00[0x48];
    void (*callback)(void);
    int callback_context;
} YgPathRenderState;

/* citizenFlags: one byte per citizen (0x32 of them) and questFlags: one byte
   per path (0x24 of them); the low nibble of a path byte is its status
   (YG2PATH_STATUS_*, 0 leaves the path without a message). yg2PathModelHandles.actor is the loaded model. */
extern u8 citizenFlags[];
extern u8 questFlags[];
/* The path scene's model state: the actor of the loaded citizen model and a random delay stored at load time. */
typedef struct YgPathModelState {
    struct XenoActor *actor;
    int unused_04;
    int randomDelay;
    int unmodeled_0C;
} YgPathModelState;

extern YgPathModelState yg2PathModelHandles;
extern void *yg2PathPortraitBuffer;
extern void *yg2PathCitizenAlphaBuffer;
extern YgPathRenderState sRender;

/* Returns the citizen flag at index (0x32 entries), 0 when out of range. */
int yg2PathGetCitizenFlag(u32 index) {
    if (index < 0x32U) {
        return citizenFlags[index];
    }
    return 0;
}

static YgQuestRecord *yg2PathQuestMessages;
static YgCitizenRecord *yg2PathCitizenRecords;
static unsigned char yg2PathMode;
static signed char yg2PathDetailOpen;
static signed char yg2PathDetailIndex;
static signed char yg2PathMenuChoice;
static signed char yg2PathCurrentPath;
static signed char yg2PathCurrentCitizen;
static float yg2PathCursorBox[4];
static MsgRange yg2PathMessageRange;
extern char yg2PathEmptyText[];
extern char yg2PathNumberText[];
extern char yg2PathDigitChars[];

void yg2PathShowPathMessage(void) {
    int pathNumber;

    if (questFlags[yg2PathCurrentPath] == 0) {
        msgRangeSetText(&yg2PathMessageRange, yg2PathEmptyText);
        return;
    }
    pathNumber = yg2PathCurrentPath + 1;
    yg2PathNumberText[9] = yg2PathDigitChars[(pathNumber / 10) % 10];
    yg2PathNumberText[10] = yg2PathDigitChars[pathNumber % 10];
    msgRangeSetText(&yg2PathMessageRange, yg2PathNumberText);
}

void yg2PathClearProgress(void) {
    memset(questFlags, 0, YG2PATH_PATH_COUNT);
    memset(citizenFlags, 0, YG2PATH_CITIZEN_COUNT);
}


/* Clears the path and citizen cursor state, then stores the position of the current path on its
   6-column grid (44-unit cells, odd rows shifted by 22) in the two float pairs at yg2PathCursorBox. */
void yg2PathInit(void) {
    const int columns = YG2PATH_COLUMNS;
    float cellX;
    float cellY;

    yg2PathMode = YG2PATH_PAGE_MENU;
    yg2PathMenuChoice = 0;
    yg2PathCurrentPath = 0;
    yg2PathCurrentCitizen = 0;
    yg2PathDetailOpen = 0;
    menuResetState(&yg2PathMessageRange);
    cellX = (float) ((yg2PathCurrentPath % columns) * YG2PATH_CELL_PITCH
                 + ((yg2PathCurrentPath / columns) & 1) * YG2PATH_CELL_ROW_SHIFT - 2);
    yg2PathCursorBox[2] = cellX;
    cellY = (float) ((yg2PathCurrentPath / columns) * YG2PATH_CELL_PITCH + 54);
    yg2PathCursorBox[3] = cellY;
    yg2PathCursorBox[0] = cellX;
    yg2PathCursorBox[1] = cellY;
    yg2PathModelHandles.actor = 0;
}


/* Sets the mode of the loaded yg2path model (yg2PathModelHandles.actor), when one is loaded. */
void yg2PathSetModelMode(int mode) {
    if (yg2PathModelHandles.actor != 0) {
        XenoActorSetBlendFrames(yg2PathModelHandles.actor, 7.0f);
        XenoActorSetBlendLocal(yg2PathModelHandles.actor);
        XenoActorSetMode(yg2PathModelHandles.actor, mode);
    }
}


/* Reads the quoted field that follows the first line matching `pattern`:
   copies it to `out`, turns \" and \n into their characters, drops CR LF
   pairs and unknown escapes, and returns the text after the closing quote. */
char *yg2PathReadTaggedText(char *text, const char *pattern, char *out)
{
    unsigned char ch;
    unsigned char nextCh;
    char *outPos;
    int quoteCh;

    while (*text != (char) *pattern || yg2TextCompareLine(pattern, text) != 0) {
        text++;
    }
    while (*text != '"') {
        text++;
    }
    text++;
    ch = *text;
    quoteCh = '"';
    nextCh = ch;
    if ((signed char) nextCh != quoteCh) {
        outPos = out;
        do {
            if (nextCh == '\r' && (unsigned char) text[1] == '\n') {
                text += 2;
            } else if ((signed char) ch < 0) {
                char *trailOut = outPos + 1;
                *outPos = ch;
                *trailOut = text[1];
                sjisToEuc(outPos, trailOut);
                outPos += 2;
                text += 2;
            } else if ((signed char) ch == '\\') {
                switch (text[1]) {
                case 'n':
                    *outPos++ = '\n';
                    break;
                case '"':
                    *outPos++ = '"';
                }
                text += 2;
            } else {
                *outPos++ = ch;
                text++;
            }
            nextCh = *text;
            ch = nextCh;
        } while ((signed char) nextCh != '"');
    }
    return text + 1;
}


void yg2PathWaitForLoad(void) {
    while (xglCdServiceIfPending() != 0) {
        XenoActorSetFlagAll(2);
        bgLayerClearAllFlags();
        XenoActorDoit();
        XenoActorDisp();
        xglSleep();
    }
}

extern const char yg2PathSubjectTagFormat[];
extern const char yg2PathLocationTagFormat[];
extern const char yg2PathClientTagFormat[];
extern const char yg2PathMesIpgTagFormat[];
extern const char yg2PathMesScdTagFormat[];
extern const char yg2PathMesFldTagFormat[];
extern const char yg2PathUkunScdTagFormat[];
extern const char yg2PathUkunFldTagFormat[];
extern const char yg2PathQuestMessagePath[];
extern const char yg2PathSourceName[];
extern char yg2PathMessageBufferAssertText[];

/* Reads QuestMes.txt into yg2PathQuestMessages: one record per path, its text fields in the order of the tags
   read below. */
void yg2PathLoadQuestMessages(void)
{
    void *text = 0;
    int i;
    int reserveSize = YG2PATH_QUEST_BUFFER_SIZE;
    const char *tagSubject = yg2PathSubjectTagFormat;
    const char *tagLocation = yg2PathLocationTagFormat;
    const char *tagClient = yg2PathClientTagFormat;
    const char *tagMessage = yg2PathMesIpgTagFormat;
    const char *tagMessageSuccess = yg2PathMesScdTagFormat;
    const char *tagMessageFailed = yg2PathMesFldTagFormat;
    const char *tagReportSuccess = yg2PathUkunScdTagFormat;
    const char *tagReportFailed = yg2PathUkunFldTagFormat;

    yg2PathQuestMessages = (YgQuestRecord *) xglCdBufferReserve(reserveSize);
    memset(yg2PathQuestMessages, 0, reserveSize);
    ysegfReadFile(&text, yg2PathQuestMessagePath, 1, 1);
    if (text == 0) {
        assertFailed(yg2PathMessageBufferAssertText, yg2PathSourceName, 0x2DB);
    }
    yg2PathWaitForLoad();
    for (i = 0; i < YG2PATH_PATH_COUNT; i++) {
        text = yg2PathReadTaggedText(text, tagSubject, yg2PathQuestMessages[i].subject);
        text = yg2PathReadTaggedText(text, tagLocation, yg2PathQuestMessages[i].location);
        text = yg2PathReadTaggedText(text, tagClient, yg2PathQuestMessages[i].client);
        text = yg2PathReadTaggedText(text, tagMessage, yg2PathQuestMessages[i].message);
        text = yg2PathReadTaggedText(text, tagMessageSuccess, yg2PathQuestMessages[i].messageSuccess);
        text = yg2PathReadTaggedText(text, tagMessageFailed, yg2PathQuestMessages[i].messageFailed);
        text = yg2PathReadTaggedText(text, tagReportSuccess, yg2PathQuestMessages[i].reportSuccess);
        text = yg2PathReadTaggedText(text, tagReportFailed, yg2PathQuestMessages[i].reportFailed);
    }
}

extern const char yg2PathNameTagFormat[];
extern const char yg2PathHobbyTagFormat[];
extern const char yg2PathProfileTagFormat[];
extern const char yg2PathCitizenFilePath[];
extern char yg2PathSexTagFormat[];
extern char yg2PathAgeTagFormat[];

/* Reads Citizen.txt into yg2PathCitizenRecords: one record per citizen. */
void yg2PathLoadCitizens(void)
{
    void *text = 0;
    int i;
    const char *tagName = yg2PathNameTagFormat;
    const char *tagSex = yg2PathSexTagFormat;
    const char *tagAge = yg2PathAgeTagFormat;
    const char *tagHobby = yg2PathHobbyTagFormat;
    const char *tagProfile = yg2PathProfileTagFormat;

    yg2PathCitizenRecords = (YgCitizenRecord *) xglCdBufferReserve(YG2PATH_CITIZEN_BUFFER_SIZE);
    memset(yg2PathCitizenRecords, 0, YG2PATH_CITIZEN_BUFFER_SIZE);
    ysegfReadFile(&text, yg2PathCitizenFilePath, 1, 0);
    if (text == 0) {
        assertFailed(yg2PathMessageBufferAssertText, yg2PathSourceName, 0x30E);
    }
    yg2PathWaitForLoad();
    for (i = 0; i < YG2PATH_CITIZEN_COUNT; i++) {
        text = yg2PathReadTaggedText(text, tagName, yg2PathCitizenRecords[i].name);
        text = yg2PathReadTaggedText(text, tagSex, yg2PathCitizenRecords[i].sex);
        text = yg2PathReadTaggedText(text, tagAge, yg2PathCitizenRecords[i].age);
        text = yg2PathReadTaggedText(text, tagHobby, yg2PathCitizenRecords[i].hobby);
        text = yg2PathReadTaggedText(text, tagProfile, yg2PathCitizenRecords[i].profile);
    }
}

extern float yg2PathModelStartX[];

/* Partial view of the XenoActor record the path scene creates, for the accesses below that the original makes through
   a union view (alias set 0: it reloads the handle after each store; struct XenoActor's plain vectors would not). */
typedef struct YgPathActor {
    unsigned char unmodeled_00[0x10];
    DoublewordVector4 position;      /* 0x010 */
    unsigned char unmodeled_20[0x50 - 0x20];
    DoublewordVector4 rotation;      /* 0x050 */
    unsigned char unmodeled_60[0xBC - 0x60];
    int drawFlags;                   /* 0x0BC */
    unsigned char unmodeled_c0[0x4C0 - 0xC0];
    unsigned int visibilityFlags;    /* 0x4C0: first word of the visibility block */
} YgPathActor;

/* Partial view of the xgl camera (main/xgl_camera.h XglCamera: its travel record at +0x90) with the travel
 * vectors this file writes. The DoublewordVector4 unions are alias set 0, which keeps the original store order
 * (through the XglCamera fields the fov store moves ahead of the position stores). */
typedef struct YgPathCamera {
    unsigned char unmodeled_00[0x94];
    float fov;                       /* 0x094 */
    unsigned char unmodeled_98[0xA0 - 0x98];
    DoublewordVector4 rotation;      /* 0x0A0 */
    unsigned char unmodeled_b0[0xD0 - 0xB0];
    DoublewordVector4 position;      /* 0x0D0 */
} YgPathCamera;

extern const char yg2PathCitizenPxyName[];
extern const char yg2PathCitizenTxyName[];
extern const char yg2PathCitizenXhrName[];
extern const char yg2PathCitizenMotionName[];
extern const char yg2PathAchievementHelpText[];
extern const char yg2PathCitizenImageName[];
extern const char yg2PathCitizenImageAssertText[];
extern const char yg2PathCitizenAlphaImageName[];
extern const char yg2PathCitizenAlphaAssertText[];
extern char yg2PathPxyAssertText[];
extern char yg2PathTxyAssertText[];
extern char yg2PathXhrAssertText[];
extern char yg2PathXapAssertText[];
extern YgPathCamera *yg2PathCamera;
extern unsigned char yg2PathImageFade;

extern int yg2PathScroll;

/* One-byte state cell view. The two byte stores in yg2PathLoadScene go through it, which gives them an alias set of
 * their own (a plain unsigned char store is alias set 0 and schedules after the camera stores instead). */
typedef struct Yg2PathFlag {
    unsigned char value;
} Yg2PathFlag;

void yg2PathLoadScene(void) {
    int modelFiles[4];
    struct XenoActor *actor;
    YgPathCamera *camera;
    Yg2PathFlag *pageMode;
    Yg2PathFlag *menuChoice;

    yg2PathDetailOpen = 0;
    modelFiles[0] = 0;
    modelFiles[1] = 0;
    modelFiles[2] = 0;
    modelFiles[3] = 0;
    yg2PathScroll = 0;
    yg2PathWaitForLoad();
    menuResetState(&yg2PathMessageRange);
    cfeFileTempInit((void *) xglCdBufferReserve(YG2PATH_FILE_TEMP_SIZE), YG2PATH_FILE_TEMP_SIZE);
    yg2PathLoadQuestMessages();
    yg2PathLoadCitizens();
    yg2PathModelHandles.actor = 0;
    ysegfReadFile((void **) &modelFiles[0], yg2PathCitizenPxyName, 1, 1);
    ysegfReadFile((void **) &modelFiles[1], yg2PathCitizenTxyName, 1, 1);
    ysegfReadFile((void **) &modelFiles[2], yg2PathCitizenXhrName, 1, 1);
    ysegfReadFile((void **) &modelFiles[3], yg2PathCitizenMotionName, 1, 1);
    yg2PathWaitForLoad();
    if (modelFiles[0] == 0) {
        assertFailed(yg2PathPxyAssertText, yg2PathSourceName, 842);
    }
    if (modelFiles[1] == 0) {
        assertFailed(yg2PathTxyAssertText, yg2PathSourceName, 843);
    }
    if (modelFiles[2] == 0) {
        assertFailed(yg2PathXhrAssertText, yg2PathSourceName, 844);
    }
    if (modelFiles[3] == 0) {
        assertFailed(yg2PathXapAssertText, yg2PathSourceName, 845);
    }
    actor = XenoActorAlloc();
    actor->rsrc[XAct_Rsrc_MDL] = modelFiles[0];
    actor->shadowType = 0;
    yg2PathModelHandles.actor = actor;
    actor->rsrc[XAct_Rsrc_TEX] = modelFiles[1];
    actor->rsrc[XAct_Rsrc_XHR] = modelFiles[2];
    actor->rsrc[XAct_Rsrc_XAP] = modelFiles[3];
    XenoActorAllocMatrices(actor);
    XenoActorSetBlendFrames(yg2PathModelHandles.actor, 0.0f);
    XenoActorSetBlendLocal(yg2PathModelHandles.actor);
    XenoActorSetMode(yg2PathModelHandles.actor, 0);
    ((YgPathActor *) yg2PathModelHandles.actor)->position.vector.x = yg2PathModelStartX[0];
    ((YgPathActor *) yg2PathModelHandles.actor)->position.vector.y = 29.4f;
    ((YgPathActor *) yg2PathModelHandles.actor)->position.vector.z = 10.0f;
    ((YgPathActor *) yg2PathModelHandles.actor)->rotation.vector.x = 0.0f;
    ((YgPathActor *) yg2PathModelHandles.actor)->rotation.vector.y = 0.0f;
    ((YgPathActor *) yg2PathModelHandles.actor)->rotation.vector.z = 0.0f;
    yg2PathModelHandles.unused_04 = 0;
    yg2PathModelHandles.randomDelay = xglSRand() % 200U + 100;
    ((YgPathActor *) yg2PathModelHandles.actor)->visibilityFlags |= 4;
    xglCameraSetWindow(xglStudioSelectGetActiveCamera(0), 0, 0, 512, 448);
    camera = (YgPathCamera *) xglStudioSelectGetActiveCamera(1);
    yg2PathCamera = camera;
    camera->position.vector.y = 30.0f;
    camera->position.vector.z = 14.0f;
    camera->position.vector.x = 0.0f;
    camera->rotation.vector.x = 0.0f;
    camera->rotation.vector.y = 0.0f;
    camera->rotation.vector.z = 0.0f;
    camera->fov = 0.69813174f;
    pageMode = (Yg2PathFlag *) &yg2PathMode;
    menuChoice = (Yg2PathFlag *) &yg2PathMenuChoice;
    *pageMode = (Yg2PathFlag) { YG2PATH_PAGE_MENU };
    *menuChoice = (Yg2PathFlag) { 0 };
    msgRangeSetText(&yg2PathMessageRange, yg2PathAchievementHelpText);
    yg2PathPortraitBuffer = ysegfAllocFileBuffer(yg2PathCitizenImageName);
    if (yg2PathPortraitBuffer == 0) {
        assertFailed(yg2PathCitizenImageAssertText, yg2PathSourceName, 898);
    }
    yg2PathImageFade = 0;
    ysegfReadFile(&yg2PathCitizenAlphaBuffer, yg2PathCitizenAlphaImageName, 1, 1);
    if (yg2PathCitizenAlphaBuffer == 0) {
        assertFailed(yg2PathCitizenAlphaAssertText, yg2PathSourceName, 901);
    }
    yg2PathWaitForLoad();
}


void yg2PathUnload(void) {
    if (yg2PathModelHandles.actor != 0) {
        XenoActorRelease(yg2PathModelHandles.actor);
        yg2PathModelHandles.actor = 0;
    }
    yg2PathPortraitBuffer = 0;
    yg2PathCitizenAlphaBuffer = 0;
    xglCdBufferRewind();
    pxySetCameraEnable(1, 0);
    sRender.callback = xglCallbackQueueRun;
    sRender.callback_context = 0;
}

extern int yg2PathFrameDepth;

void yg2PathDrawFramedImage(int x, int y, int width, int height) {
    fontObjSetOpacity(0x80);
    fontObjSetDepth(yg2PathFrameDepth);
    menuDrawCellSized(0x1EB, x, y, width, height, 0x20010);
    fontObjSetDepth(0);
    panelDrawNineSlice(yg2PathFrameImage, x, y, width, height);
    panelDrawNineSlice(yg2PathFrameBorderImage, x - 5, y - 5, width + 10, height + 10);
}

/* Submits the request set for the given mode: the low two bits pick the
   variant, and the third argument is not read. */
void yg2PathDrawCellFrame(int x, int y, int unused, int mode)
{
    int variant = mode & 3;

    if (variant != 0) {
        menuDrawCell(0x1FD, x, y, 0);
        menuDrawCell(0x1FF, x, y, 1);
        if (variant == 3) {
            menuDrawCell(0x200, x + 0xC, y + 0x1C, 0);
            menuDrawCell(0x201, x + 0x5, y + 0x14, 1);
        } else if (variant == 2) {
            menuDrawCell(0x1FB, x + 0xC, y + 0x1C, 0);
            menuDrawCell(0x1FC, x + 0x5, y + 0x14, 1);
        }
    } else {
        menuDrawCell(0x1FE, x, y, 1);
    }
}

void yg2PathDrawPageChrome(void) {
    fontObjSetOpacity(0x80);
    if (yg2PathMode < YG2PATH_PAGE_MESSAGE) {
        if (yg2PathMode == YG2PATH_PAGE_MENU) {
            fontObjSetDepth(0x1FFFF);
            menuDrawCellSized(0x206, 0, 0x92, 0x200, 0x8C, 0x20001);
            menuDrawCell(0x18E, 0, 0x30, 0x20001);
            menuDrawCell(0x1FA, 0x164, 0x6F, 0x20001);
            fontObjSetDepth(0);
        }
        menuDrawCell(0x190, 0, 0x14, 1);
        menuDrawCell(0x18D, 0, 0, 1);
        menuDrawCell(0x208, 0x10, 0x12, 1);
    }
    if (yg2PathDetailOpen == 0) {
        menuDrawCell(0x207, 0, 0x170, 1);
        menuDrawCell(0x18F, 0, 0x170, 1);
    }
    fontObjResetOpacity();
}

void yg2PathDrawMainMenu(int opacity) {
    fontObjSetOpacity(opacity);
    if (yg2PathMenuChoice == 0) {
        fontObjResetColor();
    } else {
        fontObjSetColor(0x3F80000040808080ULL);
    }
    menuDrawCell(504, 311, 174, 5);
    menuDrawCell(505, 275, 149, 5);
    menuDrawCell(501, 326, 190, 4);
    menuDrawCell(502, 326, 190, 5);
    menuDrawCell(500, 290, 165, 4);
    menuDrawCell(503, 290, 165, 5);
    if (yg2PathMenuChoice == 0) {
        fontObjSetColor(0x3F80000080000000ULL);
        menuDrawCell(496, 347, 197, 4);
        fontObjSetColor(0x3F80000080505080ULL);
        menuDrawCell(496, 346, 194, 4);
        fontObjSetColor(0x3F80000040808080ULL);
    } else {
        fontObjSetColor(0x3F80000040000000ULL);
        menuDrawCell(497, 347, 197, 4);
        fontObjSetColor(0x3F80000040808080ULL);
        menuDrawCell(497, 346, 194, 4);
        fontObjResetColor();
    }
    menuDrawCell(504, 299, 222, 5);
    menuDrawCell(505, 263, 197, 5);
    menuDrawCell(501, 314, 238, 4);
    menuDrawCell(502, 314, 238, 5);
    menuDrawCell(500, 278, 213, 4);
    menuDrawCell(503, 278, 213, 5);
    if (yg2PathMenuChoice == 0) {
        fontObjSetColor(0x3F80000040000000ULL);
        menuDrawCell(499, 327, 245, 4);
        fontObjSetColor(0x3F80000040808080ULL);
        menuDrawCell(499, 326, 242, 4);
        menuDrawCell(492, 276, 149, 1);
        menuDrawCell(493, 276, 149, 1);
    } else {
        fontObjSetColor(0x3F80000080000000ULL);
        menuDrawCell(498, 327, 245, 4);
        fontObjSetColor(0x3F80000080505080ULL);
        menuDrawCell(498, 326, 242, 4);
        menuDrawCell(492, 264, 197, 1);
        menuDrawCell(493, 264, 197, 1);
    }
    if (yg2PathScroll >= YG2PATH_SCROLL_FULL) {
        yg2PathDrawFramedImage(40, 80, 216, 254);
    } else {
        float fraction = (float) yg2PathScroll * 0.0078125f;

        yg2PathDrawFramedImage(40, (int) (207.0f - fraction * 127.0f), 216, (int) (fraction * 254.0f));
    }
    fontObjResetColor();
    fontObjResetOpacity();
}


void yg2PathDrawPathGrid(int opacity)
{
    int i;
    int columns;
    int x;
    int y;
    int status;
    signed char currentPath;
    float fraction;

    columns = YG2PATH_COLUMNS;
    fontObjSetOpacity(opacity);
    for (i = 0; i < YG2PATH_PATH_COUNT; i++) {
        x = 18 + (i % columns) * YG2PATH_CELL_PITCH + ((i / columns) & 1) * YG2PATH_CELL_ROW_SHIFT;
        y = 74 + (i / columns) * YG2PATH_CELL_PITCH;
        yg2PathDrawCellFrame(x, y, i, questFlags[i]);
    }
    i = 0;
    {
        int cols = YG2PATH_COLUMNS;

        while (i < YG2PATH_PATH_COUNT) {
            x = 28 + (i % cols) * YG2PATH_CELL_PITCH + ((i / cols) & 1) * YG2PATH_CELL_ROW_SHIFT;
            y = 86 + (i / cols) * YG2PATH_CELL_PITCH;
            status = questFlags[i] & 3;
            if (status != 0) {
                i++;
                textDrawDigitsZero(x, y, i, 2);
            } else {
                i++;
            }
        }
    }
    currentPath = yg2PathCurrentPath;
    yg2PathCursorBox[2] = (float) (YG2PATH_CELL_PITCH * (currentPath % YG2PATH_COLUMNS) - 2 + YG2PATH_CELL_ROW_SHIFT * ((currentPath / YG2PATH_COLUMNS) & 1));
    yg2PathCursorBox[3] = (float) (54 + YG2PATH_CELL_PITCH * (currentPath / YG2PATH_COLUMNS));
    menuDrawCell(0x1EE, (int) yg2PathCursorBox[0], (int) yg2PathCursorBox[1], 1);
    menuDrawCell(0x1EF, (int) yg2PathCursorBox[0], (int) yg2PathCursorBox[1], 1);
    if (yg2PathScroll >= YG2PATH_SCROLL_FULL) {
        yg2PathDrawFramedImage(0x13D, 0x61, 0xB2, 0xDD);
    } else {
        fraction = (float) yg2PathScroll * 0.0078125f;
        yg2PathDrawFramedImage(0x13D, (int) (207.0f - fraction * 110.0f), 0xB2, (int) (fraction * 221.0f));
    }
    fontObjResetOpacity();
}

/* Draws the details prompt of a path; the new badge is drawn only when newFlag is set. */
void yg2PathDrawDetailPrompt(int x, int y, int newFlag) {
    fontObjSetColor(0x80000000ULL);
    if (newFlag != 0) {
        menuDrawCell(0x2D8, x, y, 0x10004);
    }
    menuDrawCell(0x2D7, x + 12, y - 28, 0x10004);
    fontObjResetColor();
    fontObjSetColor(0x80808080ULL);
    menuDrawCell(0x2D6, x + 12, y - 28, 0x10001);
    menuDrawCell(0x2DA, x + 85, y - 17, 0x10001);
    if (newFlag != 0) {
        menuDrawCell(0x2D9, x + 35, y - 1, 0x10001);
    }
    menuDrawKindIcon(x + 68, y - 29, 3);
}

/* 24-byte file-name record: a u64 pair, an int and a flag byte (the scaffold items at 0x68B090 and 0x68B0A8). */
typedef struct YgCitizenFileName {
    unsigned long long first;
    unsigned long long second;
    int third;
    unsigned char fourth;
} YgCitizenFileName;

/* Portrait file path buffer: the 20/21-byte directory prefix is copied in as 64-bit words, then sprintf appends the
   number to the prefix it reads from the same buffer. */
typedef union YgCitizenFileRequest {
    YgCitizenFileName prefix;
    char path[0x40];
} YgCitizenFileRequest;

int sprintf(char *destination, const char *format, ...);
extern int yg2PathPanelImage[];
extern const char *yg2PathNoDataText;
extern const char yg2PathTextHeader[];
extern const YgCitizenFileName yg2PathPortraitPathPrefix;
extern const YgCitizenFileName yg2PathPortraitPathPrefixPadded[];
extern const char yg2PathPortraitFormat[];
extern char yg2PathDataTitleText[];
extern char yg2PathProfileTitleText[];
extern char yg2PathNameLabelText[];
extern char yg2PathSexLabelText[];
extern char yg2PathAgeLabelText[];
extern char yg2PathHobbyLabelText[];
extern signed char yg2PathPortraitIndexTable[];
extern int yg2PathMessagePanelImage[];
extern char yg2PathSubjectTitleText[];
extern const char yg2PathLocationTitleText[];
extern char yg2PathMessageTitleText[];
extern char yg2PathClientTitleText[];

/* Message page: the current path's quest message record and its
   questFlags byte (low nibble: availability, bit 0x10: flag). */
void yg2PathDrawMessagePage(int scroll)
{
  YgCitizenFileRequest fileRequest;
  unsigned int fadeNext;
  int questState;

  fontObjSetOpacity(scroll);
  panelDrawNineSlice(yg2PathMessagePanelImage, 12, 15, 415, 89);
  menuDrawCell(454, 32, 15, 1);
  textDrawDigitsWideAlt(74, 15, yg2PathCurrentPath + 1, 2);
  menuDrawOutlineWindow(24, 40, 391, 50);
  menuDrawTitledFrameBlue(29, 47, 235, 37, yg2PathSubjectTitleText);
  menuDrawTitledFrameBlue(267, 47, 144, 37, yg2PathLocationTitleText);
  panelDrawNineSlice(yg2PathPanelImage, 26, 109, 460, 250);
  menuDrawCell(490, 53, 114, 1);
  menuDrawCell(418, 34, 140, 1);
  menuDrawCell(428, 462, 140, 0);
  menuDrawCell(420, 462, 140, 1);
  menuDrawCellSized(419, 50, 140, 412, 16, 1);
  menuDrawCellSized(417, 34, 156, 16, 172, 1);
  menuDrawCellSized(427, 462, 156, 16, 155, 0);
  menuDrawCell(424, 34, 328, 0);
  menuDrawCell(416, 34, 328, 1);
  menuDrawCell(426, 462, 311, 0);
  menuDrawCell(436, 320, 324, 0);
  menuDrawCellSized(425, 50, 328, 270, 16, 0);
  menuDrawCellSized(425, 352, 311, 110, 16, 0);
  menuDrawOutlineWindow(54, 148, 130, 148);
  menuDrawOutlineWindow(200, 148, 265, 137);
  menuDrawTitledFrameRed(206, 155, 255, 123, yg2PathMessageTitleText);
  menuDrawTitledFrameRed(44, 300, 150, 37, yg2PathClientTitleText);
  menuDrawCellSized(517, 208, 187, 249, 5, 1);
  menuDrawCellSized(517, 208, 207, 249, 5, 1);
  menuDrawCellSized(517, 208, 227, 249, 5, 1);
  menuDrawCellSized(517, 208, 247, 249, 5, 1);
  menuDrawCellSized(517, 208, 267, 249, 5, 1);
  menuDrawOutlineWindow(215, 290, 240, 30);
  menuDrawCell(531, 225, 298, 1);
  fontObjSetColor(0x80000000ULL);
  menuDrawCell(532, 225, 298, 4);
  fontObjResetColor();
  menuDrawCell(525, 258, 294, 1);
  menuDrawCellSized(526, 266, 294, 168, 22, 1);
  menuDrawCell(527, 434, 294, 1);
  menuDrawCell(528, 258, 294, 0);
  menuDrawCellSized(529, 266, 294, 168, 22, 0);
  menuDrawCell(530, 434, 294, 0);

  questState = questFlags[yg2PathCurrentPath] & YG2PATH_STATUS_MASK;
  switch (questState) {
  case YG2PATH_STATUS_FAILED:
    menuDrawCell(512, 229, 303, 0);
    menuDrawCell(513, 222, 295, 1);
    break;
  case YG2PATH_STATUS_SUCCESS:
    menuDrawCell(507, 229, 303, 0);
    menuDrawCell(508, 222, 295, 1);
    break;
  }
  menuDrawCell(457, 1, 220, 1);
  menuDrawCell(457, 488, 220, 1);
  menuDrawCell(455, 1, 220, 1);
  menuDrawCell(456, 488, 220, 1);
  menuDrawCell(28, 372, 333, 64);
  textDrawDigitsPair(416, 333, yg2PathCurrentPath + 1, 36, 2);
  textDrawSegment(0, 0, yg2PathTextHeader, 0);
  textDrawSegment(38, 59, yg2PathQuestMessages[yg2PathCurrentPath].subject, 0);
  textDrawSegment(276, 59, yg2PathQuestMessages[yg2PathCurrentPath].location, 0);
  textDrawSegment(59, 311, yg2PathQuestMessages[yg2PathCurrentPath].client, 0);
  menuDrawCellSized(757, 258, 294, 184, 22, 0x10000);
  fontObjSetColor(0x80000000ULL);
  menuDrawCell(721, 258, 294, 0x10004);
  fontObjResetColor();

  questState = questFlags[yg2PathCurrentPath] & YG2PATH_STATUS_MASK;
  switch (questState) {
  case YG2PATH_STATUS_IN_PROGRESS:
    menuDrawCell(723, 290, 294, 0x10001);
    textDrawSegment(221, 169, yg2PathQuestMessages[yg2PathCurrentPath].message, 0);
    break;
  case YG2PATH_STATUS_FAILED:
    menuDrawCell(722, 293, 294, 0x10001);
    textDrawSegment(221, 169, yg2PathQuestMessages[yg2PathCurrentPath].messageFailed, 0);
    yg2PathDrawDetailPrompt(340, 114, questFlags[yg2PathCurrentPath] & YG2PATH_FLAG_NEW);
    break;
  case YG2PATH_STATUS_SUCCESS:
    menuDrawCell(725, 292, 294, 0x10001);
    textDrawSegment(221, 169, yg2PathQuestMessages[yg2PathCurrentPath].messageSuccess, 0);
    yg2PathDrawDetailPrompt(340, 114, questFlags[yg2PathCurrentPath] & YG2PATH_FLAG_NEW);
    break;
  }

  if (yg2PathImageFade != 0 && xglCdServiceIfPending() == 0) {
    xglPacketTextureTrans(yg2PathCitizenAlphaBuffer);
    renderSpriteClutAlpha(63, 159, 112, 127, 32, yg2PathImageFade);
    xglPacketTextureTrans(yg2PathPortraitBuffer);
    renderSpriteClut(63, 159, 112, 127, 32);
    fontObjBuildBaseVifPacket();
    fadeNext = yg2PathImageFade + 0x10;
    if ((fadeNext & 0xFF) >= 0x81) {
      yg2PathImageFade = -0x80;
    } else {
      yg2PathImageFade = fadeNext;
    }
    panelDrawNineSlice(yg2PathFrameImage, 63, 159, 112, 127);
  } else if (xglCdServiceIfPending() == 0) {
    if (yg2PathPortraitIndexTable[yg2PathCurrentPath] + 1 >= 10) {
      fileRequest.prefix.first = yg2PathPortraitPathPrefix.first;
      fileRequest.prefix.second = yg2PathPortraitPathPrefix.second;
      fileRequest.prefix.third = yg2PathPortraitPathPrefix.third;
    } else {
      fileRequest.prefix.first = yg2PathPortraitPathPrefixPadded[0].first;
      fileRequest.prefix.second = yg2PathPortraitPathPrefixPadded[0].second;
      fileRequest.prefix.third = yg2PathPortraitPathPrefixPadded[0].third;
      fileRequest.prefix.fourth = yg2PathPortraitPathPrefixPadded[0].fourth;
    }
    sprintf(fileRequest.path, yg2PathPortraitFormat, fileRequest.path, yg2PathPortraitIndexTable[yg2PathCurrentPath] + 1);
    ysegfReadFile(&yg2PathPortraitBuffer, fileRequest.path, 0, 1);
    yg2PathImageFade = 0x10;
  }
  fontObjResetOpacity();
}

/* Citizen profile page. The record is copied field by field from one of two
   template blocks: the 21-byte one carries the fourth byte, the 20-byte one does not. */
void yg2PathDrawCitizenProfile(int scroll)
{
  YgCitizenFileRequest fileRequest;
  unsigned int fadeNext;

  fontObjSetOpacity(scroll);
  panelDrawNineSlice(yg2PathPanelImage, 0x2F, 0x1B, 0x1A1, 0x148);
  menuDrawCell(0x203, 0x4D, 0x1F, 1);
  menuDrawCell(0x1C6, 0xD0, 0x20, 1);
  textDrawDigitsWideAlt(0xFA, 0x20, yg2PathCurrentCitizen + 1, 2);
  menuDrawCell(0x1A2, 0x37, 0x3A, 1);
  menuDrawCell(0x1AC, 0x1B8, 0x3A, 0);
  menuDrawCell(0x1A4, 0x1B8, 0x3A, 1);
  menuDrawCellSized(0x1A3, 0x47, 0x3A, 0x171, 0x10, 1);
  menuDrawCellSized(0x1A1, 0x37, 0x4A, 0x10, 0x8B, 1);
  menuDrawCellSized(0x1AB, 0x1B8, 0x4A, 0x10, 0x7A, 0);
  menuDrawCell(0x1A8, 0x37, 0xD5, 0);
  menuDrawCell(0x1A0, 0x37, 0xD5, 1);
  menuDrawCell(0x1AA, 0x1B8, 0xC4, 0);
  menuDrawCell(0x1B4, 0x118, 0xD1, 0);
  menuDrawCellSized(0x1A9, 0x47, 0xD5, 0xD1, 0x10, 0);
  menuDrawCellSized(0x1A9, 0x138, 0xC4, 0x80, 0x10, 0);
  menuDrawOutlineWindow(0x42, 0x45, 0x82, 0x94);
  menuDrawOutlineWindow(0xCD, 0x45, 0xF1, 0x81);
  menuDrawTitledFrameRed(0xD2, 0x4C, 0xE7, 0x73, yg2PathDataTitleText);
  menuDrawCellSized(0x205, 0xE6, 0x6D, 0xC6, 5, 1);
  menuDrawCellSized(0x205, 0xE6, 0x85, 0xC6, 5, 1);
  menuDrawCellSized(0x205, 0xE6, 0x9D, 0xC6, 5, 1);
  menuDrawCellSized(0x205, 0xE6, 0xB5, 0xC6, 5, 1);
  menuDrawShortOutlineFrame(0x37, 0xEA, 0x191, 0x6B);
  menuDrawTitledFrameRed(0x42, 0xF7, 0x17B, 0x53, yg2PathProfileTitleText);
  menuDrawCellSized(0x205, 0x44, 0x117, 0x177, 5, 1);
  menuDrawCellSized(0x205, 0x44, 0x12B, 0x177, 5, 1);
  menuDrawCellSized(0x205, 0x44, 0x13F, 0x177, 5, 1);
  menuDrawCell(0x1C9, 0xE, 0xB2, 1);
  menuDrawCell(0x1C9, 0x1DA, 0xB2, 1);
  menuDrawCell(0x1C7, 0xE, 0xB2, 1);
  menuDrawCell(0x1C8, 0x1DA, 0xB2, 1);
  menuDrawCell(0x1C, 0x150, 0xD9, 0x40);
  textDrawDigitsPair(0x17C, 0xD9, yg2PathCurrentCitizen + 1, 0x32, 2);
  textDrawSegment(0, 0, yg2PathTextHeader, 0);
  textDrawSegment(0xD9, 0x5A, yg2PathNameLabelText, 0);
  textDrawSegment(0xD9, 0x72, yg2PathSexLabelText, 0);
  textDrawSegment(0xD9, 0x8A, yg2PathAgeLabelText, 0);
  textDrawSegment(0xD9, 0xA2, yg2PathHobbyLabelText, 0);
  if (citizenFlags[yg2PathCurrentCitizen] != 0) {
    textDrawSegment(0x112, 0x5A, yg2PathCitizenRecords[yg2PathCurrentCitizen].name, 0);
    textDrawSegment(0x112, 0x72, yg2PathCitizenRecords[yg2PathCurrentCitizen].sex, 0);
    textDrawSegment(0x112, 0x8A, yg2PathCitizenRecords[yg2PathCurrentCitizen].age, 0);
    textDrawSegment(0x112, 0xA2, yg2PathCitizenRecords[yg2PathCurrentCitizen].hobby, 0);
    textDrawSegment(0x52, 0x105, yg2PathCitizenRecords[yg2PathCurrentCitizen].profile, 0);
    if (yg2PathImageFade != 0 && xglCdServiceIfPending() == 0) {
      xglPacketTextureTrans(yg2PathCitizenAlphaBuffer);
      renderSpriteClutAlpha(0x4B, 0x50, 0x70, 0x7F, 0x20, yg2PathImageFade);
      xglPacketTextureTrans(yg2PathPortraitBuffer);
      renderSpriteClut(0x4B, 0x50, 0x70, 0x7F, 0x20);
      fontObjBuildBaseVifPacket();
      fadeNext = yg2PathImageFade + 0x10;
      if ((fadeNext & 0xFF) >= 0x81) {
        yg2PathImageFade = -0x80;
      } else {
        yg2PathImageFade = fadeNext;
      }
      fontObjSetOpacity(yg2PathImageFade);
      panelDrawNineSlice(yg2PathFrameImage, 0x4B, 0x50, 0x70, 0x7F);
      fontObjSetOpacity(scroll);
    } else if (xglCdServiceIfPending() == 0) {
      if (yg2PathCurrentCitizen + 1 >= 10) {
        fileRequest.prefix.first = yg2PathPortraitPathPrefix.first;
        fileRequest.prefix.second = yg2PathPortraitPathPrefix.second;
        fileRequest.prefix.third = yg2PathPortraitPathPrefix.third;
      } else {
        fileRequest.prefix.first = yg2PathPortraitPathPrefixPadded[0].first;
        fileRequest.prefix.second = yg2PathPortraitPathPrefixPadded[0].second;
        fileRequest.prefix.third = yg2PathPortraitPathPrefixPadded[0].third;
        fileRequest.prefix.fourth = yg2PathPortraitPathPrefixPadded[0].fourth;
      }
      sprintf(fileRequest.path, yg2PathPortraitFormat, fileRequest.path, yg2PathCurrentCitizen + 1);
      ysegfReadFile(&yg2PathPortraitBuffer, fileRequest.path, 0, 1);
      yg2PathImageFade = 0x10;
    }
  } else {
    textDrawSegment(0x12C, 0x5A, yg2PathNoDataText, 0);
    textDrawSegment(0x12C, 0x72, yg2PathNoDataText, 0);
    textDrawSegment(0x12C, 0x8A, yg2PathNoDataText, 0);
    textDrawSegment(0x12C, 0xA2, yg2PathNoDataText, 0);
    menuDrawCell(0x1EB, 0x4B, 0x50, 0);
    panelDrawNineSlice(yg2PathFrameImage, 0x4B, 0x50, 0x70, 0x7F);
  }
  fontObjResetOpacity();
}

extern const char yg2PathDetailTextHeader[];

void yg2PathDrawPathDetail(void)
{
  float fraction;
  unsigned char questStatus;
  panelDrawNineSlice(yg2PathPanelImage, 0xD, 0x14, 0xF7, 0x194);
  menuDrawCell(0x202, 0x19, 0x19, 1);
  menuDrawShortOutlineFrame(0x15, 0x33, 0xE7, 0x167);
  menuDrawCell(0x20A, 0x176, 0x185, 1);
  menuDrawCell(0x209, 0x18B, 0x189, 1);
  textDrawSegment(0, 0, yg2PathDetailTextHeader, 0);
  questStatus = questFlags[yg2PathDetailIndex];
  if (questStatus == YG2PATH_STATUS_FAILED)
  {
    textDrawSegment(0x1E, 0x3C, yg2PathQuestMessages[yg2PathDetailIndex].reportFailed, 0);
  }
  else if (questStatus == YG2PATH_STATUS_SUCCESS)
  {
    textDrawSegment(0x1E, 0x3C, yg2PathQuestMessages[yg2PathDetailIndex].reportSuccess, 0);
  }
  menuDrawKindIcon(0x1BC, 0x189, 0);
  if (yg2PathScroll >= YG2PATH_SCROLL_FULL)
  {
    yg2PathDrawFramedImage(0x115, 0x57, 0xD8, 0xFE);
    return;
  }
  fraction = (float) yg2PathScroll * 0.0078125f;
  yg2PathDrawFramedImage(0x115, (int) (214.0f - fraction * 127.0f), 0xD8, (int) (fraction * 254.0f));
}

void yg2PathDrawPageChrome(void);
void yg2PathDrawMainMenu(int opacity);
void yg2PathDrawPathGrid(int opacity);
void yg2PathDrawMessagePage(int scroll);
void yg2PathDrawCitizenProfile(int scroll);
void yg2PathDrawPathDetail(void);

void yg2PathDraw(void)
{
  fontObjResetAndBuild();
  yg2PathDrawPageChrome();
  if (yg2PathDetailOpen != 0)
  {
    yg2PathDrawPathDetail();
  }
  else
  {
    switch (yg2PathMode)
    {
      case YG2PATH_PAGE_MENU:
        yg2PathDrawMainMenu(yg2PathScroll);
        break;

      case YG2PATH_PAGE_GRID:
        yg2PathDrawPathGrid(yg2PathScroll);
        break;

      case YG2PATH_PAGE_MESSAGE:
        yg2PathDrawMessagePage(yg2PathScroll);
        break;

      case YG2PATH_PAGE_PROFILE:
        yg2PathDrawCitizenProfile(yg2PathScroll);
        break;

    }

  }
  if (yg2PathDetailOpen == 0)
  {
    msgRangeDrawRevealed(&yg2PathMessageRange, 0x18, 0x175);
  }
  xglFontFlush();
}

void yg2PathDrawFrame(void) {
    yg2PathDraw();
    gifStateSuppressDrawHook();
}

extern const char yg2PathProfileHelpText[];
extern const char yg2PathPageSwitchHelpText[];

int yg2PathHandleMenuInput(void) {
    if ((PadData[0].pressed & PAD_CROSS) != 0) {
        seVoiceStartDefault(YG2PATH_SE_CANCEL);
        return 1;
    }
    if ((PadData[0].directionRepeated & (PAD_UP | PAD_DOWN)) != 0) {
        yg2PathMenuChoice = yg2PathMenuChoice ^ 1;
        yg2PathSetModelMode(yg2PathMenuChoice);
        if (yg2PathMenuChoice != 0) {
            msgRangeSetText(&yg2PathMessageRange, yg2PathProfileHelpText);
        } else {
            msgRangeSetText(&yg2PathMessageRange, yg2PathAchievementHelpText);
        }
        seVoiceStartDefault(YG2PATH_SE_CURSOR);
    }
    if ((PadData[0].pressed & PAD_CIRCLE) != 0) {
        if (yg2PathMenuChoice != 0) {
            msgRangeSetText(&yg2PathMessageRange, yg2PathPageSwitchHelpText);
            yg2PathMode = YG2PATH_PAGE_PROFILE;
        } else {
            yg2PathShowPathMessage();
            yg2PathCursorBox[0] = yg2PathCursorBox[2];
            yg2PathCursorBox[1] = yg2PathCursorBox[3];
            yg2PathMode = YG2PATH_PAGE_GRID;
            yg2PathSetModelMode(2);
        }
        seVoiceStartDefault(YG2PATH_SE_CONFIRM);
        yg2PathScroll = 0;
    }
    return 0;
}

int yg2PathHandleGridInput(void)
{

    if ((PadData[0].pressed & PAD_CROSS) != 0) {
        msgRangeSetText(&yg2PathMessageRange, yg2PathAchievementHelpText);
        yg2PathSetModelMode(0);
        seVoiceStartDefault(YG2PATH_SE_CANCEL);
        yg2PathScroll = 0;
        return YG2PATH_PAGE_MENU;
    }
    if ((PadData[0].pressed & PAD_CIRCLE) != 0 && (questFlags[yg2PathCurrentPath] & YG2PATH_STATUS_MASK) != 0) {
        msgRangeSetText(&yg2PathMessageRange, yg2PathPageSwitchHelpText);
        seVoiceStartDefault(YG2PATH_SE_CONFIRM);
        yg2PathScroll = 0;
        return YG2PATH_PAGE_MESSAGE;
    }
    if ((PadData[0].directionRepeated & PAD_UP) != 0) {
        yg2PathCurrentPath -= YG2PATH_COLUMNS;
        while (yg2PathCurrentPath < 0) {
            yg2PathCurrentPath += YG2PATH_PATH_COUNT;
        }
        yg2PathShowPathMessage();
        seVoiceStartDefault(YG2PATH_SE_CURSOR);
    }
    if ((PadData[0].directionRepeated & PAD_DOWN) != 0) {
        yg2PathCurrentPath += YG2PATH_COLUMNS;
        while (yg2PathCurrentPath >= YG2PATH_PATH_COUNT) {
            yg2PathCurrentPath -= YG2PATH_PATH_COUNT;
        }
        yg2PathShowPathMessage();
        seVoiceStartDefault(YG2PATH_SE_CURSOR);
    }
    if ((PadData[0].directionRepeated & PAD_LEFT) != 0) {
        yg2PathCurrentPath -= 1;
        while (yg2PathCurrentPath < 0) {
            yg2PathCurrentPath += YG2PATH_PATH_COUNT;
        }
        yg2PathShowPathMessage();
        seVoiceStartDefault(YG2PATH_SE_CURSOR);
    }
    if ((PadData[0].directionRepeated & PAD_RIGHT) != 0) {
        yg2PathCurrentPath += 1;
        while (yg2PathCurrentPath >= YG2PATH_PATH_COUNT) {
            yg2PathCurrentPath -= YG2PATH_PATH_COUNT;
        }
        yg2PathShowPathMessage();
        seVoiceStartDefault(YG2PATH_SE_CURSOR);
    }
    do {
        yg2PathCursorBox[2] = (float) (YG2PATH_CELL_PITCH * (yg2PathCurrentPath % YG2PATH_COLUMNS) + YG2PATH_CELL_ROW_SHIFT * ((yg2PathCurrentPath / YG2PATH_COLUMNS) & 1) - 2);
        yg2PathCursorBox[3] = (float) (YG2PATH_CELL_PITCH * (yg2PathCurrentPath / YG2PATH_COLUMNS) + 54);
        return yg2PathMode;
    } while (0);
}

/* Next available path after `path`, wrapping around to the paths up to `path` itself. The two gotos enter the
   shared scan: with the structured loops (tried as for/while/do-while/infinite-for) gcc duplicates the loop-exit
   test in front of the loop instead of jumping into it, so the first scan entry has to stay a jump. */
int yg2PathFindNextPath(int path)
{
    int current;
    int start;

    current = (signed char) path;
    start = current;
    current = (signed char) (start + 1);
    if (current >= YG2PATH_PATH_COUNT) {
        current = 0;
        goto wrapped;
    }
    do {
        path = current;
    check:
        if (questFlags[current] & YG2PATH_STATUS_MASK) {
            return current;
        }
        current = (signed char) (current + 1);
        if (current >= YG2PATH_PATH_COUNT) {
            break;
        }
        goto check;
    } while (0);
    current = 0;
    for (;;) {
    wrapped:
        if (start == current) {
            return current;
        }
        if (questFlags[current] & YG2PATH_STATUS_MASK) {
            return current;
        }
        current = (signed char) (current + 1);
    }
}

/* Previous available path before `path`, wrapping around to the paths down to `path` (same scan shape as
   yg2PathFindNextPath, with the same goto entries). */
int yg2PathFindPrevPath(int path)
{
    int current;
    int start;

    current = (signed char) path;
    start = current;
    current = (signed char) (start - 1);
    if (current < 0) {
        current = YG2PATH_LAST_PATH;
        goto wrapped;
    }
    do {
        path = current;
    check:
        if (questFlags[current] & YG2PATH_STATUS_MASK) {
            return current;
        }
        current = (signed char) (current - 1);
        if (current < 0) {
            break;
        }
        goto check;
    } while (0);
    current = YG2PATH_LAST_PATH;
    for (;;) {
    wrapped:
        if (start == current) {
            return current;
        }
        if (questFlags[current] & YG2PATH_STATUS_MASK) {
            return current;
        }
        current = (signed char) (current - 1);
    }
}

int yg2PathHandlePathInput(void)
{
    int current;

    if ((PadData[0].pressed & PAD_CIRCLE) != 0) {
        int mode;
        int flags;

        flags = questFlags[yg2PathCurrentPath];
        mode = flags & YG2PATH_STATUS_MASK;
        if (mode == YG2PATH_STATUS_SUCCESS || mode == YG2PATH_STATUS_FAILED) {
            questFlags[yg2PathCurrentPath] = flags & ~YG2PATH_FLAG_NEW;
            yg2PathDetailOpen = 1;
            yg2PathDetailIndex = (unsigned char) yg2PathCurrentPath;
            seVoiceStartDefault(YG2PATH_SE_CONFIRM);
            yg2PathScroll = 0;
            if (mode == YG2PATH_STATUS_FAILED) {
                yg2PathSetModelMode(0);
            } else {
                yg2PathSetModelMode(3);
            }
        }
    } else if ((PadData[0].pressed & PAD_CROSS) != 0) {
        yg2PathShowPathMessage();
        yg2PathCursorBox[0] = yg2PathCursorBox[2];
        yg2PathCursorBox[1] = yg2PathCursorBox[3];
        seVoiceStartDefault(YG2PATH_SE_CANCEL);
        yg2PathImageFade = 0;
        yg2PathScroll = 0;
        yg2PathSetModelMode(2);
        return 1;
    } else {
        if ((PadData[0].directionRepeated & (PAD_LEFT | PAD_L1)) != 0) {
            int next;
            current = yg2PathCurrentPath;
            next = yg2PathFindPrevPath(current);
            yg2PathCurrentPath = next;
            if (current != (signed char) next) {
                yg2PathImageFade = 0;
                yg2PathScroll = 0;
            }
            seVoiceStartDefault(YG2PATH_SE_CURSOR);
        }
        if ((PadData[0].directionRepeated & (PAD_RIGHT | PAD_R1)) != 0) {
            int next;
            current = yg2PathCurrentPath;
            next = yg2PathFindNextPath(current);
            yg2PathCurrentPath = next;
            if (current != (signed char) next) {
                yg2PathImageFade = 0;
                yg2PathScroll = 0;
            }
            seVoiceStartDefault(YG2PATH_SE_CURSOR);
        }
        yg2PathCursorBox[2] = (float) (YG2PATH_CELL_PITCH * (yg2PathCurrentPath % YG2PATH_COLUMNS) + YG2PATH_CELL_ROW_SHIFT * ((yg2PathCurrentPath / YG2PATH_COLUMNS) & 1) - 2);
        yg2PathCursorBox[3] = (float) (YG2PATH_CELL_PITCH * (yg2PathCurrentPath / YG2PATH_COLUMNS) + 54);
    }
    return yg2PathMode;
}

/* Pad input on the citizen page: the cross button returns to the path page; the repeat buttons move the
   current citizen one step back or forward through the 50 citizens, wrapping at both ends. */
void yg2PathHandleCitizenInput(void) {
    if ((PadData[0].pressed & PAD_CROSS) != 0) {
        msgRangeSetText(&yg2PathMessageRange, yg2PathProfileHelpText);
        yg2PathSetModelMode(1);
        yg2PathMode = YG2PATH_PAGE_MENU;
        seVoiceStartDefault(YG2PATH_SE_CANCEL);
        yg2PathImageFade = 0;
        yg2PathScroll = 0;
    } else {
        if ((PadData[0].directionRepeated & (PAD_LEFT | PAD_L1)) != 0) {
            yg2PathCurrentCitizen--;
            while (yg2PathCurrentCitizen < 0) {
                yg2PathCurrentCitizen += YG2PATH_CITIZEN_COUNT;
            }
            yg2PathImageFade = 0;
            seVoiceStartDefault(YG2PATH_SE_CURSOR);
            yg2PathScroll = 0;
        }
        if ((PadData[0].directionRepeated & (PAD_RIGHT | PAD_R1)) != 0) {
            yg2PathCurrentCitizen++;
            while (yg2PathCurrentCitizen >= YG2PATH_CITIZEN_COUNT) {
                yg2PathCurrentCitizen -= YG2PATH_CITIZEN_COUNT;
            }
            yg2PathImageFade = 0;
            seVoiceStartDefault(YG2PATH_SE_CURSOR);
            yg2PathScroll = 0;
        }
    }
}

extern float yg2PathPi;

/* Partial view of the loaded yg2path model: the facing angle at +0x54 (rotation y of XenoActor). */
typedef struct YgPathModelView {
    u8 unmodeled_00[0x54];
    float facingAngle;
} YgPathModelView;

/* Sets the camera window to the rectangle (x, y, width, height). On the path detail page with quest state 3
   the model faces pi, otherwise it faces 0. */
void yg2PathUpdateCamera(int x, int y, int width, int height) {
    if (yg2PathDetailOpen != 0 && (questFlags[yg2PathCurrentPath] & YG2PATH_STATUS_MASK) == YG2PATH_STATUS_FAILED) {
        ((YgPathModelView *) yg2PathModelHandles.actor)->facingAngle = yg2PathPi;
    } else {
        ((YgPathModelView *) yg2PathModelHandles.actor)->facingAngle = 0.0f;
    }
    xglCameraSetWindow(xglStudioSelectGetActiveCamera(1), x, y, x + width, y + height);
    pxySetCameraEnable(0, 1);
    pxySetCameraEnable(1, 1);
    pxySetCameraEnable(2, 0);
    pxySetCameraEnable(3, 0);
}

/* Read-only text and struct-held file names of the TU, in address order (0x68AE10..0x68B1C0). */
const char yg2PathSubjectTagFormat[16] = "Subject:%s\n";
const char yg2PathLocationTagFormat[16] = "Locate:%s\n";
const char yg2PathClientTagFormat[16] = "Client:%s\n";
const char yg2PathMesIpgTagFormat[16] = "MesIPG:%s\n";
const char yg2PathMesScdTagFormat[16] = "MesSCD:%s\n";
const char yg2PathMesFldTagFormat[16] = "MesFLD:%s\n";
const char yg2PathUkunScdTagFormat[16] = "UkunSCD:%s\n";
const char yg2PathUkunFldTagFormat[16] = "UkunFLD:%s\n";
const char yg2PathQuestMessagePath[24] = "iwase\\QuestMes.txt";
const char yg2PathSourceName[24] = "../yg2path.euc.c";
const char yg2PathNameTagFormat[16] = "Name:%s\n";
const char yg2PathHobbyTagFormat[16] = "Hobby:%s\n";
const char yg2PathProfileTagFormat[16] = "Profile:%s\n";
const char yg2PathCitizenFilePath[24] = "iwase\\Citizen.txt";
const char yg2PathCitizenPxyName[32] = "model\\char\\citizen\\ukun00.pxy";
const char yg2PathCitizenTxyName[32] = "model\\char\\citizen\\ukun00.txy";
const char yg2PathCitizenXhrName[32] = "model\\char\\citizen\\ukun00.xhr";
const char yg2PathCitizenMotionName[24] = "motion\\ukun00.xap";
/* JP: Look at your achievements. ♪\nPress &03 to view achievements.\nPress &04 to go back.
 * EN: Look at your achievements. (note)\nPress &03 to view achievements.\nPress &04 to go back. */
const char yg2PathAchievementHelpText[88] = "Look at your achievements. \242\366\\nPress &03 to view achievements.\\nPress &04 to go back.";
const char yg2PathCitizenImageName[32] = "iwase\\img\\citizen\\c01.xtx";
const char yg2PathCitizenImageAssertText[16] = "imgCitizen";
const char yg2PathCitizenAlphaImageName[32] = "iwase\\img\\citizen\\ctz_alpha.xtx";
const char yg2PathCitizenAlphaAssertText[16] = "imgCtzAlpha";
const char yg2PathLocationTitleText[16] = "Location";
const char yg2PathTextHeader[72] = "$init;$bg2;$shadow+1,+3,000000;$colc0c0c0;$zoom14;$han1;$prop1;$xx0,20;";
const YgCitizenFileName yg2PathPortraitPathPrefix = { .first = 0x6d695c6573617769ULL, .second = 0x657a697469635c67ULL, .third = 6511726, .fourth = 0 };
const YgCitizenFileName yg2PathPortraitPathPrefixPadded[1] = { { .first = 0x6d695c6573617769ULL, .second = 0x657a697469635c67ULL, .third = 811818094, .fourth = 0 } };
const char yg2PathPortraitFormat[32] = "%s%d.xtx\000\000\000\000\000\000\000\000NO  DATA";
const char yg2PathDetailTextHeader[72] = "$init;$bg2;$shadow+1,+3,000000;$colc0c0c0;$zoom16;$han1;$prop1;$xx0,20;";
/* JP: Find out who you've met. ♪\nPress &03 to view profiles.\nPress &04 to go back.
 * EN: Find out who you've met. (note)\nPress &03 to view profiles.\nPress &04 to go back. */
const char yg2PathProfileHelpText[80] = "Find out who you've met. \242\366\\nPress &03 to view profiles.\\nPress &04 to go back.";
const char yg2PathPageSwitchHelpText[72] = "Use the L1 and R1 buttons to switch pages.\\nPress &04 to go back.";

void yg2PathEaseCursor(void) {
    double zero = 0.0;
    float cursorX = yg2PathCursorBox[0];
    float targetX = yg2PathCursorBox[2];
    double deltaX;
    double deltaY;

    cursorX = cursorX + (targetX - cursorX) * 0.5f;
    yg2PathCursorBox[0] = cursorX;
    yg2PathCursorBox[1] = yg2PathCursorBox[1] + (yg2PathCursorBox[3] - yg2PathCursorBox[1]) * 0.5f;

    deltaX = cursorX - targetX;
    if (deltaX < zero) {
        deltaX = zero - deltaX;
    }
    if (deltaX < 1.1f) {
        yg2PathCursorBox[0] = targetX;
    }

    deltaY = yg2PathCursorBox[1] - yg2PathCursorBox[3];
    if (deltaY < zero) {
        deltaY = zero - deltaY;
    }
    if (deltaY < 1.1f) {
        yg2PathCursorBox[1] = yg2PathCursorBox[3];
    }
}

const char yg2PathBgmName[24] = "201_menu";

#define CONTROL_SOFT_RESET 0x10000000

void yg2PathLoadScene(void);

void yg2PathRun(void)
{
    int exitRequest;
    int frames;

    soundRestart();
    seBgmPlay(1, yg2PathBgmName, 0x19000);
    sRender.callback = 0;
    yg2PathLoadScene();
    sRender.callback = yg2PathDrawFrame;
    exitRequest = 0;
    while (1) {
        if ((PadData[0].pressed & PAD_START) != 0) {
            if ((PadData[0].held & PAD_SOFT_RESET_HELD) == PAD_SOFT_RESET_HELD) {
                gameWork.control |= CONTROL_SOFT_RESET;
                exitRequest = 1;
            }
        }
        if ((PadData[0].pressed & PAD_TRIANGLE) != 0) {
            seVoiceStartDefault(YG2PATH_SE_CANCEL);
            exitRequest = 1;
        }
        if (exitRequest != 0) {
            break;
        }
        yg2PathScroll += YG2PATH_SCROLL_STEP;
        if (yg2PathScroll > YG2PATH_SCROLL_FULL) {
            yg2PathScroll = YG2PATH_SCROLL_FULL;
        }
        XenoActorSetFlagAll(2);
        if (yg2PathModelHandles.actor != 0) {
            ((YgPathActor *) yg2PathModelHandles.actor)->drawFlags &= ~0x2;
        }
        yg2PathEaseCursor();
        pxySetCameraEnable(1, 0);
        if (yg2PathDetailOpen != 0) {
            if (yg2PathScroll >= YG2PATH_SCROLL_FULL) {
                yg2PathUpdateCamera(0x115, 0x57, 0xD8, 0xFE);
            } else {
                yg2PathUpdateCamera(0x115, (int) (214.0f - (float) yg2PathScroll * 0.0078125f * 127.0f), 0xD8, (int) ((float) yg2PathScroll * 0.0078125f * 254.0f));
            }
            if ((PadData[0].pressed & PAD_CROSS) != 0) {
                yg2PathDetailOpen = 0;
                seVoiceStartDefault(YG2PATH_SE_CANCEL);
                yg2PathScroll = 0;
            }
        } else {
            switch (yg2PathMode) {
            case YG2PATH_PAGE_MENU:
                exitRequest = yg2PathHandleMenuInput();
                if (yg2PathScroll >= YG2PATH_SCROLL_FULL) {
                    yg2PathUpdateCamera(0x28, 0x50, 0xD8, 0xFE);
                } else {
                    yg2PathUpdateCamera(0x28, (int) (207.0f - (float) yg2PathScroll * 0.0078125f * 127.0f), 0xD8, (int) ((float) yg2PathScroll * 0.0078125f * 254.0f));
                }
                break;
            case YG2PATH_PAGE_GRID:
                yg2PathMode = yg2PathHandleGridInput();
                if (yg2PathScroll >= YG2PATH_SCROLL_FULL) {
                    yg2PathUpdateCamera(0x13D, 0x61, 0xB2, 0xDD);
                } else {
                    yg2PathUpdateCamera(0x13D, (int) (207.0f - (float) yg2PathScroll * 0.0078125f * 110.0f), 0xB2, (int) ((float) yg2PathScroll * 0.0078125f * 221.0f));
                }
                break;
            case YG2PATH_PAGE_MESSAGE:
                yg2PathMode = yg2PathHandlePathInput();
                break;
            case YG2PATH_PAGE_PROFILE:
                yg2PathHandleCitizenInput();
                break;
            }
        }
        pxyRequestOverlayPass();
        bgLayerClearAllFlags();
        XenoActorDoit();
        XenoActorDisp();
        xglSleep();
    }
    yg2PathUnload();
    if ((gameWork.control & CONTROL_SOFT_RESET) != 0) {
        return;
    }
    frames = 9;
    do {
        pxyRequestOverlayPass();
        XenoActorSetFlagAll(2);
        bgLayerClearAllFlags();
        XenoActorDoit();
        XenoActorDisp();
        xglSleep();
    } while (--frames >= 0);
    seStateRestoreDefault();
}

int yg2PathScroll = 0;

YgPathCamera *yg2PathCamera = 0;

void *yg2PathPortraitBuffer = 0;

void *yg2PathCitizenAlphaBuffer = 0;

float yg2PathPi = 3.14159203f;

/* .data of this file (0x3EC930-0x3ECAB0), address order; pt_member.c and quest_flag_set.c also read
 * questFlags and citizenFlags. */
int yg2PathMessagePanelImage[10] = { 0x1B7, 0x1B8, 0x1B9, 0x193, 0x194, 0x195, 0x1BA, 0x1BB, 0x1BC, 0x0 };

int yg2PathPanelImage[10] = { 0x199, 0x19A, 0x19B, 0x193, 0x194, 0x195, 0x196, 0x197, 0x198, 0x0 };

signed char yg2PathPortraitIndexTable[40] = {
    1, 0, 2, 26, 10, 43, 4, 5,
    7, 42, 8, 45, 30, 46, 49, 25,
    12, 28, 9, 14, 32, 19, 47, 36,
    11, 20, 17, 34, 44, 18, 27, 37,
    48, 40, 23, 39, 0, 0, 0, 0
};

/* JP: Path No. 01 achievement level. ♪\nGray = In Progress   Blue = Success   Red = Failed\nPress &03 for details. Press &04 to go back.
 * EN: Path No. 01 achievement level. (note)\nGray = In Progress   Blue = Success   Red = Failed\nPress &03 for details. Press &04 to go back. */
char yg2PathNumberText[136] = "Path No. 01 achievement level. \xa2" "\xf6" "\\nGray = In Progress   Blue = Success   Red = Failed\\nPress &03 for details. Press &04 to go back.";

char yg2PathDigitChars[16] = "0123456789";

unsigned char questFlags[40] = { 0 };

unsigned char citizenFlags[56] = { 0 };

YgPathModelState yg2PathModelHandles = { 0 };

/* .sdata of this file after its own globals (0x69D7E0-0x69D888): globals read by render_sprite.c (renderOriginX/Y) and its names. */
unsigned char yg2PathImageFade = 0;

int renderOriginX = 1792;

int renderOriginY = 0x720;

char yg2PathEmptyText[8] = { 0 };

char yg2PathMessageBufferAssertText[8] = "pBuf";

char yg2PathSexTagFormat[8] = "Sex:%s\n";

char yg2PathAgeTagFormat[8] = "Age:%s\n";

char yg2PathPxyAssertText[8] = "pPxy";

char yg2PathTxyAssertText[8] = "pTxy";

char yg2PathXhrAssertText[8] = "pXhr";

char yg2PathXapAssertText[8] = "pXap";

float yg2PathModelStartX[1] = { -0.0f };

int yg2PathFrameDepth = 131071;

char yg2PathSubjectTitleText[8] = "Subject";

char yg2PathMessageTitleText[8] = "Message";

char yg2PathClientTitleText[8] = "Client";

char yg2PathDataTitleText[8] = "Data";

char yg2PathProfileTitleText[8] = "Profile";

char yg2PathNameLabelText[8] = "Name:";

char yg2PathSexLabelText[8] = "Sex:";

char yg2PathAgeLabelText[8] = "Age:";

char yg2PathHobbyLabelText[8] = "Hobby:";

const char *yg2PathNoDataText = &yg2PathPortraitFormat[16];

