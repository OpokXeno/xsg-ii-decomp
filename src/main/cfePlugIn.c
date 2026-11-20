#include "common.h"
#include "shared.h"
#include "quadword.h"
#include "main/cfePlugIn.h"
#include "ov01/sb_vm.h"
#include "ov01/sb_natives.h"
#include "main/xgl_thread.h"
#include "main/movie_player.h"
#include "main/sound_driver.h"
#include "main/assert_fail.h"
#include "main/game_camera.h"
#include "main/xgl_mc_request.h"
#include "main/menu_draw.h"
#include "main/effect_request.h"
#include "main/msg_text_draw.h"
#include "main/cfe_entry.h"
#include "main/sef.h"
#include "main/cfe_list.h"
#include "main/cfe_track.h"
#include "main/cfe_node_pool.h"
#include "main/xgl_camera.h"
#include "main/xenoActor.h"
#include "main/xeno_actor_set.h"
#include "main/xgl_2.h"
#include "main/xgl_cd.h"
#include "main/xgl_studio.h"
#include "main/cfeEvaluate.h"
#include "main/face_point.h"
#include "main/cfe_init.h"
#include "main/msg_state.h"
#include "main/TiGlobalColl.h"
#include "main/TiWindBase.h"
#include "main/nml_model_set.h"
#include "main/pad.h"
#include "main/res_get_path.h"
#include "main/se_bank_load.h"
#include "main/xgl_font.h"
#include "main/xgl_render.h"
#include "main/game_data.h"
#include "main/fade_ctrl.h"
#include "main/game_pause_disp.h"
#include "main/xgl_cd_arena.h"


/* The modes of cfeListSetKeyMode / cfeListSetAngleKeyMode: set the key plain or linear (CFE_TRACK_LINEAR), or
 * plain and mark the list's tail to reset its first, second or both direction pairs (CFE_TRACK_RESET_*). */
#define CFE_KEY_MODE_PLAIN 0
#define CFE_KEY_MODE_LINEAR 1
#define CFE_KEY_MODE_RESET_BOTH 2
#define CFE_KEY_MODE_RESET_FIRST 3
#define CFE_KEY_MODE_RESET_SECOND 4

/* The text "0" of the default cases' assert(0). */

/* Sets key `key` of the value list to `value` in a CFE_KEY_MODE_* mode: plain (cfeListSetValue) or
 * linear (cfeListSetValueLinear), or plain with reset flags on the list's tail, which is then updated
 * (cfeTrackUpdateDirections). */
void cfeListSetKeyMode(CfeTrackNode **list, int mode, int key, float value)
{
    CfeTrackNode *tail;

    switch (mode) {
    case CFE_KEY_MODE_PLAIN:
        cfeListSetValue(list, key, value);
        break;
    case CFE_KEY_MODE_LINEAR:
        cfeListSetValueLinear(list, key, value);
        break;
    case CFE_KEY_MODE_RESET_BOTH:
        cfeListSetValue(list, key, value);
        tail = cfeListTail(*list);
        if (tail != 0) {
            tail->flags |= CFE_TRACK_RESET_FIRST | CFE_TRACK_RESET_SECOND;
            cfeTrackUpdateDirections(*list);
        }
        break;
    case CFE_KEY_MODE_RESET_FIRST:
        cfeListSetValue(list, key, value);
        tail = cfeListTail(*list);
        if (tail != 0) {
            tail->flags |= CFE_TRACK_RESET_FIRST;
            cfeTrackUpdateDirections(*list);
        }
        break;
    case CFE_KEY_MODE_RESET_SECOND:
        cfeListSetValue(list, key, value);
        tail = cfeListTail(*list);
        if (tail != 0) {
            tail->flags |= CFE_TRACK_RESET_SECOND;
            cfeTrackUpdateDirections(*list);
        }
        break;
    default:
        assertFailed("0", "../cfePlugIn.euc.c", 96);
        break;
    }
}

/* Sets key `key` of the angle list to `value` in a CFE_KEY_MODE_* mode: plain (cfeListSetAngle) or
 * linear (cfeListSetAngleLinear), or plain with reset flags on the list's tail, which is then updated
 * (cfeTrackUpdateDirections). */
void cfeListSetAngleKeyMode(CfeTrackNode **list, int mode, int key, float value)
{
    CfeTrackNode *tail;

    switch (mode) {
    case CFE_KEY_MODE_PLAIN:
        cfeListSetAngle(list, key, value);
        break;
    case CFE_KEY_MODE_LINEAR:
        cfeListSetAngleLinear(list, key, value);
        break;
    case CFE_KEY_MODE_RESET_BOTH:
        cfeListSetAngle(list, key, value);
        tail = cfeListTail(*list);
        if (tail != 0) {
            tail->flags |= CFE_TRACK_RESET_FIRST | CFE_TRACK_RESET_SECOND;
            cfeTrackUpdateDirections(*list);
        }
        break;
    case CFE_KEY_MODE_RESET_FIRST:
        cfeListSetAngle(list, key, value);
        tail = cfeListTail(*list);
        if (tail != 0) {
            tail->flags |= CFE_TRACK_RESET_FIRST;
            cfeTrackUpdateDirections(*list);
        }
        break;
    case CFE_KEY_MODE_RESET_SECOND:
        cfeListSetAngle(list, key, value);
        tail = cfeListTail(*list);
        if (tail != 0) {
            tail->flags |= CFE_TRACK_RESET_SECOND;
            cfeTrackUpdateDirections(*list);
        }
        break;
    default:
        assertFailed("0", "../cfePlugIn.euc.c", 140);
        break;
    }
}

const AlignedVector4 cfeForwardVector = { 0.0f, 0.0f, 1.0f, 1.0f };

/* The vector cfeForwardVector rotated by angles[0] about X and angles[1] about Y. */
void cfeDirectionFromAngles(float *result, const float *angles)
{
    Matrix4 matrix;
    AlignedVector4 vector = cfeForwardVector;

    xglMatrixStackUnit();
    xglMatrixStackRotX(angles[0]);
    xglMatrixStackRotY(angles[1]);
    xglMatrixStackSave(matrix);
    xglVectorMulMat(&vector, matrix, &vector);
    QuadwordCopy(result, &vector);
}

/* Number of characters of `text` equal to `character`; 0 for no text. */
int cfeStrCountChar(const char *text, char character)
{
    int count = 0;

    if (text == 0) {
        return 0;
    }
    while (*text != 0) {
        if (*text == character) {
            count++;
        }
        text++;
    }
    return count;
}

void *memset(void *destination, int value, unsigned int count);
char *strcpy(char *destination, const char *source);
const char cfeMesEmpty[8] = { 0 };
const char cfeMesFontSettings[48] = "$bg3;$prop1;$han1;$xx0,22;$xx1,8;$xx2,0;";

/* The scratchpad, where each line is laid out before it is appended. */
#define CFE_MES_SCRATCHPAD ((char *) 0x70000000)
/* The size of the scratchpad line and of the laid-out text. */
#define CFE_MES_BUFFER_SIZE 0x400
/* A leading '@' indents the first line by one space. */
#define CFE_MES_INDENT '@'
/* Where the font settings string is printed and its colour. */
#define CFE_MES_SETUP_Y 390
#define CFE_MES_SETUP_COLOR 0xFFFFFF

/* Lays the message text out line by line: each line gets a header byte of 1 and an x offset of
 * minus half its width (0 becomes 1) in front of it, built on the scratchpad and appended to a local
 * buffer, which then replaces the text. */
