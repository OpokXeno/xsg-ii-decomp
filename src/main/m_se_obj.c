#include "common.h"
#include "main/m_se_obj.h"
#include "main/xgl_camera.h"
#include "main/m_math.h"
#include "quadword.h"
#include "main/xgl_studio.h"
#include "main/sv_get_image_item.h"

/* Builds in destination the rotation that turns the unit vector from onto
 * the unit vector to: the half-angle quaternion (cross(from, to) / s, s / 2)
 * with s = sqrt(2 * (1 + dot(from, to))), expanded to a 4x4 matrix. When s is
 * below 0.01 (opposite vectors) it uses the half turn about y instead and
 * returns 0; otherwise returns 1. A hand-written VU0 macro-mode block; the
 * 0.01 threshold is the assembler's li.s literal. The original keeps an
 * untouched 16-byte frame: without the unused scratch quadword gcc drops the
 * addiu $sp pair (fd.sh: 57 insns for 59). */
int MMathMatrixBetweenVectors(Matrix4 *destination, const Vector4 *from, const Vector4 *to)
{
    int rotated;
    Vector4 scratch;

    __asm__ __volatile__(
        ".set noreorder\n"
        "lqc2 vf2, 0(%2)\n"
        "lqc2 vf3, 0(%3)\n"
        "vaddw.xyz vf7, vf0, vf0w\n"
        "vsub.xyzw vf1, vf0, vf0\n"
        "li.s $f9, 0.01\n"
        "vmula.xyz ACC, vf2, vf3\n"
        "vmaddaw.xy ACC, vf7, vf0w\n"
        "vmadd.xyz vf4, vf2, vf3\n"
        "viaddi vi1, vi0, 8\n"
        "vaddw.y vf1, vf0, vf0w\n"
        "addi %0, $0, 1\n"
        "vmulax.z ACC, vf7, vf4x\n"
        "vmadday.z ACC, vf7, vf4y\n"
        "vmadd.z vf4, vf7, vf4\n"
        "vopmula.xyz ACC, vf2, vf3\n"
        "vmfir.w vf6, vi1\n"
        "vsqrt Q, vf4z\n"
        "vopmsub.xyz vf5, vf3, vf2\n"
        "vitof4.w vf6, vf6\n"
        "vwaitq\n"
        "vaddq.x vf4, vf0, Q\n"
        "vrsqrt Q, vf0w, vf4z\n"
        "qmfc2.ni $8, vf4\n"
        "mtc1 $8, $f8\n"
        "abs.s $f8, $f8\n"
        "c.lt.s $f8, $f9\n"
        "vwaitq\n"
        "vaddq.y vf4, vf0, Q\n"
        "bc1tl 1f\n"
        "neg %0, $0\n"
        "vmulx.w vf1, vf6, vf4x\n"
        "vmuly.xyz vf1, vf5, vf4y\n"
        "1:\n"
        "vadd.xyzw vf2, vf1, vf1\n"
        "vaddw.xyzw vf9, vf0, vf0w\n"
        "vsub.xyzw vf8, vf0, vf0\n"
        "vmulz.xyzw vf5, vf2, vf1z\n"
        "vmuly.xyzw vf4, vf2, vf1y\n"
        "vmulx.xyzw vf3, vf2, vf1x\n"
        "vsub.xyzw vf7, vf0, vf0\n"
        "vsub.xyzw vf6, vf0, vf0\n"
        "vaddz.y vf10, vf4, vf5z\n"
        "vaddx.z vf11, vf5, vf3x\n"
        "vaddy.x vf12, vf3, vf4y\n"
        "vaddw.x vf8, vf5, vf4w\n"
        "vsubw.x vf7, vf4, vf5w\n"
        "vsuby.x vf6, vf9, vf10y\n"
        "vsubw.y vf8, vf5, vf3w\n"
        "vsubz.y vf7, vf9, vf11z\n"
        "vaddw.y vf6, vf3, vf5w\n"
        "vsubx.z vf8, vf9, vf12x\n"
        "vaddw.z vf7, vf4, vf3w\n"
        "vsubw.z vf6, vf3, vf4w\n"
        "sqc2 vf0, 0x30(%1)\n"
        "sqc2 vf8, 0x20(%1)\n"
        "sqc2 vf7, 0x10(%1)\n"
        "sqc2 vf6, 0(%1)\n"
        ".set reorder\n"
        : "=r"(rotated)
        : "r"(destination), "r"(from), "r"(to)
        : "$8", "$f8", "$f9", "memory");
    return rotated;
}

