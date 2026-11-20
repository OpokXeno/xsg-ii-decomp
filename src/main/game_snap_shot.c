#include "common.h"
#include "shared.h"
#include "main/game_snap_shot.h"
#include "main/xgl_jpeg.h"
#include "main/xgl_dma.h"
#include "main/game_data.h"
#include "main/control_entry.h"
#include "main/gr_screen.h"
#include "main/xgl_cd.h"

typedef unsigned int XglQuadword __attribute__((mode(TI)));

typedef union SnapshotDmaPacket {
    u64 words[24];
    XglQuadword quadwords[12];
} SnapshotDmaPacket;

enum {
    SNAPSHOT_OPEN_FLAGS = 0x602,
    SNAPSHOT_CACHE_LINE_SIZE = 64,
    SNAPSHOT_WIDTH = 512,
    SNAPSHOT_HEIGHT = 448,
    SNAPSHOT_METADATA_SIZE = 160,
    SNAPSHOT_HEADER_SIZE = 6,       /* SOI marker, APP14 marker and its length */
    SNAPSHOT_CHANNEL_LUMA = 0,
    SNAPSHOT_CHANNEL_CB = 1,
    SNAPSHOT_CHANNEL_CR = 2,
    SNAPSHOT_THUMBNAIL_WIDTH = 128,
    SNAPSHOT_THUMBNAIL_HEIGHT = 112,
    SNAPSHOT_THUMBNAIL_SOURCE_X = 0x3800,
    SNAPSHOT_THUMBNAIL_SOURCE_Y = 8,
    SNAPSHOT_THUMBNAIL_QUALITY = 50,
    SNAPSHOT_SEGMENT_OVERHEAD = 4,
    SNAPSHOT_MARKER_PREFIX = 0xFF,
    SNAPSHOT_MARKER_SOI = 0xD8,
    SNAPSHOT_MARKER_APP14 = 0xEE,
    SNAPSHOT_FILE_MODE = 0666,
    SNAPSHOT_FRAME1_FBP_MASK = 0x1ff,
    SNAPSHOT_FRAME1_FBW_MASK = 0x3f,
};

typedef struct {
    unsigned char quality;
    unsigned char unmodeled_01[3];
} GameSnapShotQualityChannel;

typedef struct {
    GameSnapShotQualityChannel channel[3];
    unsigned char unmodeled_0c[4];
} GameSnapShotQuality;

extern void SyncDCache(void *start, void *end);
extern int sceGsSyncPath(int mode, int timeout);
/* Points at the slot digit inside the snapshot file name (snapname + 0x1F). */
extern char *snapnameno;
extern GameSnapShotQuality quality_table[5];
extern SnapshotDmaPacket TestPrim_0_002BC220;

int GameSnapShotNumber(int number) {
    if (number >= 0) {
        *snapnameno = number + '0';
    }
    return *snapnameno - '0';
}

void GameSnapShotCheck(void) {
}

typedef struct {
    unsigned char unmodeled_00[0x10];
    unsigned long long frame1;
} GameSnapShotDrawEnv;

extern GameSnapShotDrawEnv DrawEnv;
extern const char snapShotLogFormat[];
extern const char snapShotQualityFormat[];
extern int printf(const char *format, ...);
extern int sceOpen(const char *path, int flags, ...);
extern int sceWrite(int fd, const void *buffer, int size);
extern int sceClose(int fd);

/* Encodes the 512x448 display frame (DrawEnv display origin) into the WorkEnd buffer and writes the JPEG to path. */
void GameSnapShotWriteJpeg(const char *path, int quality) {
    JpegEncodeRequest params;
    void *output_buffer;
    int fd;
    int frame_base;

    memset(&params, 0, sizeof(params));
    output_buffer = (void *)(((unsigned int)WorkEnd + (SNAPSHOT_CACHE_LINE_SIZE - 1)) & ~(SNAPSHOT_CACHE_LINE_SIZE - 1));
    params.outputBuffer = output_buffer;
    printf(snapShotLogFormat, path, output_buffer);
    frame_base = DrawEnv.frame1 & SNAPSHOT_FRAME1_FBP_MASK;
    params.frameBufferBase = frame_base << 5;
    params.frameBufferWidth = (DrawEnv.frame1 >> 16) & SNAPSHOT_FRAME1_FBW_MASK;
    params.width = SNAPSHOT_WIDTH;
    params.height = SNAPSHOT_HEIGHT;
    params.originX = 0;
    params.originY = 0;
    if (quality < 0) {
        params.luminanceQuality = quality_table[~quality].channel[SNAPSHOT_CHANNEL_LUMA].quality;
        params.chrominanceQuality = quality_table[~quality].channel[SNAPSHOT_CHANNEL_CB].quality;
        params.samplingMode = quality_table[~quality].channel[SNAPSHOT_CHANNEL_CR].quality;
    } else {
        params.luminanceQuality = quality;
        params.chrominanceQuality = quality;
        params.samplingMode = JPEG_SAMPLING_444;
    }
    printf(snapShotQualityFormat, params.luminanceQuality, params.chrominanceQuality, params.samplingMode);
    xglJpegEncode(&params);
    fd = sceOpen(path, SNAPSHOT_OPEN_FLAGS, SNAPSHOT_FILE_MODE);
    sceWrite(fd, params.outputBuffer, params.outputSize);
    sceClose(fd);
}

