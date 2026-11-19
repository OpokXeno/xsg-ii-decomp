/*
 * OV01 original TU 54: 0x00aa4bd8..0x00aa5248 (8 functions)
 */
#include "common.h"
#include "ov01/tu048.h"
#include "ov01/tu054.h"
#include "ov01/tu063.h"
#include "ov01/tu005.h"
#include "ov01/tu012.h"

/* Arrow state cycle length: arrowCycleUpdate wraps the cycle counter at this value. */
#define ARROW_CYCLE_LENGTH 0x11
/* Popup number cells: the row height (the glyph height cap), the growth per update and the frames to live. */
#define MENU_POP_ROW_HEIGHT 27
#define MENU_POP_CELL_GROWTH 4
#define MENU_POP_LIFETIME 25

/* The cycle counter at +0x94 of the object: arrowCycleUpdate advances it by one
 * and resets it to zero once it reaches 0x11. */
struct ArrowObject {
    unsigned char unmodeled_00[0x04];
    void (*update)(struct ArrowObject *object);
    void (*draw)(struct ArrowObject *object);
    unsigned char unmodeled_0c[0x04];
    float x;
    float y;
    int state;
    unsigned char unmodeled_1c[0x74];
    int counter;
    int cycle;
    unsigned int direction;
};

/* The direction word is stored through this union (alias set 0), so the store is
 * not reordered against the float stores of x and y. */
typedef union {
    unsigned int direction;
} ArrowValueWord;

void *objSlotAlloc(void);
void arrowCycleUpdate(struct ArrowObject *object);
/* Arrow draw (callback +8): the sprite sits at x plus the cycle, and y; the direction flag picks the texel order. */
void arrowDraw(struct ArrowObject *object);
extern int arrowObj;

void arrowCreate(int x, int y, int direction) {
    struct ArrowObject *object;
    float floatX;
    float floatY;
    unsigned int directionWord;

    directionWord = direction;

    object = objSlotAlloc();
    if (object != 0) {
        object->update = arrowCycleUpdate;
        object->draw = arrowDraw;
        arrowObj = (int) object;
        ov01MatrixBlockInit(&object->x);
        floatX = x;
        floatY = y;
        object->x = floatX;
        ((ArrowValueWord *) &object->direction)->direction = directionWord;
        object->y = floatY;
        do {
            object->state = 0;
            object->cycle = 0;
            object->counter = 0;
        } while (0);
    }
}

/* Arrow update (callback +4): advances the cycle and wraps it at 0x11. */
void arrowCycleUpdate(struct ArrowObject *object) {
    int next;

    next = object->cycle + 1;
    if (next >= ARROW_CYCLE_LENGTH) {
        next = 0;
    }
    object->cycle = next;
}

void menuTexFlush(void);
void menuStatSprite2(GrPacket *packet, int x, int y, int width, int height, int srcLeft, int srcTop, int srcRight, int srcBottom);

/* Arrow draw (callback +8): the sprite sits at x plus the cycle, and y; the direction flag picks the texel order. */
void arrowDraw(struct ArrowObject *object) {
    GrPacket packet;
    int screenX;
    int screenY;
    int srcLeft;
    int srcRight;
    int srcBottom;

    menuTexFlush();
    grGpInit(&packet);
    func_00AA8530(&packet);
    screenX = object->x + object->cycle;
    screenY = object->y;
    if (object->direction == 0) {
        srcLeft = 464;
        srcRight = 488;
        srcBottom = 234;
    } else {
        srcLeft = 487;
        srcRight = 464;
        srcBottom = 234;
    }
    grPutSprOpen(&packet);
    menuStatSprite2(&packet, screenX, screenY, 24, 20, srcLeft, 214, srcRight, srcBottom);
    grPutSprClose();
    grPacketSend(&packet);
}

void objSlotRelease(int);

void arrowRemove(void) {
    if (arrowObj != 0) {
        objSlotRelease(arrowObj);
        arrowObj = 0;
    }
}

/* The object menuPopNumCreate makes: the transformed position (x, y) in its matrix
 * block, then a countdown table of two rows of four bytes at +0xa0 that
 * menuPopNumUpdate steps on each update. The last cell (+0xa7) is the lifetime. */
struct PopNumObject {
    unsigned char unmodeled_00[0x04];
    void (*update)(struct PopNumObject *object);
    void (*draw)(struct PopNumObject *object);
    unsigned char unmodeled_0c[0x04];
    float x;
    float y;
    unsigned char unmodeled_18[0x78];
    int counter;
    Ov01Object *owner;
    int amount;
    int kind;
    unsigned char cells[2][4];
};

int sefActorMulLocalVector(void *actor, int input, void *output, int apply);
void menuPopNumUpdate(struct PopNumObject *object);
void menuPopNumDraw(int objectWord);

void menuPopNumCreate(Ov01Object *owner, int amount, int kind, int scale) {
    struct PopNumObject *object;
    int cellValue;
    int row;
    int column;
    float step;

    object = objSlotAlloc();
    if (object != 0) {
        object->update = menuPopNumUpdate;
        object->draw = (void (*)(struct PopNumObject *)) menuPopNumDraw;
        ov01MatrixBlockInit(&object->x);
        sefActorMulLocalVector(owner->unit, 31, &object->x, 1);
        step = scale / 100.0f;
        if (owner->stats->state >= 4) {
            step = step * 3.0f;
        }
        object->y = object->y + step;
        object->owner = owner;
        object->amount = amount;
        object->kind = kind;
        object->counter = 0;
        cellValue = 0;
        for (row = 4; row < 6; row++) {
            for (column = 0; column < 4 && (row != 5 || column != 3); column++) {
                object->cells[row - 4][column] = cellValue;
                cellValue -= 8;
            }
        }
        object->cells[1][3] = MENU_POP_LIFETIME;
    }
}

