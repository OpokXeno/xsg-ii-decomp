#ifndef INCLUDE_OV01_M_GS_H
#define INCLUDE_OV01_M_GS_H

#include "shared.h"
#include "main/m_se_obj.h"

/* Functions of src/ov01/m_gs.c that other files call. */

void ov01SceneSystemsShutdown(void);
void ov01SceneStateReset(void);
void ov01EffectPauseSet(s16 value);
s16 ov01EffectPauseGet(void);
int ov01TickCountGet(void);

/* Effect constructors are called as int (*)(): each takes its own host view of the MSeObj. */
typedef int (*MSeFireCallback)();

/* The effect constructors MSeFireCallbacks names. Each reads the new MSeObj (main/m_se_obj.h) as its own host struct (the header
 * above, then its block at +0x20), defined in the effect file of the constructor. */
struct MEfStepObject;
struct MEfPathObject;
struct TrailHost;
struct FlagWindowHost;
struct MEfEffectObject;
struct MEfCounterObject;
struct SprayHost;
struct MEfSwayObject;
struct MEfThunder25Effect;
struct MEfThunder31Effect;
struct WaveHost;
struct MEfActorPathObject;
struct MEfHistoryObject;

int MEfCreateWait(struct MEfStepObject *object);
int MEfCreateHermitePath(struct MEfStepObject *object);
int MEfCreateSpiral(struct MEfCounterObject *object);
int MEfCreateThunder25(struct MEfThunder25Effect *object);
int MEfCreateThunder31(struct MEfThunder31Effect *object);
int MEfCreateTepodon(struct TrailHost *object);
int MEfCreateFlagWindow(struct FlagWindowHost *object);
int MEfCreateWaveEffect(struct WaveHost *host);
int MEfCreateSpray(struct SprayHost *host);
int MEfCreatePathStep(struct MEfPathObject *object);
int MEfCreateSway(struct MEfSwayObject *object);
int MEfCreateMissileTrail(struct MEfEffectObject *object);
int MEfCreateActorPath(struct MEfActorPathObject *object);
int MEfCreateActorHistory(struct MEfHistoryObject *object);

void MSeFireEntryClear(MSeFireEntry *entry);
void MSeFire(MSeFireEntry *entry);

/* A 128-bit GS quadword. */
typedef unsigned int Quadword __attribute__((mode(TI)));

/* One quadword of a GIF packet: a GIF tag, an A+D register write or a packed
 * register value. */
typedef union MEfGsQword {
    Quadword whole;
    unsigned short nloop; /* low halfword of a GIF tag: NLOOP and EOP */
    int words[4];
    struct {
        u64 tag;
        u64 registers;
    } gif;
    struct {
        u64 data;
        unsigned char address;
    } ad;
} MEfGsQword;

/* The GS direct-transfer packet MGsGPInit prepares and MGsGPTerm flushes.
 * By default it targets the scratchpad with a fixed capacity; a caller that
 * supplies its own buffer address also supplies that buffer's capacity.
 *
 *   data      the transfer base address: MGsGPInit's default/custom value,
 *             and the address MGsGPTerm passes to sceVif1PkAddDirectDataN.
 *   capacity  the buffer's byte capacity, set by MGsGPInit; no function in
 *             m_gs.c reads it back.
 *   current   the open GIF tag; reset to data by MGsGPInit and MGsGPTerm.
 *   count     quadwords written, passed to sceVif1PkAddDirectDataN and reset
 *             to zero after each flush.
 *   regCount  registers per loop of the open tag (m_ef_path_debug.c writes it); MGsGPInit
 *             and MGsGPTerm reset it to zero.
 */
typedef struct MGsPacket {
    void *data;               /* +0x00 */
    int capacity;             /* +0x04 */
    MEfGsQword *current;      /* +0x08 */
    int count;                /* +0x0C */
    int regCount;             /* +0x10 */
} MGsPacket;

/* The camera parameter block MEfCamParams (MEfCameraParams, at 0x0061B2C0 in main) comes from main/m_se_obj.h. */

/* GS packet constants shared by the effect files (one name per value). */
#define MEF_GIF_TAG_EOP 0x8000
#define MEF_GS_SETUP_TAG 0x1000000000008000
#define MEF_GS_SETUP_REGS 0xE
#define MEF_GS_STRIP_PRIM 0x4C
#define MEF_GS_STRIP_REGS 0x5151
#define MEF_GS_RGBAQ_XYZ2_REGS 0x51
#define MEF_GS_XYZ_ADC 0x8000
#define MEF_GS_TEST_1 0x47
#define MEF_GS_TEST_1_ZGEQUAL 0x5000C
#define MEF_GS_TEST_1_ATE_ATST_GREATER_ZGEQUAL 0x5000D
#define MEF_GS_ZBUF_1 0x4E
#define MEF_GS_ZBUF_1_DEPTH_WRITE 0x31000000
#define MEF_GS_ZBUF_1_NO_DEPTH_WRITE 0x131000000
#define MEF_GS_ALPHA_1 0x42
#define MEF_GS_ALPHA_1_ADDITIVE 0x48
#define MEF_GS_ALPHA_1_ADDITIVE_FIX 0x8000000048
#define MEF_GS_LINE_TAG 0x3004C00000008000
#define MEF_GS_LINE_REGS 0x551
#define MEF_GS_TRIANGLE_TAG 0x4045C00000008000
#define MEF_GS_TRIANGLE_REGS 0x5551

/* Direct-transfer packet helpers (src/ov01/m_gs.c). */
void MGsGPInit(MGsPacket *packet, void *address, int size);
void MGsGPTerm(MGsPacket *packet);

#endif /* INCLUDE_OV01_M_GS_H */
