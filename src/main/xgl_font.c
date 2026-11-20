#include "common.h"
#include "shared.h"
#include "main/xgl_font.h"
#include "main/xgl_cd.h"
#include "gs.h"
#include "main/xgl_2.h"
#include "main/xgl_camera.h"
#include "main/xgl_packet.h"
#include "main/xgl_render.h"


#define FONT_STATE_FLAGS (FS.state.flags)

/* EE scratchpad RAM: the font drawing cursor sits at its base, and xglFontFlushSubCRLF
 * reads the line origin there. */
#define SCRATCHPAD_BASE 0x70000000

/*
 * Control codes embedded in queued text. Bytes below 32 start a command whose length
 * xglFontGetSPcodeSize returns; the compact position codes 1..5 are the high byte of an
 * X (1..3) or Y (4..5) coordinate.
 */
typedef enum FontOpcode {
    FONT_OP_POS_X_0 = 1,
    FONT_OP_POS_X_1 = 2,
    FONT_OP_POS_X_2 = 3,
    FONT_OP_POS_Y_0 = 4,
    FONT_OP_POS_Y_1 = 5,
    FONT_OP_DEPTH = 6,
    FONT_OP_NOP = 7,
    FONT_OP_POSITION = 8,
    FONT_OP_TAB = 9,
    FONT_OP_NEWLINE = 10,
    FONT_OP_RESET = 11,
    FONT_OP_COLOR = 12,
    FONT_OP_EFFECT_MODE = 13,
    FONT_OP_EFFECT_PARAMS = 14,
    FONT_OP_BLEND = 15,
    FONT_OP_HEX_DIGITS = 16,
    FONT_OP_NUMBER = 17,
    FONT_OP_SCALE = 18,
    FONT_OP_EFFECT_PACKET = 19,
    FONT_OP_CONFIG = 21,
    FONT_OP_INCLUDE = 22,
    FONT_OP_CALLBACK = 23,
    FONT_OP_COMPACT = 24,
    FONT_OP_STYLE = 25,
    FONT_OP_GLYPH = 27
} FontOpcode;

/* FS.state.flags. */
#define FONT_STATE_ENABLED 1
#define FONT_STATE_DEBUG_PRINT 2
#define FONT_STATE_CLIP 8 /* skip glyphs that fall outside the GS drawing area */
/* Size of the print queue buckets and of the draw cursor's scratchpad packet. */
#define FONT_QUEUE_BUCKETS 16
/* Coordinates above these use the extended position command. */
#define FONT_COMPACT_X_LIMIT 768
#define FONT_COMPACT_Y_LIMIT 512
/* FontNumericCommand.radix value that selects a float. */
#define FONT_NUMERIC_FLOAT 0xff
/* FontState.text_mode bits, set by the $prop and $han inline commands. */
#define FONT_MODE_PROP 1
#define FONT_MODE_HAN 2
/* Glyph indices below this have proportional bounds; others advance a fixed FONT_GLYPH_ADVANCE. */
#define FONT_PROPORTIONAL_GLYPHS 0x180
#define FONT_GLYPH_ADVANCE 19
/* FontPositionCommand.flags: which coordinates follow, and whether the line origin stays. */
#define FONT_POS_SET_X 0x01
#define FONT_POS_SET_Y 0x02
#define FONT_POS_SET_DEPTH 0x04
#define FONT_POS_ADD_X 0x08
#define FONT_POS_ADD_Y 0x10
#define FONT_POS_KEEP_ORIGIN 0x80
#define FONT_TAB_WIDTH 64
/* EUC-JP code points and rows. */
#define FONT_JIS_ROW_SIZE 94
#define EUC_FIRST 0xa1
#define EUC_ROW_SYMBOLS 0xa1
#define EUC_ROW_ALNUM 0xa3
#define EUC_ROW_HIRAGANA 0xa4
#define EUC_ROW_KATAKANA 0xa5
#define EUC_ROW_GREEK 0xa6
#define EUC_ROW_NEC_SPECIAL 0xad
#define EUC_ROW_SYMBOLS_2 0xa2
#define EUC_ROW_KANJI_LEVEL1 0xb0
#define EUC_ROW_KANJI_LEVEL2 0xd0
#define EUC_SPACE 0xa1a1

struct FontImage {
    unsigned char kanji_texture_0[0x3c020];
    unsigned char kanji_texture_1[0x3c020];
    unsigned short proportional_widths[256];
};

struct FontState {
    struct FontImage *font_image;
    void *texture_buffer;
    unsigned short flags;
    unsigned char font_selection;
    unsigned char debug_mode;
    unsigned short line_height;
    unsigned short wrap_width;
    unsigned char default_width;
    unsigned char text_mode;
    unsigned char proportional_mode;
    unsigned char horizontal_shift;
    int debug_prefix;
    /* XS1 keeps the queue write cursor at +0x1c; XS2 moved it to the end of
     * the block (FontStateFromWindow.queue_cursor, FS + 0x128). */
    unsigned char unmodeled_18[8];
};

/* The partial state view shares the complete 0x20-byte retail allocation. */
typedef union FontStateStorage {
    struct FontState state;
    unsigned char bytes[0x20];
} FontStateStorage;

/*
 * Each 4-byte slot of the font print-queue buffer at fontPrintQueueBuffer (16 slots,
 * scaffold-owned splat name): link is the byte offset, from this array's
 * base, of the next queued record for this bucket (0 marks the bucket
 * empty -- xglFontFlush follows it as a chain), and selfOffset is the
 * slot's own byte offset within the array.
 */
typedef struct FontQueueSlot {
    unsigned short link;
    unsigned short selfOffset;
} FontQueueSlot;

typedef union FontPrintQueueStorage {
    unsigned long long alignment;
    FontQueueSlot slots[FONT_QUEUE_BUCKETS];
    unsigned char bytes[0x2458];
} FontPrintQueueStorage;

typedef union FontImageStorage {
    struct FontImage image;
    unsigned char bytes[0x78800];
} FontImageStorage;

typedef struct FontState FontState;

struct FontPositionCommand {
    unsigned char opcode;
    unsigned char flags;
    unsigned char x_high;
    unsigned char x_low;
    unsigned char y_high;
    unsigned char y_low;
    unsigned char ot_high;
    unsigned char ot_middle;
    unsigned char ot_low;
};

struct FontCompactPositionCommand {
    unsigned char x_high;
    unsigned char x_low;
    unsigned char y_high;
    unsigned char y_low;
    unsigned char opcode;
    unsigned char ot_high;
    unsigned char ot_middle;
    unsigned char ot_low;
};

struct FontNumericCommand {
    unsigned char opcode;
    unsigned char radix;
    unsigned char digits_minus_one;
    unsigned char value_high;
    unsigned char value_mid_high;
    unsigned char value_mid_low;
    unsigned char value_low;
};

struct KanjiClutEnvironment {
    unsigned int words[44];
};

extern unsigned char spCodeParameterSizeTable[];
void set_xyz(int x, int y, int ot);
void xglFontPrintSub(void *args);
void xglFontPrintDirectCore(const char *text);
void set_ot(int ot);
extern float dptofp(unsigned long long value);
extern unsigned short ascii2euc[96];
extern struct KanjiClutEnvironment KanjiClutEnv;
extern unsigned char spcode_0[32];
extern signed char tbl_7[];
extern FontStateStorage FS;
/* xglFontLoad reads the font texture files into this buffer (through fontStateFromWindow.texture_buffer). */
int xglFontCheckProportional(unsigned char row, int column);
unsigned int hex2val(signed char digit);
int xglFontGetStringWidth2(const char *text, int delimiter);
/* Defined here, not with the data above: moving it before the functions that use it changes the original code or data order. */
extern unsigned char spCodeParameterSizeTable[8];

/*
 * The FontState fields from offset 4 on, read through the scaffold symbol fontStateFromWindow (FS + 4),
 * up to the queue write cursor that sits just below the print-queue slot array. The queue
 * functions write their command bytes through queue_cursor; buffer_reset points it
 * at the end of the slot array, where the first record goes.
 */
struct FontStateFromWindow {
    void *texture_buffer;
    unsigned short flags;
    unsigned char font_selection;
    unsigned char debug_mode;
    unsigned short line_height;
    unsigned short wrap_width;
    unsigned char default_width;
    unsigned char text_mode;
    unsigned char proportional_mode;
    unsigned char horizontal_shift;
    int debug_prefix;
    /* "$rubyb;" marks where the base text of a ruby annotation starts. */
    const unsigned char *ruby_base;
    /* From the end of the annotation back to the end of its base text; set by "$rubys;". */
    unsigned short ruby_advance;
    /* How far the annotation is raised above the line, besides half its glyph height ("$rubyy"). */
    unsigned short ruby_offset_y;
    /* The default glyph scale in 1/16 units (FONT_SCALE_ONE each); xglFontSetTextureFilter tests both bytes at once. */
    union {
        unsigned short default_scale;
        struct { signed char compact_width; signed char compact_height; };
    };
    unsigned char unmodeled_1e[6];
    /* Scratch copy of one word while xglFontFitWords measures a line. */
    char word_buffer[0x100];
    unsigned char *queue_cursor;
};

static struct FontStateFromWindow fontStateFromWindow;
static FontPrintQueueStorage fontPrintQueueBuffer;
static unsigned char fontTextureBuffer[0x78800];

/* The state block ends where the print-queue slot array begins. */
#define FONT_STATE_BLOCK() ((struct FontStateFromWindow *) &fontPrintQueueBuffer - 1)

struct FontExtCommand {
    unsigned char opcode;
    unsigned char function_high;
    unsigned char function_middle;
    unsigned char function_low;
    unsigned char argument_high;
    unsigned char argument_mid_high;
    unsigned char argument_mid_low;
    unsigned char argument_low;
};


/* xglFontPrintExtFunc's queue record: the callback command, then the byte
 * that ends the record. */
struct FontExtRecord {
    struct FontExtCommand command;
    unsigned char terminator;
};

/* xglFontDebugHex's record: FONT_OP_RESET, then FONT_OP_HEX_DIGITS as the
 * second opcode byte and the digit count and value. */
struct FontHexCommand {
    unsigned char opcode;
    unsigned char hex_opcode;
    unsigned char digits_minus_one;
    unsigned char value_high;
    unsigned char value_mid_high;
    unsigned char value_mid_low;
    unsigned char value_low;
    unsigned char terminator;
};

/* Queues the text at the given address to be printed in place (xglFontFlushCore recurses into it). */
struct FontIncludeCommand {
    unsigned char opcode;
    unsigned char text_high;
    unsigned char text_mid_high;
    unsigned char text_mid_low;
    unsigned char text_low;
};

/* The variadic arguments after `last`, as the 8-byte register save slots of the EE ABI. */
#define FONT_ARGUMENT_START(list, last) \
    ((list) = (unsigned long long *)((char *)__builtin_next_arg(last) - \
              (8 - __builtin_args_info(2)) * 8))

/* One A+D register write: the 64-bit value, then the register address (the
 * upper word of the address doubleword is never written). */
struct FontGsRegisterWrite {
    unsigned long long value;
    unsigned int register_address;
    unsigned int register_address_high;
};

/* The two debug-mode GS packets; xglFontDebugMode patches their FRAME value. */
struct FontModePacket {
    unsigned int vif_commands[4];
    unsigned long long gif_tag[2];
    struct FontGsRegisterWrite frame;
    struct FontGsRegisterWrite state_registers[5];
};

extern struct FontModePacket ModeEnv[2];
/* "font0.us" and "font1.us". */
extern const char fontTexture0Path[];
extern const char fontTexture1Path[];

void xglFontDebugMode(int mode)
{
    if (mode >= 0) {
        struct FontModePacket *packets = ModeEnv;
        int frame0;
        unsigned long long frame1;

        fontStateFromWindow.debug_mode = mode;
        frame1 = fontStateFromWindow.debug_mode | 0x50000ULL;
        frame0 = 0x50000;
        frame0 |= ((unsigned int)fontStateFromWindow.debug_mode & 0xff) + 0x23;

        packets[0].frame.value = frame0;
        packets[1].frame.value = frame1;
    }
}

/* Nonzero for the EUC-JP codes drawn from the font texture's own glyph rows: the whole hiragana,
 * Greek and NEC special rows, and a few symbols of rows 1 and 2. */
static int xglFontIsTextureGlyphCode(unsigned char high, unsigned char low)
{
    if (low == 0) {
        return 0;
    }
    if (high == EUC_ROW_HIRAGANA || high == EUC_ROW_GREEK || high == EUC_ROW_NEC_SPECIAL) {
        return 1;
    }
    if (high == EUC_ROW_SYMBOLS && (low == 0xfb || low == 0xbd || low == 0xda || low == 0xdb || low == 0xdf ||
                         low == 0xf9 || low == 0xfa)) {
        return 1;
    }
    if (high == EUC_ROW_SYMBOLS_2 && ((unsigned char) (low + 0x5e) < 3 || low == 0xf6)) {
        return 1;
    }
    return 0;
}


/*
 * Reserves count A+D writes in the cursor's packet and writes their GIF tag: NLOOP = count with
 * the EOP bit 0x8000, one register (NREG = 1 in the upper word) and the register list GIF_AD.
 * Returns the address 16 bytes in.
 */
void *xglFontAllocGifPacket(FontDrawCursor *cursor, int count)
{
    unsigned int *packet;

    packet = sceVif1PkReserve(cursor->packet, count * 4 + 4);
    packet[0] = count + 0x8000;
    packet[1] = 0x10000000;
    packet[2] = GIF_AD;
    return packet + 4;
}

int xglFontGetKanjiClutUV(int code, int *clut)
{
    unsigned short index = code;
    unsigned char column = index >> 8;
    unsigned char row = index;
    int first_range_or_texture_column;
    int second_range;

    first_range_or_texture_column = row < 0xad;
    second_range = row < EUC_ROW_KANJI_LEVEL1;
    index = (column + row * FONT_JIS_ROW_SIZE) - EUC_FIRST;
    if (first_range_or_texture_column) {
        index += 0xc4e2;
    } else if (second_range) {
        index += 0xc30c;
    } else {
        index += 0xc250;
    }
    column = index & 0x1f;
    row = index >> 5;
    first_range_or_texture_column = column;
    if (clut != 0) {
        *clut = ((row >> 5) << 12) + 0x1004;
    }
    return ((row & 0x1f) * 0x01800000) +
        (first_range_or_texture_column * 0x140);
}