void menuPopNumUpdate(struct PopNumObject *object) {
    int row;
    int column;
    int rowOffset;
    unsigned char *cell;
    int lifetime;

    row = 4;
    rowOffset = 16;
    while (row < 6) {
        for (column = 0; column < 4 && (row != 5 || column != 3); column++) {
            if ((signed char) object->cells[row - 4][column] < MENU_POP_ROW_HEIGHT) {
                object->cells[row - 4][column] += MENU_POP_CELL_GROWTH;
            } else {
                object->cells[row - 4][column] = MENU_POP_ROW_HEIGHT;
            }
        }
        rowOffset += 4;
        row++;
    }
    lifetime = object->cells[1][3];
    object->cells[1][3] = lifetime - 1;
    if ((signed char) lifetime <= 0) {
        objSlotRelease((int) object);
    }
}

int grBBCalc(int, int *, int *);

/* The draw view of the object menuPopNumCreate makes, as menuPopNumDrawDigits reads it:
 * kind is an unsigned byte index and the cells are signed bytes there. */
struct PopNumDrawObject {
    unsigned char unmodeled_00[0x98];
    int amount;
    unsigned char kind;
    unsigned char unmodeled_9d[0x03];
    signed char cells[2][4];
};

void menuPopNumDrawDigits(GrPacket *packet, int x, int y, struct PopNumDrawObject *object);

/* Draw callback of the object made by menuPopNumCreate (its +0x08 slot): grBBCalc
 * projects the object's origin (+0x10) to the screen, and when it is on screen
 * menuPopNumDrawDigits draws the entry there. The object is passed as an int here. */
void menuPopNumDraw(int objectWord) {
    GrPacket packet;
    int screenX;
    int screenY;

    if (grBBCalc(objectWord + 0x10, &screenX, &screenY) != 0) {
        menuTexFlush();
        grGpInit(&packet);
        menuNumStyle1TexState(&packet);
        grPutSprOpen(&packet);
        menuPopNumDrawDigits(&packet, screenX, screenY, (struct PopNumDrawObject *) objectWord);
        grPutSprClose();
        grPacketSend(&packet);
    }
}

typedef struct MenuRGBA {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} MenuRGBA;

struct MenuStyle {
    int advance[10];
    int srcLeft[10];
};

extern MenuRGBA menuPopNumColors[];
extern int menuPopNumColorIndex[];
void *menuStyleEntry(int index);
void menuStatSprite2RGB(GrPacket *packet, int x, int y, int width, int height, int srcLeft, int srcTop, int srcRight, int srcBottom, MenuRGBA *colour);

/* Draws the object's value (amount) in decimal, most significant place first.
 * Leading places are skipped (at least one digit is drawn). Each digit's glyph
 * advance comes from the style table, its height from the object's cell row,
 * and the colour from kind through menuPopNumColorIndex. */
void menuPopNumDrawDigits(GrPacket *packet, int x, int y, struct PopNumDrawObject *object) {
    struct MenuStyle *style;
    MenuRGBA *colour;
    int colourIndex;
    int remaining;
    int place;
    int divisor;
    int digits;
    int digit;
    int glyphWidth;
    int cellHeight;
    int srcLeft;
    int cellGap;
    int centreOffset;
    int rowHeight;

    rowHeight = MENU_POP_ROW_HEIGHT;
    divisor = 100000;
    colourIndex = menuPopNumColorIndex[object->kind];
    remaining = object->amount;
    digits = 0;
    place = 1;
    do {
        if (remaining >= place) {
            digits++;
        }
        place = place * 10;
    } while (!(divisor < place));
    if (digits == 0) {
        digits = 1;
    }
    x -= digits * 6;
    style = menuStyleEntry(1);
    colour = &menuPopNumColors[colourIndex];
    for (place = 6; place > 0; place--) {
        digit = remaining / divisor;
        remaining = remaining % divisor;
        divisor = divisor / 10;
        if (digits >= place) {
            glyphWidth = style->advance[digit];
            if (place >= 5) {
                cellHeight = object->cells[1][(place - 1) % 4];
            } else {
                cellHeight = object->cells[0][place - 1];
            }
            if (cellHeight > 0) {
                srcLeft = style->srcLeft[digit];
                cellGap = rowHeight - cellHeight;
                centreOffset = rowHeight / 2 - cellHeight / 2;
                menuStatSprite2RGB(packet, x, y + (centreOffset - cellGap), glyphWidth, cellHeight, srcLeft, 23, srcLeft + glyphWidth, 50, colour);
            }
            x += glyphWidth;
        }
    }
}

/* Colours of the pop-up numbers (r, g, b, a), picked through menuPopNumColorIndex. */
MenuRGBA menuPopNumColors[6] = {
    { 0x80, 0x80, 0x80, 0x80 },
    { 0x60, 0xff, 0x60, 0x80 },
    { 0xff, 0x60, 0xff, 0x80 },
    { 0x60, 0xff, 0xff, 0x80 },
    { 0xf0, 0xfd, 0x95, 0x80 },
    { 0, 0, 0, 0 }
};

/* Colour index of each pop-up kind (kind 0..11) into menuPopNumColors. */
int menuPopNumColorIndex[12] = { 0, 0, 1, 2, 3, 0, 0, 4, 4, 2, 2, 0 };
