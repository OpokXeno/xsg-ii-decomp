#include "common.h"
#include "shared.h"
#include "main/tu017.h"

/* Movie playback through the player library at 0x14E8A8..0x151CE8. Its creation parameters, its
 * frame record and its method table have the layout of CRI Sofdec's MWPLY interface (creation
 * parameters: file type, maximum bit rate, maximum size, frame pools, streams, work area and its
 * size, composition mode; methods 5 to 11: destroy, start by file name, stop, get status, get
 * frame, release frame, set volume). The names below follow that interface where this TU's own
 * accesses agree with it; each such access is noted at the member. */

/* DMA tag ID (bits 28..30 of the first tag word). */
#define DMA_TAG_ID_CNT 0x10000000U
#define DMA_TAG_ID_REF 0x30000000U
#define DMA_TAG_ID_RET 0x60000000U
/* VIF1 DIRECT code with its quadword count; in a DMA tag it is the high word of the second
 * doubleword. */
#define VIF_DIRECT 0x51000000U
#define DMA_TAG_VIF_DIRECT(qwords) ((u64) (VIF_DIRECT | (qwords)) << 32)
/* GIF tag fields. */
#define GIF_TAG_EOP 0x8000
#define GIF_TAG_NREG_1 0x1000000000000000
#define GIF_TAG_FLG_IMAGE 0x0800000000000000
#define GIF_REG_AD 14
#define SCE_GS_TEXFLUSH 0x3F
#define SCE_GS_BITBLTBUF 0x50
#define SCE_GS_TRXPOS 0x51
#define SCE_GS_TRXREG 0x52
#define SCE_GS_TRXDIR 0x53
#define GS_TRXDIR_LOCAL_TO_LOCAL 2
/* A decoded frame is uploaded as 16x16 RGBA32 macroblocks. */
#define MOVIE_MACROBLOCK 16
#define MOVIE_MACROBLOCK_BYTES 0x400
#define MOVIE_MACROBLOCK_QWORDS 0x40
#define MOVIE_GIF_TAG_AD_2 (GIF_TAG_NREG_1 | GIF_TAG_EOP | 2)
#define MOVIE_GIF_TAG_AD_5 (GIF_TAG_NREG_1 | GIF_TAG_EOP | 5)
#define MOVIE_GIF_TAG_IMAGE_BLOCK (GIF_TAG_FLG_IMAGE | GIF_TAG_EOP | MOVIE_MACROBLOCK_QWORDS)
/* BITBLTBUF buffer widths, in units of 64 pixels: source 512, destination 512 and 896. */
#define MOVIE_BITBLTBUF_SBW_512 0x80000
#define MOVIE_BITBLTBUF_DBW_512 0x8000000000000
#define MOVIE_BITBLTBUF_DBW_896 ((u64) 0xe000 << 36)
#define MOVIE_TRXREG_MACROBLOCK 0x1000000010
#define MOVIE_WIDTH 512
#define MOVIE_HEIGHT 448
/* The frame copy of moviePlayerUpdate: one tag quadword, one GIF tag and five A+D registers. */
#define MOVIE_COPY_QWORDS 7
/* EE scratchpad RAM. */
#define SCRATCHPAD_BASE 0x70000000
#define UNCACHED(address) ((u64 *) (((address) & 0x0fffffff) | 0x20000000))

typedef struct XglDmaRefTag {
    unsigned int tag;
    unsigned int address;
    u64 upperQword;
} XglDmaRefTag;

/* Builds the DMA chain that uploads a width x height RGBA32 image at `image` to the GS, one
 * 16x16 macroblock per REF tag, and returns its length in quadwords. With a null `destination`
 * nothing is written and only the length is computed. The destination BITBLTBUF of the header is
 * a placeholder; moviePlayerUpdate overwrites it. */