void cfeMesLayoutText(char *text)
{
    char result[CFE_MES_BUFFER_SIZE];
    char *start;
    int indent = 0;
    char *line;
    char *destination;
    int width;
    int last;

    start = text;
    result[0] = cfeMesEmpty[0];
    memset(result + 1, 0, sizeof(result) - 1);
    xglFontGetStringWidth(cfeMesFontSettings);
    if (*start == CFE_MES_INDENT) {
        indent = 1;
        text = start + 1;
    }
    do {
        destination = CFE_MES_SCRATCHPAD;
        memset(CFE_MES_SCRATCHPAD, 0, CFE_MES_BUFFER_SIZE);
        if (indent) {
            CFE_MES_SCRATCHPAD[0] = ' ';
            destination = CFE_MES_SCRATCHPAD + 1;
            indent = 0;
        }
        line = destination;
        destination = line + 2;
        for (;;) {
            *destination++ = *text++;
            if (*text == '\n' || *text == 0) {
                *destination = *text;
                break;
            }
        }
        xglFontPrint(0, CFE_MES_SETUP_Y, CFE_MES_SETUP_COLOR, cfeMesFontSettings);
        line[0] = 1;
        width = xglFontGetStringWidth(line);
        line[1] = -1 - width / 2;
        if (line[1] == 0) {
            line[1] = 1;
        }
        strcat(result, CFE_MES_SCRATCHPAD);
        last = *text++;
    } while (last != 0);
    strcpy(start, result);
}

/* Copies a message, turning each "\\n" into a newline; another escaped character is dropped (its place
 * is skipped). */
void cfeMesUnescape(char *destination, const char *source)
{
    while (*source != 0) {
        if (*source == '\\') {
            if (source[1] == 'n') {
                *destination = '\n';
            }
            source += 2;
            destination++;
        } else {
            *destination++ = *source++;
        }
    }
    *destination = 0;
}

/* The resource block the cfe object loaders read. */
typedef struct CfeResourceBlock {
    int pack;
    int texture;
    int hierarchy;
    int lookLink;
    int slotTable;
} CfeResourceBlock;


/* Loads resource `id` (waiting for the CD first), creates an actor for it with the block's pack and
 * texture and sets the pack as the culling map. */
XenoActor *cfeLoadResourceObject(int id)
{
    void *data;
    CfeResourceBlock *resource;
    XenoActor *actor;

    data = 0;
    do {
    } while (xglCdServiceIfPending() != 0);
    if (GameResLoadID(id, &data, 0, 0) == -1) {
        assertFailed("GameResLoadID(Number, &Adrs, NULL, NULL) != -1", "../cfePlugIn.euc.c", 283);
    }
    resource = data;
    actor = XenoActorAlloc();
    ((XenoActorKindView *) actor)->kind = (void *) id;
    actor->rsrc[XAct_Rsrc_MDL] = resource->pack;
    actor->rsrc[XAct_Rsrc_TEX] = resource->texture;
    if (resource->pack != 0) {
        xglCullingMapSet((const PxyHeader *)resource->pack);
    }
    return actor;
}


float asinf(float value);
double sqrt(double value);
float atan2f(float y, float x);

/* The record of part `index` with its address computed index first (index * size + records), as the
 * original computes it for the two quadword copies below; `&group->records[index]` adds in the other
 * order (addu operands swapped). */
#define CFE_GROUP_RECORD(group, index) \
    ((CfeEvalRecord *) ((index) * sizeof(CfeEvalRecord) + (int) (group)->records))

/* Starts the records of every part of the group from the part's matrix: the position (w = 1), the Euler
 * angles (x from the pitch row, y from the heading of the third row, z the roll, reflected across pi when the
 * up row points down) and unit scale; both are kept as the initial values. */
void cfeMapInitRecords(CfeMap *group)
{
    float *matrix;
    int i;

    if (group->parts == 0) {
        return;
    }
    for (i = 0; i < group->parts->geometryCount; i++) {
        matrix = &group->matrices[i][0][0];
        group->records[i].evaluated[0].vector.x = matrix[12];
        group->records[i].evaluated[0].vector.y = matrix[13];
        group->records[i].evaluated[0].vector.z = matrix[14];
        group->records[i].evaluated[0].vector.w = 1.0f;
        group->records[i].evaluated[1].vector.x = asinf(matrix[9]);
        group->records[i].evaluated[1].vector.y = atan2f(matrix[8], matrix[10]);
        if (matrix[5] >= 0.0f) {
            double sine = matrix[1];

            group->records[i].evaluated[1].vector.z = asinf(sine / sqrt(1.0f - matrix[9] * matrix[9]));
        } else {
            double sine = matrix[1];

            group->records[i].evaluated[1].vector.z
                = 3.1415927f - asinf(sine / sqrt(1.0f - matrix[9] * matrix[9]));
        }
        group->records[i].evaluated[1].vector.w = 1.0f;
        group->records[i].evaluated[2].vector.x = 1.0f;
        group->records[i].evaluated[2].vector.y = 1.0f;
        group->records[i].evaluated[2].vector.z = 1.0f;
        group->records[i].evaluated[2].vector.w = 1.0f;
        QuadwordCopy(&CFE_GROUP_RECORD(group, i)->initial[0], &CFE_GROUP_RECORD(group, i)->evaluated[0]);
        QuadwordCopy(&CFE_GROUP_RECORD(group, i)->initial[1], &CFE_GROUP_RECORD(group, i)->evaluated[1]);
    }
}


/* The head of an actor's hierarchy as cfeLoadActorObject reads it. */
typedef struct CfeHierarchyHeader {
    unsigned char unmodeled_00[0x1C];
    int needsShared;            /* +0x1C: set when the hierarchy needs the shared resource */
} CfeHierarchyHeader;

/* The shared resource the hierarchy loads (0x30041). */
#define CFE_SHARED_RESOURCE_ID 0x30041

/* Loads resource `id` as a full actor: pack, texture, hierarchy, slot table and look link from the
 * block (draw mode 6). A hierarchy that needs it also gets the shared resource. The matrices are
 * built and both channels start at 0. */
XenoActor *cfeLoadActorObject(int id)
{
    void *data;
    CfeResourceBlock *resource;
    XenoActor *actor;
    int pack;

    data = 0;
    do {
    } while (xglCdServiceIfPending() != 0);
    if (GameResLoadID(id, &data, 0, 0) == -1) {
        assertFailed("GameResLoadID(Number, &Adrs, NULL, NULL) != -1", "../cfePlugIn.euc.c", 346);
    }
    resource = data;
    actor = XenoActorAlloc();
    do {
    } while (xglCdServiceIfPending() != 0);
    pack = resource->pack;
    actor->shadowType = 6;
    actor->rsrc[XAct_Rsrc_MDL] = pack;
    actor->kind = id;
    actor->rsrc[XAct_Rsrc_TEX] = resource->texture;
    actor->rsrc[XAct_Rsrc_XHR] = resource->hierarchy;
    actor->rsrc[XAct_Rsrc_XAP] = resource->slotTable;
    actor->rsrc[XAct_Rsrc_EntryTableC] = resource->lookLink;
    if (actor->rsrc[XAct_Rsrc_XHR] != 0 && ((CfeHierarchyHeader *) actor->rsrc[XAct_Rsrc_XHR])->needsShared != 0) {
        do {
        } while (xglCdServiceIfPending() != 0);
        GameResLoadID(CFE_SHARED_RESOURCE_ID, &data, 0, 0);
        actor->rsrc[XAct_Rsrc_SlotTableB] = data;
        XenoActorSetBlendFramesB(actor, 0.0f);
        XenoActorSetSelector(actor, 0);
    }
    XenoActorAllocMatrices(actor);
    XenoActorSetBlendFrames(actor, 0.0f);
    XenoActorSetBlendFramesB(actor, 0.0f);
    XenoActorSetModeInSlot(actor, 0, 0);
    return actor;
}


/* Script command `cfe.ldAt` (1 argument): loads actor argument 1 into the next entry of cfeActors, copies the play
 * range of its first motion state, sets draw mode 6 and allocates a cleared record block of two words per
 * pack geometry. */