/* FONT_OP_STYLE argument bits, and the cursor flags they set. */
#define FONT_STYLE_PROPORTIONAL 1
#define FONT_STYLE_WIDE_ASCII 2
#define FONT_CURSOR_COMPACT 1
#define FONT_CURSOR_PROPORTIONAL 2
#define FONT_CURSOR_WIDE_ASCII 4
/* $han "n2" mode: 8x8 glyphs keep their native size and are not doubled by xglFontFlushSub. */
#define FONT_CURSOR_NATIVE_8X8 8
/* Set between "$rubyb;" and "$rubye;": the base text of a ruby annotation was marked. */
#define FONT_CURSOR_RUBY 0x10
#define FONT_CURSOR_STYLE_CLEAR \
    (~(FONT_CURSOR_COMPACT | FONT_CURSOR_PROPORTIONAL | FONT_CURSOR_WIDE_ASCII) & 0xff)

/* One A+D register write after the GIF tag xglFontAllocGifPacket writes. */
struct FontAlphaWrite {
    unsigned int alpha_abcd;
    unsigned int alpha_fix;
    unsigned int register_address;
    unsigned int register_address_high;
};

/* GS TEST_1 values: depth test GREATER, plus alpha test GREATER than 0 for the drawn effects. */
#define FONT_TEST_DEPTH_GREATER 0x70000
#define FONT_TEST_ALPHA_GREATER 0xD

/* FONT_OP_EFFECT_MODE: how a glyph is drawn (cursor->effect). */
#define FONT_EFFECT_PLAIN 1
#define FONT_EFFECT_SHADOW 2
#define FONT_EFFECT_OUTLINE 3
#define FONT_EFFECT_BOLD 4
#define FONT_COLOR_NEUTRAL 0x80
#define FONT_ALPHA_NEUTRAL 0x80


#define FONT_SCALE_ONE 16
#define FONT_SCALE_PAIR_ONE ((FONT_SCALE_ONE << 16) | FONT_SCALE_ONE)
#define FONT_DEFAULT_SCALE_ONE ((FONT_SCALE_ONE << 8) | FONT_SCALE_ONE)
/* The two texture-filter packets: 1:1 glyphs and scaled glyphs. */
extern unsigned int nearest_5[8];
extern unsigned int biliner_6[8];

/* Selects the texture filter for the cursor's own scale (mode != 0) or the font state's default scale. */
void xglFontSetTextureFilter(FontDrawCursor *cursor, int mode)
{
    int scaled;

    if (mode == 0) {
        scaled = fontStateFromWindow.default_scale != FONT_DEFAULT_SCALE_ONE;
    } else {
        /* scale_x and scale_y compared as one word. A scale_pair union member
           would put both halfwords in the union's alias set and reorders
           their stores in zoom and xglFontFlushCore. */
        scaled = *(unsigned int *)&cursor->scale_x != FONT_SCALE_PAIR_ONE;
    }
    if (!scaled) {
        sceVif1PkAddDirectDataN(cursor->packet, nearest_5, 2);
    } else {
        sceVif1PkAddDirectDataN(cursor->packet, biliner_6, 2);
    }
}

void set_ot(int ot)
{
    struct FontStateFromWindow *state = FONT_STATE_BLOCK();
    FontQueueSlot *slot = &fontPrintQueueBuffer.slots[((ot >> 22) & 0x3c) / 4];
    unsigned char *queue_bytes = (unsigned char *)&fontPrintQueueBuffer;
    unsigned short offset = state->queue_cursor - queue_bytes;
    unsigned char *previous = queue_bytes + slot->selfOffset;

    previous[0] = offset;
    previous[1] = offset >> 8;
    slot->selfOffset = offset;
    state->queue_cursor[0] = 0;
    state->queue_cursor[1] = 0;
    state->queue_cursor += 2;
}

void set_xyz(int x, int y, int ot)
{
    set_ot(ot);
    if ((unsigned int)x >= FONT_COMPACT_X_LIMIT || y < 0 || y >= FONT_COMPACT_Y_LIMIT) {
        struct FontPositionCommand *command = (struct FontPositionCommand *)fontStateFromWindow.queue_cursor;

        command->opcode = FONT_OP_POSITION;
        command->flags = 7;
        command->x_high = x >> 8;
        command->x_low = x;
        command->y_high = y >> 8;
        command->y_low = y;
        command->ot_high = ot >> 16;
        command->ot_middle = ot >> 8;
        command->ot_low = ot;
        fontStateFromWindow.queue_cursor += sizeof(*command);
    } else {
        struct FontCompactPositionCommand *command = (struct FontCompactPositionCommand *)fontStateFromWindow.queue_cursor;

        command->x_high = (x >> 8) + FONT_OP_POS_X_0;
        command->x_low = x;
        command->y_high = (y >> 8) + FONT_OP_POS_Y_0;
        command->y_low = y;
        command->opcode = FONT_OP_DEPTH;
        command->ot_high = ot >> 16;
        command->ot_middle = ot >> 8;
        command->ot_low = ot;
        fontStateFromWindow.queue_cursor += sizeof(*command);
    }
}

int xglFontGetSPcodeSize(int code, const unsigned char *text)
{
    unsigned char size;

    code &= 0xff;
    size = spcode_0[code];

    if (size == 255) {
        switch (code) {
        case FONT_OP_POSITION: {
            unsigned char parameter_sizes[8];
            unsigned char flags = text[1];
            int bit;

            __builtin_memcpy(parameter_sizes, spCodeParameterSizeTable,
                             sizeof(parameter_sizes));
            size = 1;
            for (bit = 0; bit < 8; bit++) {
                if (flags & 1) {
                    size += parameter_sizes[bit];
                }
                flags >>= 1;
            }
            break;
        }
        case FONT_OP_CONFIG:
            size = text[1] + 1;
            break;
        }
    }

    return size;
}

/*
 * Expands a printf-style argument list (format string first) into the print queue: control codes are
 * copied with their parameters, "%b", "%d", "%x" and "%f" become FONT_OP_NUMBER commands, "%s" and "%c"
 * are copied in place, "%$" and "%%" queue the character itself. X1's function plus the "%$" case; the
 * shared numeric tail keeps X1's goto: the original sets only the radix
 * in each of the three cases (li in the branch delay slots) and runs one shared body. Three inlined copies, a
 * helper call or a radix expression in one case all change the size or the switch layout.
 */
void xglFontPrintSub(void *args)
{
    unsigned long long *list = (unsigned long long *)args;
    const char *text = (const char *)(int)list[0];
    unsigned char ahead = *text;

    list++;
    for (;;) {
        unsigned int code;

        if (*text == 0) {
            break;
        }
        code = *text & 0xff;

        if (code < 32) {
            int remaining = xglFontGetSPcodeSize(code, (const unsigned char *)text);

            for (; remaining >= 0; remaining--) {
                *fontStateFromWindow.queue_cursor++ = *text++;
            }
        } else if (code == '%') {
            int width = 0;
            unsigned int radix;

            text++;
            for (;;) {
                code = (unsigned char)*text++;

                switch (code) {
                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':
                    width = width * 10 + (code - '0');
                    continue;
                case 'b':
                    radix = 2;
                    goto numeric_tail;
                case 'd':
                    radix = 10;
                    goto numeric_tail;
                case 'x':
                    radix = 16;
                numeric_tail:
                    {
                        unsigned long long value = (unsigned int)list[0];
                        struct FontNumericCommand *command = (struct FontNumericCommand *)fontStateFromWindow.queue_cursor;

                        list++;
                        if (width == 0) {
                            width = 1;
                        }
                        command->opcode = FONT_OP_NUMBER;
                        command->radix = radix;
                        command->digits_minus_one = (unsigned char)(width - 1);
                        command->value_high = value >> 24;
                        command->value_mid_high = value >> 16;
                        command->value_mid_low = value >> 8;
                        command->value_low = value;
                        fontStateFromWindow.queue_cursor += sizeof(*command);
                    }
                    break;
                case 'f':
                    {
                        float value = dptofp(list[0]);
                        /* The float's bits through a cast: a local union puts the
                           value through the stack and costs two instructions. */
                        unsigned int bits = *(unsigned int *)&value;
                        struct FontNumericCommand *command = (struct FontNumericCommand *)fontStateFromWindow.queue_cursor;

                        list++;
                        command->opcode = FONT_OP_NUMBER;
                        command->radix = FONT_NUMERIC_FLOAT;
                        command->digits_minus_one = 0xff;
                        command->value_high = bits >> 24;
                        command->value_mid_high = bits >> 16;
                        command->value_mid_low = bits >> 8;
                        command->value_low = bits;
                        fontStateFromWindow.queue_cursor += sizeof(*command);
                    }
                    break;
                case 's': {
                    const unsigned char *string = (const unsigned char *)(int)list[0];
                    /* The loop tests one load of the byte and copies a second one:
                       `while (*string != 0)` shares the load (two instructions fewer). */
                    int terminator_test;

                    list++;
                    while ((terminator_test = *string) != 0) {
                        *fontStateFromWindow.queue_cursor++ = *string++;
                    }
                    break;
                }
                case 'c':
                    code = (unsigned char)list[0];
                    list++;
                    /* fall through */
                case '$':
                case '%':
                    *fontStateFromWindow.queue_cursor++ = code;
                    break;
                default:
                    break;
                }
                break;
            }
        } else {
            text++;
            *fontStateFromWindow.queue_cursor++ = ahead;
        }
        ahead = *text;
    }
    *fontStateFromWindow.queue_cursor++ = 0;
}

void xglFontPrintDirectCore(const char *text)
{
    while (*text != 0) {
        unsigned char code = *text;

        if (code < 32) {
            int remaining = xglFontGetSPcodeSize(code, (const unsigned char *)text);

            while (remaining > 0) {
                *fontStateFromWindow.queue_cursor++ = *text++;
                remaining--;
            }
        }
        *fontStateFromWindow.queue_cursor++ = *text++;
    }
    *fontStateFromWindow.queue_cursor++ = 0;
}

void xglFontPrintf(int x, int y, unsigned int ot, void *arg)
{
    if (FONT_STATE_FLAGS & FONT_STATE_ENABLED & 0xFFFF) {
        set_xyz(x, y, ot);
        xglFontPrintSub(arg);
    }
}

void xglFontPrint(int x, int y, unsigned int ot, const char *text)
{
    if (FONT_STATE_FLAGS & FONT_STATE_ENABLED & 0xFFFF) {
        set_xyz(x, y, ot);
        xglFontPrintDirectCore(text);
    }
}

void xglFontPrintDirect(const char *text)
{
    xglFontPrintDirectOT(0, text);
}

void xglFontPrintDirectOT(int ot, const char *text)
{
    if (FONT_STATE_FLAGS & FONT_STATE_ENABLED & 0xFFFF) {
        set_ot(ot);
        xglFontPrintDirectCore(text);
    }
}

void xglFontPrintExtFunc(int ot, FontExtFunc function, void *argument)
{
    struct FontStateFromWindow *state = &fontStateFromWindow;
    struct FontExtRecord *record;
    unsigned int function_address = (unsigned int)function;
    unsigned int argument_word = (unsigned int)argument;

    if (state->flags & FONT_STATE_ENABLED & 0xffff) {
        set_ot(ot);
        record = (struct FontExtRecord *)fontStateFromWindow.queue_cursor;
        record->command.opcode = FONT_OP_CALLBACK;
        record->command.function_high = function_address >> 16;
        record->command.function_middle = function_address >> 8;
        record->command.function_low = function_address;
        record->command.argument_high = argument_word >> 24;
        record->command.argument_mid_high = argument_word >> 16;
        record->command.argument_mid_low = argument_word >> 8;
        record->command.argument_low = argument_word;
        record->terminator = 0;
        fontStateFromWindow.queue_cursor += sizeof(*record);
    }
}

/* Stores the debug prefix string address in the font state's debug_prefix field. */
void xglFontDebugPrefixSet(int debug_prefix) {
    FS.state.debug_prefix = debug_prefix;
}

void xglFontDebugPrintf(int x, int y, const char *format, ...)
{
    unsigned long long arguments[32];
    unsigned long long *output;
    unsigned long long *list;

    if ((fontStateFromWindow.flags & (FONT_STATE_ENABLED | FONT_STATE_DEBUG_PRINT))
        == (FONT_STATE_ENABLED | FONT_STATE_DEBUG_PRINT)) {
        arguments[0] = (unsigned int)format;
        {
        int count;

        FONT_ARGUMENT_START(list, format);
        output = arguments + 1;
        count = 30;
        do {
            unsigned long long *argument = list++;
            unsigned long long value = *argument;

            count--;
            *output = value;
            output++;
        } while (count >= 0);
        }
        if (fontStateFromWindow.debug_mode == 0) {
            x <<= 1;
            y <<= 1;
        }
        set_xyz(x, y, -1);
        /* XS2 addition: a text set with xglFontDebugPrefixSet is queued as an
           include in place of the reset, so it prefixes every debug line. */
        if (fontStateFromWindow.debug_prefix != 0) {
            struct FontIncludeCommand *command = (struct FontIncludeCommand *)fontStateFromWindow.queue_cursor;
            unsigned int text = fontStateFromWindow.debug_prefix;

            command->opcode = FONT_OP_INCLUDE;
            command->text_high = text >> 24;
            command->text_mid_high = text >> 16;
            command->text_mid_low = text >> 8;
            command->text_low = text;
            fontStateFromWindow.queue_cursor += sizeof(*command);
        } else {
            *fontStateFromWindow.queue_cursor++ = FONT_OP_RESET;
        }
        xglFontPrintSub(arguments);
    }
}

void xglFontDebugHex(int x, int y, unsigned int value, int digits)
{
    struct FontStateFromWindow *state = &fontStateFromWindow;
    struct FontHexCommand *command;

    if ((state->flags & (FONT_STATE_ENABLED | FONT_STATE_DEBUG_PRINT))
            == (FONT_STATE_ENABLED | FONT_STATE_DEBUG_PRINT)
        && digits != 0) {
        if (state->debug_mode == 0) {
            x <<= 1;
            y <<= 1;
        }
        set_xyz(x, y, -1);
        command = (struct FontHexCommand *)fontStateFromWindow.queue_cursor;
        command->opcode = FONT_OP_RESET;
        command->hex_opcode = FONT_OP_HEX_DIGITS;
        command->digits_minus_one = digits - 1;
        command->value_high = value >> 24;
        command->value_mid_high = value >> 16;
        command->value_mid_low = value >> 8;
        command->value_low = value;
        command->terminator = 0;
        fontStateFromWindow.queue_cursor += sizeof(*command);
    }
}