int makeGstransChain(unsigned int destination, unsigned int image, int width, int height)
{
    u64 *packet = UNCACHED(destination);
    /* a copy of image: stepping the parameter itself puts its register copy earlier */
    unsigned int block = image;
    int x;
    int y;

    if (destination != 0) {
        packet[0] = DMA_TAG_ID_CNT | 3;
        packet[1] = DMA_TAG_VIF_DIRECT(3);
        packet[2] = MOVIE_GIF_TAG_AD_2;
        packet[3] = GIF_REG_AD;
        packet[4] = MOVIE_BITBLTBUF_DBW_896;
        packet[5] = SCE_GS_BITBLTBUF;
        packet[6] = MOVIE_TRXREG_MACROBLOCK;
        packet[7] = SCE_GS_TRXREG;
    }
    packet += 8;
    for (x = 0; x < width; x += MOVIE_MACROBLOCK) {
        for (y = 0; y < height; y += MOVIE_MACROBLOCK) {
            if (destination != 0) {
                packet[0] = DMA_TAG_ID_CNT | 4;
                packet[1] = DMA_TAG_VIF_DIRECT(4);
                packet[2] = MOVIE_GIF_TAG_AD_2;
                packet[3] = GIF_REG_AD;
                packet[4] = ((u64) x << 32) | ((u64) y << 48);
                packet[5] = SCE_GS_TRXPOS;
                packet[6] = 0;
                packet[7] = SCE_GS_TRXDIR;
                packet[8] = MOVIE_GIF_TAG_IMAGE_BLOCK;
                packet[9] = 0;
            }
            packet += 10;
            if (destination != 0) {
                ((XglDmaRefTag *) packet)->address = block;
                ((XglDmaRefTag *) packet)->tag = DMA_TAG_ID_REF | MOVIE_MACROBLOCK_QWORDS;
                ((XglDmaRefTag *) packet)->upperQword = DMA_TAG_VIF_DIRECT(MOVIE_MACROBLOCK_QWORDS);
            }
            packet += 2;
            block += MOVIE_MACROBLOCK_BYTES;
        }
    }
    if (destination != 0) {
        packet[1] = 0;
        packet[0] = DMA_TAG_ID_RET;
    }
    packet += 2;
    return (((unsigned int) packet - destination) & 0xFFFFFF) >> 4;
}

typedef struct MoviePlayer MoviePlayer;
typedef void (*MoviePlayerMethod)(MoviePlayer *self);

/* The player's method table. Evidenced here: start_file gets the file key (moviePlayerStartFile),
 * get_status returns the MOVIE_STATUS_* value moviePlayerUpdate tests, set_volume gets the result
 * of seVolumeCurve, pause gets game_movie's on/off flag (moviePlayerPause passes its second
 * argument through in $a1). destroy and stop are called with the player alone (moviePlayerDestroy);
 * their names follow the interface order. */
typedef struct MoviePlayerMethods {
    MoviePlayerMethod unmodeled_00[5];
    void (*destroy)(MoviePlayer *self);
    void (*start_file)(MoviePlayer *self, const char *file_key);
    void (*stop)(MoviePlayer *self);
    int (*get_status)(MoviePlayer *self);
    MoviePlayerMethod unmodeled_24;
    void (*pause)(MoviePlayer *self, int on);
    void (*set_volume)(MoviePlayer *self, int volume);
} MoviePlayerMethods;

struct MoviePlayer {
    MoviePlayerMethods *methods;
};

/* get_status values moviePlayerUpdate tells apart. */
#define MOVIE_STATUS_PLAYING 3
#define MOVIE_STATUS_PLAY_END 4

/* The creation parameters moviePlayerParamsInit fills and func_0014FD80 takes. max_width and
 * max_height are also the size makeGstransChain is built for; base and size are the work area.
 * The other members are only stored here. */
typedef struct AllocLayout {
    int file_type;
    int max_bit_rate;
    int max_width;
    int max_height;
    int frame_pool_count;
    int stream_count;
    int base;
    int size;
    int composition_mode;
    unsigned char unmodeled_24[0xC];
} AllocLayout;

#define MOVIE_FILE_TYPE 1
#define MOVIE_MAX_BIT_RATE 7000000
#define MOVIE_FRAME_POOL_COUNT 3
#define MOVIE_STREAM_COUNT 1

