#include "common.h"
#include "shared.h"
#include "main/xgl_timer0.h"

extern u8 *WorkEnd;


/*
 * EE Timer 0.  T0_COUNT is the free-running counter and T0_MODE its control
 * word; bit 7 (0x80) of T0_MODE is CUE, which starts the count.
 */
#define T0_COUNT ((volatile u32 *)0x10000000)

#define T0_COUNT_MASK 0xffff

/*
 * ARX stream layout, in words: magic "ARX\0", decoded size in bytes, packed
 * payload size in bytes, flags (must be zero), 30 table entries, then the
 * bit stream.  The table is copied to the 64-byte aligned address after
 * WorkEnd; the 32 words that follow it receive the unread tail of the stream
 * when the output cursor catches up with the read pointer.
 */
#define ARX_MAGIC 0x585241

#define ARX_MAGIC_WORD 0
#define ARX_SIZE_WORD 1
#define ARX_PAYLOAD_SIZE_WORD 2
#define ARX_FLAGS_WORD 3
#define ARX_TABLE_WORD 4
#define ARX_TABLE_COUNT 30
#define ARX_TABLE_SLOTS 32
#define ARX_DATA_WORD 34
#define ARX_TABLE_ALIGN 64

/*
 * One output word: a clear flag bit takes the next stream word as is; a set
 * flag bit is followed by a unary length prefix that selects a table entry
 * together with `offsetBits` further bits.  `unitStep` is always 1; each prefix
 * bit adds it shifted by the current `offsetBits`.
 */
#define ARX_DECODE_WORD(word) \
    do { \
        if (getbit1(&reader) == 0) { \
            word = *reader.data++; \
        } else { \
            offsetBits = 1; \
            tableIndex = 0; \
            unitStep = 1; \
            while (getbit1(&reader) != 0) { \
                tableIndex += unitStep << offsetBits; \
                offsetBits++; \
            } \
            word = table[tableIndex + getbits(&reader, offsetBits)]; \
        } \
    } while (0)

typedef struct ArxBitReader {
    u32 value;
    u32 bit_count;
    u32 *data;
} ArxBitReader;

int xglTimer0Get(void)
{
    return *T0_COUNT & T0_COUNT_MASK;
}

/* The two-letter codes "en", "jp", "en", "de", "fr", "it", "es", "zh" and "ko",
 * each read as one halfword (first letter in the low byte). */
typedef struct XglLanguageCodes {
    unsigned short code[9];
} XglLanguageCodes;

extern const XglLanguageCodes xglLanguageCodeTable;
/* Unsized: the original addresses it absolutely, not through the gp. */
extern unsigned char xglLanguageIndex[];

/* Copies the nine two-letter language codes in xglLanguageCodeTable to the stack and
 * returns the one selected by xglLanguageIndex; a non-null buffer receives its two bytes. */
unsigned short xglGetLanguageCode(char *buffer)
{
    XglLanguageCodes codes = xglLanguageCodeTable;
    unsigned short code = codes.code[xglLanguageIndex[0]];

    if (buffer != 0) {
        buffer[0] = code;
        buffer[1] = code >> 8;
    }
    return code;
}

static u32 getbit1(ArxBitReader *reader)
{
    u32 bit;

    if (reader->bit_count == 0) {
        reader->value = *reader->data++;
    }
    reader->bit_count = (reader->bit_count - 1) & 0x1f;
    bit = reader->value >> 31;
    reader->value <<= 1;
    return bit;
}

static u32 getbits(ArxBitReader *reader, u32 count)
{
    u32 highBits = 0;
    u32 bitsLeft = reader->bit_count;
    u32 result;

    if (bitsLeft < count) {
        highBits = reader->value >> (32 - bitsLeft);
        reader->value = *reader->data++;
        count -= bitsLeft;
        reader->bit_count = 32;
        bitsLeft = 32;
    }
    result = highBits << count | reader->value >> -count;
    reader->bit_count = bitsLeft - count;
    reader->value <<= count;
    return result;
}

int xglArxExtract(u32 *dest, u32 *src)
{
    ArxBitReader reader;
    u32 *table;
    u32 *cursor = dest;
    int size;
    int payloadSize;
    int i;
    u32 offsetBits;
    u32 tableIndex;
    u32 word;
    u32 unitStep;

    if (src[ARX_MAGIC_WORD] != ARX_MAGIC || src[ARX_FLAGS_WORD] != 0) {
        return -1;
    }
    size = src[ARX_SIZE_WORD];
    payloadSize = src[ARX_PAYLOAD_SIZE_WORD];
    table = (u32 *)(((u32)WorkEnd + ARX_TABLE_ALIGN - 1) & ~(ARX_TABLE_ALIGN - 1));
    for (i = 0; i < ARX_TABLE_COUNT; i++) {
        table[i] = src[ARX_TABLE_WORD + i];
    }
    reader.value = 0;
    reader.bit_count = 0;
    reader.data = src + ARX_DATA_WORD;
    if (!((reader.data < dest || dest + size / 4 < reader.data) &&
          (reader.data + payloadSize / 4 < dest ||
           dest + size / 4 < reader.data + payloadSize / 4))) {
        if (cursor < (u32 *)((u8 *)dest + size)) {
            u32 *payloadEnd = src + payloadSize / 4;

            while (1) {
                ARX_DECODE_WORD(word);
                *cursor++ = word;
                if (cursor >= reader.data) {
                    u32 *tailSrc = reader.data;
                    u32 *tailDst = table + ARX_TABLE_SLOTS;
                    u32 *tailEnd = payloadEnd + ARX_DATA_WORD;

                    reader.data = tailDst;
                    while (tailSrc <= tailEnd) {
                        *tailDst++ = *tailSrc++;
                    }
                    break;
                }
                if (cursor >= (u32 *)((u8 *)dest + size)) {
                    break;
                }
            }
        }
    }
    while (cursor < (u32 *)((u8 *)dest + size)) {
        ARX_DECODE_WORD(word);
        *cursor++ = word;
    }
    return size;
}