extern void *memset(void *destination, int value, unsigned int count);
extern unsigned short MSeObjReady; /* unsigned: MSeObjProc tests it with lhu, andi 1, andi 0xffff (as short, fd.sh
                                     MSeObjProc 14/172 in place) */

#define MSE_OBJ_SLOTS 127
#define MSE_OBJ_BUSY 0x1
#define MSE_OBJ_FREE_REQUESTED 0x100

/* The pool lives at a fixed address: MSE_OBJ_SLOTS slots of 0x800 bytes. */
#define MSE_OBJ_POOL_ADDRESS 0x01FB0000

extern int MSeObjCount;
extern MSeObj *MSeObjPool;
extern MSeHook MSeMode1Hook;
extern MSeHook MSeMode2Hook;

/* Initialises the MSe object pool: MSE_OBJ_SLOTS slots of 0x800 bytes at MSE_OBJ_POOL_ADDRESS. */
void MSeObjInit(void) {
    MSeObjPool = (MSeObj *)MSE_OBJ_POOL_ADDRESS;
    MSeObjReady = 0;
    MSeObjCount = 0;
    MSeMode1Hook = 0;
    MSeMode2Hook = 0;
    memset((void *)MSE_OBJ_POOL_ADDRESS, 0, MSE_OBJ_SLOTS * sizeof(MSeObj));
    MSeObjReady = 1;
}

/* Shutdown: clears the ready flag, the live count and the pool pointer. */
void MSeObjShutdown(void) {
    MSeObjReady = 0;
    MSeObjCount = 0;
    MSeObjPool = 0;
}

extern char MSeObjProcInvalidTypeFormat[];

/* Runs one pass over the busy slots of the pool: mode 0 takes the camera snapshot first, mode 2 forces each
 * object's image mapper, mode 3 releases the slots MSeObjRequestFree marked; modes 1 and 2 end with their hook. */
void MSeObjProc(unsigned int mode) {
    MSeObj *object = MSeObjPool;
    int count = MSeObjCount;
    int slot = MSE_OBJ_SLOTS;
    unsigned int mapper = 0;
    XglCamera *camera;

    if (!(MSeObjReady & 1)) {
        return;
    }
    switch (mode) {
    case 0:
        camera = xglStudioGetCamera2(0);
        MEfCamParams.camera = camera;
        QuadwordCopyMatrixT0(MEfCamParams.viewMatrix, camera->volume[0].viewMatrix);
        QuadwordCopyT0(&MEfCamParams.rotation, &camera->travel.rotation);
        MMathRotateMatrixYXZ(&MEfCamParams.rotationMatrix, 0, &MEfCamParams.rotation);
        while (slot-- != 0) {
            if (object->flags & MSE_OBJ_BUSY) {
                if (object->modeProc[0] != 0) {
                    object->modeProc[0](object);
                }
                if (--count == 0) {
                    break;
                }
            }
            object++;
        }
        break;
    case 1:
        while (slot-- != 0) {
            if (object->flags & MSE_OBJ_BUSY) {
                if (object->modeProc[1] != 0) {
                    object->modeProc[1](object);
                }
                if (--count == 0) {
                    break;
                }
            }
            object++;
        }
        if (MSeMode1Hook != 0) {
            MSeMode1Hook();
        }
        break;
    case 2:
        svForceImageMapper(0);
        while (slot-- != 0) {
            if (object->flags & MSE_OBJ_BUSY) {
                if (object->modeProc[2] != 0) {
                    if (object->imageMapper != 0 && object->imageMapper != mapper) {
                        svForceImageMapper(object->imageMapper);
                        mapper = object->imageMapper;
                    }
                    object->modeProc[2](object);
                }
                if (--count == 0) {
                    break;
                }
            }
            object++;
        }
        if (MSeMode2Hook != 0) {
            MSeMode2Hook();
        }
        break;
    case 3:
        while (slot-- != 0) {
            if (object->flags & MSE_OBJ_BUSY) {
                if (object->modeProc[3] != 0) {
                    object->modeProc[3](object);
                }
                if (object->flags & MSE_OBJ_FREE_REQUESTED) {
                    object->flags = 0;
                    MSeObjCount--;
                }
                if (--count == 0) {
                    break;
                }
            }
            object++;
        }
        break;
    default:
        MOutputDebugStringWarn(MSeObjProcInvalidTypeFormat, mode);
        break;
    }
}