/* frame_stamp: the stamp of the last frame taken, -1 from the start of a file until its first
 * frame. frame_held: a frame was taken and is not released yet. max_value is the volume level
 * given to seVolumeCurve; it is read unsigned (lbu at 0x1A039C). */
typedef struct AllocRecord {
    int handle;
    int data_start;
    int frame_stamp;
    unsigned char frame_held;
    unsigned char extra_enabled;
    unsigned char max_value;
} AllocRecord;

/* The record's handle is the player object func_0014FD80 creates. It stays an int: typed as a
 * pointer it lands in another alias set and movieRenderBufferAlloc stores it in a different order. */
#define RECORD_PLAYER(record) ((MoviePlayer *) (record)->handle)

/* This TU's view of the 0x30-byte file-position record xglCdCheckFile fills (CdFilePosition in
 * xgl_cd.c; the integrator reconciles). stored_bytes is unsigned here: moviePlayerStartFile turns
 * it into sectors with a logical shift (srl at 0x1A045C). */
typedef struct MovieFileRecord {
    unsigned int lbn;
    unsigned int stored_bytes;
    unsigned char unmodeled_08[0x28];
} MovieFileRecord;

#define MOVIE_PATH_MAX 256
#define CD_SECTOR_SHIFT 11

/* The disc folder of the movie files ("movie\"), and the "%08x.%08x" key format (start sector,
 * then sectors). */
extern const char *moviePathPrefix;
extern const char movieFileKeyFormat[];
int seVolumeCurve(int level);
int xglCdCheckFile(const char *path, MovieFileRecord *record);
int sprintf(char *destination, const char *format, ...);

/* Starts the movie file `name` on the record's player: sets the volume (unless the record has the
 * extra sound block), builds "<moviePathPrefix><name with / as \>", looks it up on the disc and
 * gives the player the key "lbn.sectors". Returns -1 when the file is missing, else 0. */
int moviePlayerStartFile(AllocRecord *player, const char *name)
{
    char path[MOVIE_PATH_MAX];
    MovieFileRecord file;
    const char *source;
    char *destination;
    char character;

    if (player->extra_enabled == 0) {
        RECORD_PLAYER(player)->methods->set_volume(RECORD_PLAYER(player), seVolumeCurve(player->max_value));
    }
    destination = path;
    source = moviePathPrefix;
    while (*source != 0) {
        *destination = *source;
        destination++;
        source++;
    }
    source = name;
    do {
        character = *source++;
        if (character == '/') {
            character = '\\';
        }
        *destination++ = character;
    } while (character != 0);
    /* As in the original, this third copy goes on from the byte after the name's terminator
     * (the code at 0x1A0410..0x1A0430 does not load a new source). */
    while (*source != 0) {
        *destination = *source;
        destination++;
        source++;
    }
    if (xglCdCheckFile(path, &file) != 0) {
        return -1;
    }
    /* the key is written over the path, from its second character on */
    sprintf(path + 1, movieFileKeyFormat, file.lbn, file.stored_bytes >> CD_SECTOR_SHIFT);
    RECORD_PLAYER(player)->methods->start_file(RECORD_PLAYER(player), path + 1);
    player->frame_stamp = -1;
    return 0;
}

typedef struct XglPacket XglPacket;

/* The frame record func_00150988 fills: a null image means no new frame. width and height go to
 * makeGstransChain; stamp is kept in the record (never negative for a frame, see frame_stamp). */
typedef struct MovieFrame {
    unsigned int image;
    unsigned char unmodeled_04[4];
    int width;
    int height;
    unsigned char unmodeled_10[0x10];
    int stamp;
    unsigned char unmodeled_24[0x2C];
} MovieFrame;

/* The halfwords of sRender this TU reads, named as in XglRenderVram of xgl_render.c. Here the
 * decoded frame is uploaded to the page flip_base and copied from there to the page `width`. */
typedef struct MovieRenderState {
    unsigned char unmodeled_00[0x10];
    u16 width;
    u16 height;
    u16 flip_base;
} MovieRenderState;