int cfeCmdLoadActor(SbThread *handle)
{
    XenoActor *actor;
    CfePackHeader *pack;
    void *block;

    if (VM_PLUGIN_CNT_GET(handle) != 1) {
        assertFailed("VM_PLUGIN_CNT_GET() == 1", "../cfePlugIn.euc.c", 394);
    }
    cfeActors[cfeEntryCount].actor = cfeLoadActorObject(vmArgGet(handle, 1));
    if (cfeActors[cfeEntryCount].actor == 0) {
        assertFailed("actor[numActor].pAct", "../cfePlugIn.euc.c", 398);
    }
    actor = cfeActors[cfeEntryCount].actor;
    /* XenoActor keeps the pack as an int. */
    pack = (CfePackHeader *) actor->rsrc[XAct_Rsrc_MDL];
    cfeActors[cfeEntryCount].playStart = actor->motion[0].range[0];
    cfeActors[cfeEntryCount].playEnd = actor->motion[0].range[1];
    cfeActors[cfeEntryCount].drawMode = 6;
    if (pack != 0 && pack->geometryCount != 0) {
        block = GameResBufferAlloc(pack->geometryCount * 8);
        cfeActors[cfeEntryCount].records = block;
        memset(block, 0, pack->geometryCount * 8);
    } else {
        cfeActors[cfeEntryCount].records = 0;
    }
    cfeEntryCount++;
    return 1;
}


int cfeCmdLoadMotion(SbThread *handle)
{
    char buffer[0x80];
    void *data;
    int slot;
    int index;
    int kind;
    struct XenoActor **slotActor;
    struct XenoActor *actor;

    data = 0;
    if (VM_PLUGIN_CNT_GET(handle) != 3) {
        assertFailed("VM_PLUGIN_CNT_GET() == 3", "../cfePlugIn.euc.c", 430);
    }
    slot = vmArgGet(handle, 1);
    index = vmArgGet(handle, 2);
    kind = vmArgGet(handle, 3);
    strcpy(buffer, vmKeyedEntryGet(handle, kind));
    GameResLoadName(buffer, &data, 0, 0);
    do {
    } while (xglCdServiceIfPending() != 0);
    if (data != 0) {
        slotActor = &cfeActors[slot].actor;
        actor = *slotActor;
        actor->rsrc[XAct_Rsrc_XAP + index] = (int) data;
        if (index == 0) {
            XenoActorSetModeInSlot(actor, 0, 0);
            XenoActorSetSelector(*slotActor, 0);
        }
    }
    return 1;
}



/* The initial scale of a command (1, 1, 1). */
const AlignedVector4 cfeUnitScale = { 1.0f, 1.0f, 1.0f, 1.0f };

/* Script command `cfe.ldEf` (2 or 3 arguments): starts the effect of the next command of cfeEffects (page index
 * and offset; the 3-argument form also gives the bank flag) and sets the command's position and
 * rotation to the origin and its scale to cfeUnitScale. */
int cfeCmdLoadEffect(SbThread *handle)
{
    AlignedVector4 position;
    AlignedVector4 rotation;
    AlignedVector4 scale;
    CfeEffect *command;

    memset(&position, 0, sizeof(position));
    position.w = 1.0f;
    memset(&rotation, 0, sizeof(rotation));
    rotation.w = 1.0f;
    scale = cfeUnitScale;
    command = &cfeEffects[cfeCommandCount];
    if (VM_PLUGIN_CNT_GET(handle) == 2) {
        command->page_index = vmArgGet(handle, 1);
        command->page_offset = vmArgGet(handle, 2);
        sefStartEffectKeyDefault(0, (command->page_index << CFE_PAGE_SHIFT) | command->page_offset | CFE_BANK_PLAIN);
    } else if (VM_PLUGIN_CNT_GET(handle) == 3) {
        command->flag = vmArgGet(handle, 1);
        command->page_index = vmArgGet(handle, 2);
        command->page_offset = vmArgGet(handle, 3);
        if (command->flag != 0) {
            sefStartEffectKeyDefault(0, (command->page_index << CFE_PAGE_SHIFT) | command->page_offset
                                     | CFE_BANK_FLAGGED);
        } else {
            sefStartEffectKeyDefault(0, (command->page_index << CFE_PAGE_SHIFT) | command->page_offset
                                     | CFE_BANK_PLAIN);
        }
    }
    QuadwordCopy(&command->position, &position);
    QuadwordCopy(&command->rotation, &rotation);
    QuadwordCopy(command->scale, &scale);
    cfeCommandCount++;
    return 1;
}


/* Script command `cfe.ldMp` (1 argument): loads resource argument 1 as the object of the next group; when its
 * pack has a part pack, the group gets it, its matrices and one cleared record per geometry. */
int cfeCmdLoadMap(SbThread *handle)
{
    CfeMap *group;
    XenoActor *object;
    CfePackHeader *pack;
    CfePackHeader *parts;
    int offset;
    CfeEvalRecord *records;

    if (VM_PLUGIN_CNT_GET(handle) != 1) {
        assertFailed("VM_PLUGIN_CNT_GET() == 1", "../cfePlugIn.euc.c", 502);
    }
    object = cfeLoadResourceObject(vmArgGet(handle, 1));
    /* XenoActor keeps the pack as an int. */
    pack = (CfePackHeader *) object->rsrc[XAct_Rsrc_MDL];
    offset = pack->subPackOffset;
    group = &cfeMaps[cfeGroupCount];
    group->object = object;
    /* subPackOffset is a byte offset from the pack. */
    parts = (CfePackHeader *) ((unsigned char *) pack + offset);
    if (offset != 0) {
        group->parts = parts;
        /* matrixOffset is a byte offset from the part pack. */
        group->matrices = (Matrix4 *) ((unsigned char *) parts + parts->matrixOffset);
        records = GameResBufferAlloc(parts->geometryCount * sizeof(CfeEvalRecord));
        group = &cfeMaps[cfeGroupCount];
        group->records = records;
        memset(records, 0, group->parts->geometryCount * sizeof(CfeEvalRecord));
    } else {
        group->parts = 0;
        group->matrices = 0;
        group->records = 0;
    }
    cfeMapInitRecords(&cfeMaps[cfeGroupCount]);
    cfeGroupCount++;
    return 1;
}

int sprintf(char *buffer, const char *format, ...);

int cfeCmdLoadMapCollision(SbThread *handle)
{
    char buffer[0x40];
    void *data;
    int slot;
    char *text;

    data = 0;
    slot = vmArgGet(handle, 1);
    text = vmKeyedEntryGet(handle, vmArgGet(handle, 2));
    sprintf(buffer, "motion/cfevent/%s", text);
    GameResLoadName(buffer, &data, 0, 0);
    do {
    } while (xglCdServiceIfPending() != 0);
    cfeMaps[slot].collision = data;
    return 1;
}

const char cfeMovieOpenFailedText[24] = "failed movie open\n";

/* Script command `cfe.ldMnt` (1 argument): opens the movie named by argument 1 in the next entry of
 * cfeMonitors: reads the file into a new block (walking the block table when the allocation
 * fails), opens it and allocates its two frame buffers. */
