#ifndef INCLUDE_MAIN_PAD_BUTTONS_H
#define INCLUDE_MAIN_PAD_BUTTONS_H

/* Button bits of the pad halfwords (PadRecord.held / pressed / repeated, standard PS2 layout). */
#define PAD_L2 0x0001
#define PAD_R2 0x0002
#define PAD_L1 0x0004
#define PAD_R1 0x0008
#define PAD_TRIANGLE 0x0010
#define PAD_CIRCLE 0x0020
#define PAD_CROSS 0x0040
#define PAD_SQUARE 0x0080
#define PAD_SELECT 0x0100
#define PAD_L3 0x0200
#define PAD_R3 0x0400
#define PAD_START 0x0800
#define PAD_UP 0x1000
#define PAD_RIGHT 0x2000
#define PAD_DOWN 0x4000
#define PAD_LEFT 0x8000

/* L2+R2+L1+R1+SELECT: the buttons held for a soft reset. */
#define PAD_SOFT_RESET_HELD (PAD_L2 | PAD_R2 | PAD_L1 | PAD_R1 | PAD_SELECT)
/* PadRecord.heldAndPressed: PAD_SOFT_RESET_HELD held with START pressed (pressed is the upper halfword). */
#define PAD_SOFT_RESET (PAD_SOFT_RESET_HELD | (PAD_START << 16))

#endif /* INCLUDE_MAIN_PAD_BUTTONS_H */