/* Advance of each printable ASCII character (32..127) in the debug font. */
extern int fontAsciiWidthTable[];

int xglFontParseNumber(const char **cursor, int *value);
unsigned short xglFontAscii2Euc(signed char character, unsigned char **text);
unsigned int xglFontGetDebugCell(unsigned char high, unsigned char low);
unsigned short xglFontGetGlyphIndex(unsigned char high, unsigned char low);

int xglFontGetStringWidth2(const char *text, int delimiter)
{
    int width = 0;
    int compact = 0;
    int first = 1;
    unsigned char high;

    while (*text != 0 && (high = *text, !(!first && delimiter != 0 && high == ' '))) {
        unsigned char low;

        if (high == '$') {
            if (text[1] == 'p' && text[2] == 'r' && text[3] == 'o' && text[4] == 'p') {
                fontStateFromWindow.text_mode &= ~FONT_MODE_PROP;
                fontStateFromWindow.text_mode |= text[5] & FONT_MODE_PROP;
            } else if (text[1] == 'h' && text[2] == 'a' && text[3] == 'n') {
                fontStateFromWindow.text_mode &= ~FONT_MODE_HAN;
                fontStateFromWindow.text_mode |= (text[4] & 1) << 1;
            } else if (text[1] == 'x' && text[2] == 'x') {
                if (text[3] == '1' && text[4] == ',') {
                    const char *number = text + 5;
                    int value;

                    xglFontParseNumber(&number, &value);
                    fontStateFromWindow.default_width = value;
                }
                if (text[3] == '3' && text[4] == ',') {
                    const char *number = text + 5;
                    int value;

                    xglFontParseNumber(&number, &value);
                    fontStateFromWindow.proportional_mode = value;
                }
            } else if (text[1] == 'r' && text[2] == 'u' && text[3] == 'b' && text[4] == 'y' &&
                       text[5] == 's' && text[6] == ';') {
                const unsigned char *scan = (const unsigned char *) text + 7;

                while (*scan != 0) {
                    if (scan[0] == '$' && scan[1] == 'r' && scan[2] == 'u' && scan[3] == 'b' &&
                        scan[4] == 'y' && scan[5] == 'e' && scan[6] == ';') {
                        text = (const char *) scan;
                        break;
                    }
                    if (*scan < 0x20) {
                        scan += xglFontGetSPcodeSize(*scan, scan + 1);
                    }
                    scan++;
                }
            }
            do {
                high = *text++;
            } while (high != ';');
        } else if (high < 0x20) {
            switch (high) {
            case FONT_OP_CONFIG:
                switch (text[2]) {
                case 2:
                    fontStateFromWindow.default_width = text[3];
                    break;
                case 4:
                    fontStateFromWindow.proportional_mode = text[3];
                    break;
                }
                break;
            case FONT_OP_COMPACT:
                compact = text[1];
                break;
            case FONT_OP_STYLE:
                fontStateFromWindow.text_mode = text[1];
                break;
            }
            text += xglFontGetSPcodeSize(high, (const unsigned char *) text) + 1;
        } else {
            if (high < 0xa0) {
                if ((signed char) high >= 0) {
                    if (!(fontStateFromWindow.text_mode & FONT_MODE_HAN)) {
                        width += fontAsciiWidthTable[high - 32];
                        text++;
                        first = 0;
                        continue;
                    }
                }
            }
            if ((signed char) high >= 0) {
                unsigned short code = xglFontAscii2Euc(high, (unsigned char **) &text);
                low = code;
                high = code >> 8;
                text++;
            } else {
                low = text[1];
                if (xglFontIsTextureGlyphCode(high, low)) {
                    text += 2;
                } else {
                    text++;
                    low = high + 0x80;
                    high = 0xa9;
                }
            }
            if (compact) {
                if (xglFontGetDebugCell(high, low) & 0x20000000) {
                    width += (fontStateFromWindow.compact_width * 5) >> 4;
                }
                width += (fontStateFromWindow.compact_width * 8) >> 4;
            } else {
                int index = xglFontGetGlyphIndex(high, low);
                if ((fontStateFromWindow.text_mode & FONT_MODE_PROP) && index < FONT_PROPORTIONAL_GLYPHS) {
                    unsigned int bounds = xglFontGetProportionalSize(index);
                    width += ((bounds >> 8) & 0xff) - (bounds & 0xff) + 1;
                } else {
                    width += FONT_GLYPH_ADVANCE;
                }
            }
        }
        first = 0;
    }
    return width;
}

int xglFontGetStringWidth(const char *text)
{
    return xglFontGetStringWidth2(text, 0);
}

/*
 * Copies the next unit of `text` into `word` (the font state's word buffer when null): any control
 * codes with their parameters and "$...;" commands (a "$rubys;" block up to its "$rubye"), then one
 * character of one or two bytes. Stores the bytes taken in *length (if given) and returns the text
 * after them.
 */
const char *xglFontCopyTextUnit(char *word, const char *text, int *length)
{
    const unsigned char *cursor = (const unsigned char *)text;
    unsigned char high;

    if (word == 0) {
        word = fontStateFromWindow.word_buffer;
    }
    while (*cursor != 0) {
        if (*cursor == '$') {
            if (cursor[1] == 'r' && cursor[2] == 'u' && cursor[3] == 'b' && cursor[4] == 'y' &&
                cursor[5] == 's' && cursor[6] == ';') {
                const unsigned char *scan = cursor + 7;

                while (*scan != 0) {
                    if (scan[0] == '$' && scan[1] == 'r' && scan[2] == 'u' && scan[3] == 'b' &&
                        scan[4] == 'y' && scan[5] == 'e' && scan[6] == ';') {
                        cursor = scan;
                        break;
                    }
                    if (*scan < 0x20) {
                        scan += xglFontGetSPcodeSize(*scan, scan + 1);
                    }
                    scan++;
                }
            }
            while (*cursor != ';') {
                *word++ = *cursor++;
            }
        } else if (*cursor < 0x20) {
            int remaining = xglFontGetSPcodeSize(*cursor, cursor);

            while (remaining >= 0) {
                *word++ = *cursor++;
                remaining--;
            }
        } else {
            int wide;

            high = *cursor;
            wide = xglFontIsTextureGlyphCode(high, cursor[1]);
            *word = high;
            if (wide) {
                word[1] = cursor[1];
                cursor += 2;
            } else {
                cursor++;
            }
            break;
        }
    }
    if (length != 0) {
        *length = (const char *)cursor - text;
    }
    return (const char *)cursor;
}

/*
 * Measures `text` word by word until the next word would pass *width; stores the width reached
 * and returns the number of bytes that fit.
 */
int xglFontFitWords(const char *text, int *width)
{
    const char *cursor = text;
    int total = 0;

    while (*cursor != 0) {
        int length;
        const char *next = xglFontCopyTextUnit(fontStateFromWindow.word_buffer, cursor, &length);

        fontStateFromWindow.word_buffer[length] = 0;
        total += xglFontGetStringWidth2(fontStateFromWindow.word_buffer, 0);
        if (*width < total) {
            break;
        }
        cursor = next;
    }
    *width = total;
    return cursor - text;
}

/*
 * Reads a hex number up to ';' or ',' and moves the text cursor past the delimiter; returns 0 on any
 * other character. Upper-case digits come out wrong in the original: they take 'a' - 'A' off
 * instead of adding it before the lower-case step.
 */
int xglFontParseHex(const char **cursor, int *value)
{
    const unsigned char *text = (const unsigned char *)*cursor;
    unsigned int result = 0;
    unsigned char ch = *text++;

    while (ch != ';' && ch != ',') {
        result = (result << 4) + ch;
        switch (ch) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            result -= '0';
            break;
        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
            result -= 'a' - 'A';
            /* fall through */
        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
            result -= 'a' - 10;
            break;
        default:
            return 0;
        }
        ch = *text++;
    }
    *value = result;
    *cursor = (const char *)text;
    return 1;
}

/*
 * Reads a number up to ';' or ',' and moves the text cursor past the delimiter: an optional sign, then
 * decimal digits or "0x" and a hex number (xglFontParseHex). Returns 0 on any other character.
 */
int xglFontParseNumber(const char **cursor, int *value)
{
    const unsigned char *text = (const unsigned char *)*cursor;
    int sign = 1;
    int result = 0;
    unsigned char ch;

    switch (text[0]) {
    case '-':
        sign = -1;
        /* fall through */
    case '+':
        text++;
        break;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        break;
    default:
        return 0;
    }
    if (text[0] == '0' && text[1] == 'x') {
        *cursor = (const char *)(text + 2);
        if (!xglFontParseHex(cursor, &result)) {
            return 0;
        }
        *value = sign * result;
        return 1;
    }
    ch = *text++;
    while (ch != ';' && ch != ',') {
        if ((unsigned char)(ch - '0') >= 10) {
            return 0;
        }
        result = result * 10 + ch - '0';
        ch = *text++;
    }
    *cursor = (const char *)text;
    *value = sign * result;
    return 1;
}

/* Inline position command: the letter before the number picks x (also the line origin), y or depth. */
int xglFontCmdAx(FontDrawCursor *cursor, const char **text)
{
    const char *next = *text;
    unsigned char axis = next[-1];
    int value;

    if (!xglFontParseNumber(&next, &value)) {
        return 0;
    }
    switch (axis) {
    case 'x':
        cursor->x = value;
        cursor->origin_x = value;
        break;
    case 'y':
        cursor->y = value;
        break;
    case 'z':
        cursor->depth = value;
        break;
    default:
        return 0;
    }
    *text = next;
    return 1;
}

/* Inline move command: adds the number to x or y, picked by the letter before it. */
int xglFontCmdRx(FontDrawCursor *cursor, const char **text)
{
    const char *next = *text;
    unsigned char axis = next[-1];
    int value;

    if (!xglFontParseNumber(&next, &value)) {
        return 0;
    }
    switch (axis) {
    case 'x':
        cursor->x += value;
        break;
    case 'y':
        cursor->y += value;
        break;
    default:
        return 0;
    }
    *text = next;
    return 1;
}

/* GS state packet of the inline reset (4 quadwords). */
extern unsigned int normal_2[16];

/*
 * Inline "init" / "save" / "load" command: resets the draw cursor (colour, effect, blend, scale,
 * style) and the font state defaults of xglFontInitial, then skips the ';'.
 */
int xglFontCmdInit(FontDrawCursor *cursor, const char **text)
{
    cursor->red = cursor->green = cursor->blue = FONT_COLOR_NEUTRAL;
    cursor->effect = FONT_EFFECT_PLAIN;
    cursor->flags &= FONT_CURSOR_STYLE_CLEAR;
    cursor->effect_x = 1;
    cursor->alpha = FONT_ALPHA_NEUTRAL;
    cursor->effect_y = 1;
    cursor->effect_red = 0;
    cursor->effect_green = 0;
    cursor->effect_blue = 0;
    cursor->alpha_abcd = 0;
    cursor->alpha_fix = 0;
    cursor->scale_x = FONT_SCALE_ONE;
    cursor->scale_y = FONT_SCALE_ONE;
    if (!fontStateFromWindow.debug_mode) {
        sceVif1PkAddDirectDataN(cursor->packet, normal_2, 4);
    }
    fontStateFromWindow.line_height = 24;
    fontStateFromWindow.default_width = 4;
    fontStateFromWindow.proportional_mode = 0;
    fontStateFromWindow.horizontal_shift = 0;
    fontStateFromWindow.wrap_width = 0;
    fontStateFromWindow.compact_width = 16;
    fontStateFromWindow.compact_height = 16;
    fontStateFromWindow.ruby_offset_y = 2;
    (*text)++;
    return 1;
}

/* Inline colour command "col": reads a 0xBBGGRR value after the text cursor into the draw colour. */
int xglFontCmdCol(FontDrawCursor *cursor, const char **text)
{
    const char *next = *text;
    const char *end;
    int value;

    if (!xglFontParseHex(&next, &value)) {
        return 0;
    }
    end = next;
    cursor->blue = (unsigned int)value >> 16;
    cursor->green = (unsigned int)value >> 8;
    cursor->red = value;
    *text = end;
    return 1;
}

/* Inline effect command: "N;" with N = 0..3 selects the glyph effect and its TEST_1 setting. */
int xglFontCmdBg(FontDrawCursor *cursor, const char **text)
{
    const unsigned char *next = (const unsigned char *)*text;
    struct FontGsRegisterWrite *test;
    int digit;

    if (next[1] == ';') {
        digit = next[0];
        if (digit < '4') {
            if (digit >= '0') {
                *text = (const char *)(next + 2);
                cursor->effect = next[0] - '0';
                test = xglFontAllocGifPacket(cursor, 1);
                test->register_address = GS_TEST_1;
                switch (cursor->effect) {
                case 0:
                case 2:
                case 3:
                case 4:
                    test->value = FONT_TEST_DEPTH_GREATER | FONT_TEST_ALPHA_GREATER;
                    break;
                case 1:
                default:
                    test->value = FONT_TEST_DEPTH_GREATER;
                    break;
                }
                return 1;
            }
        }
    }
    return 0;
}

/* Inline shadow command: "x,y,0xBBGGRR;" sets the effect offset and colour. Same local record as zoom. */
int xglFontCmdShadow(FontDrawCursor *cursor, const char **text)
{
    struct {
        const char *next;
        int x;
        int y;
        int color;
    } work;

    work.next = *text;
    if (!xglFontParseNumber(&work.next, &work.x)) {
        return 0;
    }
    if (!xglFontParseNumber(&work.next, &work.y)) {
        return 0;
    }
    if (!xglFontParseHex(&work.next, &work.color)) {
        return 0;
    }
    cursor->effect_x = work.x;
    cursor->effect_y = work.y;
    cursor->effect_red = work.color;
    cursor->effect_green = (unsigned int)work.color >> 8;
    cursor->effect_blue = (unsigned int)work.color >> 16;
    *text = work.next;
    return 1;
}

/*
 * Inline blend command: six numbers A, B, C, D (2 bits each of ALPHA_1), the mode byte and the
 * alpha value; writes ALPHA_1 at once.
 */