int cfeCmdLoadMonitor(SbThread *handle)
{
    CfeMonitor *entry = &cfeMonitors[cfeEvalEntryCount];
    unsigned int slot;

    strcpy(entry->name, vmKeyedEntryGet(handle, vmArgGet(handle, 1)));
    xglMovieInfoInit(&entry->movie);
    do {
    } while (xglCdServiceIfPending() != 0);
    entry->movie.sourceSize = xglCdGetFileSize(entry->name);
    entry->movie.source = GameResBufferAlloc(entry->movie.sourceSize);
    if (entry->movie.source == 0) {
        /* An empty walk to the first unused slot; the result is not used. */
        for (slot = 1; slot < GAME_RES_BLOCK_COUNT - 1 && gameResBlockTable[slot + 1].owner != -1; slot++) {
        }
    }
    do {
    } while (xglCdServiceIfPending() != 0);
    xglCdReadFile(entry->name, entry->movie.source, 0, CD_NOTIFY_DUMMY);
    entry->movie.loop = 1;
    /* assert(!"failed movie open
") inside `if (xglMovieOpen(...) != 0)`: the string is the assert's own
     * condition, so the test of cfeMovieOpenFailedText is the assert. */
    if (xglMovieOpen(&entry->movie, 0) != 0 && cfeMovieOpenFailedText != 0) {
        assertFailed("!\"failed movie open\\n\"", "../cfePlugIn.euc.c", 604);
    }
    entry->movie.imageAddress[0] = GameResBufferAlloc(entry->movie.imageQwords * 16 + 0x20);
    entry->movie.imageAddress[1] = GameResBufferAlloc(entry->movie.imageQwords * 16 + 0x20);
    cfeEvalEntryCount++;
    return 1;
}

int cfeCmdLoadDev(void)
{
    return 1;
}

int cfeCmdLoadSound(SbThread *handle)
{
    int count;
    CfeSndRecord *record;
    char *text;

    text = vmKeyedEntryGet(handle, vmArgGet(handle, 1));
    strcpy(cfeSndRecords[cfeSndRecordCount].name, text);
    count = cfeSndRecordCount;
    record = &cfeSndRecords[count];
    count = count + 1;
    cfeSndRecordCount = count;
    seBgmNameSet(record->name);
    return 1;
}

extern int cfePaused;
extern int cfeStartDelay;

/* The position the parked record actors are moved to before they are released. */
const AlignedVector4 cfeParkedPosition = { 99999.0f, 99999.0f, 99999.0f, 1.0f };

/* The uwamonoRecords entries and the two flags this command sets on every used record. */
#define CFE_RECORD_COUNT MAP_RECORD_COUNT
#define CFE_RECORD_FLAG_400000 0x400000
#define CFE_RECORD_FLAG_800000 0x800000

/* Script command `cfe.init`: ends the map scene for a cut scene. Waits for the CD, resets the messages, the
 * font, the path slot and the sound levels, restarts the cut-scene state, releases the actors of the
 * player and part records (moved to cfeParkedPosition first) and marks every used record, then frees all
 * actors and resets the camera and the fade. */
int cfeCmdInit(SbThread *handle)
{
    AlignedVector4 parked;
    int i;

    do {
    } while (xglCdServiceIfPending() != 0);
    msgResetAll();
    xglFontLoad(0, CD_NOTIFY_CURRENT);
    resPlayerSlotUpdate(-3);
    xglMovieInit();
    cfePaused = 0;
    cfeStartDelay = 0;
    soundSlotSetPause(0, 0);
    soundSlotSetPause(1, 0);
    cfeInit();
    cfeEntryCount = 0;
    tiGlobalCollClearHandle();
    pxyTickScaleDouble = 0;
    studioSceneFlagSet();
    gameWork.control |= CONTROL_CFE_NO_FADE | CONTROL_CFE_SCENE;
    for (i = 0; i < CFE_RECORD_COUNT; i++) {
        switch (uwamonoRecords[i].kind) {
        case 0:
            break;
        case 4:
        case 5:
            parked = cfeParkedPosition;
            QuadwordCopy(&uwamonoRecords[i].body.actor->position, &parked);
            QuadwordCopy(&uwamonoRecords[i].position, &parked);
            QuadwordCopy(&uwamonoRecords[i].moveFrom, &parked);
            QuadwordCopy(&uwamonoRecords[i].moveTo, &parked);
            XenoActorRelease(uwamonoRecords[i].body.actor);
            uwamonoRecords[i].body.actor = 0;
            /* fall through */
        case 1:
        case 2:
        case 3:
        case 6:
        default:
            uwamonoRecords[i].flags |= CFE_RECORD_FLAG_400000;
            uwamonoRecords[i].flags |= CFE_RECORD_FLAG_800000;
            break;
        }
    }
    XenoActorReleaseAll();
    XenoActorPoolInitDefault();
    xglStudioGetCamera2(0)->state = 1;
    fadeClear();
    return 1;
}

/* Script command `cfe.stKyAt` (5 arguments): sets key `key` of track `sub` (CFE_ACTOR_TRACK_*) of actor `index` to
 * the 8.8 value. Lists 3 to 5 hold angles (cfeListSetAngleLinear / cfeListSetAngle), the others plain values
 * (cfeListSetValueLinear / cfeListSetValue); the linear forms are used when `linear` is set. */
int cfeCmdSetActorKey(SbThread *handle)
{
    int index;
    int sub;
    int key;
    int linear;
    int value;

    if (VM_PLUGIN_CNT_GET(handle) != 5) {
        assertFailed("VM_PLUGIN_CNT_GET() == 5", "../cfePlugIn.euc.c", 756);
    }
    index = vmArgGet(handle, 1);
    sub = vmArgGet(handle, 2);
    key = vmArgGet(handle, 3);
    linear = vmArgGet(handle, 4);
    value = vmArgGet(handle, 5);
    if (sub >= 3 && sub < 6) {
        if (linear != 0) {
            cfeListSetAngleLinear(&cfeActors[index].track[sub], key, CFE_FIXED_TO_FLOAT(value));
        } else {
            cfeListSetAngle(&cfeActors[index].track[sub], key, CFE_FIXED_TO_FLOAT(value));
        }
    } else {
        if (linear != 0) {
            cfeListSetValueLinear(&cfeActors[index].track[sub], key, CFE_FIXED_TO_FLOAT(value));
        } else {
            cfeListSetValue(&cfeActors[index].track[sub], key, CFE_FIXED_TO_FLOAT(value));
        }
    }
    return 1;
}

/* Script command `cfe.stKyAtHd` (5 arguments): sets key `key` of head track `sub` of actor `index`
 * (CFE_ACTOR_HEAD_*) to the 8.8 value, with flag
 * linear (cfeListSetValueLinear) when `linear` is set. */
int cfeCmdSetActorHeadKey(SbThread *handle)
{
    int index;
    int sub;
    int key;
    int linear;
    int value;

    if (VM_PLUGIN_CNT_GET(handle) != 5) {
        assertFailed("VM_PLUGIN_CNT_GET() == 5", "../cfePlugIn.euc.c", 795);
    }
    index = vmArgGet(handle, 1);
    sub = vmArgGet(handle, 2);
    key = vmArgGet(handle, 3);
    linear = vmArgGet(handle, 4);
    value = vmArgGet(handle, 5);
    if (linear != 0) {
        cfeListSetValueLinear(&cfeActors[index].headTrack[sub], key, CFE_FIXED_TO_FLOAT(value));
    } else {
        cfeListSetValue(&cfeActors[index].headTrack[sub], key, CFE_FIXED_TO_FLOAT(value));
    }
    return 1;
}


/* Script command `cfe.stKyAtPrts` (6 arguments): sets key `key` of track `column` of flag record `row` of entry `entry`
 * to the 8.8 value (linear through cfeListSetValueLinear when `linear` is set), then walks that track to its
 * end. */
