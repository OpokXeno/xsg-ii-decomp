#ifndef INCLUDE_MAIN_TU017_H
#define INCLUDE_MAIN_TU017_H

/* A decoded movie frame is uploaded as 16x16 RGBA32 macroblocks of a 512x448 picture. */
#define MOVIE_MACROBLOCK 16
#define MOVIE_MACROBLOCK_BYTES 0x400
#define MOVIE_MACROBLOCK_QWORDS 0x40
#define MOVIE_WIDTH 512
#define MOVIE_HEIGHT 448

/* The movie player record game_movie keeps. handle is the player object (an int, see
 * RECORD_PLAYER in tu017.c); chain_address is the GS upload chain. frame_stamp: the stamp of the
 * last frame taken, -1 from the start of a file until its first frame. frame_held: a frame was
 * taken and is not released yet. volume goes to seVolumeCurve (read unsigned, lbu at 0x1A039C). */
typedef struct MoviePlayerRecord {
    int handle;
    int chain_address;
    int frame_stamp;
    unsigned char frame_held;
    unsigned char extra_enabled;
    unsigned char volume;
    unsigned char unmodeled_0f;
} MoviePlayerRecord;

/* Functions of src/main/tu017.c that other files call. */

void moviePlayerInit(void);
int moviePlayerStartFile(MoviePlayerRecord *player, const char *name);
int moviePlayerUpdate(MoviePlayerRecord *player);
int moviePlayerPause(MoviePlayerRecord *player, int on);
int moviePlayerDestroy(MoviePlayerRecord *player);
int movieRenderBufferSize(int include_extra);
int movieRenderBufferAlloc(MoviePlayerRecord *record, int cursor, int extra_enabled);

#endif /* INCLUDE_MAIN_TU017_H */