int xglFontCmdAlpha(FontDrawCursor *cursor, const char **text)
{
    int values[6];
    const char *next = *text;
    int index;
    struct FontAlphaWrite *packet;

    for (index = 0; index < 6; index++) {
        if (!xglFontParseNumber(&next, &values[index])) {
            return 0;
        }
    }
    *text = next;
    cursor->alpha_abcd = values[0] + (values[1] << 2) + (values[2] << 4) + (values[3] << 6);
    cursor->alpha_fix = values[4];
    cursor->alpha = values[5];
    packet = xglFontAllocGifPacket(cursor, 1);
    packet->alpha_abcd = cursor->alpha_abcd;
    packet->alpha_fix = cursor->alpha_fix;
    packet->register_address = GS_ALPHA_1;
    return 1;
}

/*
 * Inline zoom command: "x;" or "x,y;" sets the glyph scale (1/16 units) and picks the texture filter.
 * The parse cursor and the parsed values are one local record: the parsers take their addresses, and
 * only as members of one object do the reloads after the calls match the original.
 */
int xglFontCmdZoom(FontDrawCursor *cursor, const char **text)
{
    struct {
        const char *next;
        int scale_x;
        int scale_y;
    } work;

    work.next = *text;
    if (!xglFontParseNumber(&work.next, &work.scale_x)) {
        return 0;
    }
    if (((const unsigned char *)work.next)[-1] == ';') {
        work.scale_y = work.scale_x;
    } else if (!xglFontParseNumber(&work.next, &work.scale_y)) {
        return 0;
    }
    *text = work.next;
    cursor->scale_x = work.scale_x;
    cursor->scale_y = work.scale_y;
    xglFontSetTextureFilter(cursor, 1);
    return 1;
}


/*
 * Inline "xx" command: "N,value;" sets a font state default (0 line height, 1 default width,
 * 2 character spacing, 3 proportional mode, 4 horizontal shift); "5,u;" or "5,u,v;" writes
 * CLAMP_1 with a repeat mask per axis (bit 7: masked with a random value), like FONT_OP_EFFECT_PACKET.
 */
int xglFontCmdXx(FontDrawCursor *cursor, const char **text)
{
    const char *next = *text;
    int value;

    if (!xglFontParseNumber(&next, &value)) {
        return 0;
    }
    switch (value) {
    case 0:
        if (!xglFontParseNumber(&next, &value)) {
            return 0;
        }
        fontStateFromWindow.line_height = value;
        break;
    case 1:
        if (!xglFontParseNumber(&next, &value)) {
            return 0;
        }
        fontStateFromWindow.default_width = value;
        break;
    case 2:
        if (!xglFontParseNumber(&next, &value)) {
            return 0;
        }
        fontStateFromWindow.wrap_width = value;
        break;
    case 3:
        if (!xglFontParseNumber(&next, &value)) {
            return 0;
        }
        fontStateFromWindow.proportional_mode = value;
        break;
    case 4:
        if (!xglFontParseNumber(&next, &value)) {
            return 0;
        }
        fontStateFromWindow.horizontal_shift = value;
        break;
    case 5: {
        int horizontal_repeat;
        int vertical_repeat;
        struct FontGsRegisterWrite *clamp;
        long long horizontal_mode;
        unsigned short horizontal_mask;
        unsigned short horizontal_random;
        long long vertical_mode;
        unsigned short vertical_mask;
        unsigned short vertical_random;

        if (!xglFontParseNumber(&next, &horizontal_repeat)) {
            return 0;
        }
        if ((unsigned char)next[-1] == ';') {
            vertical_repeat = horizontal_repeat;
        } else if (!xglFontParseNumber(&next, &vertical_repeat)) {
            return 0;
        }
        clamp = xglFontAllocGifPacket(cursor, 1);
        clamp->register_address = GS_CLAMP_1;
        horizontal_mode = 1;
        horizontal_random = 0;
        horizontal_mask = 0;
        if (horizontal_repeat != 0) {
            horizontal_mode = 3;
            horizontal_mask = horizontal_repeat ^ 0x3FF;
            if (horizontal_repeat & 0x80) {
                horizontal_random = horizontal_repeat & xglSRand();
            }
        }
        vertical_mode = 1;
        vertical_random = 0;
        vertical_mask = 0;
        if (vertical_repeat != 0) {
            vertical_mode = 3;
            vertical_mask = vertical_repeat ^ 0x3FF;
            if (vertical_repeat & 0x80) {
                vertical_random = vertical_repeat & xglSRand();
            }
        }
        clamp->value = horizontal_mode | (vertical_mode << 2) | ((long long)horizontal_mask << 4) |
            ((long long)horizontal_random << 14) | ((long long)vertical_mask << 24) |
            ((long long)vertical_random << 34);
        break;
    }
    default:
        return 0;
    }
    *text = next;
    return 1;
}

/*
 * Inline xglFontCmdRuby (furigana) commands; the letter after "ruby" stands two bytes in front of the cursor.
 * "$rubyb;" marks the start of the base text, "$rubys;" ends it and starts the annotation, which is
 * drawn in the compact size centred above the base text, "$rubye;" ends the annotation and returns to
 * the end of the base text. "$rubyzW,H;" sets the compact glyph size, "$rubyyN;" the raise.
 */
int xglFontCmdRuby(FontDrawCursor *cursor, const char **text)
{
    const unsigned char *next;
    /* The cursor's address is taken before its first assignment and used for the first parser call:
     * every access to `next` is then a memory access of one kind, and the stores and the reloads
     * after the calls share a register as in the original. The cursor is read as bytes like in bg and
     * han; xglFontParseNumber takes the const char ** of the inline command interface, hence the casts
     * at its three calls. */
    const char **parse = (const char **)&next;
    unsigned char flags;
    int width;
    int height;
    int offset;

    next = (const unsigned char *)*text;
    flags = cursor->flags;
    next += 2;
    switch (next[-2]) {
    case 's':
        if (next[-1] != ';') {
            return 0;
        }
        if (flags & FONT_CURSOR_RUBY) {
            const unsigned char *base;
            unsigned char *word;
            const unsigned char *source;
            unsigned char *out;
            unsigned char c;
            int rise;
            int mode;
            int base_width;
            int ruby_width;

            rise = fontStateFromWindow.ruby_offset_y + ((fontStateFromWindow.compact_height * 8) >> 4);
            cursor->y -= rise;
            mode = 0;
            if (cursor->flags & FONT_CURSOR_PROPORTIONAL) {
                mode = FONT_MODE_PROP;
            }
            if (cursor->flags & FONT_CURSOR_WIDE_ASCII) {
                mode |= FONT_MODE_HAN;
            }
            fontStateFromWindow.text_mode = mode;
            base = fontStateFromWindow.ruby_base;
            word = (unsigned char *)fontStateFromWindow.word_buffer;
            while (base < next - 7) {
                *word++ = *base++;
            }
            *word = 0;
            base_width = xglFontGetStringWidth2(fontStateFromWindow.word_buffer, 0);
            out = (unsigned char *)fontStateFromWindow.word_buffer;
            out[0] = FONT_OP_COMPACT;
            out[1] = 1;
            out += 2;
            source = next;
            while (*source != 0) {
                if (*source == '$') {
                    if (source[1] == 'r' && source[2] == 'u' && source[3] == 'b' && source[4] == 'y' &&
                        source[5] == 'e' && source[6] == ';') {
                        break;
                    }
                    while (*source != ';') {
                        *out++ = *source++;
                    }
                } else if (*source < 0x20) {
                    int remaining = xglFontGetSPcodeSize(*source, source);

                    for (; remaining >= 0; remaining--) {
                        *out++ = *source++;
                    }
                } else {
                    if (*source < 0xA0) {
                        *out++ = *source++;
                    } else {
                        out[0] = source[0];
                        out[1] = source[1];
                        out += 2;
                        source += 2;
                    }
                }
            }
            *out = 0;
            ruby_width = xglFontGetStringWidth2(fontStateFromWindow.word_buffer, 0);
            fontStateFromWindow.ruby_advance = base_width / 2 - ruby_width / 2;
            cursor->x += -(base_width / 2) - ruby_width / 2;
        }
        xglFontSetTextureFilter(cursor, 0);
        flags |= FONT_CURSOR_COMPACT;
        break;
    case 'e':
        if (next[-1] != ';') {
            return 0;
        }
        if (flags & FONT_CURSOR_RUBY) {
            int rise = fontStateFromWindow.ruby_offset_y + ((fontStateFromWindow.compact_height * 8) >> 4);

            cursor->y += rise;
            cursor->x += fontStateFromWindow.ruby_advance;
        }
        xglFontSetTextureFilter(cursor, 1);
        flags &= ~(FONT_CURSOR_RUBY | FONT_CURSOR_COMPACT);
        break;
    case 'b':
        if (next[-1] != ';') {
            return 0;
        }
        flags |= FONT_CURSOR_RUBY;
        fontStateFromWindow.ruby_base = next;
        break;
    case 'z':
        next--;
        if (xglFontParseNumber(parse, &width)) {
            if (next[-1] == ';') {
                height = width;
            } else if (!xglFontParseNumber((const char **)&next, &height)) {
                break;
            }
            fontStateFromWindow.compact_width = width;
            fontStateFromWindow.compact_height = height;
        }
        break;
    case 'y':
        next--;
        if (xglFontParseNumber((const char **)&next, &offset)) {
            fontStateFromWindow.ruby_offset_y = offset;
        }
        break;
    default:
        return 0;
    }
    cursor->flags = flags;
    *text = (const char *)next;
    return 1;
}

/*
 * Inline width command: "n0;" / "n1;" / "n2;" pick the half-width mode, "p0;" / "p1;" proportional
 * spacing; the letter stands in front of the text cursor.
 */
int xglFontCmdHan(FontDrawCursor *cursor, const char **text)
{
    const unsigned char *next = (const unsigned char *)*text;
    unsigned char flags = cursor->flags;

    if (next[1] != ';') {
        return 0;
    }
    switch (next[-1]) {
    case 'p':
        flags &= ~FONT_CURSOR_PROPORTIONAL;
        switch (next[0]) {
        case '0':
            break;
        case '1':
            flags |= FONT_CURSOR_PROPORTIONAL;
            break;
        default:
            return 0;
        }
        break;
    case 'n':
        flags &= ~(FONT_CURSOR_WIDE_ASCII | FONT_CURSOR_NATIVE_8X8);
        switch (next[0]) {
        case '0':
            break;
        case '1':
            flags |= FONT_CURSOR_WIDE_ASCII;
            break;
        case '2':
            flags |= FONT_CURSOR_NATIVE_8X8;
            break;
        default:
            return 0;
        }
        break;
    default:
        return 0;
    }
    cursor->flags = flags;
    *text = (const char *)(next + 2);
    return 1;
}

void xglFontFlushSub(int texture_u, int texture_v, int size, int flags);

/* Texture coordinate / size / draw-flag arguments of xglFontFlushSub. */
#define FONT_ASCII_V_LOW 0x2f00
#define FONT_ASCII_V_HIGH 0x2f80
#define FONT_SYMBOL_V 0x2e80
#define FONT_GLYPH_8X8 0x808
#define FONT_DRAW_DOUBLE_SIZE 0x10
#define FONT_DRAW_COMPACT 0x20

/* Inline mark command: draws symbol glyph `index` (0..63 from the low row, 64.. from the high row). */
int xglFontCmdMark(FontDrawCursor *cursor, const char **text)
{
    const char *next = *text;
    int index;

    if (!xglFontParseHex(&next, &index)) {
        return 0;
    }
    if ((unsigned int)index < 0x40) {
        xglFontFlushSub((index & 0x3F) << 7, FONT_ASCII_V_LOW, FONT_GLYPH_8X8, FONT_DRAW_DOUBLE_SIZE);
    } else {
        xglFontFlushSub((index & 0x3F) << 7, FONT_ASCII_V_HIGH, FONT_GLYPH_8X8, FONT_DRAW_DOUBLE_SIZE);
    }
    *text = next;
    return 1;
}

typedef int (*FontInlineHandler)(FontDrawCursor *cursor, const char **text);

/* One entry of the inline "$name" command table; the table ends with a null name. */
struct FontInlineCommand {
    const char *name;
    FontInlineHandler handler;
};

/* ax ay az rx ry init save load col bg shadow alpha zoom xx ruby prop han mark */
extern struct FontInlineCommand fontInlineCommands[];
/* "[*error*:%c%c%c%c]\n" */
const char fontInlineErrorFormat[24] = "[*error*:%c%c%c%c]\n";
int printf(const char *format, ...);

/*
 * Runs the inline command after the '$' at *text: the first table name that is a prefix of the text
 * wins, its handler parses the arguments. Unknown names are reported with their first four characters.
 */
int xglFontInlineCommand(FontDrawCursor *cursor, unsigned char **text)
{
    const char *next = (const char *)*text + 1;
    struct FontInlineCommand *command = fontInlineCommands;
    FontInlineHandler handler = 0;
    while (command->name != 0) {
        const char *name = command->name;
        const char *scan = next;

        while (*name == *scan) {
            name++;
            scan++;
        }
        if (*name == 0) {
            next = scan;
            handler = command->handler;
            break;
        }
        command++;
    }
    if (handler != 0) {
        if (handler(cursor, &next)) {
            *text = (unsigned char *)next;
            return 1;
        }
    } else {
        printf(fontInlineErrorFormat, (unsigned char)next[0], (unsigned char)next[1], (unsigned char)next[2],
               (unsigned char)next[3]);
    }
    return 0;
}

extern FontPrintQueueStorage fontPrintQueueBuffer;


void buffer_reset(void)
{
    struct FontStateFromWindow *block;
    FontQueueSlot *slot;
    int index;

    slot = fontPrintQueueBuffer.slots;
    for (index = 0; index < FONT_QUEUE_BUCKETS; index++) {
        slot->selfOffset = (unsigned short) ((unsigned char *) slot - (unsigned char *) fontPrintQueueBuffer.slots);
        slot->link = 0;
        slot++;
    }
    /* The block ends where the slot array begins; it is the state starting at fontStateFromWindow. */
    block = (struct FontStateFromWindow *) &fontPrintQueueBuffer - 1;
    block->debug_prefix = 0;
    block->text_mode = 0;
    block->queue_cursor = (unsigned char *) slot;
}