int cfeCmdSetActorPartKey(SbThread *handle)
{
    int entry;
    int row;
    int column;
    int key;
    int linear;
    int value;
    CfeTrackNode *node;

    if (VM_PLUGIN_CNT_GET(handle) != 6) {
        assertFailed("VM_PLUGIN_CNT_GET() == 6", "../cfePlugIn.euc.c", 821);
    }
    entry = vmArgGet(handle, 1);
    row = vmArgGet(handle, 2);
    column = vmArgGet(handle, 3);
    key = vmArgGet(handle, 4);
    linear = vmArgGet(handle, 5);
    value = vmArgGet(handle, 6);
    if (linear != 0) {
        cfeListSetValueLinear(&cfeActors[entry].records[row].track[column], key, CFE_FIXED_TO_FLOAT(value));
    } else {
        cfeListSetValue(&cfeActors[entry].records[row].track[column], key, CFE_FIXED_TO_FLOAT(value));
    }
    for (node = cfeActors[entry].records[row].track[column]; node != 0; node = node->next) {
    }
    return 1;
}


/* Script command `cfe.stKyCm` (4 arguments): fetches the studio camera and sets key `key` of camera track `index`
 * (CFE_CAMERA_TRACK_*) to the 8.8 value in `mode`; lists 3 to 5 go through cfeListSetAngleKeyMode, the others
 * through cfeListSetKeyMode. */
int cfeCmdSetCameraKey(SbThread *handle)
{
    int index;
    int mode;
    int key;
    int value;

    if (VM_PLUGIN_CNT_GET(handle) != 4) {
        assertFailed("VM_PLUGIN_CNT_GET() == 4", "../cfePlugIn.euc.c", 856);
    }
    index = vmArgGet(handle, 1);
    key = vmArgGet(handle, 2);
    mode = vmArgGet(handle, 3);
    value = vmArgGet(handle, 4);
    xglStudioGetCamera(&cfeCamera.eval.camera, 0);
    if (index >= 3 && index < 6) {
        cfeListSetAngleKeyMode(&cfeCamera.eval.track[index], mode, key, CFE_FIXED_TO_FLOAT(value));
    } else {
        cfeListSetKeyMode(&cfeCamera.eval.track[index], mode, key, CFE_FIXED_TO_FLOAT(value));
    }
    return 1;
}

/* Script command `cfe.stKyEf` (5 arguments): sets key `key` of track `sub` (CFE_COMMAND_TRACK_*) of command `index`
 * to the 8.8 value. Lists 3 to 5 hold angles (cfeListSetAngleLinear / cfeListSetAngle), the others plain values
 * (cfeListSetValueLinear / cfeListSetValue); the linear forms are used when `linear` is set. */
int cfeCmdSetEffectKey(SbThread *handle)
{
    int index;
    int sub;
    int key;
    int linear;
    int value;

    if (VM_PLUGIN_CNT_GET(handle) != 5) {
        assertFailed("VM_PLUGIN_CNT_GET() == 5", "../cfePlugIn.euc.c", 896);
    }
    index = vmArgGet(handle, 1);
    sub = vmArgGet(handle, 2);
    key = vmArgGet(handle, 3);
    linear = vmArgGet(handle, 4);
    value = vmArgGet(handle, 5);
    if (sub >= 3 && sub < 6) {
        if (linear != 0) {
            cfeListSetAngleLinear(&cfeEffects[index].track[sub], key, CFE_FIXED_TO_FLOAT(value));
        } else {
            cfeListSetAngle(&cfeEffects[index].track[sub], key, CFE_FIXED_TO_FLOAT(value));
        }
    } else {
        if (linear != 0) {
            cfeListSetValueLinear(&cfeEffects[index].track[sub], key, CFE_FIXED_TO_FLOAT(value));
        } else {
            cfeListSetValue(&cfeEffects[index].track[sub], key, CFE_FIXED_TO_FLOAT(value));
        }
    }
    return 1;
}

/* Script command `cfe.stKyLgt` (5 arguments): sets key `key` of track `sub` (CFE_LIGHT_TRACK_*) of light `index` to
 * the 8.8 value. Lists 3 to 5 hold angles (cfeListSetAngleLinear / cfeListSetAngle), the others plain values
 * (cfeListSetValueLinear / cfeListSetValue); the linear forms are used when `linear` is set. */
int cfeCmdSetLightKey(SbThread *handle)
{
    int index;
    int sub;
    int key;
    int linear;
    int value;

    if (VM_PLUGIN_CNT_GET(handle) != 5) {
        assertFailed("VM_PLUGIN_CNT_GET() == 5", "../cfePlugIn.euc.c", 933);
    }
    index = vmArgGet(handle, 1);
    sub = vmArgGet(handle, 2);
    key = vmArgGet(handle, 3);
    linear = vmArgGet(handle, 4);
    value = vmArgGet(handle, 5);
    if (sub >= 3 && sub < 6) {
        if (linear != 0) {
            cfeListSetAngleLinear(&cfeLights[index].track[sub], key, CFE_FIXED_TO_FLOAT(value));
        } else {
            cfeListSetAngle(&cfeLights[index].track[sub], key, CFE_FIXED_TO_FLOAT(value));
        }
    } else {
        if (linear != 0) {
            cfeListSetValueLinear(&cfeLights[index].track[sub], key, CFE_FIXED_TO_FLOAT(value));
        } else {
            cfeListSetValue(&cfeLights[index].track[sub], key, CFE_FIXED_TO_FLOAT(value));
        }
    }
    return 1;
}

/* Script command `cfe.stKyMp` (6 or 5 arguments): sets key `key` of a list of group `group` to the 8.8 value. With
 * 6 arguments the list is source `sub` of the record of part `part` (sources 3 to 5 hold angles),
 * with 5 the group's own list `sub`; the linear forms are used when `linear` is set. */
int cfeCmdSetMapKey(SbThread *handle)
{
    int group;
    int part;
    int sub;
    int key;
    int linear;
    int value;

    if (VM_PLUGIN_CNT_GET(handle) == 6) {
        group = vmArgGet(handle, 1);
        part = vmArgGet(handle, 2);
        sub = vmArgGet(handle, 3);
        key = vmArgGet(handle, 4);
        linear = vmArgGet(handle, 5);
        value = vmArgGet(handle, 6);
        if (sub >= 3 && sub < 6) {
            if (linear != 0) {
                cfeListSetAngleLinear(&cfeMaps[group].records[part].source[sub], key, CFE_FIXED_TO_FLOAT(value));
            } else {
                cfeListSetAngle(&cfeMaps[group].records[part].source[sub], key, CFE_FIXED_TO_FLOAT(value));
            }
        } else {
            if (linear != 0) {
                cfeListSetValueLinear(&cfeMaps[group].records[part].source[sub], key, CFE_FIXED_TO_FLOAT(value));
            } else {
                cfeListSetValue(&cfeMaps[group].records[part].source[sub], key, CFE_FIXED_TO_FLOAT(value));
            }
        }
    } else {
        group = vmArgGet(handle, 1);
        sub = vmArgGet(handle, 2);
        key = vmArgGet(handle, 3);
        linear = vmArgGet(handle, 4);
        value = vmArgGet(handle, 5);
        if (linear != 0) {
            cfeListSetValueLinear(&cfeMaps[group].tracks[sub], key, CFE_FIXED_TO_FLOAT(value));
        } else {
            cfeListSetValue(&cfeMaps[group].tracks[sub], key, CFE_FIXED_TO_FLOAT(value));
        }
    }
    return 1;
}

/* Script command `cfe.stKyFog` (4 arguments): sets key `key` of fog track `index` (cfeFog.source) to the 8.8
 * value, linear (cfeListSetValueLinear) when `linear` is set. */
int cfeCmdSetFogKey(SbThread *handle)
{
    int index;
    int key;
    int linear;
    int value;

    if (VM_PLUGIN_CNT_GET(handle) != 4) {
        assertFailed("VM_PLUGIN_CNT_GET() == 4", "../cfePlugIn.euc.c", 1021);
    }
    index = vmArgGet(handle, 1);
    key = vmArgGet(handle, 2);
    linear = vmArgGet(handle, 3);
    value = vmArgGet(handle, 4);
    if (linear != 0) {
        cfeListSetValueLinear(&cfeFog.source[index], key, CFE_FIXED_TO_FLOAT(value));
    } else {
        cfeListSetValue(&cfeFog.source[index], key, CFE_FIXED_TO_FLOAT(value));
    }
    return 1;
}