extern MovieRenderState sRender;
XglPacket *xglPacketGetCurrent(void);
void sceVif1PkCnt(XglPacket *packet, int count);
void sceVif1PkOpenDirectHLCode(XglPacket *packet, int mode);
void sceVif1PkAddDirectDataN(XglPacket *packet, const void *data, int count);
void sceVif1PkCloseDirectHLCode(XglPacket *packet);
void func_0012B618(XglPacket *packet, unsigned int chain, int flag);
void func_00150988(int handle, MovieFrame *frame);
void func_00151438(int handle);

/* One step of playback: releases the frame taken last time, takes the next one and, when there is
 * one, queues its upload chain; then, once a frame has arrived, queues the copy of the uploaded
 * page to the page shown. Returns 1 while the player is playing, -1 when it reached the end,
 * 0 otherwise. */
int moviePlayerUpdate(AllocRecord *player)
{
    MovieFrame frame;
    XglPacket *packet;
    u64 *scratch;
    u64 *chain;
    int status;

    packet = xglPacketGetCurrent();
    if (player->frame_held != 0) {
        func_00151438(player->handle);
    }
    func_00150988(player->handle, &frame);
    player->frame_held = 0;
    if (frame.image != 0) {
        player->frame_stamp = frame.stamp;
        makeGstransChain(player->data_start, frame.image, frame.width, frame.height);
        chain = UNCACHED(player->data_start);
        chain[4] = ((u64) sRender.flip_base << 37) | MOVIE_BITBLTBUF_DBW_512;
        func_0012B618(packet, player->data_start, 0);
        player->frame_held = 1;
    }
    if (player->frame_stamp >= 0) {
        scratch = (u64 *) SCRATCHPAD_BASE;
        scratch[0] = 0;
        scratch[1] = VIF_DIRECT | (MOVIE_COPY_QWORDS - 1);
        scratch[2] = MOVIE_GIF_TAG_AD_5;
        scratch[3] = GIF_REG_AD;
        scratch[4] = (MOVIE_BITBLTBUF_SBW_512 | (sRender.flip_base << 5)) | ((u64) sRender.width << 37)
                     | MOVIE_BITBLTBUF_DBW_512;
        scratch[5] = SCE_GS_BITBLTBUF;
        scratch[6] = 0;
        scratch[7] = SCE_GS_TRXPOS;
        scratch[8] = ((u64) MOVIE_HEIGHT << 32) | MOVIE_WIDTH;
        scratch[9] = SCE_GS_TRXREG;
        scratch[10] = GS_TRXDIR_LOCAL_TO_LOCAL;
        scratch[11] = SCE_GS_TRXDIR;
        scratch[12] = 0;
        scratch[13] = SCE_GS_TEXFLUSH;
        sceVif1PkCnt(packet, 0);
        sceVif1PkOpenDirectHLCode(packet, 0);
        sceVif1PkAddDirectDataN(packet, scratch, MOVIE_COPY_QWORDS);
        sceVif1PkCloseDirectHLCode(packet);
    }
    status = RECORD_PLAYER(player)->methods->get_status(RECORD_PLAYER(player));
    switch (status) {
    case MOVIE_STATUS_PLAYING:
        return 1;
    case MOVIE_STATUS_PLAY_END:
        return -1;
    default:
        return 0;
    }
}

/* Pauses (on != 0) or resumes the player of record; game_movie toggles it with START. */
int moviePlayerPause(MoviePlayer **player_slot, int on)
{
    MoviePlayer *player;

    player = *player_slot;
    player->methods->pause(player, on);
    return 0;
}

void func_0014E8C0(int handle);
void func_0017D868(void);
void func_0017D150(void);
void func_0013A8A0(int value);
void soundSlotsReopen(void);

/* Stops and destroys the player of record, and when the record had the extra sound block,
 * releases it and brings the sound slots back. */