/* GIF tag fields, register descriptors and GS PRIM bits of the glyph packets (XS1 include/main/xgl_packet.h). */
#define GIF_TAG_EOP 0x8000
#define GIF_TAG_PRE 0x4000
#define GIF_TAG_PRIM_SHIFT 15
#define GIF_TAG_NREG_SHIFT 28
#define GIF_REG_RGBAQ 1
#define GIF_REG_UV 3
#define GIF_REG_XYZ2 5
#define GIF_REG_TEX0_1 6
#define GS_PRIM_TRISTRIP 4
#define GS_PRIM_TME 0x10
#define GS_PRIM_ABE 0x40
#define GS_PRIM_FST 0x100
/* Sprite GIF tags: PRE with a textured, alpha-blended, UV-addressed sprite; the strip form is
 * used for the horizontally sheared glyph. */
#define FONT_SPRITE_TAG_HIGH ((6 << GIF_TAG_NREG_SHIFT) | \
    ((GS_PRIM_SPRITE | GS_PRIM_TME | GS_PRIM_ABE | GS_PRIM_FST) << GIF_TAG_PRIM_SHIFT) | GIF_TAG_PRE)
#define FONT_SPRITE_TAG_REGS (GIF_REG_TEX0_1 | (GIF_REG_RGBAQ << 4) | (GIF_REG_UV << 8) | \
    (GIF_REG_XYZ2 << 12) | (GIF_REG_UV << 16) | (GIF_REG_XYZ2 << 20))
#define FONT_STRIP_TAG_HIGH ((10 << GIF_TAG_NREG_SHIFT) | \
    ((GS_PRIM_TRISTRIP | GS_PRIM_TME | GS_PRIM_ABE | GS_PRIM_FST) << GIF_TAG_PRIM_SHIFT) | GIF_TAG_PRE)
#define FONT_STRIP_TAG_REGS_LOW (FONT_SPRITE_TAG_REGS | (GIF_REG_UV << 24) | (GIF_REG_XYZ2 << 28))
#define FONT_STRIP_TAG_REGS_HIGH (GIF_REG_UV | (GIF_REG_XYZ2 << 4))
/* Qword counts sent for one sprite and one sheared glyph. */
#define FONT_SPRITE_QWORDS 7
#define FONT_STRIP_QWORDS 11
/* Glyphs whose top-left corner lies beyond this (12.4 fixed point) are skipped when clipping. */
#define FONT_CLIP_LIMIT 0xf000U
/* xglFontFlushSub flags: the low bits pick the texture (bit 0: second CLUT), FONT_DRAW_DOUBLE_SIZE doubles. */
#define FONT_DRAW_ATTRIBUTE_MASK 15
/* Widest texture span a glyph is drawn from. */
#define FONT_TEXTURE_SPAN_MAX 320

struct FontSpriteCorner {
    unsigned int texture_u_or_x;
    unsigned int texture_v_or_y;
    unsigned int backup_x;
    unsigned int backup_y;
    unsigned int x;
    unsigned int y;
    unsigned int depth;
    unsigned int depth_upper;
};

/* The GIF packet a glyph is drawn with, in the scratchpad after the draw cursor. */
struct FontSpritePacket {
    unsigned int tag_low;
    unsigned int tag_high;
    unsigned int registers_low;
    unsigned int registers_high;
    unsigned long long texture;
    unsigned char unmodeled_18[8];
    unsigned int red;
    unsigned int green;
    unsigned int blue;
    unsigned int alpha;
    struct FontSpriteCorner corners[4];
};

/* Unit offsets of the outline effect: up, left, right, down (signed x, y pairs). */
extern signed char fontOutlineDirections[4][2];

void xglFontFlushSub(int texture_u, int texture_v, int size, int flags)
{
    struct FontDrawCursor *cursor = (struct FontDrawCursor *)SCRATCHPAD_BASE;
    struct FontSpritePacket *sprite = (struct FontSpritePacket *)(SCRATCHPAD_BASE + 0x100);
    int glyph_width = size >> 8;
    unsigned int glyph_height = size & 0xff;
    int texture_width;
    int texture_height;
    int draw_width;
    int draw_height;
    int effect_x;
    int effect_y;
    unsigned int left = (((short)cursor->offset_x + (short)cursor->x) * 16) - 8;
    unsigned int top = (((short)cursor->offset_y + (short)cursor->y) * 16) - 9;
    int attributes = flags & FONT_DRAW_ATTRIBUTE_MASK;

    texture_width = glyph_width * 16;
    texture_height = glyph_height * 16;
    if (!fontStateFromWindow.debug_mode && (flags & FONT_DRAW_DOUBLE_SIZE) && !(cursor->flags & FONT_CURSOR_NATIVE_8X8)) {
        draw_width = texture_width * 2;
        draw_height = texture_height * 2;
    } else {
        draw_width = glyph_width * 16;
        draw_height = glyph_height * 16;
    }
    effect_x = cursor->effect_x * 16;
    effect_y = cursor->effect_y * 16;
    if (texture_width > FONT_TEXTURE_SPAN_MAX) {
        texture_width = FONT_TEXTURE_SPAN_MAX;
    }
    if (flags & FONT_DRAW_COMPACT) {
        draw_width = (draw_width * fontStateFromWindow.compact_width) >> 4;
        draw_height = (draw_height * fontStateFromWindow.compact_height) >> 4;
    } else {
        draw_width = (draw_width * (short)cursor->scale_x) >> 4;
        draw_height = (draw_height * (short)cursor->scale_y) >> 4;
    }
    cursor->x += draw_width / 16;
    if ((fontStateFromWindow.flags & FONT_STATE_CLIP) && (left > FONT_CLIP_LIMIT || top > FONT_CLIP_LIMIT)) {
        return;
    }
    sprite->tag_low = GIF_TAG_EOP | 1;
    sprite->tag_high = FONT_SPRITE_TAG_HIGH;
    sprite->registers_low = FONT_SPRITE_TAG_REGS;
    sprite->registers_high = 0;
    /* TEX0: the 4-bit glyph texture at 0x3c20 (width 10, 1024x1024), its CLUT at 0x3c00 or the
     * next one (attribute bit 0), CLUT load on; alpha from the texture (TCC) except in debug mode. */
    sprite->texture = GS_SET_TEX0(0x3c20, 10, GS_PSMT4, 10, 10, 0, 0, 0, 0, 0, 0, 0) |
        ((!fontStateFromWindow.debug_mode) ? GS_SET_TEX0(0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1)
                                  : GS_SET_TEX0(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1)) |
        GS_SET_TEX0(0, 0, 0, 0, 0, 0, 0, (attributes & 1) + 0x3c00, 0, 0, 0, 0);
    sprite->red = cursor->red;
    sprite->green = cursor->green;
    sprite->blue = cursor->blue;
    sprite->alpha = cursor->alpha;
    sprite->corners[0].texture_u_or_x = texture_u;
    sprite->corners[0].texture_v_or_y = texture_v;
    if (fontStateFromWindow.horizontal_shift) {
        unsigned int shifted_left = left + fontStateFromWindow.horizontal_shift * 16;
        int u_end = texture_u + texture_width;

        sprite->corners[0].y = top;
        sprite->corners[2].texture_u_or_x = texture_u;
        sprite->corners[1].texture_v_or_y = texture_v;
        sprite->tag_low = GIF_TAG_EOP | 1;
        sprite->tag_high = FONT_STRIP_TAG_HIGH;
        sprite->registers_low = FONT_STRIP_TAG_REGS_LOW;
        sprite->registers_high = FONT_STRIP_TAG_REGS_HIGH;
        sprite->corners[0].x = shifted_left;
        sprite->corners[0].depth = cursor->depth;
        sprite->corners[0].depth_upper = 0;
        sprite->corners[1].y = top;
        sprite->corners[1].x = shifted_left + draw_width;
        sprite->corners[1].texture_u_or_x = u_end;
        sprite->corners[1].depth = cursor->depth;
        sprite->corners[1].depth_upper = 0;
        sprite->corners[2].x = left;
        sprite->corners[2].texture_v_or_y = texture_v + texture_height;
        sprite->corners[2].y = top + draw_height;
        sprite->corners[2].depth = cursor->depth;
        sprite->corners[2].depth_upper = 0;
        sprite->corners[3].texture_u_or_x = u_end;
        sprite->corners[3].texture_v_or_y = texture_v + texture_height;
        sprite->corners[3].x = left + draw_width;
        sprite->corners[3].y = top + draw_height;
        sprite->corners[3].depth = cursor->depth;
        sprite->corners[3].depth_upper = 0;
        sceVif1PkAddDirectDataN(cursor->packet, sprite, FONT_STRIP_QWORDS);
        return;
    }
    sprite->corners[0].x = left;
    sprite->corners[0].y = top;
    sprite->corners[0].depth = cursor->depth;
    sprite->corners[0].depth_upper = 0;
    sprite->corners[1].texture_u_or_x = texture_u + texture_width;
    sprite->corners[1].texture_v_or_y = texture_v + texture_height;
    sprite->corners[1].x = left + draw_width;
    sprite->corners[1].y = top + draw_height;
    sprite->corners[1].depth = cursor->depth;
    sprite->corners[1].depth_upper = 0;
    sceVif1PkAddDirectDataN(cursor->packet, sprite, FONT_SPRITE_QWORDS);
    sprite->corners[0].depth--;
    sprite->corners[1].depth--;
    if (cursor->effect == FONT_EFFECT_BOLD) {
        unsigned int old_left = sprite->corners[0].x;
        unsigned int old_right = sprite->corners[1].x;
        unsigned int old_top = sprite->corners[0].y;
        unsigned int old_bottom = sprite->corners[1].y;

        sprite->corners[0].x = old_left + 16;
        sprite->corners[2].texture_v_or_y = old_top;
        sprite->corners[2].backup_x = old_right;
        sprite->corners[2].backup_y = old_bottom;
        sprite->corners[0].y = old_top;
        sprite->corners[1].y = old_bottom;
        sprite->corners[1].x = old_right + 16;
        sprite->corners[2].texture_u_or_x = old_left;
        sceVif1PkAddDirectDataN(cursor->packet, sprite, FONT_SPRITE_QWORDS);
        sprite->corners[0].x = sprite->corners[2].texture_u_or_x;
        sprite->corners[0].y = sprite->corners[2].texture_v_or_y + 16;
        sprite->corners[1].x = sprite->corners[2].backup_x;
        sprite->corners[1].y = sprite->corners[2].backup_y + 16;
        sceVif1PkAddDirectDataN(cursor->packet, sprite, FONT_SPRITE_QWORDS);
    } else if (cursor->effect >= FONT_EFFECT_SHADOW) {
        unsigned char effect_red = cursor->effect_red;
        unsigned char effect_green = cursor->effect_green;
        unsigned char effect_blue = cursor->effect_blue;
        unsigned int old_left = sprite->corners[0].x;
        unsigned int old_top = sprite->corners[0].y;
        unsigned int old_right = sprite->corners[1].x;
        unsigned int old_bottom = sprite->corners[1].y;

        sprite->red = effect_red;
        sprite->green = effect_green;
        sprite->blue = effect_blue;
        sprite->corners[2].texture_u_or_x = old_left;
        sprite->corners[2].texture_v_or_y = old_top;
        sprite->corners[2].backup_x = old_right;
        sprite->corners[2].backup_y = old_bottom;
        if (cursor->effect == FONT_EFFECT_SHADOW) {
            sprite->corners[0].x = old_left + effect_x;
            sprite->corners[0].y = old_top + effect_y;
            sprite->corners[1].x = old_right + effect_x;
            sprite->corners[1].y = old_bottom + effect_y;
            sceVif1PkAddDirectDataN(cursor->packet, sprite, FONT_SPRITE_QWORDS);
        } else {
            int row;
            int column;
            int direction;

            for (row = 16; row <= effect_y; row += 16) {
                for (column = 16; column <= effect_x; column += 16) {
                    for (direction = 0; direction < 4; direction++) {
                        int offset_x = fontOutlineDirections[direction][0] * column;
                        int offset_y = fontOutlineDirections[direction][1] * row;

                        sprite->corners[0].x = sprite->corners[2].texture_u_or_x + offset_x;
                        sprite->corners[0].y = sprite->corners[2].texture_v_or_y + offset_y;
                        sprite->corners[1].x = sprite->corners[2].backup_x + offset_x;
                        sprite->corners[1].y = sprite->corners[2].backup_y + offset_y;
                        sceVif1PkAddDirectDataN(cursor->packet, sprite, FONT_SPRITE_QWORDS);
                    }
                }
            }
        }
    }
}

/* Draws one hex digit (0..15): hex digit glyphs are 6x8 texels, 8 texels apart at FONT_ASCII_V_LOW. */
void xglFontFlushSubHex(int digit)
{
    xglFontFlushSub(digit << 7, FONT_ASCII_V_LOW, (6 << 8) | 8, FONT_DRAW_DOUBLE_SIZE);
}

/*
 * EE scratchpad at SCRATCHPAD_BASE: halfword slot 2 keeps the start x of the line, slot 4 is set back
 * to it and slot 5 (y) moves down by one line height.
 */
void xglFontFlushSubCRLF(void)
{
    unsigned short *scratchpad = (unsigned short *) SCRATCHPAD_BASE;

    scratchpad[4] = scratchpad[2];
    scratchpad[5] += FS.state.line_height;
}

/* Two-byte code pairs, terminated by a 0 first byte. */
extern unsigned char fontPairGlyphCodes[];

/* Glyph index of a code pair from the fontPairGlyphCodes list (0xF95 + position), or 0x7F if absent. */
int xglFontFindPairGlyph(unsigned char high, unsigned char low)
{
    unsigned char *pair = fontPairGlyphCodes;
    int index = 0;

    while (pair[0] != 0) {
        if (high == pair[0] && low == pair[1]) {
            return index + 0xF95;
        }
        pair += 2;
        index++;
    }
    return 0x7F;
}


/* Glyph index of codes the font does not have (also xglFontFindPairGlyph's miss). */
#define FONT_GLYPH_NONE 0x7F
/* The level-1 kanji glyphs follow the 0x400 symbol and kana glyphs. */
#define FONT_GLYPH_KANJI_BASE 0x400

/* EUC code (row high, column low) to glyph index of the font texture: symbol, alphanumeric, kana, Greek
 * and Cyrillic rows map onto the first 0x400 glyphs, level-1 kanji follow; FONT_GLYPH_NONE otherwise. */