/* Script command `cfe.stKyMnt` (5 arguments): sets key `key` of track `sub` of monitor `index` (cfeMonitors)
 * (140 per row) to the 8.8 value, linear (cfeListSetValueLinear) when
 * `linear` is set. */
int cfeCmdSetMonitorKey(SbThread *handle)
{
    int index;
    int sub;
    int key;
    int linear;
    int value;

    if (VM_PLUGIN_CNT_GET(handle) != 5) {
        assertFailed("VM_PLUGIN_CNT_GET() == 5", "../cfePlugIn.euc.c", 1047);
    }
    index = vmArgGet(handle, 1);
    sub = vmArgGet(handle, 2);
    key = vmArgGet(handle, 3);
    linear = vmArgGet(handle, 4);
    value = vmArgGet(handle, 5);
    if (linear != 0) {
        cfeListSetValueLinear(&cfeMonitors[index].source[sub], key, CFE_FIXED_TO_FLOAT(value));
    } else {
        cfeListSetValue(&cfeMonitors[index].source[sub], key, CFE_FIXED_TO_FLOAT(value));
    }
    return 1;
}

/* Script command `cfe.stKyScrEff` (5 arguments): sets key `key` of track `sub` (CFE_SLOT_SOURCE_*) of screen effect
 * `index` (6 per row) to the 8.8 value, linear (cfeListSetValueLinear) when `linear` is set. */
int cfeCmdSetScreenEffectKey(SbThread *handle)
{
    int index;
    int sub;
    int key;
    int linear;
    int value;

    if (VM_PLUGIN_CNT_GET(handle) != 5) {
        assertFailed("VM_PLUGIN_CNT_GET() == 5", "../cfePlugIn.euc.c", 1073);
    }
    index = vmArgGet(handle, 1);
    sub = vmArgGet(handle, 2);
    key = vmArgGet(handle, 3);
    linear = vmArgGet(handle, 4);
    value = vmArgGet(handle, 5);
    if (linear != 0) {
        cfeListSetValueLinear(&cfeScreenEffects[index].source[sub], key, CFE_FIXED_TO_FLOAT(value));
    } else {
        cfeListSetValue(&cfeScreenEffects[index].source[sub], key, CFE_FIXED_TO_FLOAT(value));
    }
    return 1;
}

/* Script command `cfe.stKySnd` (5 arguments): sets key `key` of track `sub` (CFE_SND_TRACK_*) of sound `index`
 * (20 per row) to the 8.8 value, linear (cfeListSetValueLinear) when
 * `linear` is set. */
int cfeCmdSetSoundKey(SbThread *handle)
{
    int index;
    int sub;
    int key;
    int linear;
    int value;

    if (VM_PLUGIN_CNT_GET(handle) != 5) {
        assertFailed("VM_PLUGIN_CNT_GET() == 5", "../cfePlugIn.euc.c", 1099);
    }
    index = vmArgGet(handle, 1);
    sub = vmArgGet(handle, 2);
    key = vmArgGet(handle, 3);
    linear = vmArgGet(handle, 4);
    value = vmArgGet(handle, 5);
    if (linear != 0) {
        cfeListSetValueLinear(&cfeSndRecords[index].track[sub], key, CFE_FIXED_TO_FLOAT(value));
    } else {
        cfeListSetValue(&cfeSndRecords[index].track[sub], key, CFE_FIXED_TO_FLOAT(value));
    }
    return 1;
}

/* One 0x44C-byte message record of cfeMessages (cfeCmdAllocMessages allocates them). */
typedef struct CfeMessageRecord {
    int firstFrame;             /* +0x000: script argument 2, the first frame the text is shown */
    int endFrame;               /* +0x004: script argument 3, the frame it is gone again */
    int soundSlot;              /* +0x008: script argument 5 (5-argument form only), the sound slot */
    char text[0x400];           /* +0x00C */
    char name[0x40];            /* +0x40C: script argument 4 (5-argument form), else empty */
} CfeMessageRecord;

extern CfeMessageRecord *cfeMessages;

/* Script command `cfe.allocMes` (1 argument): allocates and clears argument 1 message records for cfeMessages. */
int cfeCmdAllocMessages(SbThread *handle)
{
    int count;
    int size;

    if (VM_PLUGIN_CNT_GET(handle) != 1) {
        assertFailed("VM_PLUGIN_CNT_GET() == 1", "../cfePlugIn.euc.c", 1124);
    }
    count = vmArgGet(handle, 1);
    size = count * sizeof(CfeMessageRecord);
    if (count <= 0) {
        return 1;
    }
    cfeMessages = GameResBufferAlloc(size);
    if (cfeMessages == 0) {
        assertFailed("message", "../cfePlugIn.euc.c", 1131);
    }
    memset(cfeMessages, 0, size);
    return 1;
}

extern int cfeMessageCount;
extern const char cfeEmptyName[];

/* Script command `cfe.setMes` (3 or 5 arguments): fills the next message record with the unescaped, laid-out text
 * of argument 1 and arguments 2 and 3; the 5-argument form also sets the name (argument 4) and the
 * sound slot (argument 5). */
int cfeCmdSetMessage(SbThread *handle)
{
    CfeMessageRecord *record = &cfeMessages[cfeMessageCount];

    if (VM_PLUGIN_CNT_GET(handle) == 3) {
        memset(record->text, 0, sizeof(record->text));
        cfeMesUnescape(record->text, vmKeyedEntryGet(handle, vmArgGet(handle, 1)));
        cfeMesLayoutText(record->text);
        record->firstFrame = vmArgGet(handle, 2);
        record->endFrame = vmArgGet(handle, 3);
        /* The empty name: the original copies the one byte of the empty string cfeEmptyName (an inlined
         * strcpy). */
        record->name[0] = cfeEmptyName[0];
        cfeMessageCount++;
    } else if (VM_PLUGIN_CNT_GET(handle) == 5) {
        memset(record->text, 0, sizeof(record->text));
        cfeMesUnescape(record->text, vmKeyedEntryGet(handle, vmArgGet(handle, 1)));
        cfeMesLayoutText(record->text);
        record->firstFrame = vmArgGet(handle, 2);
        record->endFrame = vmArgGet(handle, 3);
        cfeMesUnescape(record->name, vmKeyedEntryGet(handle, vmArgGet(handle, 4)));
        record->soundSlot = vmArgGet(handle, 5);
        cfeMessageCount++;
    }
    return 1;
}

int cfeCmdSetMessageScr(void)
{
    return 1;
}


int cfeCmdSetStartFrame(SbThread *handle)
{
    int value;
    if (VM_PLUGIN_CNT_GET(handle) != 1) {
        assertFailed("VM_PLUGIN_CNT_GET() == 1", "../cfePlugIn.euc.c", 1201);
    }
    value = vmArgGet(handle, 1);
    cfeRangeSetMin(value);
    return 1;
}


int cfeCmdSetEndFrame(SbThread *handle)
{
    int value;
    if (VM_PLUGIN_CNT_GET(handle) != 1) {
        assertFailed("VM_PLUGIN_CNT_GET() == 1", "../cfePlugIn.euc.c", 1217);
    }
    value = vmArgGet(handle, 1);
    cfeRangeSetMax(value);
    return 1;
}


void cfeSoundSlotStartWait(const char *file, int id)
{
    soundSlotSetPause(id, 1);
    soundSlotSetVolume(id, -1, 0);
    adxSetSlotFile(id, file);
    while (soundSlotStatus(id) != 3) {
        xglSleep();
    }
}

