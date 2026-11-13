#include "common.h"
#include "shared.h"
#include "main/control_entry.h"
#include "main/gifState.h"
#include "main/tu092.h"
#include "main/tu094.h"
#include "main/tu097.h"
#include "main/tu101.h"
#include "main/xenoActor.h"
#include "main/xgl_cd.h"
#include "main/yg2path.h"
#include "main/pad.h"
#include "main/sdv_033.h"
#include "main/tu021.h"

/* Page shown by yg2PathDraw; yg2PathMode selects it and yg2PathRun dispatches the pad input on it. */
enum YgPathPage {
    YG2PATH_PAGE_MENU = 0,
    YG2PATH_PAGE_GRID = 1,
    YG2PATH_PAGE_MESSAGE = 2,
    YG2PATH_PAGE_PROFILE = 3
};

/* Quest status: the low nibble of a questFlags byte (YG2PATH_STATUS_MASK); each value selects the message
   text of the matching YgQuestRecord field: message, messageState2, messageState3. */
enum YgPathStatus {
    YG2PATH_STATUS_MESSAGE = 1,
    YG2PATH_STATUS_MESSAGE2 = 2,
    YG2PATH_STATUS_MESSAGE3 = 3
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
#define YG2PATH_QUEST_RECORD_SIZE 2886       /* 0xB46 bytes per quest message record */
#define YG2PATH_CITIZEN_RECORD_SIZE 0xD7     /* bytes per citizen record */
#define YG2PATH_QUEST_BUFFER_SIZE 0x195D8
#define YG2PATH_CITIZEN_BUFFER_SIZE 0x29FE
#define YG2PATH_FILE_TEMP_SIZE 0x30000

/* One quest message record (YG2PATH_QUEST_RECORD_SIZE bytes per path): the text fields of QuestMes.txt. */
typedef struct YgQuestRecord {
    char subject[0x1F];
    char location[0x11];
    char client[0x11];
    char message[0x101];
    char messageState2[0x101];
    char messageState3[0x101];
    char detailState2[0x401];
    char detailState3[0x401];
} YgQuestRecord;

/* One citizen record (YG2PATH_CITIZEN_RECORD_SIZE bytes per citizen): the text fields of Citizen.txt. */
typedef struct YgCitizenRecord {
    char name[0x11];
    char sex[0x7];
    char age[0x11];
    char hobby[0x17];
    char profile[0x97];
} YgCitizenRecord;

extern void *memset(void *destination, int value, unsigned int count);

/* Partial view of sRender: a callback pointer at +0x48 and the word that
   follows it at +0x4C; the prefix before +0x48 is not modeled here. */
typedef struct YgPathRenderState {
    u8 unmodeled_00[0x48];
    void (*callback)(void);
    int callback_context;
} YgPathRenderState;

/* citizenFlags: one byte per citizen (0x32 of them) and questFlags: one byte
   per path (0x24 of them); the low nibble of a path byte says whether the
   path is available. yg2PathModelHandles[0] holds the handle of the loaded model. */
extern u8 citizenFlags[];
extern u8 questFlags[];
extern int yg2PathModelHandles[];
extern int yg2PathPortraitBuffer;
extern int D_0069D7DC;
extern YgPathRenderState sRender;
void xglCallbackQueueRun(void);

/* Returns the citizen flag at index (0x32 entries), 0 when out of range. */
u8 yg2PathGetCitizenFlag(u32 index) {
    if (index < 0x32U) {
        return citizenFlags[index];
    }
    return 0;
}

extern signed char yg2PathCurrentPath;
extern unsigned char yg2PathMessageRange[];
extern unsigned char D_0069D7F0[];
extern unsigned char yg2PathNumberText[];
extern unsigned char yg2PathDigitChars[];
void msgRangeSetText(unsigned char *text, unsigned char *value);

void yg2PathShowPathMessage(void) {
    int pathNumber;

    if (questFlags[yg2PathCurrentPath] == 0) {
        msgRangeSetText(yg2PathMessageRange, D_0069D7F0);
        return;
    }
    pathNumber = yg2PathCurrentPath + 1;
    yg2PathNumberText[9] = yg2PathDigitChars[(pathNumber / 10) % 10];
    yg2PathNumberText[10] = yg2PathDigitChars[pathNumber % 10];
    msgRangeSetText(yg2PathMessageRange, yg2PathNumberText);
}

void yg2PathClearProgress(void) {
    memset(questFlags, 0, YG2PATH_PATH_COUNT);
    memset(citizenFlags, 0, YG2PATH_CITIZEN_COUNT);
}

void menuResetState(void *object);
extern unsigned char yg2PathMode;
extern signed char yg2PathDetailOpen;
extern signed char yg2PathCurrentCitizen;
extern signed char yg2PathMenuChoice;
extern float yg2PathCursorBox[];

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
    menuResetState(yg2PathMessageRange);
    cellX = (float) ((yg2PathCurrentPath % columns) * YG2PATH_CELL_PITCH
                 + ((yg2PathCurrentPath / columns) & 1) * YG2PATH_CELL_ROW_SHIFT - 2);
    yg2PathCursorBox[2] = cellX;
    cellY = (float) ((yg2PathCurrentPath / columns) * YG2PATH_CELL_PITCH + 54);
    yg2PathCursorBox[3] = cellY;
    yg2PathCursorBox[0] = cellX;
    yg2PathCursorBox[1] = cellY;
    yg2PathModelHandles[0] = 0;
}

void XenoActorSetMode(int handle, int value);

/* Sets the mode of the loaded yg2path model (yg2PathModelHandles[0]), when one is loaded. */
void yg2PathSetModelMode(int mode) {
    if (yg2PathModelHandles[0] != 0) {
        XenoActorSetChannelValue((struct XenoActor *) yg2PathModelHandles[0], 7.0f);
        XenoActorEnableChannel((struct XenoActor *) yg2PathModelHandles[0]);
        XenoActorSetMode(yg2PathModelHandles[0], mode);
    }
}