unsigned short xglFontGetGlyphIndex(unsigned char high, unsigned char low)
{
    int glyph;

    if (high < EUC_ROW_KANJI_LEVEL1) {
        glyph = FONT_GLYPH_NONE;
        switch (high) {
        case EUC_ROW_SYMBOLS:
            glyph = (unsigned short)(low - EUC_FIRST);
            break;
        case EUC_ROW_SYMBOLS_2:
            switch (low & 0xF0) {
            case 0xA0:
                glyph = (low & 0xF) + 93;
                break;
            case 0xE0:
                glyph = (low & 0xF) + 108;
                break;
            case 0xF0:
                glyph = (low & 0xF) + 117;
                break;
            }
            break;
        case EUC_ROW_ALNUM:
            switch (low & 0xE0) {
            case 0xA0:
                glyph = (low & 0xF) + 128;
                break;
            case 0xC0:
                glyph = (low & 0x1F) + 137;
                break;
            case 0xE0:
                glyph = (low & 0x1F) + 163;
                break;
            }
            break;
        case EUC_ROW_HIRAGANA:
            glyph = low + 31;
            break;
        case 0xA5:
            glyph = low + 127;
            break;
        case EUC_ROW_GREEK:
            if (low < 0xC0) {
                glyph = (low & 0x1F) + 383;
            } else {
                glyph = (low & 0x1F) + 407;
            }
            break;
        case 0xA7:
            glyph = (unsigned short)(low < 0xD0 ? low + 271 : low + 256);
            break;
        case 0xA8:
            glyph = low + 351;
            break;
        case 0xA9:
            glyph = low + 256;
            break;
        case EUC_ROW_NEC_SPECIAL:
            glyph = low + 383;
            break;
        }
    } else if (high < EUC_ROW_KANJI_LEVEL2) {
        glyph = (unsigned short)((high - EUC_ROW_KANJI_LEVEL1) * FONT_JIS_ROW_SIZE + (low - EUC_FIRST)
                                 + FONT_GLYPH_KANJI_BASE);
    } else {
        glyph = (unsigned short)xglFontFindPairGlyph(high, low);
    }
    return glyph;
}

/* FONT_OP_CONFIG sub-commands. */
#define FONT_CONFIG_LINE_HEIGHT 1
#define FONT_CONFIG_DEFAULT_WIDTH 2
#define FONT_CONFIG_WRAP_WIDTH 3
#define FONT_CONFIG_PROPORTIONAL_MODE 4
#define FONT_CONFIG_HORIZONTAL_SHIFT 5
/* The compact position codes store x + 0x100 and y + 0x400 as 16-bit values. */
#define FONT_POS_X_BIAS 0x100
#define FONT_POS_Y_BIAS 0x400
/* Hex digit glyphs 0..15, then the minus sign and decimal point; 32 is blank. */
#define FONT_HEX_MINUS 16
#define FONT_HEX_POINT 17
#define FONT_HEX_BLANK 32
/* IEEE single precision fields used by the %f printer. */
#define FONT_FLOAT_MANTISSA_MASK 0x7fffff
#define FONT_FLOAT_HIDDEN_BIT 0x800000
#define FONT_FLOAT_EXPONENT_SHIFT_BASE 150 /* bias 127 + 23 mantissa bits */

struct FontLiteralHexCommand {
    unsigned char opcode;
    unsigned char digit_index;
    unsigned char value_high;
    unsigned char value_mid_high;
    unsigned char value_mid_low;
    unsigned char value_low;
};


struct FontRealDigits {
    signed char value[16];
};

/* GS state packets: the full reset, the two effect modes. */
extern unsigned int fontGsResetPacket[16];
extern unsigned int fontGsAlphaTestPacket[8];
extern unsigned int fontGsPlainTestPacket[8];

int xglFontInlineCommand(FontDrawCursor *cursor, unsigned char **text);
void xglFontSetTextureFilter(FontDrawCursor *cursor, int mode);

void xglFontFlushSubHex(int digit);
void xglFontFlushSubCRLF(void);

void xglFontFlushCore(FontDrawCursor *cursor, unsigned char *text)
{
    struct FontRealDigits real_digits;
    signed char integer_digits[32];

    while (*text != 0) {
        unsigned char opcode;

        if (*text == '$' && xglFontInlineCommand(cursor, &text) != 0) {
            continue;
        }
        opcode = *text;
        switch (opcode) {
        case FONT_OP_POS_X_0: case FONT_OP_POS_X_1: case FONT_OP_POS_X_2: {
            unsigned int position_high = text[0];
            unsigned int position_low = text[1];
            int position;
            text += 2;
            position = position_low + (position_high << 8) - FONT_POS_X_BIAS;
            cursor->x = position;
            cursor->origin_x = position;
            break;
        }
        case FONT_OP_POS_Y_0: case FONT_OP_POS_Y_1: {
            unsigned int position_high = text[0];
            unsigned int position_low = text[1];
            int position;
            text += 2;
            position = position_low + (position_high << 8) - FONT_POS_Y_BIAS;
            cursor->y = position;
            break;
        }
        case FONT_OP_DEPTH: {
            int depth = (text[1] << 16) + (text[2] << 8) + text[3];
            text += 4;
            cursor->depth = depth;
            break;
        }
        case FONT_OP_NOP:
            text++;
            break;
        case FONT_OP_POSITION: {
            unsigned char flags = text[1];
            text += 2;
            if (flags & FONT_POS_SET_X) {
                cursor->x = text[1] + (text[0] << 8);
                if (!(flags & FONT_POS_KEEP_ORIGIN)) cursor->origin_x = cursor->x;
                text += 2;
            }
            if (flags & FONT_POS_SET_Y) {
                cursor->y = text[1] + (text[0] << 8);
                text += 2;
            }
            if (flags & FONT_POS_SET_DEPTH) {
                cursor->depth = (text[0] << 16) + (text[1] << 8) + text[2];
                text += 3;
            }
            if (flags & FONT_POS_ADD_X) {
                cursor->x += text[1] + (text[0] << 8);
                if (!(flags & FONT_POS_KEEP_ORIGIN)) cursor->origin_x = cursor->x;
                text += 2;
            }
            if (flags & FONT_POS_ADD_Y) {
                cursor->y += text[1] + (text[0] << 8);
                text += 2;
            }
            break;
        }
        case FONT_OP_TAB:
            cursor->x = (cursor->x + FONT_TAB_WIDTH) & ~(FONT_TAB_WIDTH - 1) & 0xffff;
            text++;
            break;
        case FONT_OP_NEWLINE:
            xglFontFlushSubCRLF();
            text++;
            break;
        case FONT_OP_RESET:
            cursor->effect = FONT_EFFECT_PLAIN;
            cursor->red = cursor->green = cursor->blue = FONT_COLOR_NEUTRAL;
            cursor->alpha_abcd = 0;
            cursor->scale_x = FONT_SCALE_ONE;
            cursor->alpha = FONT_ALPHA_NEUTRAL;
            cursor->scale_y = FONT_SCALE_ONE;
            cursor->flags &= FONT_CURSOR_STYLE_CLEAR;
            if (!FS.state.debug_mode) sceVif1PkAddDirectDataN(cursor->packet, fontGsResetPacket, 4);
            text++;
            break;
        case FONT_OP_COLOR:
            cursor->red = text[1]; cursor->green = text[2]; cursor->blue = text[3];
            text += 4;
            break;
        case FONT_OP_EFFECT_MODE: {
            cursor->effect = text[1];
            switch (cursor->effect) {
            case 0:
            case FONT_EFFECT_SHADOW:
            case FONT_EFFECT_OUTLINE:
            case FONT_EFFECT_BOLD:
                sceVif1PkAddDirectDataN(cursor->packet, fontGsAlphaTestPacket, 2);
                break;
            case FONT_EFFECT_PLAIN:
            default:
                sceVif1PkAddDirectDataN(cursor->packet, fontGsPlainTestPacket, 2);
                break;
            }
            text += 2;
            break;
        }
        case FONT_OP_EFFECT_PARAMS:
            cursor->effect_x = text[1]; cursor->effect_y = text[2];
            cursor->effect_red = text[3]; cursor->effect_green = text[4]; cursor->effect_blue = text[5];
            text += 6;
            break;
        case FONT_OP_BLEND: {
            struct FontAlphaWrite *packet;
            cursor->alpha_abcd = text[1]; cursor->alpha_fix = text[2]; cursor->alpha = text[3];
            packet = xglFontAllocGifPacket(cursor, 1);
            packet->alpha_abcd = cursor->alpha_abcd;
            packet->alpha_fix = cursor->alpha_fix;
            packet->register_address = GS_ALPHA_1;
            text += 4;
            break;
        }
        case FONT_OP_HEX_DIGITS: {
            const struct FontLiteralHexCommand *command =
                (const struct FontLiteralHexCommand *) text;
            unsigned int value = ((unsigned int) command->value_high << 24) +
                ((unsigned int) command->value_mid_high << 16) +
                ((unsigned int) command->value_mid_low << 8) + command->value_low;
            int digit = command->digit_index;
            while (digit >= 0) {
                xglFontFlushSubHex((value >> (digit * 4)) & 15);
                digit--;
            }
            text += sizeof(*command);
            break;
        }
        case FONT_OP_NUMBER: {
            const struct FontNumericCommand *command =
                (const struct FontNumericCommand *) text;
            if (command->radix == FONT_NUMERIC_FLOAT) {
                unsigned int mid_high = (unsigned int) command->value_mid_high << 16;
                unsigned int high = (unsigned int) command->value_high << 24;
                unsigned int mid = ((unsigned int) command->value_mid_low << 8) + mid_high;
                unsigned int bits = high + command->value_low + mid;
                unsigned int mantissa = (bits & FONT_FLOAT_MANTISSA_MASK) + FONT_FLOAT_HIDDEN_BIT;
                unsigned int shift = FONT_FLOAT_EXPONENT_SHIFT_BASE - ((bits >> 23) & 255);
                long long integer;
                unsigned long long mask;
                unsigned long long fraction;
                int digit;
                int printed;
                int remaining;

                if ((int) bits < 0)
                    xglFontFlushSubHex(FONT_HEX_MINUS);
                else
                    xglFontFlushSubHex(FONT_HEX_BLANK);
                if (shift >= 32) {
                    integer = 0;
                    mask = (0x100000000ULL << (shift - 32)) - 1;
                } else {
                    integer = mantissa >> shift;
                    mask = (int) ((1U << shift) - 1);
                }
                fraction = mantissa & mask;
                for (digit = 7; digit >= 0; digit--) {
                    real_digits.value[digit] = integer % 10;
                    integer /= 10;
                    fraction *= 10;
                    if (shift >= 32)
                        real_digits.value[digit + 8] = ((long long) fraction >> 32) >> (shift - 32);
                    else
                        real_digits.value[digit + 8] = (long long) fraction >> shift;
                    fraction &= mask;
                }
                printed = 0;
                for (digit = 0; digit < 7; digit++) {
                    if (real_digits.value[digit] || printed) {
                        printed++;
                        xglFontFlushSubHex(real_digits.value[digit]);
                    }
                }
                xglFontFlushSubHex(real_digits.value[7]);
                xglFontFlushSubHex(FONT_HEX_POINT);
                printed += 2;
                remaining = 7;
                for (; remaining >= 0 && printed < 9; remaining--) {
                    printed++;
                    xglFontFlushSubHex(real_digits.value[remaining + 8]);
                }
            } else {
                int suppress_zero = 0;
                int radix = command->radix;
                int digit = command->digits_minus_one;
                unsigned int value = ((unsigned int) command->value_high << 24) +
                    ((unsigned int) command->value_mid_high << 16) +
                    ((unsigned int) command->value_mid_low << 8) + command->value_low;
                if (radix == 10) {
                    suppress_zero = 1;
                    if ((int) value < 0) { value = -value; suppress_zero = 2; }
                }
                integer_digits[digit + 1] = -1;
                while (digit >= 0) {
                    integer_digits[digit] = value % radix;
                    value /= radix;
                    digit--;
                }
                for (digit = 0; integer_digits[digit] >= 0; digit++) {
                    if (suppress_zero && !integer_digits[digit] && integer_digits[digit + 1] >= 0) {
                        if (suppress_zero == 2 && integer_digits[digit + 1] > 0) xglFontFlushSubHex(FONT_HEX_MINUS);
                        else xglFontFlushSubHex(FONT_HEX_BLANK);
                    } else {
                        xglFontFlushSubHex(integer_digits[digit]);
                        suppress_zero = 0;
                    }
                }
            }
            text += sizeof(*command);
            break;
        }
        case FONT_OP_SCALE:
            cursor->scale_x = text[1]; cursor->scale_y = text[2];
            xglFontSetTextureFilter(cursor, 1);
            text += 3;
            break;
        case FONT_OP_EFFECT_PACKET: {
            struct FontEffectScratchPacket *packet = (struct FontEffectScratchPacket *)
                (((unsigned int) &cursor->work.effect_packet + 15) & ~15);
            long long horizontal_mode;
            long long horizontal_mask;
            long long vertical_mask;
            long long vertical_mode;
            long long horizontal_random;
            long long vertical_random;
            packet->vif_tag = 0x1000000000000000ULL | 0x8000 | 1ULL;
            packet->vif_count = GIF_AD;
            if (!text[1]) {
                horizontal_mode = 1; horizontal_mask = horizontal_random = 0;
            } else {
                unsigned long long mask_byte = text[1];
                horizontal_mode = 3; horizontal_mask = mask_byte ^ 0x3ff;
                if (mask_byte & 0x80) {
                    unsigned short random = xglSRand();
                    horizontal_random = text[1] & random;
                } else {
                    horizontal_random = 0;
                }
            }
            if (!text[2]) {
                vertical_mode = 1; vertical_mask = vertical_random = 0;
            } else {
                vertical_mode = 3; vertical_mask = text[1] ^ 0x3ff;
                if (text[2] & 0x80) {
                    unsigned short random = xglSRand();
                    vertical_random = text[1] & random;
                } else {
                    vertical_random = 0;
                }
            }
            packet->payload_count = 8;
            packet->effect_state = horizontal_mode | (vertical_mode << 2) | (horizontal_mask << 4) |
                (horizontal_random << 14) | (vertical_mask << 24) | (vertical_random << 34);
            sceVif1PkAddDirectDataN(cursor->packet, packet, 2);
            text += 3;
            break;
        }
        case FONT_OP_CONFIG: {
            const unsigned char *payload = text + 2;
            int length = text[1];
            text += length + 2;
            switch (payload[0]) {
            case FONT_CONFIG_LINE_HEIGHT: if (length == 2) FS.state.line_height = payload[1]; break;
            case FONT_CONFIG_DEFAULT_WIDTH: if (length == 2) FS.state.default_width = payload[1]; break;
            case FONT_CONFIG_WRAP_WIDTH: if (length == 3) FS.state.wrap_width = (payload[1] << 8) + payload[2]; break;
            case FONT_CONFIG_PROPORTIONAL_MODE: if (length == 2) FS.state.proportional_mode = payload[1]; break;
            case FONT_CONFIG_HORIZONTAL_SHIFT: if (length == 2) FS.state.horizontal_shift = payload[1]; break;
            }
            break;
        }
        case FONT_OP_INCLUDE: {
            const struct FontIncludeCommand *command =
                (const struct FontIncludeCommand *) text;
            unsigned char *included = (unsigned char *) (((unsigned int) command->text_high << 24) +
                ((unsigned int) command->text_mid_high << 16) +
                ((unsigned int) command->text_mid_low << 8) + command->text_low);
            text += sizeof(*command);
            xglFontFlushCore(cursor, included);
            break;
        }
        case FONT_OP_CALLBACK: {
            const struct FontExtCommand *command =
                (const struct FontExtCommand *) text;
            FontExtFunc callback = (FontExtFunc)
                (((unsigned int) command->function_high << 16) +
                 ((unsigned int) command->function_middle << 8) +
                 command->function_low);
            void *argument = (void *) (((unsigned int) command->argument_high << 24) +
                ((unsigned int) command->argument_mid_high << 16) +
                ((unsigned int) command->argument_mid_low << 8) +
                command->argument_low);
            text += sizeof(*command);
            callback(cursor, argument);
            break;
        }
        case FONT_OP_COMPACT:
            if (text[1]) {
                xglFontSetTextureFilter(cursor, 0);
                cursor->flags |= FONT_CURSOR_COMPACT;
            } else {
                xglFontSetTextureFilter(cursor, 1);
                cursor->flags &= ~FONT_CURSOR_COMPACT;
            }
            text += 2;
            break;
        case FONT_OP_STYLE: {
            unsigned char bits = text[1];
            unsigned char flags = cursor->flags & (~(FONT_CURSOR_PROPORTIONAL | FONT_CURSOR_WIDE_ASCII) & 0xff);
            text += 2;
            if (bits & FONT_STYLE_PROPORTIONAL) flags |= FONT_CURSOR_PROPORTIONAL;
            if (bits & FONT_STYLE_WIDE_ASCII) flags |= FONT_CURSOR_WIDE_ASCII;
            cursor->flags = flags;
            break;
        }
        case FONT_OP_GLYPH: {
            signed char character = text[1];
            if (character < 64) {
                xglFontFlushSub((character & 63) << 7, FONT_ASCII_V_LOW, FONT_GLYPH_8X8, FONT_DRAW_DOUBLE_SIZE);
            } else {
                xglFontFlushSub((character & 63) << 7, FONT_ASCII_V_HIGH, FONT_GLYPH_8X8, FONT_DRAW_DOUBLE_SIZE);
            }
            text += 2;
            break;
        }
        case 20: case 26:
        case 28: case 29: case 30: case 31:
            break;
        default: {
            struct FontStateFromWindow *state;
            unsigned char high = opcode;
            unsigned char low;
            unsigned int code;

            if ((signed char) high >= 0) {
                if (cursor->flags & FONT_CURSOR_WIDE_ASCII) {
                    code = xglFontAscii2Euc(high, &text);
                    high = code >> 8;
                    low = code;
                } else {
                    code = high;
                    low = 0;
                }
                text++;
            } else {
                low = text[1];
                if (xglFontIsTextureGlyphCode(high, low)) {
                    text += 2;
                    code = ((high << 8) + low) & 0xffff;
                } else {
                    low = high + 0x80;
                    high = 0xa9;
                    text++;
                    code = ((high << 8) + low) & 0xffff;
                }
            }
            state = &fontStateFromWindow;
            if ((short) state->wrap_width > 0 && opcode == ' ' &&
                (short) cursor->x - cursor->origin_x + xglFontGetStringWidth2((const char *) text, 1) >
                    (short) state->wrap_width) {
                xglFontFlushSubCRLF();
                text++;
            } else if (code < 0x40) {
                xglFontFlushSub((opcode & 63) << 7, FONT_ASCII_V_LOW, FONT_GLYPH_8X8, FONT_DRAW_DOUBLE_SIZE);
            } else if (code < 0x80) {
                xglFontFlushSub((opcode & 63) << 7, FONT_ASCII_V_HIGH, FONT_GLYPH_8X8, FONT_DRAW_DOUBLE_SIZE);
            } else if (code < 0xa0) {
                xglFontFlushSub(0x1f80, FONT_ASCII_V_HIGH, FONT_GLYPH_8X8, FONT_DRAW_DOUBLE_SIZE);
            } else if (cursor->flags & FONT_CURSOR_COMPACT) {
                unsigned int glyph = xglFontGetDebugCell(high, low);
                xglFontFlushSub((glyph >> 16) & 0x1f80, glyph & 0xffff, FONT_GLYPH_8X8, FONT_DRAW_COMPACT);
                if (glyph & 0x20000000) {
                    xglFontFlushSub((((int) glyph >> 30) + 0x3e) << 7, FONT_SYMBOL_V, 0x508, FONT_DRAW_COMPACT);
                }
            } else {
                code = xglFontGetGlyphIndex(high, low);
                if ((cursor->flags & FONT_CURSOR_PROPORTIONAL) && code < 0x180) {
                    unsigned int bounds = xglFontGetProportionalSize(code);
                    unsigned short row = code >> 5;
                    unsigned int left = bounds & 255;
                    unsigned int right = (bounds >> 8) & 255;
                    xglFontFlushSub(((code & 31) * 20 + left) * 16, row * 384 + 0x1b00,
                        (right - left) * 256 + 0x118, 0);
                } else {
                    low = code & 31;
                    high = code >> 5;
                    xglFontFlushSub(low * 320, (high & 31) * 384, 0x1318, high >> 5);
                }
            }
            break;
        }
        }
    }
}