/* The running fade (nonzero while a fade is under way). */
extern int fadeUpdateHook;

extern const char cfeMesFontSettings[];
int strcmp(const char *first, const char *second);

/* Frames after the start of the cut scene before start can pause it, and the sound of pausing. */
#define CFE_PAUSE_DELAY 31
#define CFE_PAUSE_SOUND 0x2000011
/* Triangle while paused jumps this many frames ahead (past the end of any cut scene). */
#define CFE_SKIP_FRAMES 30000
/* The subtitle line: one line low on the screen, two or more lines higher up. */
#define CFE_SUBTITLE_Y 370
#define CFE_SUBTITLE_Y2 348
#define CFE_SUBTITLE_COLOR 0xFFFFFF
/* The slot value scale for effectRequestToCommand; kinds 0x22 and 0x23 take 0.03 of it as kinds 0x1A and 0x1B. */
#define CFE_SLOT_SCALE 16777215.0f
#define CFE_SLOT_SHORT 0.03f


/* Script command `cfe.stFrm` (1 argument): one frame of the cut scene at the time in argument 1. Start pauses and
 * resumes (triangle while paused skips 30000 frames ahead), the cut scene is evaluated (camera, directions, nodes,
 * groups, commands, entries, slots, sound) and drawn with its subtitles, and the time to continue with is returned:
 * one frame later unless paused. */
int cfeCmdStepFrame(SbThread *handle)
{
    CfeActor *entry;
    int time;
    int i;
    int y;

    if (VM_PLUGIN_CNT_GET(handle) != 1) {
        assertFailed("VM_PLUGIN_CNT_GET() == 1", "../cfePlugIn.euc.c", 1250);
    }
    while (xglCdServiceIfPending() != 0) {
    }
    time = vmArgGet(handle, 1);
    cfeRangeSetValue(time);
    if (cfeStartDelay != 0) {
        cfeStartDelay--;
    }
    if (cfeRangeGetValue() - cfeRangeGetMin() >= CFE_PAUSE_DELAY && fadeUpdateHook == 0
        && (PadData[0].pressed & PAD_START)) {
        cfeStartDelay = 2;
        cfePaused = cfePaused == 0;
        seVoiceStartDefault(CFE_PAUSE_SOUND);
        if (cfePaused != 0) {
            xglClockSampleDayTime();
        }
    }
    if (cfeMessageCount > 0 && time == cfeRangeGetMin() && strcmp(cfeMessages->name, cfeEmptyName) != 0) {
        cfeSoundSlotStartWait(cfeMessages->name, cfeMessages->soundSlot);
    }
    if (cfePaused == 0) {
        cfeCameraEvaluate(time, 0);
    }
    cfeLightTableEvaluate(time);
    cfeGlobalEvaluate(time);
    cfeActorTableEvaluate(time);
    cfeGroupTableEvaluate(time);
    cfeCommandTableEvaluate(time);
    cfeGlobalEvaluate(time);
    cfeEntryTableEvaluate(time);
    cfeSlotTableEvaluate(time);
    cfeSndTableEvaluate(time);
    pxySetDefaultLight(&cfeLightSet);
    if (cfeFog.fogEnabled != 0) {
        nmlModelSetGlobalFogDist(cfeFog.evaluated[1].x, cfeFog.evaluated[1].y, cfeFog.evaluated[1].z,
                            cfeFog.evaluated[1].w);
        nmlModelSetGlobalFogCol(&cfeFog.evaluated[0]);
    }
    for (i = 0; i < cfeEntryCount; i++) {
        entry = &cfeActors[i];
        if (entry->flags & CFE_ACTOR_LOOK_ENABLED) {
            XenoActorSetFocusTarget(entry->actor, &entry->lookTarget);
        }
        cfeDampDirOffset(entry->actor, entry->dampOffset);
    }
    if (time == cfeRangeGetMin()) {
        fadeReset();
        fadeStartIn(30);
    }
    for (i = 0; i < cfeMessageCount; i++) {
        if (cfeMessages[i].firstFrame <= time && time < cfeMessages[i].endFrame && cfeMessages[i].firstFrame == time
            && strcmp(cfeMessages[i].name, cfeEmptyName) != 0 && cfePaused == 0) {
            soundSlotSetPause(cfeMessages[i].soundSlot, 0);
        }
    }
    for (i = 0; i < cfeMessageCount; i++) {
        if (cfeMessages[i].firstFrame <= time && time < cfeMessages[i].endFrame
            && (SaveData.caption_all != 0 || cfeMessages[i].text[0] == ' ')) {
            y = CFE_SUBTITLE_Y;
            if (cfeStrCountChar(cfeMessages[i].text, '\n') >= 2) {
                y = CFE_SUBTITLE_Y2;
            }
            xglFontPrint(0, y, CFE_SUBTITLE_COLOR, cfeMesFontSettings);
            xglFontPrintDirect(cfeMessages[i].text);
        }
    }
    if (cfePaused != 0) {
        xglClockAccumulate();
        recordUpdatesHold();
        XenoActorSetHoldFlag();
        soundSlotSetPause(0, 1);
        soundSlotSetPause(1, 1);
        seSetMuted(1, 1);
        pauseDrawScreen();
        if ((PadData[0].pressed & PAD_TRIANGLE) && cfeStartDelay == 0) {
            PadData[0].pressed &= ~PAD_TRIANGLE;
            time += CFE_SKIP_FRAMES;
            soundSlotStop(0);
            soundSlotStop(1);
            soundSlotSetPause(0, 0);
            soundSlotSetPause(1, 0);
            seSetMuted(1, 0);
            seVolumeSet(0, 0);
            seVoiceStartDefault(CFE_PAUSE_SOUND);
            sefKillEffect(-1);
            return time;
        }
    } else {
        if (PadData[0].pressed & PAD_START) {
            soundSlotSetPause(0, 0);
            soundSlotSetPause(1, 0);
            seSetMuted(1, 0);
            XenoActorClearHoldFlag();
        }
        time++;
    }
    if (PadData[0].pressed & PAD_START) {
        if ((PadData[0].held & PAD_SOFT_RESET_HELD) == PAD_SOFT_RESET_HELD) {
            gameWork.control |= CONTROL_SOFT_RESET;
        }
    }
    for (i = 0; i < 3; i++) {
        if ((unsigned int) (cfeScreenEffects[i].effectKind - 1) < 2 && cfeScreenEffects[i].sample_int > 0) {
            fadeClear();
        }
        if (cfeScreenEffects[i].effectKind == 0x22) {
            effectRequestToCommand(i, 0x1A, cfeScreenEffects[i].sample_float * CFE_SLOT_SCALE * CFE_SLOT_SHORT,
                          cfeScreenEffects[i].sample_int);
        } else if (cfeScreenEffects[i].effectKind == 0x23) {
            effectRequestToCommand(i, 0x1B, cfeScreenEffects[i].sample_float * CFE_SLOT_SCALE * CFE_SLOT_SHORT,
                          cfeScreenEffects[i].sample_int);
        } else {
            effectRequestToCommand(i, cfeScreenEffects[i].effectKind, cfeScreenEffects[i].sample_float * CFE_SLOT_SCALE,
                          cfeScreenEffects[i].sample_int);
        }
    }
    cfeEntryTableUpdate(cfePaused);
    XenoActorDoit();
    XenoActorDisp();
    if (cfePaused == 0) {
        sefExecEffect();
    }
    return time;
}


/* Script command `cfe.stFdIn`: starts no fade; it only kills every running sound effect (sefKillEffect(-1)). */
int cfeCmdKillEffects(SbThread *handle)
{
    if (VM_PLUGIN_CNT_GET(handle) != 1) {
        assertFailed("VM_PLUGIN_CNT_GET() == 1", "../cfePlugIn.euc.c", 1456);
    }
    sefKillEffect(-1);
    return 1;
}


