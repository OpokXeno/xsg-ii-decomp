#include "common.h"
#include "main/pad.h"

/*
 * Controller record, 0x80 bytes per port (0x68 in XS1). The fields up to
 * +0x68 keep the XS1 layout: xglPadInitial clears 0x80 bytes per port and
 * writes button_map at +0x58 and axis_dead_zone at +0x60; xglPadSetRepeat
 * writes +0x48 to +0x4d. The pressed halfword at +0x2a is also read by
 * xgl_hdd.c, the direction halfwords at +0x32/+0x34 by xgl_menu.c.
 */
/* xglPadRead clears the analog group with `por $4,$0,$0; sq $4,0x30(pad)`:
 * a C store of a 128-bit zero. QuadwordZero (`sq $0`) drops the por and
 * changes the register allocation of the whole function, so the group is
 * stored through this type, the name the other main files use for it. */
typedef unsigned int XglQuadword __attribute__((mode(TI)));

typedef struct XglPadRecord {
    unsigned char packet[0x20];
    /* xglPadRead's probe state clears each state group with one store of its
     * width: mode/actuator_count, buttons, the analog group, pressure,
     * actuator and axis. */
    union {
        struct {
            unsigned char mode;
            unsigned char actuator_count;
        };
        unsigned short mode_state;
    };
    short mode_id;
    unsigned char unmodeled_24[2];
    unsigned char connection_state;
    unsigned char setup_state;
    union {
        struct {
            unsigned short buttons;
            unsigned short pressed;
            unsigned short repeated;
            unsigned short released;
        };
        /* +0x28: the four button halfwords read as one doubleword
           (script.c sceneObserver's talk-cancel test in XS1). */
        unsigned long long button_state;
    };
    union {
        struct {
            unsigned short directions;
            unsigned short direction_pressed;
            unsigned short direction_repeated;
            unsigned short direction_released;
            signed char position[4];
            short horizontal_pressure;
            short vertical_pressure;
        };
        XglQuadword analog_state;       /* +0x30 */
    };
    union {
        signed char pressure[8];
        unsigned long long pressure_state; /* +0x40 */
    };
    signed char repeat_delay;       /* +0x48 */
    signed char repeat_interval;    /* +0x49 */
    unsigned short repeat_mask;     /* +0x4a */
    signed char repeat_wait;        /* +0x4c */
    signed char repeat_count;       /* +0x4d */
    unsigned char horizontal_dead_zone; /* +0x4e */
    unsigned char vertical_dead_zone;   /* +0x4f */
    union {
        struct {
            unsigned char actuator[6];      /* +0x50 */
            unsigned char actuator_pending; /* +0x56 */
            unsigned char unmodeled_57;     /* +0x57 */
        };
        struct {
            unsigned int actuator_low;    /* actuator[0..3] */
            unsigned short actuator_high; /* actuator[4..5] */
        };
    };
    unsigned char button_map[8];    /* +0x58 */
    unsigned char axis_dead_zone[4]; /* +0x60 */
    union {
        signed char axis[4];        /* +0x64 */
        unsigned int axis_state;
    };
    unsigned char unmodeled_68[0x18];
} XglPadRecord;

/* Controller setup state machine (XglPadRecord.setup_state); a pad that is
 * disconnected restarts at PAD_SETUP_PROBE. */
#define PAD_SETUP_PROBE 0
#define PAD_SETUP_QUERY_ACTUATORS_MODE 0x40
#define PAD_SETUP_SET_MAIN_MODE 0x41
#define PAD_SETUP_WAIT_MAIN_MODE 0x42
#define PAD_SETUP_ACTUATOR_ALIGN 0x70
#define PAD_SETUP_WAIT_ALIGN 0x71
#define PAD_SETUP_PRESSURE_INFO 0x80
#define PAD_SETUP_PRESSURE_ENTER 0x81
#define PAD_SETUP_WAIT_PRESSURE 0x82
#define PAD_SETUP_READY 0xff

