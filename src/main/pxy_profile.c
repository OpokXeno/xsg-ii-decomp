#include "common.h"
#include "shared.h"
#include "main/xgl_render.h"
#include "main/pxy_profile.h"
#include "main/pxy_debug_draw.h"
#define PXY_ENTRY_COUNT 32

/* One profiling entry of pxyEntries: the start timer count, the ticks accumulated by pxyProfileEnd, the in-use flag,
 * the entry name (name[0] is nonzero while the entry is drawn) and the formatted text line. */
typedef struct PxyProfileEntry {
    u32 startTicks;
    u32 totalTicks;
    unsigned char unmodeled_08[4];
    int inUse;
    char name[128];
    char text[128];
} PxyProfileEntry;

static PxyProfileEntry pxyEntries[PXY_ENTRY_COUNT];
int strcmp(const char *, const char *);
char *strcpy(char *, const char *);
int sprintf(char *, const char *, ...);
const char pxyProfileEntryFormat[16] = "%s  %3.2f";

/* EE timer 1 count register: the free-running counter that pxyProfileBegin and pxyProfileEnd read for the entry start and end. */
#define T1_COUNT ((volatile u32 *)0x10000800)

float pxyKeyInterpolate(Vector4Elements *keyA, Vector4Elements *keyB, float time, float blend)
{
    __asm__ __volatile__(
        ".set noat\n\t"
        ".set noreorder\n\t"
        "lw $4, 0($30)\n\t"
        "lw $3, 4($30)\n\t"
        "lwc1 $f1, 8($30)\n\t"
        "lwc1 $f0, 12($30)\n\t"
        "lqc2 vf20, 0($4)\n\t"
        "lqc2 vf21, 0($3)\n\t"
        "mfc1 $8, $f1\n\t"
        "qmtc2.ni $8, vf19\n\t"
        "vmr32.xyzw vf19, vf19\n\t"
        "vmove.xyzw vf24, vf20\n\t"
        "addiu $1, $0, 0\n\t"
        "mtc1 $1, $f0\n\t"
        "nop\n\t"
        "mfc1 $8, $f0\n\t"
        "nop\n\t"
        "mtc1 $8, $f1\n\t"
        "nop\n\t"
        "c.lt.s $f1, $f0\n\t"
        "nop\n\t"
        "bc1t 1f\n\t"
        "nop\n\t"
        "vnop\n\t"
        "vsub.x vf22, vf21, vf20\n\t"
        "vsubx.w vf23, vf19, vf20x\n\t"
        "vdiv Q, vf23w, vf22x\n\t"
        "vwaitq\n\t"
        "vsub.y vf24, vf20, vf21\n\t"
        "vsub.xyzw vf25, vf25, vf25\n\t"
        "vaddq.w vf25, vf25, Q\n\t"
        "vsub.w vf26, vf25, vf0\n\t"
        "vmulw.z vf26, vf21, vf25w\n\t"
        "vmul.w vf30, vf20, vf26\n\t"
        "vmul.w vf27, vf26, vf23\n\t"
        "vmul.w vf28, vf25, vf25\n\t"
        "vadd.w vf29, vf26, vf26\n\t"
        "vaddw.z vf26, vf26, vf30w\n\t"
        "vsub.w vf29, vf29, vf0\n\t"
        "vmul.w vf29, vf29, vf28\n\t"
        "vmuly.w vf29, vf29, vf24y\n\t"
        "vmulz.w vf27, vf27, vf26z\n\t"
        "vaddy.w vf27, vf27, vf20y\n\t"
        "vadd.w vf27, vf27, vf29\n\t"
        "vaddw.x vf25, vf25, vf27w\n\t"
        "j 2f\n\t"
        "nop\n\t"
        "vnop\n\t"
        "1:\n\t"
        "vsub.x vf22, vf21, vf20\n\t"
        "vdiv Q, vf0w, vf22x\n\t"
        "vsubx.w vf25, vf19, vf20x\n\t"
        "vaddw.x vf25, vf0, vf25w\n\t"
        "vwaitq\n\t"
        "vmulq.x vf25, vf25, Q\n\t"
        "vsubx.w vf26, vf0, vf25x\n\t"
        "vmuly.x vf25, vf25, vf21y\n\t"
        "vmuly.w vf26, vf26, vf20y\n\t"
        "vaddw.x vf25, vf25, vf26w\n\t"
        "2:\n\t"
        "qmfc2.ni $2, vf25\n\t"
        "mtc1 $2, $f0\n\t"
        ".set reorder\n\t"
        ".set at\n\t"
        : : : "memory", "$1", "$2", "$3", "$4", "$8", "$f0", "$f1");
}

/* Loads the four vectors at src, transforms them by the current matrix (vf27-vf29) into
 * the three result vectors and normalizes each result, storing the three vectors at dst. */