/* The kana cell codes of the debug font, one per EUC column (copied to the stack before use). */
extern const unsigned char kanaFontCodeTable[104];

/* The glyph columns of the debug font texture: the U of a glyph cell is its column << 23 (the high half of the cell word). */
#define FONT_GLYPH_COLUMN(row) ((unsigned int)(row) << 23)
/* The blank cell: last column of the high ASCII block. */
#define FONT_GLYPH_BLANK (FONT_GLYPH_COLUMN(0x3F) + FONT_ASCII_V_HIGH)

/*
 * Texture cell (column << 23 | V) of an EUC code in the debug font, used by the compact text mode: the alphanumeric row 0xA3, a few
 * symbols of row 0xA1, and the kana rows 0xA4 / 0xA5 through a per-column cell table. Anything else
 * gives the blank cell.
 */
unsigned int xglFontGetDebugCell(unsigned char high, unsigned char low)
{
    unsigned int cell = FONT_GLYPH_BLANK;

    switch (high) {
    case EUC_ROW_SYMBOLS:
        switch (low) {
        case 0xA1:
            cell = 0;
            break;
        case 0xA2:
            cell = FONT_GLYPH_COLUMN(4) + FONT_SYMBOL_V;
            break;
        case 0xA3:
            cell = FONT_GLYPH_COLUMN(1) + FONT_SYMBOL_V;
            break;
        case 0xA6:
            cell = FONT_GLYPH_COLUMN(5) + FONT_SYMBOL_V;
            break;
        case 0xBC:
        case 0xDD:
            cell = FONT_GLYPH_COLUMN(16) + FONT_SYMBOL_V;
            break;
        }
        break;
    case EUC_ROW_ALNUM:
        if (low < 0xC0) {
            cell = FONT_GLYPH_COLUMN(low & 0x3F) + FONT_ASCII_V_LOW;
        } else {
            cell = FONT_GLYPH_COLUMN(low & 0x3F) + FONT_ASCII_V_HIGH;
        }
        break;
    case EUC_ROW_HIRAGANA:
    case EUC_ROW_KATAKANA: {
        unsigned char kana[96];

        __builtin_memcpy(kana, kanaFontCodeTable, sizeof(kana));
        low = kana[low - 0xA0];
        cell = FONT_GLYPH_COLUMN(low) + ((0x101 - high) << 7);
        break;
    }
    }
    return cell;
}