/* Takes the first free slot of the pool: its flags become MSE_OBJ_BUSY, the mapper and the four callbacks 0.
 * Returns 0 when the pool already holds MSE_OBJ_SLOTS objects or no slot is free. */
MSeObj *MSeObjAlloc(void) {
    MSeObj *object = MSeObjPool;
    int slot = MSE_OBJ_SLOTS;

    if (MSeObjCount < MSE_OBJ_SLOTS) {
        while (slot-- != 0) {
            if (!(object->flags & MSE_OBJ_BUSY)) {
                /* Volatile keeps the two header stores in source order (fd.sh: plain stores
                 * issue the flags sh before the mapper sw, 26/28 in place; this view 28/28). */
                volatile MSeObj *header = object;

                header->imageMapper = 0;
                header->flags = MSE_OBJ_BUSY;
                MSeObjCount++;
                object->modeProc[0] = 0;
                object->modeProc[1] = 0;
                object->modeProc[2] = 0;
                object->modeProc[3] = 0;
                return object;
            }
            object++;
        }
    }
    return 0;
}

/* Marks the object (its flags word is the first field) for release: MSeObjProc mode 3 clears slots carrying
 * MSE_OBJ_FREE_REQUESTED. */
void MSeObjRequestFree(void *object) {
    unsigned short *flags = object;

    *flags |= MSE_OBJ_FREE_REQUESTED;
}

/* Sets the callback that MSeObjProc calls with no arguments after its mode 1 pass (MSeMode1Hook). */
void MSeSetMode1Hook(MSeHook hook) {
    MSeMode1Hook = hook;
}

/* Sets the callback that MSeObjProc calls with no arguments after its mode 2 pass (MSeMode2Hook). */
void MSeSetMode2Hook(MSeHook hook) {
    MSeMode2Hook = hook;
}
/* Data of this unit's section .sdata: the MSe pool state and hooks, the trace format, the word of
 * saveReadImage and saveWriteImage (startStockCount) and the trace string. */
unsigned short MSeObjReady = 0;
int MSeObjCount = 0;
MSeObj *MSeObjPool = 0;
MSeHook MSeMode1Hook = 0;
MSeHook MSeMode2Hook = 0;
int startStockCount = 0;
const char tracePrintFormat[8] = "%s";

/* Data of this unit's section .data: the save blocks and tables that saveReadImage and saveWriteImage copy
 * (zero until the first save or load), and the two large zero areas. */
/* The camera snapshot of MSeObjProc mode 0 (include/main/m_se_obj.h). */
MEfCameraParams MEfCamParams = { 0 };
int battleSlotTable[0x420] = { 0 };
int slotRecordTable[0x100] = { 0 };

short dataBoxQuantityTable1[0x100] = { 0 };
short dataBoxQuantityTable2[0x100] = { 0 };
short dataBoxQuantityTable3[0x100] = { 0 };
short dataBoxQuantityTable4[0x100] = { 0 };
int sdvSpecialBuf[0xC00] = { 0 };
unsigned char unusedWorkBuffer[0x11140] = { 0 };