int cfeCmdFadeOut(SbThread *handle)
{
    int value;
    if (VM_PLUGIN_CNT_GET(handle) != 1) {
        assertFailed("VM_PLUGIN_CNT_GET() == 1", "../cfePlugIn.euc.c", 1475);
    }
    value = vmArgGet(handle, 1);
    fadeStartOut(value);
    return 1;
}

int sceGsSyncPath(int mode, int timeout);

/* The record flag the cut-scene end clears on the current record. */
#define CFE_RECORD_FLAG_2 0x2

/* Script command `cfe.destroy`: ends a cut scene. Stops both sound slots and the owned effects, copies the shown
 * frame to the drawn one, clears the scene control bits and the current record's flag 0x2, frees the
 * message records, the group and entry buffers, the collision, wind, actors and resource slots, and
 * gives the player and part records new actors. */
int cfeCmdDestroy(SbThread *handle)
{
    int i;
    int current;

    soundSlotStop(0);
    soundSlotStop(1);
    sefKillEffect(-1);
    xglRenderClearDepth();
    sceGsSyncPath(0, 0);
    xglRenderCopyDisp2Draw();
    xglSleep();
    xglSleep();
    xglRenderCopyDisp2Draw();
    current = uwamonoCurrentIndex;
    gameWork.control &= ~CONTROL_CFE_SCENE;
    uwamonoRecords[current].flags &= ~CFE_RECORD_FLAG_2;
    gameWork.control &= ~CONTROL_CFE_NO_FADE;
    if (cfeMessages != 0) {
        GameResBlockFreeByBuffer(cfeMessages);
        cfeMessages = 0;
    }
    for (i = 0; i < cfeGroupCount; i++) {
        if (cfeMaps[i].records != 0) {
            GameResBlockFreeByBuffer(cfeMaps[i].records);
        }
        GameResBlockFreeByOwner(cfeMaps[i].object->kind);
    }
    cfeGroupCount = 0;
    for (i = 0; i < cfeEvalEntryCount; i++) {
        if (cfeMonitors[i].movie.source != 0) {
            GameResBlockFreeByBuffer(cfeMonitors[i].movie.source);
        }
        if (cfeMonitors[i].movie.imageAddress[0] != 0) {
            GameResBlockFreeByBuffer(cfeMonitors[i].movie.imageAddress[0]);
        }
        if (cfeMonitors[i].movie.imageAddress[1] != 0) {
            GameResBlockFreeByBuffer(cfeMonitors[i].movie.imageAddress[1]);
        }
    }
    cfeEvalEntryCount = 0;
    tiGlobalCollClearHandle();
    tiWindDisableAll();
    XenoActorReleaseAll();
    GameResourceReset();
    resPlayerSlotsReset();
    for (i = 0; i < CFE_RECORD_COUNT; i++) {
        switch (uwamonoRecords[i].kind) {
        case 4:
        case 5:
            uwamonoRecords[i].body.actor = XenoActorAlloc();
            break;
        }
    }
    xglFontLoad(1, CD_NOTIFY_CURRENT);
    pxyResetLightToDefault();
    return 1;
}

const char cfeNativeNameDestroy[8] = "destroy";
const char cfeNativeNameStEndFrm[16] = "stEndFrm";
const char cfeNativeNameStStrtFrm[16] = "stStrtFrm";
const char cfeNativeNameSetMesScr[16] = "setMesScr";
const char cfeNativeNameSetMes[8] = "setMes";
const char cfeNativeNameAllocMes[16] = "allocMes";
const char cfeNativeNameStKySnd[8] = "stKySnd";
const char cfeNativeNameStKyScrEff[16] = "stKyScrEff";
const char cfeNativeNameStKyMnt[8] = "stKyMnt";
const char cfeNativeNameStKyFog[8] = "stKyFog";
const char cfeNativeNameStKyMp[8] = "stKyMp";
const char cfeNativeNameStKyLgt[8] = "stKyLgt";
const char cfeNativeNameStKyEf[8] = "stKyEf";
const char cfeNativeNameStKyCm[8] = "stKyCm";
const char cfeNativeNameStKyAtPrts[16] = "stKyAtPrts";
const char cfeNativeNameStKyAtHd[16] = "stKyAtHd";
const char cfeNativeNameStKyAt[8] = "stKyAt";
const char cfeNativeNameStFrm[8] = "stFrm";
const char cfeNativeNameStFdOut[8] = "stFdOut";
const char cfeNativeNameStFdIn[8] = "stFdIn";
const char cfeNativeNameLdSnd[8] = "ldSnd";
const char cfeNativeNameLdDev[8] = "ldDev";
const char cfeNativeNameLdMnt[8] = "ldMnt";
const char cfeNativeNameLdMpDev[8] = "ldMpDev";
const char cfeNativeNameLdMp[8] = "ldMp";
const char cfeNativeNameLdEf[8] = "ldEf";
const char cfeNativeNameLdMt[8] = "ldMt";
const char cfeNativeNameLdAt[8] = "ldAt";
const char cfeNativeNameInit[8] = "init";


int cfePaused = 0;
int cfeStartDelay = 0;
int cfeMessageCount = 0;
CfeMessageRecord *cfeMessages = 0;
const char cfePlugInName[16] = "cfe";

SbNativeMethod cfePlugInTable[30] = {
    { cfeNativeNameInit, cfeCmdInit },
    { cfeNativeNameLdAt, cfeCmdLoadActor },
    { cfeNativeNameLdMt, cfeCmdLoadMotion },
    { cfeNativeNameLdEf, cfeCmdLoadEffect },
    { cfeNativeNameLdMp, cfeCmdLoadMap },
    { cfeNativeNameLdMpDev, cfeCmdLoadMapCollision },
    { cfeNativeNameLdMnt, cfeCmdLoadMonitor },
    { cfeNativeNameLdDev, cfeCmdLoadDev },
    { cfeNativeNameLdSnd, cfeCmdLoadSound },
    { cfeNativeNameStFdIn, cfeCmdKillEffects },
    { cfeNativeNameStFdOut, cfeCmdFadeOut },
    { cfeNativeNameStFrm, cfeCmdStepFrame },
    { cfeNativeNameStKyAt, cfeCmdSetActorKey },
    { cfeNativeNameStKyAtHd, cfeCmdSetActorHeadKey },
    { cfeNativeNameStKyAtPrts, cfeCmdSetActorPartKey },
    { cfeNativeNameStKyCm, cfeCmdSetCameraKey },
    { cfeNativeNameStKyEf, cfeCmdSetEffectKey },
    { cfeNativeNameStKyLgt, cfeCmdSetLightKey },
    { cfeNativeNameStKyMp, cfeCmdSetMapKey },
    { cfeNativeNameStKyFog, cfeCmdSetFogKey },
    { cfeNativeNameStKyMnt, cfeCmdSetMonitorKey },
    { cfeNativeNameStKyScrEff, cfeCmdSetScreenEffectKey },
    { cfeNativeNameStKySnd, cfeCmdSetSoundKey },
    { cfeNativeNameAllocMes, cfeCmdAllocMessages },
    { cfeNativeNameSetMes, cfeCmdSetMessage },
    { cfeNativeNameSetMesScr, cfeCmdSetMessageScr },
    { cfeNativeNameStStrtFrm, cfeCmdSetStartFrame },
    { cfeNativeNameStEndFrm, cfeCmdSetEndFrame },
    { cfeNativeNameDestroy, cfeCmdDestroy },
    { 0, 0 },
};

void cfeRegisterPlugIn(void)
{
    sbNativeOwnerAdd(cfePlugInName, cfePlugInTable);
}

const char cfeEmptyName[8] = { 0 };