const unsigned char kanaFontCodeTable[104] = {
    0, 7, 17, 8, 18, 9, 19, 10,
    20, 11, 21, 22, 86, 23, 87, 24,
    88, 25, 89, 26, 90, 27, 91, 28,
    92, 29, 93, 30, 94, 31, 95, 32,
    96, 33, 97, 15, 34, 98, 35, 99,
    36, 100, 37, 38, 39, 40, 41, 42,
    106, 234, 43, 107, 235, 44, 108, 236,
    45, 109, 237, 46, 110, 238, 47, 48,
    49, 50, 51, 12, 52, 13, 53, 14,
    54, 55, 56, 57, 58, 59, 0, 60,
    0, 0, 6, 61, 83, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

/* Glyph base per 32-code block of the EUC row 0xA3. */
extern signed char fontAlnumGlyphBase[];

/* Glyph index of a few EUC symbols and of row 0xA3 (alphanumerics), or -1. */
int xglFontGetAlnumGlyph(unsigned char high, unsigned char low)
{
    if (high == EUC_ROW_SYMBOLS) {
        if (low == 0xA1) {
            return 0x25;
        }
        if (low == 0xA5) {
            return 0xA;
        }
    }
    if (high == EUC_ROW_ALNUM) {
        return fontAlnumGlyphBase[(low - 0xA0) >> 5] + (low & 0x1F);
    }
    return -1;
}

/* xglFontGetProportionalSize's packed result: the glyph's left texel bound in
 * the low byte, its right bound in the high byte (the caller draws right - left
 * texels from left on). */
#define FONT_BOUNDS(left, right) (((right) << 8) | (left))
/* The fixed bounds while proportional_mode is set (XS1 computes a pair here). */
#define FONT_FIXED_BOUNDS FONT_BOUNDS(6, 15)

/*
 * Bounds of glyph `code`, packed as FONT_BOUNDS. The widths table of the font
 * image stores left + 1 and right - 1, so adding 0xFF restores both; a stored
 * left of 0 (now 0xFF) means no entry and gives bounds 0 to default_width.
 */
int xglFontGetProportionalSize(int code)
{
    struct FontStateFromWindow *state;
    struct FontImage *image;
    int bounds;

    state = &fontStateFromWindow;
    bounds = FONT_FIXED_BOUNDS;
    if (state->proportional_mode == 0) {
        bounds = 0xFFFF;
        image = (struct FontImage *) state->texture_buffer;
        bounds = (image->proportional_widths[code] + 0xFF) & bounds;
        if ((bounds & 0xFF) == 0xFF) {
            bounds = FONT_BOUNDS(0, state->default_width);
        }
    }
    return bounds;
}

/* ASCII character to EUC-JP code, indexed by the raw character; a control character becomes a full-width space. */
extern unsigned short asciiEucTable[];

unsigned short xglFontAscii2Euc(signed char character, unsigned char **text)
{
    if (character < 0x20) {
        return EUC_SPACE;
    }
    return asciiEucTable[character];
}


/* The kanji texture upload chain: transfer setup, texture, CLUT, then the draw environment. */
extern unsigned int KanjiTex0Env_8[28];
extern unsigned int KanjiDrawEnv[36];
/* Half the font texture, in quadwords. */
#define FONT_KANJI_TEXTURE_QWORDS 0x3C02
#define VIF_CODE_DIRECTHL 0x51000000


/*
 * Closes an open DIRECTHL block and queues the kanji texture upload. XS1 picked the kanji or window
 * texture from `request`; XS2 only has the kanji path left and no longer reads it.
 */
void xglFontReloadTexture(FontDrawCursor *cursor, unsigned int request)
{
    XglPacket *packet = cursor->packet;
    unsigned int *saved_direct_state;
    struct FontGsRegisterWrite *test;

    if (packet->directCode && (saved_direct_state = packet->directState)) {
        sceVif1PkCloseDirectHLCode(packet);
        if (*saved_direct_state == VIF_CODE_DIRECTHL) {
            cursor->packet->write_cursor -= 4;
        }
    }
    sceVif1PkRef(cursor->packet, KanjiTex0Env_8, 7, 0, 0, 0);
    sceVif1PkRef(cursor->packet, fontStateFromWindow.texture_buffer, FONT_KANJI_TEXTURE_QWORDS, 0, 0, 0);
    sceVif1PkRef(cursor->packet, &KanjiClutEnv, 15, 0, 0, 0);
    sceVif1PkRef(cursor->packet, KanjiDrawEnv, 9, 0, 0, 0);
    if (fontStateFromWindow.debug_mode) {
        sceVif1PkRef(cursor->packet, &ModeEnv[LoopCount & 1], 8, 0, 0, 0);
    }
    sceVif1PkCnt(cursor->packet, 0);
    sceVif1PkOpenDirectHLCode(cursor->packet, 0);
    if (fontStateFromWindow.debug_mode == 0) {
        test = xglFontAllocGifPacket(cursor, 1);
        test->register_address = GS_TEST_1;
        switch (cursor->effect) {
        case 0:
        case 2:
        case 3:
        case 4:
            test->value = FONT_TEST_DEPTH_GREATER | FONT_TEST_ALPHA_GREATER;
            break;
        case 1:
        default:
            test->value = FONT_TEST_DEPTH_GREATER;
            break;
        }
    }
}

/* EE scratchpad RAM, where xglFontFlush builds the draw cursor. */
/* xglFontFlush's cursor defaults: screen origin, colour and alpha. */
#define FONT_SCREEN_ORIGIN_X 0x700
#define FONT_SCREEN_ORIGIN_Y 0x720
#define FONT_COLOR_INITIAL 127
#define FONT_ALPHA_SOURCE_BLEND 0x44
#define GS_ALPHA_OPAQUE 0x80

/* A queued record: the byte offset of the next record of its bucket, then
 * the text. */
struct FontQueueRecord {
    unsigned char next_low;
    unsigned char next_high;
};

void buffer_reset(void);
/* The GS packets xglFontFlush closes with (6 quadwords in debug mode, then 2). */
extern unsigned int zbuf_14[24];
extern unsigned int test_15[8];

void xglFontFlush(void)
{
    /* The font drawing cursor resides at the base of EE scratchpad RAM. */
    struct FontDrawCursor *cursor = (struct FontDrawCursor *)SCRATCHPAD_BASE;
    FontQueueSlot *slot;
    int remaining;

    *fontStateFromWindow.queue_cursor = 0;
    cursor->depth = -1;
    cursor->blue = FONT_COLOR_INITIAL;
    cursor->green = FONT_COLOR_INITIAL;
    cursor->red = FONT_COLOR_INITIAL;
    if (fontStateFromWindow.debug_mode) {
        cursor->offset_x = 0;
        cursor->offset_y = 0;
    } else {
        cursor->offset_x = FONT_SCREEN_ORIGIN_X;
        cursor->offset_y = FONT_SCREEN_ORIGIN_Y;
    }
    cursor->flags = 0;
    cursor->effect_y = cursor->effect_x = cursor->effect = 1;
    cursor->effect_red = 0;
    cursor->scale_x = FONT_SCALE_ONE;
    cursor->effect_green = 0;
    cursor->alpha_abcd = FONT_ALPHA_SOURCE_BLEND;
    cursor->alpha = GS_ALPHA_OPAQUE;
    cursor->effect_blue = 0;
    cursor->alpha_fix = 0;
    cursor->scale_y = FONT_SCALE_ONE;
    remaining = FONT_QUEUE_BUCKETS - 1;
    cursor->packet = xglPacketGetCurrent();
    xglFontReloadTexture(cursor, 0x80000001);

    slot = fontPrintQueueBuffer.slots;
    do {
        unsigned short offset = slot->link;
        if (offset) {
            do {
                struct FontQueueRecord *record =
                    (struct FontQueueRecord *)&fontPrintQueueBuffer.bytes[offset];
                unsigned char *text = (unsigned char *)(record + 1);
                offset = record->next_low + (record->next_high << 8);
                xglFontFlushCore(cursor, text);
            } while (offset);
        }
        remaining--;
        slot++;
    } while (remaining >= 0);
    if (fontStateFromWindow.debug_mode) {
        sceVif1PkAddDirectDataN(cursor->packet, zbuf_14, 6);
    }
    sceVif1PkAddDirectDataN(cursor->packet, test_15, 2);
    sceVif1PkCloseDirectHLCode(cursor->packet);
    buffer_reset();
}

void *xglFontGetLoadAddress(void)
{
    return FS.state.texture_buffer;
}

unsigned short xglFontGetFlags(void)
{
    return FONT_STATE_FLAGS;
}

void xglFontSetFlags(int flags)
{
    FONT_STATE_FLAGS = flags & (0xFFFF & ~FONT_STATE_DEBUG_PRINT);
}

unsigned char xglFontLoad(int font_index, CdCompletionCallback *notify)
{
    unsigned char previous_font;
    int read_mode;

    previous_font = fontStateFromWindow.font_selection;
    fontStateFromWindow.font_selection = (unsigned char)font_index;
    read_mode = 1;
    if (notify == CD_NOTIFY_CURRENT) {
        if (font_index != 0) {
            read_mode = 0;
            notify = CD_NOTIFY_DUMMY;
        } else {
            read_mode = 0;
            notify = CD_NOTIFY_DUMMY;
        }
    }
    if (font_index == 0) {
        xglCdReadFile(fontTexture0Path, fontStateFromWindow.texture_buffer, read_mode, notify);
    } else {
        xglCdReadFile(fontTexture1Path, fontStateFromWindow.texture_buffer, read_mode, notify);
    }
    return previous_font;
}

void xglFontInitial(void)
{
    fontStateFromWindow.font_selection = 0xFF;
    fontStateFromWindow.texture_buffer = fontTextureBuffer;
    xglFontLoad(1, CD_NOTIFY_CURRENT);
    buffer_reset();
    fontStateFromWindow.flags = FONT_STATE_ENABLED | FONT_STATE_CLIP;
    fontStateFromWindow.line_height = 24;
    fontStateFromWindow.default_width = 4;
    fontStateFromWindow.debug_mode = 0;
    fontStateFromWindow.proportional_mode = 0;
    fontStateFromWindow.horizontal_shift = 0;
    fontStateFromWindow.wrap_width = 0;
    fontStateFromWindow.compact_width = 16;
    fontStateFromWindow.compact_height = 16;
    fontStateFromWindow.ruby_offset_y = 2;
}


struct FontModePacket ModeEnv[2] = { { .vif_commands = { 0U, 0U, 0U, 1358954503U }, .gif_tag = { 1152921504606879750ULL, 14ULL }, .frame = { .value = 0ULL, .register_address = 76U, .register_address_high = 0U }, .state_registers = { { .value = 0ULL, .register_address = 24U, .register_address_high = 0U }, { .value = 62768919827382272ULL, .register_address = 64U, .register_address_high = 0U }, { .value = 4294967296ULL, .register_address = 78U, .register_address_high = 0U }, { .value = 196608ULL, .register_address = 71U, .register_address_high = 0U }, { .value = 0ULL, .register_address = 34U, .register_address_high = 0U } } }, { .vif_commands = { 0U, 0U, 0U, 1358954503U }, .gif_tag = { 1152921504606879750ULL, 14ULL }, .frame = { .value = 0ULL, .register_address = 76U, .register_address_high = 0U }, .state_registers = { { .value = 0ULL, .register_address = 24U, .register_address_high = 0U }, { .value = 62768919827382272ULL, .register_address = 64U, .register_address_high = 0U }, { .value = 4294967296ULL, .register_address = 78U, .register_address_high = 0U }, { .value = 196608ULL, .register_address = 71U, .register_address_high = 0U }, { .value = 0ULL, .register_address = 34U, .register_address_high = 0U } } } };
unsigned short asciiEucTable[128] = { 0, 0, 21845, 32853, 43690, 32938, 65535, 33023, 0, 0, 21845, 32853, 43690, 32938, 65535, 33023, 43690, 32938, 43690, 32938, 43690, 32938, 43690, 32938, 65535, 33023, 65535, 33023, 65535, 33023, 65535, 33023, 41377, 41386, 41417, 41460, 41456, 41459, 41461, 41415, 41418, 41419, 41462, 41436, 41380, 41437, 41381, 41407, 41904, 41905, 41906, 41907, 41908, 41909, 41910, 41911, 41912, 41913, 41383, 41384, 41443, 41441, 41444, 41385, 41463, 41921, 41922, 41923, 41924, 41925, 41926, 41927, 41928, 41929, 41930, 41931, 41932, 41933, 41934, 41935, 41936, 41937, 41938, 41939, 41940, 41941, 41942, 41943, 41944, 41945, 41946, 41422, 41455, 41423, 41392, 41394, 41414, 41953, 41954, 41955, 41956, 41957, 41958, 41959, 41960, 41961, 41962, 41963, 41964, 41965, 41966, 41967, 41968, 41969, 41970, 41971, 41972, 41973, 41974, 41975, 41976, 41977, 41978, 41424, 41411, 41425, 41409, 41377 };
struct KanjiClutEnvironment KanjiClutEnv = { { 0x00000000U, 0x00000000U, 0x00000000U, 0x5100000eU, 0x00000004U, 0x10000000U, 0x0000000eU, 0x00000000U, 0x00000000U, 0x00053c00U, 0x00000050U, 0x00000000U, 0x00000000U, 0x00000000U, 0x00000051U, 0x00000000U, 0x00000010U, 0x00000002U, 0x00000052U, 0x00000000U, 0x00000000U, 0x00000000U, 0x00000053U, 0x00000000U, 0x00008008U, 0x08000000U, 0x00000000U, 0x00000000U, 0x00000000U, 0x80555555U, 0x80aaaaaaU, 0x80ffffffU, 0x00000000U, 0x80555555U, 0x80aaaaaaU, 0x80ffffffU, 0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U, 0x80555555U, 0x80555555U, 0x80555555U, 0x80555555U } };
unsigned char fontPairGlyphCodes[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
int fontAsciiWidthTable[98] = { 5, 4, 7, 13, 12, 15, 14, 6, 6, 6, 10, 12, 5, 6, 4, 8, 11, 6, 12, 10, 12, 11, 11, 11, 11, 12, 5, 4, 13, 12, 12, 9, 19, 14, 10, 13, 14, 10, 9, 14, 13, 5, 7, 12, 10, 16, 13, 15, 11, 15, 12, 10, 12, 12, 13, 18, 14, 14, 12, 6, 8, 6, 10, 5, 8, 11, 11, 10, 11, 10, 9, 11, 10, 5, 6, 10, 4, 15, 11, 11, 11, 11, 7, 8, 8, 10, 10, 14, 10, 10, 10, 9, 5, 9, 15, 5, 0, 0 };
unsigned int nearest_5[8] = { 32769U, 268435456U, 14U, 0U, 0U, 0U, 20U, 0U };
unsigned int biliner_6[8] = { 32769U, 268435456U, 14U, 0U, 96U, 0U, 20U, 0U };
unsigned char spcode_0[32] = { 0, 1, 1, 1, 1, 1, 3, 0, 255, 0, 0, 0, 3, 1, 5, 3, 5, 6, 2, 2, 0, 255, 4, 7, 1, 1, 0, 1, 0, 0, 0, 0 };
unsigned int normal_2[16] = { 32771U, 268435456U, 14U, 0U, 0U, 0U, 20U, 0U, 458752U, 0U, 71U, 0U, 0U, 0U, 66U, 0U };

const char fontTexture0Path[16] = "font0.us";
const char fontTexture1Path[16] = "font1.us";

extern char fontCommandNameMark[];
extern char fontCommandNameHan[];
extern char fontCommandNameProp[];
extern char fontCommandNameRuby[];
extern char fontCommandNameXx[];
extern char fontCommandNameZoom[];
extern char fontCommandNameAlpha[];
extern char fontCommandNameShadow[];
extern char fontCommandNameBg[];
extern char fontCommandNameCol[];
extern char fontCommandNameLoad[];
extern char fontCommandNameSave[];
extern char fontCommandNameInit[];
extern char fontCommandNameRy[];
extern char fontCommandNameRx[];
extern char fontCommandNameAz[];
extern char fontCommandNameAy[];
extern char fontCommandNameAx[];
struct FontInlineCommand fontInlineCommands[20] = {
    { fontCommandNameAx, xglFontCmdAx }, { fontCommandNameAy, xglFontCmdAx }, { fontCommandNameAz, xglFontCmdAx }, { fontCommandNameRx, xglFontCmdRx }, { fontCommandNameRy, xglFontCmdRx },
    { fontCommandNameInit, xglFontCmdInit }, { fontCommandNameSave, xglFontCmdInit }, { fontCommandNameLoad, xglFontCmdInit }, { fontCommandNameCol, xglFontCmdCol }, { fontCommandNameBg, xglFontCmdBg },
    { fontCommandNameShadow, xglFontCmdShadow }, { fontCommandNameAlpha, xglFontCmdAlpha }, { fontCommandNameZoom, xglFontCmdZoom }, { fontCommandNameXx, xglFontCmdXx }, { fontCommandNameRuby, xglFontCmdRuby },
    { fontCommandNameProp, xglFontCmdHan }, { fontCommandNameHan, xglFontCmdHan }, { fontCommandNameMark, xglFontCmdMark }, { 0, 0 }, { 0, 0 }
};
unsigned int fontGsResetPacket[16] = {
    32771, 0x10000000u, 14, 0, 0, 0, 20, 0,
    0x70000u, 0, 71, 0, 0, 0, 66, 0
};
unsigned int fontGsAlphaTestPacket[8] = {
    32769, 0x10000000u, 14, 0, 0x7000Du, 0, 71, 0
};
unsigned int fontGsPlainTestPacket[8] = {
    32769, 0x10000000u, 14, 0, 0x70000u, 0, 71, 0
};
signed char fontAlnumGlyphBase[16] = {
    -16, 10, 37, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};
unsigned int KanjiTex0Env_8[28] = {
    0, 0, 0x11000000u, 0x51000006u, 5, 0x10000000u, 14, 0,
    0, 0, 63, 0, 0, 0x53C20u, 80, 0,
    0, 0, 81, 0, 320, 192, 82, 0,
    0, 0, 83, 0
};
unsigned int KanjiDrawEnv[36] = {
    0, 0, 0, 0x51000008u, 32775, 0x10000000u, 14, 0,
    0, 0, 63, 0, 0, 0, 66, 0,
    0, 0, 20, 0, 5, 0, 8, 0,
    0x31000000u, 0, 78, 0, 0, 0, 73, 0,
    0, 0, 74, 0
};
unsigned int zbuf_14[24] = {
    32773, 0x10000000u, 14, 0, 28672, 29184, 24, 0,
    0x1FF0000u, 0x1BF0000u, 64, 0, 0x31000000u, 0, 78, 0,
    0x70000u, 0, 71, 0, 0x80070u, 0, 76, 0
};
unsigned int test_15[8] = {
    32769, 0x10000000u, 14, 0, 0, 0, 63, 0
};
unsigned char spCodeParameterSizeTable[8] = { 2, 2, 3, 2, 2, 0, 0, 0 };
char fontCommandNameMark[8] = "mark";
char fontCommandNameHan[8] = "han";
char fontCommandNameProp[8] = "prop";
char fontCommandNameRuby[8] = "ruby";
char fontCommandNameXx[8] = "xx";
char fontCommandNameZoom[8] = "zoom";
char fontCommandNameAlpha[8] = "alpha";
char fontCommandNameShadow[8] = "shadow";
char fontCommandNameBg[8] = "bg";
char fontCommandNameCol[8] = "col";
char fontCommandNameLoad[8] = "load";
char fontCommandNameSave[8] = "save";
char fontCommandNameInit[8] = "init";
char fontCommandNameRy[8] = "ry";
char fontCommandNameRx[8] = "rx";
char fontCommandNameAz[8] = "az";
char fontCommandNameAy[8] = "ay";
char fontCommandNameAx[8] = "ax";
signed char fontOutlineDirections[4][2] = { { 0, -1 }, { -1, 0 }, { 1, 0 }, { 0, 1 } };
