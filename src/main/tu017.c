#include "common.h"
#include "main/tu017.h"

INCLUDE_ASM("asm/main/nonmatchings/tu017", makeGstransChain);

INCLUDE_ASM("asm/main/nonmatchings/tu017", func_001A0378);

INCLUDE_ASM("asm/main/nonmatchings/tu017", func_001A04A0);

typedef void (*VirtualMethod)(void *self);

typedef struct VirtualObject {
    VirtualMethod *vtable;
} VirtualObject;

int func_001A0688(VirtualObject **object_slot)
{
    VirtualObject *object;

    object = *object_slot;
    object->vtable[10](object);
    return 0;
}

typedef struct AllocLayout {
    unsigned char unmodeled_00[8];
    int count_a;
    int count_b;
    unsigned char unmodeled_10[8];
    int base;
    int size;
    unsigned char unmodeled_20[0x10];
} AllocLayout;

typedef struct AllocRecord {
    int handle;
    int data_start;
    unsigned char unmodeled_08[4];
    unsigned char clear_state;
    unsigned char extra_enabled;
    signed char max_value;
} AllocRecord;

/* The record's handle is the player object func_0014FD80 creates. It stays an int: typed as a
 * pointer it lands in another alias set and movieRenderBufferAlloc stores it in a different order. */
#define RECORD_PLAYER(record) ((VirtualObject *) (record)->handle)

void func_0014E8C0(int handle);
void func_0017D868(void);
void func_0017D150(void);
void func_0013A8A0(int value);
void soundSlotsReopen(void);

/* Stops the player of record (methods 7 and 5), and when the record had the extra sound block,
 * releases it and brings the sound slots back. */
int func_001A06B8(AllocRecord *record)
{
    RECORD_PLAYER(record)->vtable[7](RECORD_PLAYER(record));
    if (record->extra_enabled != 0) {
        func_0014E8C0(record->handle);
    }
    RECORD_PLAYER(record)->vtable[5](RECORD_PLAYER(record));
    if (record->extra_enabled != 0) {
        func_0017D868();
        func_0017D150();
        func_0013A8A0(-1);
        soundSlotsReopen();
    }
    return 0;
}

INCLUDE_ASM("asm/main/nonmatchings/tu017", func_001A0748);


int func_001561C8(void);
int makeGstransChain(int, int, int, int);
void func_001A0748(AllocLayout *layout);

/*
 * Bytes one render buffer needs: the layout block, the GS packet chain for count_a x count_b
 * (16 bytes per quadword), and the optional extra block. Each part is rounded up to 64 bytes.
 */
int movieRenderBufferSize(int include_extra)
{
    AllocLayout layout;
    int align_mask;
    int total;

    align_mask = -0x40;
    func_001A0748(&layout);
    total = (layout.size + 0x3F) & align_mask;
    total += ((makeGstransChain(0, 0, layout.count_a, layout.count_b) * 0x10) + 0x3F) & align_mask;
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
    record->clear_state = 0;
    record->extra_enabled = extra_enabled;
    if ((unsigned char) extra_enabled != 0) {
        func_0017D850();
    }
    align_mask = -0x40;
    func_001A0748(&layout);
    layout.base = cursor;
    cursor += (layout.size + 0x3F) & align_mask;
    record->handle = func_0014FD80(&layout);
    record->data_start = cursor;
    cursor += ((makeGstransChain(cursor, 0, layout.count_a, layout.count_b) * 0x10) + 0x3F) & align_mask;
    if (record->extra_enabled != 0) {
        int extra_size;

        extra_size = func_001561C8();
        func_0014E8A8(record->handle, cursor, extra_size);
        cursor += (func_001561C8() + 0x3F) & align_mask;
    }
    return cursor;
}

int func_00131450(void);

void func_001A0920(void)
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
void *memset(void *dest, int value, unsigned int size);
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