int func_0024F480(unsigned char *pattern, char *text);
void sjisToEuc(char *first, char *second);

/* Reads the quoted field that follows the first line matching `pattern`:
   copies it to `out`, turns \" and \n into their characters, drops CR LF
   pairs and unknown escapes, and returns the text after the closing quote. */
char *yg2PathReadTaggedText(char *text, unsigned char *pattern, char *out)
{
    unsigned char ch;
    unsigned char nextCh;
    char *outPos;
    int quoteCh;

    while (*text != (char) *pattern || func_0024F480(pattern, text) != 0) {
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

void XenoActorSetFlagAll(int count);
void XenoActorDoit(void);
void XenoActorDisp(void);

void yg2PathWaitForLoad(void) {
    while (xglCdServiceIfPending() != 0) {
        XenoActorSetFlagAll(2);
        bgLayerClearAllFlags();
        XenoActorDoit();
        XenoActorDisp();
        xglSleep();
    }
}

extern char *yg2PathQuestMessages;
extern unsigned char D_0068AE10[];
extern unsigned char D_0068AE20[];
extern unsigned char D_0068AE30[];
extern unsigned char D_0068AE40[];
extern unsigned char D_0068AE50[];
extern unsigned char D_0068AE60[];
extern unsigned char D_0068AE70[];
extern unsigned char D_0068AE80[];
extern unsigned char D_0068AE90[];
extern unsigned char yg2PathSourceName[];
extern unsigned char D_0069D7F8[];
int xglCdBufferReserve(long long size);
void ysegfReadFile(void **buffer, void *name, int allocate, int mode);
void assertFailed(unsigned char *message, unsigned char *file, int line);

/* Reads QuestMes.txt into yg2PathQuestMessages: one record per path. The text fields sit at the record
   offsets 0x0, 0x1F, 0x30, 0x41, 0x142, 0x243, 0x344 and 0x745, in the order of the tags read below. */
void yg2PathLoadQuestMessages(void)
{
    char *text = 0;
    int offset;
    int i;
    int reserveSize = YG2PATH_QUEST_BUFFER_SIZE;
    unsigned char *tagSubject = D_0068AE10;
    unsigned char *tagLocation = D_0068AE20;
    unsigned char *tagClient = D_0068AE30;
    unsigned char *tagMessage = D_0068AE40;
    unsigned char *tagMessageState2 = D_0068AE50;
    unsigned char *tagMessageState3 = D_0068AE60;
    unsigned char *tagDetailState2 = D_0068AE70;
    unsigned char *tagDetailState3 = D_0068AE80;

    yg2PathQuestMessages = (char *) xglCdBufferReserve(reserveSize);
    memset(yg2PathQuestMessages, 0, reserveSize);
    ysegfReadFile((void **) &text, D_0068AE90, 1, 1);
    if (text == 0) {
        assertFailed(D_0069D7F8, yg2PathSourceName, 0x2DB);
    }
    yg2PathWaitForLoad();
    for (i = 0; i < YG2PATH_PATH_COUNT; i++) {
        offset = i * YG2PATH_QUEST_RECORD_SIZE;
        text = yg2PathReadTaggedText(text, tagSubject, yg2PathQuestMessages + offset);
        text = yg2PathReadTaggedText(text, tagLocation, yg2PathQuestMessages + offset + 0x1F);
        text = yg2PathReadTaggedText(text, tagClient, yg2PathQuestMessages + offset + 0x30);
        text = yg2PathReadTaggedText(text, tagMessage, yg2PathQuestMessages + offset + 0x41);
        text = yg2PathReadTaggedText(text, tagMessageState2, yg2PathQuestMessages + offset + 0x142);
        text = yg2PathReadTaggedText(text, tagMessageState3, yg2PathQuestMessages + offset + 0x243);
        text = yg2PathReadTaggedText(text, tagDetailState2, yg2PathQuestMessages + offset + 0x344);
        text = yg2PathReadTaggedText(text, tagDetailState3, yg2PathQuestMessages + offset + 0x745);
    }
}

extern char *yg2PathCitizenRecords;
extern unsigned char D_0068AEC0[];
extern unsigned char D_0068AED0[];
extern unsigned char D_0068AEE0[];
extern unsigned char D_0068AEF0[];
extern unsigned char D_0069D800[];
extern unsigned char D_0069D808[];

/* Reads Citizen.txt into yg2PathCitizenRecords: one record of YG2PATH_CITIZEN_RECORD_SIZE bytes per citizen. */
void yg2PathLoadCitizens(void)
{
    char *text = 0;
    int offset;
    int i;
    unsigned char *tagName = D_0068AEC0;
    unsigned char *tagSex = D_0069D800;
    unsigned char *tagAge = D_0069D808;
    unsigned char *tagHobby = D_0068AED0;
    unsigned char *tagProfile = D_0068AEE0;

    yg2PathCitizenRecords = (char *) xglCdBufferReserve(YG2PATH_CITIZEN_BUFFER_SIZE);
    memset(yg2PathCitizenRecords, 0, YG2PATH_CITIZEN_BUFFER_SIZE);
    ysegfReadFile((void **) &text, D_0068AEF0, 1, 0);
    if (text == 0) {
        assertFailed(D_0069D7F8, yg2PathSourceName, 0x30E);
    }
    yg2PathWaitForLoad();
    offset = 0;
    for (i = 0; i < YG2PATH_CITIZEN_COUNT; i++) {
        text = yg2PathReadTaggedText(text, tagName, yg2PathCitizenRecords + offset);
        text = yg2PathReadTaggedText(text, tagSex, yg2PathCitizenRecords + offset + 0x11);
        text = yg2PathReadTaggedText(text, tagAge, yg2PathCitizenRecords + offset + 0x18);
        text = yg2PathReadTaggedText(text, tagHobby, yg2PathCitizenRecords + offset + 0x29);
        text = yg2PathReadTaggedText(text, tagProfile, yg2PathCitizenRecords + offset + 0x40);
        offset += YG2PATH_CITIZEN_RECORD_SIZE;
    }
}

extern float D_0069D830[4];

/* Partial view of the XenoActor record the path scene creates (the full record is in main/xenoActor):
   the fields this file touches. */
typedef struct YgPathActor {
    unsigned char unmodeled_00[0x10];
    DoublewordVector4 position;      /* 0x010 */
    unsigned char unmodeled_20[0x50 - 0x20];
    DoublewordVector4 rotation;      /* 0x050 */
    unsigned char unmodeled_60[0xBC - 0x60];
    int drawFlags;                   /* 0x0BC */
    unsigned char unmodeled_c0[0xC4 - 0xC0];
    int drawMode;                    /* 0x0C4 */
    unsigned char unmodeled_c8[0x4C0 - 0xC8];
    unsigned int visibilityFlags;    /* 0x4C0: first word of the visibility block */
    unsigned char unmodeled_4c4[0x5EC - 0x4C4];
    int pack;                        /* 0x5EC */
    int effect;                      /* 0x5F0 */
    int motion;                      /* 0x5F4 */
    unsigned char unmodeled_5f8[4];
    int resource;                    /* 0x5FC */
} YgPathActor;

/* Partial view of the xgl camera (main/sdv_033.h XglCamera) with the travel vectors this file writes. */
typedef struct YgPathCamera {
    unsigned char unmodeled_00[0x94];
    float fov;                       /* 0x094 */
    unsigned char unmodeled_98[0xA0 - 0x98];
    DoublewordVector4 rotation;      /* 0x0A0 */
    unsigned char unmodeled_b0[0xD0 - 0xB0];
    DoublewordVector4 position;      /* 0x0D0 */
} YgPathCamera;

extern unsigned char D_0068AF08[];
extern unsigned char D_0068AF28[];
extern unsigned char D_0068AF48[];
extern unsigned char D_0068AF68[];
extern unsigned char D_0068AF80[];
extern unsigned char D_0068AFD8[];
extern unsigned char D_0068AFF8[];
extern unsigned char D_0068B008[];
extern unsigned char D_0068B028[];
extern unsigned char D_0069D810[];
extern unsigned char D_0069D818[];
extern unsigned char D_0069D820[];
extern unsigned char D_0069D828[];
extern YgPathCamera *yg2PathCamera;
extern unsigned char yg2PathImageFade;

/* One-byte state cells. The byte stores in yg2PathLoadScene go through the struct, which gives them an alias set of their own. */
typedef struct Yg2PathFlag {
    unsigned char value;
} Yg2PathFlag;

extern int yg2PathScroll;
unsigned short xglSRand(void);
void cfeFileTempInit(void *buffer, int size);
YgPathActor *XenoActorAlloc(void);
void XenoActorAllocMatrices(YgPathActor *actor);
YgPathCamera *xglStudioSelectGetActiveCamera(int index);
void xglCameraSetWindow(YgPathCamera *camera, int x, int y, int width, int height);
void *ysegfAllocFileBuffer(unsigned char *name);

void yg2PathLoadScene(void) {
    int modelFiles[4];
    YgPathActor *actor;
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
    menuResetState(yg2PathMessageRange);
    cfeFileTempInit((void *) xglCdBufferReserve(YG2PATH_FILE_TEMP_SIZE), YG2PATH_FILE_TEMP_SIZE);
    yg2PathLoadQuestMessages();
    yg2PathLoadCitizens();
    yg2PathModelHandles[0] = 0;
    ysegfReadFile((void **) &modelFiles[0], D_0068AF08, 1, 1);
    ysegfReadFile((void **) &modelFiles[1], D_0068AF28, 1, 1);
    ysegfReadFile((void **) &modelFiles[2], D_0068AF48, 1, 1);
    ysegfReadFile((void **) &modelFiles[3], D_0068AF68, 1, 1);
    yg2PathWaitForLoad();
    if (modelFiles[0] == 0) {
        assertFailed(D_0069D810, yg2PathSourceName, 842);
    }
    if (modelFiles[1] == 0) {
        assertFailed(D_0069D818, yg2PathSourceName, 843);
    }
    if (modelFiles[2] == 0) {
        assertFailed(D_0069D820, yg2PathSourceName, 844);
    }
    if (modelFiles[3] == 0) {
        assertFailed(D_0069D828, yg2PathSourceName, 845);
    }
    actor = XenoActorAlloc();
    actor->pack = modelFiles[0];
    actor->drawMode = 0;
    yg2PathModelHandles[0] = (int) actor;
    actor->effect = modelFiles[1];
    actor->motion = modelFiles[2];
    actor->resource = modelFiles[3];
    XenoActorAllocMatrices(actor);
    XenoActorSetChannelValue((struct XenoActor *) yg2PathModelHandles[0], 0.0f);
    XenoActorEnableChannel((struct XenoActor *) yg2PathModelHandles[0]);
    XenoActorSetMode(yg2PathModelHandles[0], 0);
    ((YgPathActor *) yg2PathModelHandles[0])->position.vector.x = D_0069D830[0];
    ((YgPathActor *) yg2PathModelHandles[0])->position.vector.y = 29.4f;
    ((YgPathActor *) yg2PathModelHandles[0])->position.vector.z = 10.0f;
    ((YgPathActor *) yg2PathModelHandles[0])->rotation.vector.x = 0.0f;
    ((YgPathActor *) yg2PathModelHandles[0])->rotation.vector.y = 0.0f;
    ((YgPathActor *) yg2PathModelHandles[0])->rotation.vector.z = 0.0f;
    yg2PathModelHandles[1] = 0;
    yg2PathModelHandles[2] = xglSRand() % 200U + 100;
    ((YgPathActor *) yg2PathModelHandles[0])->visibilityFlags |= 4;
    xglCameraSetWindow(xglStudioSelectGetActiveCamera(0), 0, 0, 512, 448);
    camera = xglStudioSelectGetActiveCamera(1);
    yg2PathCamera = camera;
    camera->position.vector.y = 30.0f;
    camera->position.vector.z = 14.0f;
    camera->position.vector.x = 0.0f;
    camera->rotation.vector.x = 0.0f;
    camera->rotation.vector.y = 0.0f;
    camera->rotation.vector.z = 0.0f;
    camera->fov = 0.69813174f;
    pageMode = &yg2PathMode;
    menuChoice = &yg2PathMenuChoice;
    *pageMode = (Yg2PathFlag) { 0 };
    *menuChoice = (Yg2PathFlag) { 0 };
    msgRangeSetText(yg2PathMessageRange, D_0068AF80);
    yg2PathPortraitBuffer = (int) ysegfAllocFileBuffer(D_0068AFD8);
    if (yg2PathPortraitBuffer == 0) {
        assertFailed(D_0068AFF8, yg2PathSourceName, 898);
    }
    yg2PathImageFade = 0;
    ysegfReadFile((void **) &D_0069D7DC, D_0068B008, 1, 1);
    if (D_0069D7DC == 0) {
        assertFailed(D_0068B028, yg2PathSourceName, 901);
    }
    yg2PathWaitForLoad();
}

void pxySetCameraEnable(int first, int second);
void XenoActorRelease(int handle);

void yg2PathUnload(void) {
    if (yg2PathModelHandles[0] != 0) {
        XenoActorRelease(yg2PathModelHandles[0]);
        yg2PathModelHandles[0] = 0;
    }
    yg2PathPortraitBuffer = 0;
    D_0069D7DC = 0;
    xglCdBufferRewind();
    pxySetCameraEnable(1, 0);
    sRender.callback = xglCallbackQueueRun;
    sRender.callback_context = 0;
}

void panelDrawNineSlice(unsigned char *, int, int, int, int);
extern int D_0069D834;
extern unsigned char yg2PathFrameImage[];
extern unsigned char yg2PathFrameBorderImage[];

void yg2PathDrawFramedImage(int x, int y, int width, int height) {
    fontObjSetOpacity(0x80);
    fontObjSetDepth(D_0069D834);
    packetSubmitRequestEx(0x1EB, x, y, width, height, 0x20010);
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
        packetSubmitRequest(0x1FD, x, y, 0);
        packetSubmitRequest(0x1FF, x, y, 1);
        if (variant == 3) {
            packetSubmitRequest(0x200, x + 0xC, y + 0x1C, 0);
            packetSubmitRequest(0x201, x + 0x5, y + 0x14, 1);
        } else if (variant == 2) {
            packetSubmitRequest(0x1FB, x + 0xC, y + 0x1C, 0);
            packetSubmitRequest(0x1FC, x + 0x5, y + 0x14, 1);
        }
    } else {
        packetSubmitRequest(0x1FE, x, y, 1);
    }
}

void yg2PathDrawPageChrome(void) {
    fontObjSetOpacity(0x80);
    if (yg2PathMode < YG2PATH_PAGE_MESSAGE) {
        if (yg2PathMode == YG2PATH_PAGE_MENU) {
            fontObjSetDepth(0x1FFFF);
            packetSubmitRequestEx(0x206, 0, 0x92, 0x200, 0x8C, 0x20001);
            packetSubmitRequest(0x18E, 0, 0x30, 0x20001);
            packetSubmitRequest(0x1FA, 0x164, 0x6F, 0x20001);
            fontObjSetDepth(0);
        }
        packetSubmitRequest(0x190, 0, 0x14, 1);
        packetSubmitRequest(0x18D, 0, 0, 1);
        packetSubmitRequest(0x208, 0x10, 0x12, 1);
    }
    if (yg2PathDetailOpen == 0) {
        packetSubmitRequest(0x207, 0, 0x170, 1);
        packetSubmitRequest(0x18F, 0, 0x170, 1);
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
    packetSubmitRequest(504, 311, 174, 5);
    packetSubmitRequest(505, 275, 149, 5);
    packetSubmitRequest(501, 326, 190, 4);
    packetSubmitRequest(502, 326, 190, 5);
    packetSubmitRequest(500, 290, 165, 4);
    packetSubmitRequest(503, 290, 165, 5);
    if (yg2PathMenuChoice == 0) {
        fontObjSetColor(0x3F80000080000000ULL);
        packetSubmitRequest(496, 347, 197, 4);
        fontObjSetColor(0x3F80000080505080ULL);
        packetSubmitRequest(496, 346, 194, 4);
        fontObjSetColor(0x3F80000040808080ULL);
    } else {
        fontObjSetColor(0x3F80000040000000ULL);
        packetSubmitRequest(497, 347, 197, 4);
        fontObjSetColor(0x3F80000040808080ULL);
        packetSubmitRequest(497, 346, 194, 4);
        fontObjResetColor();
    }
    packetSubmitRequest(504, 299, 222, 5);
    packetSubmitRequest(505, 263, 197, 5);
    packetSubmitRequest(501, 314, 238, 4);
    packetSubmitRequest(502, 314, 238, 5);
    packetSubmitRequest(500, 278, 213, 4);
    packetSubmitRequest(503, 278, 213, 5);
    if (yg2PathMenuChoice == 0) {
        fontObjSetColor(0x3F80000040000000ULL);
        packetSubmitRequest(499, 327, 245, 4);
        fontObjSetColor(0x3F80000040808080ULL);
        packetSubmitRequest(499, 326, 242, 4);
        packetSubmitRequest(492, 276, 149, 1);
        packetSubmitRequest(493, 276, 149, 1);
    } else {
        fontObjSetColor(0x3F80000080000000ULL);
        packetSubmitRequest(498, 327, 245, 4);
        fontObjSetColor(0x3F80000080505080ULL);
        packetSubmitRequest(498, 326, 242, 4);
        packetSubmitRequest(492, 264, 197, 1);
        packetSubmitRequest(493, 264, 197, 1);
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

void textDrawDigitsZero(int x, int y, int unused, int mode);

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
    packetSubmitRequest(0x1EE, (int) yg2PathCursorBox[0], (int) yg2PathCursorBox[1], 1);
    packetSubmitRequest(0x1EF, (int) yg2PathCursorBox[0], (int) yg2PathCursorBox[1], 1);
    if (yg2PathScroll >= YG2PATH_SCROLL_FULL) {
        yg2PathDrawFramedImage(0x13D, 0x61, 0xB2, 0xDD);
    } else {
        fraction = (float) yg2PathScroll * 0.0078125f;
        yg2PathDrawFramedImage(0x13D, (int) (207.0f - fraction * 110.0f), 0xB2, (int) (fraction * 221.0f));
    }
    fontObjResetOpacity();
}

void yg2PathDrawNewMarker(int x, int y, int newFlag) {
    fontObjSetColor(0x80000000ULL);
    if (newFlag != 0) {
        packetSubmitRequest(0x2D8, x, y, 0x10004);
    }
    packetSubmitRequest(0x2D7, x + 12, y - 28, 0x10004);
    fontObjResetColor();
    fontObjSetColor(0x80808080ULL);
    packetSubmitRequest(0x2D6, x + 12, y - 28, 0x10001);
    packetSubmitRequest(0x2DA, x + 85, y - 17, 0x10001);
    if (newFlag != 0) {
        packetSubmitRequest(0x2D9, x + 35, y - 1, 0x10001);
    }
    menuDrawKindIcon(x + 68, y - 29, 3);
}

typedef struct YgCitizenFileRequest {
    unsigned long long first;
    unsigned long long second;
    int third;
    unsigned char fourth;
    unsigned char unmodeled_15[0x2B];
} YgCitizenFileRequest;

void textDrawSegment(int, int, unsigned char *, int);
void menuDrawShortOutlineFrame(int, int, int, int);
void textDrawDigitsPair(int, int, int, int, int);
void textDrawDigitsWideAlt(int, int, int, int);
void func_0021CEE0(int, int, int, int);
void menuDrawTitledFrameRed(int, int, int, int, unsigned char *);
void renderSpriteClutAlpha(int, int, int, int, int, unsigned char);
void renderSpriteClut(int, int, int, int, int);
void sprintf(YgCitizenFileRequest *, unsigned char *, YgCitizenFileRequest *, int);
void xglPacketTextureTrans(int);
void fontObjBuildBaseVifPacket(void);
extern unsigned char yg2PathPanelImage[];
extern unsigned char *D_0069D880;
extern unsigned char yg2PathTextHeader[];
extern const YgCitizenFileRequest D_0068B090;
extern const YgCitizenFileRequest D_0068B0A8[];
extern unsigned char yg2PathPortraitFormat[];
extern unsigned char D_0069D850[];
extern unsigned char D_0069D858[];
extern unsigned char D_0069D860[];
extern unsigned char D_0069D868[];
extern unsigned char D_0069D870[];
extern unsigned char D_0069D878[];
extern signed char D_003EC980[];
extern unsigned char D_003EC930[];
extern unsigned char D_0069D838[];
extern unsigned char D_0068B038[];
extern unsigned char D_0069D840[];
extern unsigned char D_0069D848[];
void menuDrawTitledFrameBlue(int, int, int, int, unsigned char *);

/* Message page: the current path's quest message record and its
   questFlags byte (low nibble: availability, bit 0x10: flag). */
void yg2PathDrawMessagePage(int scroll)
{
  YgCitizenFileRequest fileRequest;
  unsigned int fadeNext;
  int questState;

  fontObjSetOpacity(scroll);
  panelDrawNineSlice(D_003EC930, 12, 15, 415, 89);
  packetSubmitRequest(454, 32, 15, 1);
  textDrawDigitsWideAlt(74, 15, yg2PathCurrentPath + 1, 2);
  func_0021CEE0(24, 40, 391, 50);
  menuDrawTitledFrameBlue(29, 47, 235, 37, D_0069D838);
  menuDrawTitledFrameBlue(267, 47, 144, 37, D_0068B038);
  panelDrawNineSlice(yg2PathPanelImage, 26, 109, 460, 250);
  packetSubmitRequest(490, 53, 114, 1);
  packetSubmitRequest(418, 34, 140, 1);
  packetSubmitRequest(428, 462, 140, 0);
  packetSubmitRequest(420, 462, 140, 1);
  packetSubmitRequestEx(419, 50, 140, 412, 16, 1);
  packetSubmitRequestEx(417, 34, 156, 16, 172, 1);
  packetSubmitRequestEx(427, 462, 156, 16, 155, 0);
  packetSubmitRequest(424, 34, 328, 0);
  packetSubmitRequest(416, 34, 328, 1);
  packetSubmitRequest(426, 462, 311, 0);
  packetSubmitRequest(436, 320, 324, 0);
  packetSubmitRequestEx(425, 50, 328, 270, 16, 0);
  packetSubmitRequestEx(425, 352, 311, 110, 16, 0);
  func_0021CEE0(54, 148, 130, 148);
  func_0021CEE0(200, 148, 265, 137);
  menuDrawTitledFrameRed(206, 155, 255, 123, D_0069D840);
  menuDrawTitledFrameRed(44, 300, 150, 37, D_0069D848);
  packetSubmitRequestEx(517, 208, 187, 249, 5, 1);
  packetSubmitRequestEx(517, 208, 207, 249, 5, 1);
  packetSubmitRequestEx(517, 208, 227, 249, 5, 1);
  packetSubmitRequestEx(517, 208, 247, 249, 5, 1);
  packetSubmitRequestEx(517, 208, 267, 249, 5, 1);
  func_0021CEE0(215, 290, 240, 30);
  packetSubmitRequest(531, 225, 298, 1);
  fontObjSetColor(0x80000000ULL);
  packetSubmitRequest(532, 225, 298, 4);
  fontObjResetColor();
  packetSubmitRequest(525, 258, 294, 1);
  packetSubmitRequestEx(526, 266, 294, 168, 22, 1);
  packetSubmitRequest(527, 434, 294, 1);
  packetSubmitRequest(528, 258, 294, 0);
  packetSubmitRequestEx(529, 266, 294, 168, 22, 0);
  packetSubmitRequest(530, 434, 294, 0);

  questState = questFlags[yg2PathCurrentPath] & YG2PATH_STATUS_MASK;
  switch (questState) {
  case YG2PATH_STATUS_MESSAGE3:
    packetSubmitRequest(512, 229, 303, 0);
    packetSubmitRequest(513, 222, 295, 1);
    break;
  case YG2PATH_STATUS_MESSAGE2:
    packetSubmitRequest(507, 229, 303, 0);
    packetSubmitRequest(508, 222, 295, 1);
    break;
  }
  packetSubmitRequest(457, 1, 220, 1);
  packetSubmitRequest(457, 488, 220, 1);
  packetSubmitRequest(455, 1, 220, 1);
  packetSubmitRequest(456, 488, 220, 1);
  packetSubmitRequest(28, 372, 333, 64);
  textDrawDigitsPair(416, 333, yg2PathCurrentPath + 1, 36, 2);
  textDrawSegment(0, 0, yg2PathTextHeader, 0);
  textDrawSegment(38, 59, ((YgQuestRecord *) yg2PathQuestMessages)[yg2PathCurrentPath].subject, 0);
  textDrawSegment(276, 59, ((YgQuestRecord *) yg2PathQuestMessages)[yg2PathCurrentPath].location, 0);
  textDrawSegment(59, 311, ((YgQuestRecord *) yg2PathQuestMessages)[yg2PathCurrentPath].client, 0);
  packetSubmitRequestEx(757, 258, 294, 184, 22, 0x10000);
  fontObjSetColor(0x80000000ULL);
  packetSubmitRequest(721, 258, 294, 0x10004);
  fontObjResetColor();

  questState = questFlags[yg2PathCurrentPath] & YG2PATH_STATUS_MASK;
  switch (questState) {
  case YG2PATH_STATUS_MESSAGE:
    packetSubmitRequest(723, 290, 294, 0x10001);
    textDrawSegment(221, 169, ((YgQuestRecord *) yg2PathQuestMessages)[yg2PathCurrentPath].message, 0);
    break;
  case YG2PATH_STATUS_MESSAGE3:
    packetSubmitRequest(722, 293, 294, 0x10001);
    textDrawSegment(221, 169, ((YgQuestRecord *) yg2PathQuestMessages)[yg2PathCurrentPath].messageState3, 0);
    yg2PathDrawNewMarker(340, 114, questFlags[yg2PathCurrentPath] & YG2PATH_FLAG_NEW);
    break;
  case YG2PATH_STATUS_MESSAGE2:
    packetSubmitRequest(725, 292, 294, 0x10001);
    textDrawSegment(221, 169, ((YgQuestRecord *) yg2PathQuestMessages)[yg2PathCurrentPath].messageState2, 0);
    yg2PathDrawNewMarker(340, 114, questFlags[yg2PathCurrentPath] & YG2PATH_FLAG_NEW);
    break;
  }

  if (yg2PathImageFade != 0 && xglCdServiceIfPending() == 0) {
    xglPacketTextureTrans(D_0069D7DC);
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
    if (D_003EC980[yg2PathCurrentPath] + 1 >= 10) {
      fileRequest.first = D_0068B090.first;
      fileRequest.second = D_0068B090.second;
      fileRequest.third = D_0068B090.third;
    } else {
      fileRequest.first = D_0068B0A8[0].first;
      fileRequest.second = D_0068B0A8[0].second;
      fileRequest.third = D_0068B0A8[0].third;
      fileRequest.fourth = D_0068B0A8[0].fourth;
    }
    sprintf(&fileRequest, yg2PathPortraitFormat, &fileRequest, D_003EC980[yg2PathCurrentPath] + 1);
    ysegfReadFile((void **) &yg2PathPortraitBuffer, &fileRequest, 0, 1);
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
  packetSubmitRequest(0x203, 0x4D, 0x1F, 1);
  packetSubmitRequest(0x1C6, 0xD0, 0x20, 1);
  textDrawDigitsWideAlt(0xFA, 0x20, yg2PathCurrentCitizen + 1, 2);
  packetSubmitRequest(0x1A2, 0x37, 0x3A, 1);
  packetSubmitRequest(0x1AC, 0x1B8, 0x3A, 0);
  packetSubmitRequest(0x1A4, 0x1B8, 0x3A, 1);
  packetSubmitRequestEx(0x1A3, 0x47, 0x3A, 0x171, 0x10, 1);
  packetSubmitRequestEx(0x1A1, 0x37, 0x4A, 0x10, 0x8B, 1);
  packetSubmitRequestEx(0x1AB, 0x1B8, 0x4A, 0x10, 0x7A, 0);
  packetSubmitRequest(0x1A8, 0x37, 0xD5, 0);
  packetSubmitRequest(0x1A0, 0x37, 0xD5, 1);
  packetSubmitRequest(0x1AA, 0x1B8, 0xC4, 0);
  packetSubmitRequest(0x1B4, 0x118, 0xD1, 0);
  packetSubmitRequestEx(0x1A9, 0x47, 0xD5, 0xD1, 0x10, 0);
  packetSubmitRequestEx(0x1A9, 0x138, 0xC4, 0x80, 0x10, 0);
  func_0021CEE0(0x42, 0x45, 0x82, 0x94);
  func_0021CEE0(0xCD, 0x45, 0xF1, 0x81);
  menuDrawTitledFrameRed(0xD2, 0x4C, 0xE7, 0x73, D_0069D850);
  packetSubmitRequestEx(0x205, 0xE6, 0x6D, 0xC6, 5, 1);
  packetSubmitRequestEx(0x205, 0xE6, 0x85, 0xC6, 5, 1);
  packetSubmitRequestEx(0x205, 0xE6, 0x9D, 0xC6, 5, 1);
  packetSubmitRequestEx(0x205, 0xE6, 0xB5, 0xC6, 5, 1);
  menuDrawShortOutlineFrame(0x37, 0xEA, 0x191, 0x6B);
  menuDrawTitledFrameRed(0x42, 0xF7, 0x17B, 0x53, D_0069D858);
  packetSubmitRequestEx(0x205, 0x44, 0x117, 0x177, 5, 1);
  packetSubmitRequestEx(0x205, 0x44, 0x12B, 0x177, 5, 1);
  packetSubmitRequestEx(0x205, 0x44, 0x13F, 0x177, 5, 1);
  packetSubmitRequest(0x1C9, 0xE, 0xB2, 1);
  packetSubmitRequest(0x1C9, 0x1DA, 0xB2, 1);
  packetSubmitRequest(0x1C7, 0xE, 0xB2, 1);
  packetSubmitRequest(0x1C8, 0x1DA, 0xB2, 1);
  packetSubmitRequest(0x1C, 0x150, 0xD9, 0x40);
  textDrawDigitsPair(0x17C, 0xD9, yg2PathCurrentCitizen + 1, 0x32, 2);
  textDrawSegment(0, 0, yg2PathTextHeader, 0);
  textDrawSegment(0xD9, 0x5A, D_0069D860, 0);
  textDrawSegment(0xD9, 0x72, D_0069D868, 0);
  textDrawSegment(0xD9, 0x8A, D_0069D870, 0);
  textDrawSegment(0xD9, 0xA2, D_0069D878, 0);
  if (citizenFlags[yg2PathCurrentCitizen] != 0) {
    textDrawSegment(0x112, 0x5A, ((YgCitizenRecord *) yg2PathCitizenRecords)[yg2PathCurrentCitizen].name, 0);
    textDrawSegment(0x112, 0x72, ((YgCitizenRecord *) yg2PathCitizenRecords)[yg2PathCurrentCitizen].sex, 0);
    textDrawSegment(0x112, 0x8A, ((YgCitizenRecord *) yg2PathCitizenRecords)[yg2PathCurrentCitizen].age, 0);
    textDrawSegment(0x112, 0xA2, ((YgCitizenRecord *) yg2PathCitizenRecords)[yg2PathCurrentCitizen].hobby, 0);
    textDrawSegment(0x52, 0x105, ((YgCitizenRecord *) yg2PathCitizenRecords)[yg2PathCurrentCitizen].profile, 0);
    if (yg2PathImageFade != 0 && xglCdServiceIfPending() == 0) {
      xglPacketTextureTrans(D_0069D7DC);
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
        fileRequest.first = D_0068B090.first;
        fileRequest.second = D_0068B090.second;
        fileRequest.third = D_0068B090.third;
      } else {
        fileRequest.first = D_0068B0A8[0].first;
        fileRequest.second = D_0068B0A8[0].second;
        fileRequest.third = D_0068B0A8[0].third;
        fileRequest.fourth = D_0068B0A8[0].fourth;
      }
      sprintf(&fileRequest, yg2PathPortraitFormat, &fileRequest, yg2PathCurrentCitizen + 1);
      ysegfReadFile((void **) &yg2PathPortraitBuffer, &fileRequest, 0, 1);
      yg2PathImageFade = 0x10;
    }
  } else {
    textDrawSegment(0x12C, 0x5A, D_0069D880, 0);
    textDrawSegment(0x12C, 0x72, D_0069D880, 0);
    textDrawSegment(0x12C, 0x8A, D_0069D880, 0);
    textDrawSegment(0x12C, 0xA2, D_0069D880, 0);
    packetSubmitRequest(0x1EB, 0x4B, 0x50, 0);
    panelDrawNineSlice(yg2PathFrameImage, 0x4B, 0x50, 0x70, 0x7F);
  }
  fontObjResetOpacity();
}

extern signed char yg2PathDetailIndex;
extern unsigned char yg2PathDetailTextHeader[];

void yg2PathDrawPathDetail(void)
{
  float fraction;
  unsigned char questStatus;
  panelDrawNineSlice(yg2PathPanelImage, 0xD, 0x14, 0xF7, 0x194);
  packetSubmitRequest(0x202, 0x19, 0x19, 1);
  menuDrawShortOutlineFrame(0x15, 0x33, 0xE7, 0x167);
  packetSubmitRequest(0x20A, 0x176, 0x185, 1);
  packetSubmitRequest(0x209, 0x18B, 0x189, 1);
  textDrawSegment(0, 0, yg2PathDetailTextHeader, 0);
  questStatus = questFlags[yg2PathDetailIndex];
  if (questStatus == YG2PATH_STATUS_MESSAGE3)
  {
    textDrawSegment(0x1E, 0x3C, ((YgQuestRecord *) yg2PathQuestMessages)[yg2PathDetailIndex].detailState3, 0);
  }
  else if (questStatus == YG2PATH_STATUS_MESSAGE2)
  {
    textDrawSegment(0x1E, 0x3C, ((YgQuestRecord *) yg2PathQuestMessages)[yg2PathDetailIndex].detailState2, 0);
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

void msgRangeDrawRevealed();
void yg2PathDrawPageChrome(void);
void yg2PathDrawMainMenu(int opacity);
void yg2PathDrawPathGrid(int opacity);
void yg2PathDrawMessagePage(int scroll);
void yg2PathDrawCitizenProfile(int scroll);
void yg2PathDrawPathDetail(void);
void xglFontFlush();

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
    msgRangeDrawRevealed(yg2PathMessageRange, 0x18, 0x175);
  }
  xglFontFlush();
}


void yg2PathDrawFrame(void) {
    yg2PathDraw();
    gifStateSuppressDrawHook();
}

extern PadRecord PadData[2];
extern unsigned char D_0068B128[];
extern unsigned char D_0068B178[];
void seVoiceStartDefault(int voice);

int yg2PathHandleMenuInput(void) {
    if ((PadData[0].pressed & PAD_CROSS) != 0) {
        seVoiceStartDefault(YG2PATH_SE_CANCEL);
        return 1;
    }
    if ((PadData[0].directionRepeated & (PAD_UP | PAD_DOWN)) != 0) {
        yg2PathMenuChoice = yg2PathMenuChoice ^ 1;
        yg2PathSetModelMode(yg2PathMenuChoice);
        if (yg2PathMenuChoice != 0) {
            msgRangeSetText(yg2PathMessageRange, D_0068B128);
        } else {
            msgRangeSetText(yg2PathMessageRange, D_0068AF80);
        }
        seVoiceStartDefault(YG2PATH_SE_CURSOR);
    }
    if ((PadData[0].pressed & PAD_CIRCLE) != 0) {
        if (yg2PathMenuChoice != 0) {
            msgRangeSetText(yg2PathMessageRange, D_0068B178);
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
        msgRangeSetText(yg2PathMessageRange, D_0068AF80);
        yg2PathSetModelMode(0);
        seVoiceStartDefault(YG2PATH_SE_CANCEL);
        yg2PathScroll = 0;
        return YG2PATH_PAGE_MENU;
    }
    if ((PadData[0].pressed & PAD_CIRCLE) != 0 && (questFlags[yg2PathCurrentPath] & YG2PATH_STATUS_MASK) != 0) {
        msgRangeSetText(yg2PathMessageRange, D_0068B178);
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
        if (mode == YG2PATH_STATUS_MESSAGE2 || mode == YG2PATH_STATUS_MESSAGE3) {
            questFlags[yg2PathCurrentPath] = flags & ~YG2PATH_FLAG_NEW;
            yg2PathDetailOpen = 1;
            yg2PathDetailIndex = (unsigned char) yg2PathCurrentPath;
            seVoiceStartDefault(YG2PATH_SE_CONFIRM);
            yg2PathScroll = 0;
            if (mode == YG2PATH_STATUS_MESSAGE3) {
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

/* Pad input on the citizen page: the square button returns to the path page; the repeat buttons move the
   current citizen one step back or forward through the 50 citizens, wrapping at both ends. */
void yg2PathHandleCitizenInput(void) {
    if ((PadData[0].pressed & PAD_CROSS) != 0) {
        msgRangeSetText(yg2PathMessageRange, D_0068B128);
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

extern float D_0069B2A0;

/* Partial view of the loaded yg2path model: the facing angle at +0x54 (rotation y of XenoActor). */
typedef struct YgPathModelView {
    u8 unmodeled_00[0x54];
    float facingAngle;
} YgPathModelView;

/* Sets the camera window to the rectangle (x, y, width, height). On the path detail page with quest state 3
   the model faces pi, otherwise it faces 0. */
void yg2PathUpdateCamera(int x, int y, int width, int height) {
    if (yg2PathDetailOpen != 0 && (questFlags[yg2PathCurrentPath] & YG2PATH_STATUS_MASK) == YG2PATH_STATUS_MESSAGE3) {
        ((YgPathModelView *) yg2PathModelHandles[0])->facingAngle = D_0069B2A0;
    } else {
        ((YgPathModelView *) yg2PathModelHandles[0])->facingAngle = 0.0f;
    }
    xglCameraSetWindow(xglStudioSelectGetActiveCamera(1), x, y, x + width, y + height);
    pxySetCameraEnable(0, 1);
    pxySetCameraEnable(1, 1);
    pxySetCameraEnable(2, 0);
    pxySetCameraEnable(3, 0);
}

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

/* Partial view of gameWork: control at +0x8 (the bits are the CONTROL_* values of tu051.h). */
typedef struct YgPathGameWork {
    unsigned char unmodeled_00[8];
    int control;
} YgPathGameWork;

extern YgPathGameWork gameWork;

#define CONTROL_SOFT_RESET 0x10000000

extern unsigned char D_0068B1C8[];
void soundRestart(void);
void yg2PathLoadScene(void);
void func_001BA68C(void);
void func_001E0E98(void);

void yg2PathRun(void)
{
    int exitRequest;
    int frames;

    soundRestart();
    seBgmPlay(1, D_0068B1C8, 0x19000);
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
        if (yg2PathModelHandles[0] != 0) {
            ((YgPathActor *) yg2PathModelHandles[0])->drawFlags &= ~0x2;
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
        func_001BA68C();
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
        func_001BA68C();
        XenoActorSetFlagAll(2);
        bgLayerClearAllFlags();
        XenoActorDoit();
        XenoActorDisp();
        xglSleep();
    } while (--frames >= 0);
    func_001E0E98();
}