int moviePlayerDestroy(AllocRecord *player)
{
    RECORD_PLAYER(player)->methods->stop(RECORD_PLAYER(player));
    if (player->extra_enabled != 0) {
        func_0014E8C0(player->handle);
    }
    RECORD_PLAYER(player)->methods->destroy(RECORD_PLAYER(player));
    if (player->extra_enabled != 0) {
        func_0017D868();
        func_0017D150();
        func_0013A8A0(-1);
        soundSlotsReopen();
    }
    return 0;
}

void *memset(void *dest, int value, unsigned int size);
int func_0014EF18(AllocLayout *layout);

/* Fills the creation parameters for one 512x448 movie player and has func_0014EF18 compute the
 * size of the work area they need. */
void moviePlayerParamsInit(AllocLayout *layout)
{
    memset(layout, 0, sizeof(*layout));
    layout->file_type = MOVIE_FILE_TYPE;
    layout->max_bit_rate = MOVIE_MAX_BIT_RATE;
    layout->max_width = MOVIE_WIDTH;
    layout->max_height = MOVIE_HEIGHT;
    layout->frame_pool_count = MOVIE_FRAME_POOL_COUNT;
    layout->stream_count = MOVIE_STREAM_COUNT;
    layout->composition_mode = 0;
    layout->size = func_0014EF18(layout);
}

int func_001561C8(void);

/*
 * Bytes one render buffer needs: the layout block, the GS packet chain for max_width x max_height
 * (16 bytes per quadword), and the optional extra block. Each part is rounded up to 64 bytes.
 */
int movieRenderBufferSize(int include_extra)
{
    AllocLayout layout;
    int align_mask;
    int total;

    align_mask = -0x40;
    moviePlayerParamsInit(&layout);
    total = (layout.size + 0x3F) & align_mask;
    total += ((makeGstransChain(0, 0, layout.max_width, layout.max_height) * 0x10) + 0x3F) & align_mask;
    if (include_extra != 0) {
        total += (func_001561C8() + 0x3F) & align_mask;
    }
    return total;
}

int func_0014E8A8(int handle, int address, int size);
int func_0014FD80(AllocLayout *layout);
int func_0017D850(void);

/* Fills one allocation record for the render buffer placed at cursor and returns the cursor past its data. */
int movieRenderBufferAlloc(AllocRecord *record, int cursor, int extra_enabled)
{
    AllocLayout layout;
    int align_mask;

    record->max_value = 0x7F;
    record->frame_held = 0;
    record->extra_enabled = extra_enabled;
    if ((unsigned char) extra_enabled != 0) {
        func_0017D850();
    }
    align_mask = -0x40;
    moviePlayerParamsInit(&layout);
    layout.base = cursor;
    cursor += (layout.size + 0x3F) & align_mask;
    record->handle = func_0014FD80(&layout);
    record->data_start = cursor;
    cursor += ((makeGstransChain(cursor, 0, layout.max_width, layout.max_height) * 0x10) + 0x3F) & align_mask;
    if (record->extra_enabled != 0) {
        int extra_size;

        extra_size = func_001561C8();
        func_0014E8A8(record->handle, cursor, extra_size);
        cursor += (func_001561C8() + 0x3F) & align_mask;
    }
    return cursor;
}

int func_00131450(void);

void xglMovieInit(void)
{
    func_00131450();
}

typedef struct MoviePlayerInitParams {
    float value;
    int flag_a;
    int flag_b;
    int flag_c;
    unsigned char unmodeled_10[0x10];
} MoviePlayerInitParams;

int func_00151988(MoviePlayerInitParams *params);
extern float D_0069B620;

void moviePlayerInit(void)
{
    MoviePlayerInitParams params;

    memset(&params, 0, sizeof(params));
    params.flag_c = 0;
    params.value = D_0069B620;
    params.flag_b = (params.flag_a = 1);
    func_00151988(&params);
}

int func_00139E18(void);
int func_00151CE8(void);
int func_0017B580(void);
int func_00182EC0(void);
int sceGsSyncVCallback(void (*callback)(void));

void func_001A0980(void)
{
    sceGsSyncVCallback(0);
    func_00139E18();
    func_00151CE8();
    func_00182EC0();
    func_0017B580();
}