/* scePadGetState results and the scePadInfoMode terminal ids. */
#define PAD_STATE_STABLE 6
#define PAD_STATE_FINDCTP1 2
#define PAD_STATE_EXECCMD 5
#define PAD_TERM_ID_DIGITAL 4
#define PAD_TERM_ID_DUALSHOCK 7

/* High nibble of packet[1]: the controller type. */
#define PAD_TYPE_MASK 0xf0
#define PAD_TYPE_MOUSE 0x10
#define PAD_TYPE_NEGCON 0x20
#define PAD_TYPE_DIGITAL 0x40
#define PAD_TYPE_ANALOG_JOYSTICK 0x50
#define PAD_TYPE_DUALSHOCK 0x70
#define PAD_PACKET_ID_PRESSURE 0x79

#define PAD_PORT_COUNT 2
#define PAD_DMA_BUFFER_SIZE 0x100
#define PAD_ANALOG_CENTER 128
#define PAD_FULL_PRESSURE 255

#define PAD_REPEAT_MASK (PAD_UP | PAD_RIGHT | PAD_DOWN | PAD_LEFT | PAD_L1 | PAD_R1)
#define PAD_REPEAT_DELAY 8
#define PAD_REPEAT_INTERVAL 1
#define PAD_DEFAULT_DEAD_ZONE 64
#define PAD_AXIS_THRESHOLD 32

extern XglPadRecord PadData[2];
extern unsigned char PadDmaBuffer[2 * PAD_DMA_BUFFER_SIZE];
/* scePadSetActAlign table: small motor on byte 0, large motor on byte 1. */
extern unsigned char PadActAlign[6];

void xglPadSetRepeat(int port, unsigned short mask, int delay, int interval);
int scePadInit(int mode);
int scePadPortOpen(int port, int slot, unsigned char *dma_buffer);
int scePadGetState(int port, int slot);
int scePadInfoMode(int port, int slot, int term, int offset);
int scePadSetMainMode(int port, int slot, int mode, int lock);
int scePadGetReqState(int port, int slot);
int scePadInfoAct(int port, int slot, int actuator, int term);
int scePadSetActAlign(int port, int slot, unsigned char *alignment);
int scePadInfoPressMode(int port, int slot);
int scePadEnterPressMode(int port, int slot);
int scePadRead(int port, int slot, unsigned char *packet);
int scePadSetActDirect(int port, int slot, unsigned char *actuator);

/* Returns at once with no effect. Only caller: InitializeSystem. */
void xglDmaInitial(void) {
}

void xglPadInitial(void) {
    unsigned int byte_index;
    int map_remaining;
    int dead_zone_remaining;
    unsigned char axis_threshold;
    int button;
    unsigned char *first_record_bytes;
    unsigned char *second_record_bytes;

    scePadInit(0);
    scePadPortOpen(0, 0, PadDmaBuffer);
    scePadPortOpen(1, 0, &PadDmaBuffer[PAD_DMA_BUFFER_SIZE]);

    first_record_bytes = (unsigned char *)&PadData[0];
    second_record_bytes = (unsigned char *)&PadData[1];
    /* Keep the original paired byte walk across the two 0x68-byte records. */
    byte_index = 0;
    do {
        *first_record_bytes = 0;
        *second_record_bytes = 0;
        ++first_record_bytes;
        ++second_record_bytes;
        ++byte_index;
    } while (byte_index < sizeof(XglPadRecord));

    xglPadSetRepeat(0, PAD_REPEAT_MASK, PAD_REPEAT_DELAY, PAD_REPEAT_INTERVAL);
    xglPadSetRepeat(1, PAD_REPEAT_MASK, PAD_REPEAT_DELAY, PAD_REPEAT_INTERVAL);

    button = 1;
    map_remaining = sizeof(PadData[0].button_map) - 1;
    do {
        PadData[0].button_map[sizeof(PadData[0].button_map) - 1 - map_remaining] = button;
        PadData[1].button_map[sizeof(PadData[1].button_map) - 1 - map_remaining] = button;
        button <<= 1;
        --map_remaining;
    } while (map_remaining >= 0);

    PadData[0].horizontal_dead_zone = PAD_DEFAULT_DEAD_ZONE;
    PadData[0].vertical_dead_zone = PAD_DEFAULT_DEAD_ZONE;
    PadData[1].horizontal_dead_zone = PAD_DEFAULT_DEAD_ZONE;
    PadData[1].vertical_dead_zone = PAD_DEFAULT_DEAD_ZONE;
    axis_threshold = PAD_AXIS_THRESHOLD;
    dead_zone_remaining = sizeof(PadData[0].axis_dead_zone) - 1;
    do {
        PadData[0].axis_dead_zone[sizeof(PadData[0].axis_dead_zone) - 1 - dead_zone_remaining] = axis_threshold;
        PadData[1].axis_dead_zone[sizeof(PadData[1].axis_dead_zone) - 1 - dead_zone_remaining] = axis_threshold;
        --dead_zone_remaining;
    } while (dead_zone_remaining >= 0);
}