/* Points at the three-character extension of the snapshot file name. */
extern char *snapnamext;
extern char snapname[];


typedef struct {
    XglClock save_time;
    XglClock play_time;
    int save_word;
    unsigned char unmodeled_14[0x8C];
} GameSnapShotMetadata;

/* Calls GameSnapShotWriteJpeg; with flags & 1 writes the
 * 160-byte metadata to the .bin name first, then advances the four-digit counter in snapnameno. */
void GameSnapShotExecute(int quality, int flags) {
    GameSnapShotMetadata metadata;
    int file_handle;

    GameSnapShotWriteJpeg(snapname, quality);
    if (flags & 1) {
        snapnamext[0] = 'b';
        snapnamext[1] = 'i';
        snapnamext[2] = 'n';
        memset(&metadata, 0, sizeof(metadata));
        xglClockRead(&metadata.save_time);
        xglClockUInt2DayTime(&metadata.play_time, 0);
        metadata.save_word = gameWork.resourceId;
        file_handle = sceOpen(snapname, SNAPSHOT_OPEN_FLAGS, SNAPSHOT_FILE_MODE);
        sceWrite(file_handle, &metadata, sizeof(metadata));
        sceClose(file_handle);
        snapnamext[0] = 'j';
        snapnamext[1] = 'p';
        snapnamext[2] = 'g';
    }
    if (snapnameno[3] == '9') {
        if (snapnameno[2] == '9') {
            snapnameno[1]++;
            snapnameno[2] = '0';
        } else {
            snapnameno[2]++;
        }
        snapnameno[3] = '0';
    } else {
        snapnameno[3]++;
    }
}

unsigned int GameSnapShotSaveThumbnail(int quality, void *output_buffer) {
    JpegEncodeRequest params;
    unsigned long long bitbltbuf;

    bitbltbuf = 0x24020000 | (sRender.draw_buffer_base << 5);
    TestPrim_0_002BC220.words[10] = bitbltbuf | (0xC800ULL << 19);
    TestPrim_0_002BC220.words[8] = 0x801C0;
    SyncDCache(TestPrim_0_002BC220.words, TestPrim_0_002BC220.words + 24);
    sceGsSyncPath(0, 0);
    xglDmaDirectNormal(2, (u32)TestPrim_0_002BC220.words, 12);
    sceGsSyncPath(0, 0);

    memset(&params, 0, sizeof(params));
    params.outputBuffer = output_buffer;
    params.frameBufferBase = SNAPSHOT_THUMBNAIL_SOURCE_X;
    params.frameBufferWidth = SNAPSHOT_THUMBNAIL_SOURCE_Y;
    params.width = SNAPSHOT_THUMBNAIL_WIDTH;
    params.height = SNAPSHOT_THUMBNAIL_HEIGHT;
    params.originX = 0;
    params.originY = 0;
    if (quality < 0) {
        params.luminanceQuality = quality_table[~quality].channel[SNAPSHOT_CHANNEL_LUMA].quality;
        params.chrominanceQuality = quality_table[~quality].channel[SNAPSHOT_CHANNEL_CB].quality;
        params.samplingMode = quality_table[~quality].channel[SNAPSHOT_CHANNEL_CR].quality;
    } else {
        params.luminanceQuality = quality;
        params.chrominanceQuality = quality;
        params.samplingMode = JPEG_SAMPLING_444;
    }
    xglJpegEncode(&params);
    return params.outputSize;
}