void _CurMatrixMul33normXYZW(float *dst, const float *src)
{
    __asm__ __volatile__(
        "lqc2 vf20, 0(%1)\n\t"
        "lqc2 vf21, 16(%1)\n\t"
        "lqc2 vf22, 32(%1)\n\t"
        "lqc2 vf26, 48(%1)\n\t"
        "vmulax.xyz ACC, vf27, vf20x\n\t"
        "vmadday.xyz ACC, vf28, vf20y\n\t"
        "vmaddz.xyz vf20, vf29, vf20z\n\t"
        "vmulax.xyz ACC, vf27, vf21x\n\t"
        "vmadday.xyz ACC, vf28, vf21y\n\t"
        "vmaddz.xyz vf21, vf29, vf21z\n\t"
        "vmulax.xyz ACC, vf27, vf22x\n\t"
        "vmadday.xyz ACC, vf28, vf22y\n\t"
        "vmaddz.xyz vf22, vf29, vf22z\n\t"
        "vmulax.xyz ACC, vf27, vf26x\n\t"
        "vmadday.xyz ACC, vf28, vf26y\n\t"
        "vmaddz.xyz vf26, vf29, vf26z\n\t"
        "vsub.w vf19, vf19, vf19\n\t"
        "vaddz.x vf25, vf0, vf20z\n\t"
        "vaddy.x vf24, vf0, vf20y\n\t"
        "vaddx.x vf23, vf0, vf20x\n\t"
        "vaddz.y vf25, vf0, vf21z\n\t"
        "vaddy.y vf24, vf0, vf21y\n\t"
        "vaddx.y vf23, vf0, vf21x\n\t"
        "vaddz.z vf25, vf0, vf22z\n\t"
        "vaddy.z vf24, vf0, vf22y\n\t"
        "vaddx.z vf23, vf0, vf22x\n\t"
        "vaddz.w vf25, vf19, vf26z\n\t"
        "vaddy.w vf24, vf19, vf26y\n\t"
        "vaddx.w vf23, vf19, vf26x\n\t"
        "vmula.xyzw ACC, vf23, vf23\n\t"
        "vmadda.xyzw ACC, vf24, vf24\n\t"
        "vmadd.xyzw vf10, vf25, vf25\n\t"
        "vrsqrt Q, vf0w, vf10x\n\t"
        "vwaitq\n\t"
        "vmulq.x vf23, vf23, Q\n\t"
        "vmulq.x vf24, vf24, Q\n\t"
        "vmulq.x vf25, vf25, Q\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vrsqrt Q, vf0w, vf10y\n\t"
        "vwaitq\n\t"
        "vmulq.y vf23, vf23, Q\n\t"
        "vmulq.y vf24, vf24, Q\n\t"
        "vmulq.y vf25, vf25, Q\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vrsqrt Q, vf0w, vf10z\n\t"
        "vwaitq\n\t"
        "vmulq.z vf23, vf23, Q\n\t"
        "vmulq.z vf24, vf24, Q\n\t"
        "vmulq.z vf25, vf25, Q\n\t"
        "vnop\n\t"
        "vnop\n\t"
        "vrsqrt Q, vf0w, vf10w\n\t"
        "vwaitq\n\t"
        "vmulq.w vf23, vf23, Q\n\t"
        "vmulq.w vf24, vf24, Q\n\t"
        "vmulq.w vf25, vf25, Q\n\t"
        "sqc2 vf23, 0(%0)\n\t"
        "sqc2 vf24, 16(%0)\n\t"
        "sqc2 vf25, 32(%0)\n\t"
        : : "r"(dst), "r"(src) : "memory");
}

/* Clears the in-use flag of every entry. */
void pxyProfileClearEntries(void)
{
    int i;

    for (i = 0; i < PXY_ENTRY_COUNT; i++) {
        pxyEntries[i].inUse = 0;
    }
}

/* Starts the entry for name: reuses the in-use entry of that name, or else takes the first free entry and clears it.
 * The entry start is the EE timer 1 count. */
void pxyProfileBegin(const char *name)
{
    int i;
    int found;

    found = -1;
    for (i = 0; i < PXY_ENTRY_COUNT; i++) {
        if (pxyEntries[i].inUse != 0) {
            if (strcmp(pxyEntries[i].name, name) == 0) {
                found = i;
                break;
            }
        }
    }
    if (found < 0) {
        for (i = 0; i < PXY_ENTRY_COUNT; i++) {
            if (pxyEntries[i].inUse == 0) {
                found = i;
                strcpy(pxyEntries[i].name, name);
                pxyEntries[i].startTicks = 0;
                pxyEntries[i].totalTicks = 0;
                pxyEntries[i].text[0] = 0;
                pxyEntries[i].inUse = 1;
                break;
            }
        }
    }
    if (found >= 0) {
        if (found < PXY_ENTRY_COUNT) {
            pxyEntries[found].startTicks = *T1_COUNT;
        }
    }
}