void xglPadSetRepeat(int port, unsigned short mask, int delay, int interval)
{
    unsigned short button_mask = mask;
    XglPadRecord *pad;

    if (port == -1) {
        xglPadSetRepeat(0, button_mask, delay, interval);
        xglPadSetRepeat(1, button_mask, delay, interval);
        return;
    }

    pad = &PadData[port];
    pad->repeat_delay = delay;
    pad->repeat_interval = interval;
    pad->repeat_mask = button_mask;
    pad->repeat_wait = 0;
    pad->repeat_count = 0;
}

void xglPadRead(void)
{
    int port;
    unsigned char button_bits;
    unsigned char button_down;
    unsigned char direction_down;
    unsigned char direction_bits;
    int button_index;
    signed char vertical;
    signed char horizontal;

    for (port = 0; port < PAD_PORT_COUNT; ++port) {
        XglPadRecord *pad;
        int buttons;
        int directions;
        /* The 0x60 button word and the analog left-stick dead zone share one
           local: the original keeps both in $18, and the button word must
           still be in $18 when the direction bits are merged below. With two
           locals the 0x60 word lands in $7, an extra move is scheduled into
           the branch delay slot and the dead-zone tests are allocated
           differently. */
        unsigned short button_word_or_dead_zone;
        /* The request-state reply of the mode setup and the repeated-button
           word share one local as well. With two locals the repeat_mask load
           at the end moves from $8 to $10 (three instructions differ). */
        int request_state_or_repeated;

        pad = &PadData[port];
        buttons = 0;
        pad->connection_state = scePadGetState(port, 0);
        if (!pad->connection_state) {
            pad->setup_state = PAD_SETUP_PROBE;
        }
        switch (pad->setup_state) {
        case PAD_SETUP_PROBE: {
            int mode;

            pad->mode_state = 0;
            pad->button_state = 0;
            pad->analog_state = 0;
            pad->pressure_state = 0;
            pad->actuator_low = 0;
            pad->actuator_high = 0;
            pad->axis_state = 0;
            if (pad->connection_state == PAD_STATE_STABLE || pad->connection_state == PAD_STATE_FINDCTP1) {
                mode = scePadInfoMode(port, 0, 1, 0);
                if (mode) {
                    pad->mode_id = scePadInfoMode(port, 0, 2, 0);
                    if (mode == PAD_TERM_ID_DIGITAL) {
                        pad->setup_state = PAD_SETUP_QUERY_ACTUATORS_MODE;
                    } else if (mode == PAD_TERM_ID_DUALSHOCK) {
                        pad->setup_state = PAD_SETUP_ACTUATOR_ALIGN;
                    } else {
                        pad->setup_state = PAD_SETUP_READY;
                    }
                }
            }
            break;
        }
        case PAD_SETUP_QUERY_ACTUATORS_MODE:
            if (scePadInfoMode(port, 0, PAD_TERM_ID_DIGITAL, -1)) {
                pad->setup_state = PAD_SETUP_SET_MAIN_MODE;
            } else {
                pad->setup_state = PAD_SETUP_READY;
            }
            break;
        case PAD_SETUP_SET_MAIN_MODE:
            if (scePadSetMainMode(port, 0, 1, 3) == 1) {
                pad->setup_state = PAD_SETUP_WAIT_MAIN_MODE;
            }
            break;
        case PAD_SETUP_WAIT_MAIN_MODE:
            request_state_or_repeated = scePadGetReqState(port, 0);
            switch (request_state_or_repeated) {
            case 0:
                pad->setup_state = PAD_SETUP_PROBE;
                break;
            case 1:
                pad->setup_state = PAD_SETUP_SET_MAIN_MODE;
                break;
            }
            break;
        case PAD_SETUP_ACTUATOR_ALIGN:
            pad->actuator_count = scePadInfoAct(port, 0, -1, 0);
            if (pad->actuator_count) {
                if (scePadSetActAlign(port, 0, PadActAlign)) {
                    pad->setup_state = PAD_SETUP_WAIT_ALIGN;
                }
            } else {
                pad->setup_state = PAD_SETUP_READY;
            }
            break;
        case PAD_SETUP_WAIT_ALIGN:
            if (pad->connection_state != PAD_STATE_EXECCMD) {
                pad->setup_state = PAD_SETUP_PRESSURE_INFO;
            }
            break;
        case PAD_SETUP_PRESSURE_INFO:
            if (scePadInfoPressMode(port, 0) != 1) {
                pad->setup_state = PAD_SETUP_READY;
            } else {
                pad->setup_state = PAD_SETUP_PRESSURE_ENTER;
            }
            break;
        case PAD_SETUP_PRESSURE_ENTER:
            if (scePadEnterPressMode(port, 0) == 1) {
                pad->setup_state = PAD_SETUP_WAIT_PRESSURE;
            }
            break;
        case PAD_SETUP_WAIT_PRESSURE:
            request_state_or_repeated = scePadGetReqState(port, 0);
            switch (request_state_or_repeated) {
            case 0:
                pad->setup_state = PAD_SETUP_READY;
                break;
            case 1:
                pad->setup_state = PAD_SETUP_PRESSURE_ENTER;
                break;
            }
            break;
        case PAD_SETUP_READY: {
            scePadRead(port, 0, pad->packet);
            pad->mode = pad->packet[1];
            if (pad->actuator_count) {
                if (pad->actuator_pending) {
                    pad->actuator[0] = 0;
                    pad->actuator[1] = 0;
                    pad->actuator[2] = 0;
                    pad->actuator[3] = 0;
                    pad->actuator[4] = 0;
                    pad->actuator[5] = 0;
                }
                scePadSetActDirect(port, 0, pad->actuator);
                pad->actuator[0] = 0;
                pad->actuator[1] = 0;
                pad->actuator[2] = 0;
                pad->actuator[3] = 0;
                pad->actuator[4] = 0;
                pad->actuator[5] = 0;
            }
            if (pad->packet[0] == 0) {
            switch (pad->packet[1] & PAD_TYPE_MASK) {
            case PAD_TYPE_MOUSE: {
                int stick_x;
                int stick_y;

                stick_x = pad->packet[4];
                stick_y = pad->packet[5];
                pad->position[0] = pad->position[0] + stick_x;
                pad->position[1] = pad->position[1] + stick_y;
                pad->position[2] = stick_x;
                pad->position[3] = stick_y;
                buttons = (pad->packet[3] << 3) ^ 0xfff8;
                break;
            }
            case PAD_TYPE_NEGCON: {
                int stick_x;
                int stick_y;
                int right_x;
                int right_y;

                buttons = ~(pad->packet[3] + (pad->packet[2] << 8)) & 0xffff;
                stick_x = pad->packet[4];
                right_x = pad->packet[6];
                pad->position[0] = stick_x;
                stick_y = pad->packet[5];
                right_y = pad->packet[7];
                pad->position[1] = stick_y;
                pad->position[2] = right_x;
                pad->position[3] = right_y;
                break;
            }
            case 0x60: {
                button_word_or_dead_zone = ~(pad->packet[3] + (pad->packet[2] << 8));
                pad->position[0] = pad->packet[4] + pad->position[2];
                pad->position[1] = pad->packet[6] + pad->position[3];
                if ((button_word_or_dead_zone & 0x6008) == 0x6008) {
                    pad->position[2] = -96 - pad->position[0];
                    pad->position[3] = 120 - pad->position[1];
                }
                buttons = button_word_or_dead_zone;
                break;
            }
            case 0xe0:
                pad->position[0] = pad->packet[4];
                pad->position[1] = pad->packet[6];
                buttons = ~(pad->packet[3] + (pad->packet[2] << 8)) & 0xffff;
                break;
            case 0x30:
                buttons = ~(pad->packet[3] + (pad->packet[2] << 8)) & 0xffff;
                break;
            case PAD_TYPE_DIGITAL:
                buttons = ~(pad->packet[3] + (pad->packet[2] << 8)) & 0xffff;
                break;
            case PAD_TYPE_ANALOG_JOYSTICK:
            case PAD_TYPE_DUALSHOCK: {
                unsigned char first_vertical_position;
                unsigned char second_axis_threshold;
                unsigned char third_axis_threshold;
                unsigned char fourth_axis_threshold;
                int axis;

                button_word_or_dead_zone = pad->axis_dead_zone[0];
                axis = pad->packet[4] + PAD_ANALOG_CENTER;
                pad->position[1] = pad->packet[5] + PAD_ANALOG_CENTER;
                pad->position[2] = pad->packet[6] + PAD_ANALOG_CENTER;
                pad->position[3] = pad->packet[7] + PAD_ANALOG_CENTER;
                pad->horizontal_pressure = pad->packet[8] - pad->packet[9];
                pad->vertical_pressure = pad->packet[11] - pad->packet[10];
                pad->pressure[0] = pad->packet[12];
                pad->pressure[1] = pad->packet[13];
                pad->pressure[2] = pad->packet[14];
                pad->pressure[3] = pad->packet[15];
                pad->pressure[4] = pad->packet[16];
                pad->pressure[5] = pad->packet[17];
                pad->pressure[6] = pad->packet[18];
                pad->pressure[7] = pad->packet[19];
                pad->position[0] = axis;
                pad->axis[0] = axis;
                if (-button_word_or_dead_zone < (signed char)axis &&
                    (signed char)axis < button_word_or_dead_zone)
                    pad->axis[0] = 0;
                first_vertical_position = pad->position[1];
                second_axis_threshold = pad->axis_dead_zone[1];
                pad->axis[1] = first_vertical_position;
                if (-second_axis_threshold < (signed char)first_vertical_position &&
                    (signed char)first_vertical_position < second_axis_threshold)
                    pad->axis[1] = 0;
                third_axis_threshold = pad->axis_dead_zone[2];
                pad->axis[2] = pad->position[2];
                if (-third_axis_threshold < pad->position[2] &&
                    pad->position[2] < third_axis_threshold)
                    pad->axis[2] = 0;
                fourth_axis_threshold = pad->axis_dead_zone[3];
                pad->axis[3] = pad->position[3];
                if (-fourth_axis_threshold < pad->position[3] &&
                    pad->position[3] < fourth_axis_threshold)
                    pad->axis[3] = 0;
                buttons = ~(pad->packet[3] + (pad->packet[2] << 8)) & 0xffff;
                break;
            }
            case 0x80:
                continue;
            }
            }
            break;
        }
        }
        if (pad->packet[1] != PAD_PACKET_ID_PRESSURE) {
            /* No pressure bytes: derive full or zero pressure from the
               digital bits. The packet bits are active-low, so each
               *_released flag is nonzero while its button is up. */
            int maximum_pressure;
            int digital_horizontal;
            int digital_vertical;
            int triangle_released;
            int circle_released;
            int cross_released;
            int square_released;
            int l1_released;
            int r1_released;
            int l2_released;
            int r2_released;
            int first_buttons;
            int up_released;
            int right_released;
            int down_released;
            int left_released;
            int second_buttons;

            maximum_pressure = PAD_FULL_PRESSURE;
            first_buttons = pad->packet[2];
            second_buttons = pad->packet[3];
            right_released = first_buttons & 0x20;
            down_released = first_buttons & 0x40;
            left_released = first_buttons & 0x80;
            up_released = first_buttons & 0x10;
            digital_horizontal = maximum_pressure;
            digital_vertical = maximum_pressure;
            if (right_released) {
                digital_horizontal -= maximum_pressure;
            }
            if (!left_released) {
                digital_horizontal -= maximum_pressure;
            }
            r2_released = second_buttons & 2;
            triangle_released = second_buttons & 0x10;
            circle_released = second_buttons & 0x20;
            cross_released = second_buttons & 0x40;
            square_released = second_buttons & 0x80;
            l1_released = second_buttons & 4;
            r1_released = second_buttons & 8;
            l2_released = second_buttons & 1;
            triangle_released = triangle_released != 0;
            circle_released = circle_released != 0;
            cross_released = cross_released != 0;
            square_released = square_released != 0;
            l1_released = l1_released != 0;
            r1_released = r1_released != 0;
            r2_released = r2_released != 0;
            if (down_released) {
                digital_vertical -= maximum_pressure;
            }
            if (!up_released) {
                digital_vertical -= maximum_pressure;
            }
            pad->horizontal_pressure = digital_horizontal;
            pad->vertical_pressure = digital_vertical;
            pad->pressure[0] = triangle_released - 1;
            pad->pressure[1] = circle_released - 1;
            pad->pressure[2] = cross_released - 1;
            pad->pressure[3] = square_released - 1;
            pad->pressure[4] = l1_released - 1;
            pad->pressure[5] = r1_released - 1;
            pad->pressure[6] = l2_released - 1;
            pad->pressure[7] = r2_released - 1;
        }
        directions = buttons;
        horizontal = pad->position[2];
        if (horizontal < -pad->horizontal_dead_zone) {
            directions = (buttons | PAD_LEFT) & 0xffff;
        }
        if (horizontal > pad->horizontal_dead_zone) {
            directions = (directions | PAD_RIGHT) & 0xffff;
        }
        vertical = pad->position[3];
        if (vertical < -pad->vertical_dead_zone) {
            directions = (directions | PAD_UP) & 0xffff;
        }
        if (vertical > pad->vertical_dead_zone) {
            directions = (directions | PAD_DOWN) & 0xffff;
        }
        button_bits = buttons & 0xff;
        buttons &= 0xff00;
        direction_bits = directions & 0xff;
        directions &= 0xff00;
        for (button_index = 0; button_index < (int)sizeof(pad->button_map); ++button_index) {
            button_down = button_bits & 1;
            if (button_down) {
                buttons = (buttons | pad->button_map[button_index]) & 0xffff;
            }
            button_bits >>= 1;
            direction_down = direction_bits & 1;
            if (direction_down) {
                directions = (directions | pad->button_map[button_index]) & 0xffff;
            }
            direction_bits >>= 1;
        }
        {
            int changed = buttons ^ pad->buttons;
            int direction_changed = directions ^ pad->directions;
            int pressed = buttons & changed;
            int direction_pressed = directions & direction_changed;

            pad->released = changed & ~buttons;
            pad->repeated = pad->pressed = pressed;
            pad->buttons = buttons;
            pad->direction_released = direction_changed & ~directions;
            pad->direction_repeated = pad->direction_pressed = direction_pressed;
            pad->directions = directions;
        }
        {
            int repeat = directions & pad->repeat_mask;
            unsigned char repeat_count;

            if (repeat && pad->repeat_wait > pad->repeat_delay) {
                repeat_count = pad->repeat_count;
                if (pad->repeat_count > pad->repeat_interval) {
                    pad->direction_repeated = repeat;
                    request_state_or_repeated = buttons & pad->repeat_mask;
                    pad->repeated = request_state_or_repeated;
                    repeat_count = 0;
                    pad->repeat_count = 0;
                }
                pad->repeat_count = repeat_count + 1;
            } else if (repeat) {
                ++pad->repeat_wait;
            } else {
                pad->repeat_wait = 0;
            }
        }
    }
}