int GameSnapShotSave(int quality, unsigned char *output) {
    JpegEncodeRequest params;
    GameSnapShotMetadata meta;
    unsigned char *output_head;
    unsigned char *write_cursor;
    unsigned int thumbnail_size;
    int app14_length;
    int frame_base;

    output_head = output;
    write_cursor = output;
    write_cursor[1] = SNAPSHOT_MARKER_SOI;
    write_cursor[0] = SNAPSHOT_MARKER_PREFIX;
    write_cursor[2] = SNAPSHOT_MARKER_PREFIX;
    write_cursor[3] = SNAPSHOT_MARKER_APP14;
    write_cursor += SNAPSHOT_HEADER_SIZE;

    memset(&meta, 0, SNAPSHOT_METADATA_SIZE);
    xglClockRead(&meta.save_time);
    xglClockUInt2DayTime(&meta.play_time, 0);
    meta.save_word = gameWork.resourceId;
    *(GameSnapShotMetadata *)write_cursor = meta;
    write_cursor += SNAPSHOT_METADATA_SIZE;

    thumbnail_size = GameSnapShotSaveThumbnail(SNAPSHOT_THUMBNAIL_QUALITY, write_cursor);
    app14_length = thumbnail_size + SNAPSHOT_METADATA_SIZE + SNAPSHOT_SEGMENT_OVERHEAD;
    output_head[5] = app14_length;
    output_head[4] = app14_length >> 8;
    write_cursor += thumbnail_size;
    thumbnail_size += SNAPSHOT_METADATA_SIZE + SNAPSHOT_HEADER_SIZE;

    memset(&params, 0, sizeof(params));
    params.outputBuffer = write_cursor;
    frame_base = DrawEnv.frame1 & SNAPSHOT_FRAME1_FBP_MASK;
    params.frameBufferBase = frame_base << 5;
    params.frameBufferWidth = (DrawEnv.frame1 >> 16) & SNAPSHOT_FRAME1_FBW_MASK;
    params.width = SNAPSHOT_WIDTH;
    params.height = SNAPSHOT_HEIGHT;
    params.originX = 0;
    params.originY = 0;
    if (quality < 0) {
        params.luminanceQuality = quality_table[~quality].channel[SNAPSHOT_CHANNEL_LUMA].quality;
        params.chrominanceQuality = quality_table[~quality].channel[SNAPSHOT_CHANNEL_CB].quality;
        params.samplingMode = quality_table[~quality].channel[SNAPSHOT_CHANNEL_CR].quality;
    } else {
        params.luminanceQuality = quality;
        params.chrominanceQuality = quality;
        params.samplingMode = JPEG_SAMPLING_444;
    }
    printf(snapShotLogFormat, snapname, params.outputBuffer);
    printf(snapShotQualityFormat, params.luminanceQuality, params.chrominanceQuality, params.samplingMode);
    xglJpegEncode(&params);
    return thumbnail_size + params.outputSize;
}

int GameSnapShotSaveFile(int slot_number, const void *data, int size)
{
    int file_handle;

    if (slot_number >= 0) {
        snapnameno[3] = slot_number % 10 + '0';
        slot_number /= 10;
        snapnameno[2] = slot_number % 10 + '0';
        slot_number /= 10;
        snapnameno[1] = slot_number % 10 + '0';
        slot_number /= 10;
        snapnameno[0] = slot_number % 10 + '0';
    }
    file_handle = sceOpen(snapname, SNAPSHOT_OPEN_FLAGS, SNAPSHOT_FILE_MODE);
    sceWrite(file_handle, data, size);
    sceClose(file_handle);
    slot_number = (snapnameno[3] - '0') * 1000 + (snapnameno[2] - '0') * 100 +
            (snapnameno[1] - '0') * 10 + (snapnameno[0] - '0');
    if (snapnameno[3] == '9') {
        if (snapnameno[2] == '9') {
            snapnameno[1]++;
            snapnameno[2] = '0';
        } else {
            snapnameno[2]++;
        }
        snapnameno[3] = '0';
        return slot_number;
    }
    snapnameno[3]++;
    return slot_number;
}

const char snapShotLogFormat[32] = "*** SnapShot:%s,%08x ***\n";

const char snapShotQualityFormat[24] = "%d,%d,%d\n";

char snapname[40] = "host0:/home/xeno/work/snap/game0000.jpg";

GameSnapShotQuality quality_table[5] = {
    { { { 100, 0, 0, 0 }, { 100, 0, 0, 0 }, { 0, 0, 0, 0 } }, { 0, 0, 0, 0 } },
    { { { 90, 0, 0, 0 }, { 90, 0, 0, 0 }, { 0, 0, 0, 0 } }, { 0, 0, 0, 0 } },
    { { { 75, 0, 0, 0 }, { 75, 0, 0, 0 }, { 0, 0, 0, 0 } }, { 0, 0, 0, 0 } },
    { { { 50, 0, 0, 0 }, { 50, 0, 0, 0 }, { 0, 0, 0, 0 } }, { 0, 0, 0, 0 } },
    { { { 30, 0, 0, 0 }, { 30, 0, 0, 0 }, { 0, 0, 0, 0 } }, { 0, 0, 0, 0 } },
};

char *snapnameno = snapname + 31;

char *snapnamext = snapname + 36;

SnapshotDmaPacket TestPrim_0_002BC220 = { {
    0x0000000000000000ULL, 0x5100000B00000000ULL, 0xA08B400000008001ULL, 0x000000E53531EEEEULL,
    0x0000000000071001ULL, 0x0000000000000047ULL, 0x0000000000000060ULL, 0x0000000000000014ULL,
    0x0000000000000000ULL, 0x000000000000004CULL, 0x0000000000000000ULL, 0x0000000000000006ULL,
    0x0000008000000080ULL, 0x0000000000000080ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
    0x000071F700006FF8ULL, 0x00000000FFFFFFFFULL, 0x00001C0000002000ULL, 0x0000000000000000ULL,
    0x000078F7000077F8ULL, 0x00000000FFFFFFFFULL, 0x0000000000000000ULL, 0x000000000000003FULL,
} };