/* Ends the entry for name: adds the EE timer 1 ticks since its start to its total, formats the name and the total
 * (converted from the milliseconds in result) into its text line, and returns result (0.0 when no in-use entry matches). */
float pxyProfileEnd(const char *name)
{
    int i;
    int found;
    float result;
    float msPerTick;
    register char *textLine;
    register char *entryName;

    found = -1;
    result = 0.0f;
    for (i = 0; i < PXY_ENTRY_COUNT; i++) {
        if (pxyEntries[i].inUse != 0) {
            if (strcmp(pxyEntries[i].name, name) == 0) {
                found = i;
                break;
            }
        }
    }
    if (found >= 0) {
        msPerTick = 0.0052173799f;
        if (pxyTickScaleDouble == 1) {
            msPerTick = msPerTick * 2.0f;
        }
        pxyEntries[found].totalTicks = pxyEntries[found].totalTicks + (*T1_COUNT - pxyEntries[found].startTicks);
        result = (float) pxyEntries[found].totalTicks * msPerTick;
        textLine = pxyEntries[found].text;
        entryName = pxyEntries[found].name;
        sprintf(textLine, pxyProfileEntryFormat, entryName, result);
    }
    return result;
}

/* Adds the EE timer 1 ticks since the entry started to its total, then returns the total in milliseconds (0.0 when
 * no in-use entry matches). pxyTickScaleDouble set to 1 doubles the tick scale. */
float pxyProfileEndMs(const char *name)
{
    int i;
    int found;
    float result;
    float msPerTick;

    found = -1;
    result = 0.0f;
    for (i = 0; i < PXY_ENTRY_COUNT; i++) {
        if (pxyEntries[i].inUse != 0) {
            if (strcmp(pxyEntries[i].name, name) == 0) {
                found = i;
                break;
            }
        }
    }
    if (found >= 0) {
        if (found < PXY_ENTRY_COUNT) {
            msPerTick = 0.0052173799f;
            if (pxyTickScaleDouble == 1) {
                msPerTick = msPerTick * 2.0f;
            }
            pxyEntries[found].totalTicks = pxyEntries[found].totalTicks + (*T1_COUNT - pxyEntries[found].startTicks);
            result = (float) pxyEntries[found].totalTicks * msPerTick;
        }
    }
    return result;
}

/* Returns the accumulated ticks of the first in-use entry named name, in milliseconds (0.0 when none matches).
 * pxyTickScaleDouble set to 1 doubles the tick scale. The entry is not changed. */
float pxyProfileGetMs(const char *name)
{
    int i;
    int found;
    float result;
    float msPerTick;

    found = -1;
    result = 0.0f;
    for (i = 0; i < PXY_ENTRY_COUNT; i++) {
        if (pxyEntries[i].inUse != 0) {
            if (strcmp(pxyEntries[i].name, name) == 0) {
                found = i;
                break;
            }
        }
    }
    if (found >= 0) {
        if (found < PXY_ENTRY_COUNT) {
            msPerTick = 0.0052173799f;
            if (pxyTickScaleDouble == 1) {
                msPerTick = msPerTick * 2.0f;
            }
            result = (float) pxyEntries[found].totalTicks * msPerTick;
        }
    }
    return result;
}

/* Returns the accumulated ticks of the first entry whose name matches, in milliseconds, without testing the in-use
 * flag (0.0 when none matches). pxyTickScaleDouble set to 1 doubles the tick scale. */
float pxyProfileGetMsUnchecked(const char *name)
{
    int i;
    int found;
    float result;
    float msPerTick;

    found = -1;
    result = 0.0f;
    for (i = 0; i < PXY_ENTRY_COUNT; i++) {
        if (strcmp(pxyEntries[i].name, name) == 0) {
            found = i;
            break;
        }
    }
    if (found >= 0) {
        msPerTick = 0.0052173799f;
        if (pxyTickScaleDouble == 1) {
            msPerTick = msPerTick * 2.0f;
        }
        result = (float) pxyEntries[found].totalTicks * msPerTick;
    }
    return result;
}

void pxyProfileDrawEntries(void)
{
    int i;
    int lineCount;

    lineCount = 0;
    for (i = 0; i < PXY_ENTRY_COUNT; i++) {
        if (pxyEntries[i].inUse != 0) {
            if (pxyEntries[i].name[0] != 0) {
                pxyDebugDrawStringShadow(pxyEntries[i].text, 10, lineCount * 17 + 100, 1.3f, -1, 0x00FFFFFF, 1);
                lineCount++;
            }
        }
    }
}
