#include "common.h"
#include "main/res_get_path.h"
#include "main/test_128.h"
#include "main/oLib.h"
#include "main/assert_fail.h"
#include "main/game_camera.h"
#include "main/xenoActor.h"

/* Source file name passed with the allocation and free calls. */
static const char oLibSourceFile[16] = "../oLib.euc.c";
/* Fixed EE RAM address of the object library heap. */
#define OLIB_HEAP_BASE ((void *)0x01000000)
extern void *oLibHandle;

/* Object library setup: the handle for the oLib file goes into oLibHandle. */
void oLibInit(void)
{
    struct RgHeap *setup_heap;
    void *handle;

    setup_heap = InstanceOfRgHeapById(4);
    InitRgHeap(setup_heap, OLIB_HEAP_BASE, 0x01200000);
    handle = RgHeapAllocById(4, 0x800010, oLibSourceFile, 118);
    oLibHandle = handle;
    GameResourceInit(handle, 0x800000);
}

/* Object library teardown: frees the handle in oLibHandle. */
void oLibExit(void)
{
    GameResourceReset();
    RgHeapFree(oLibHandle, oLibSourceFile, 134);
}

/* Loads the object resource Id (asserted at line 157) and creates the actor from its
 * resource table (model, texture, hierarchy, motion table, lookLink) as uwamonoLoadPlayer does. */
void oLibLoadObject(struct XenoActor **object, int Id)
{
    void *SrcData;
    int *values;
    struct XenoActor *actor;

    SrcData = 0;
    if (GameResLoadID(Id, &SrcData, 0, 0) == -1) {
        assertFailed("GameResLoadID(Id, &SrcData, NULL, NULL) != -1", oLibSourceFile, 157);
    }
    values = SrcData;
    actor = XenoActorAlloc();
    ((XenoActorKindView *) actor)->kind = (void *) Id;
    actor->rsrc[XAct_Rsrc_MDL] = values[0];
    actor->shadowType = 1;
    *object = actor;
    actor->rsrc[XAct_Rsrc_TEX] = values[1];
    actor->rsrc[XAct_Rsrc_XHR] = values[2];
    actor->rsrc[XAct_Rsrc_XAP] = values[4];
    actor->rsrc[XAct_Rsrc_EntryTableC] = values[3];
    XenoActorAllocMatrices(actor);
    XenoActorSetBlendFrames(*object, 0.0f);
    XenoActorSetModeInSlot(*object, 0, 0);
    XenoActorSetBlendFramesB(*object, 0.0f);
    XenoActorSetSelector(*object, 0);
}

/* The .sdata flags xglCosDegTableReady and xglSinDegTableReady precede oLibHandle. */

/* Converts an x coordinate to 12.4 fixed point, offset into the GS display area. */
int gsVertexX(int x)
{
    int magnitude = (x >= 0) ? x : -x;

    if (x > 0) {
        return (x << 4) + 0x7008;
    }
    return -(magnitude << 4) - 0x7008;
}

/* Converts a y coordinate to 12.4 fixed point, offset into the GS display area. */
int gsVertexY(int y)
{
    int magnitude = (y >= 0) ? y : -y;

    if (y > 0) {
        return (y << 4) + 0x7208;
    }
    return -(magnitude << 4) - 0x7208;
}

/* Converts a coordinate to 12.4 fixed point with half-unit rounding by sign (gsRoundCoord). */
int gsRoundCoord(int coordinate)
{
  int zero;
  int magnitude;
  zero = 0;
  magnitude = coordinate;
  if (coordinate < zero)
  {
    magnitude = -magnitude;
  }
  if (zero < coordinate)
  {
    return (coordinate * 0x10) + 8;
  }
  return (-(magnitude * 0x10)) - 8;
}

/* Converts a coordinate to 12.4 fixed point with half-unit rounding by sign: (value << 4) + 8 above zero, else
   -(|value| << 4) - 8, so 0 gives -8; the same conversion as gsRoundCoord. The absolute value is taken
   at entry as the original does: written as (value << 4) - 8 in the branch, fd.sh gives 6 insns for the 11. */
int xglRoundCoord(int value)
{
    int magnitude = (value >= 0) ? value : -value;

    if (value > 0) {
        return (value << 4) + 8;
    }
    return -(magnitude << 4) - 8;
}

double sqrt(double value);

/* Distance between two points, truncated to an integer: the float sum of squares goes through the
 * soft-float double conversions (fptodp, the double sqrt, dptoli). */
int xglPointDistanceInt(Vector4 *from, Vector4 *to)
{
    float dx = to->x - from->x;
    float dy = to->y - from->y;
    float dz = to->z - from->z;

    return sqrt(dx * dx + dy * dy + dz * dz);
}

extern int xglCosDegTableReady;
static float xglCosDegTable[38];
void xglCosDegTableInit(void);

/* Cosine of an angle in degrees from the 10-degree table (built on first use); the table is
 * indexed by |degrees| % 360 rounded down to a 10-degree step. */
float xglCosDeg(int degrees)
{
    if (!xglCosDegTableReady) {
        xglCosDegTableInit();
        xglCosDegTableReady = 1;
    }
    /* |degrees| taken in place: with a separate local gcc moves the abs after the init call
     * (bgez/negu there instead of bltzl/negu at entry). */
    degrees = (degrees >= 0) ? degrees : -degrees;
    return xglCosDegTable[(degrees % 360) / 10];
}

extern float cosf(float angle);

void xglCosDegTableInit(void)
{
    float degToRad = 0.017453289f;
    int i;

    for (i = 0; i < 37; i++) {
        xglCosDegTable[i] = cosf((float) (i * 10) * degToRad);
    }
}

extern int xglSinDegTableReady;
static float xglSinDegTable[38];
void xglSinDegTableInit(void);

/* Sine of |degrees| from the 10-degree table (built on first use): a negative angle returns the
 * table entry of the positive angle, not its negated sine. */
float xglSinDeg(int degrees)
{
    if (!xglSinDegTableReady) {
        xglSinDegTableInit();
        xglSinDegTableReady = 1;
    }
    /* |degrees| taken in place, as in xglCosDeg. */
    degrees = (degrees >= 0) ? degrees : -degrees;
    return xglSinDegTable[(degrees % 360) / 10];
}

extern float sinf(float angle);

void xglSinDegTableInit(void)
{
    float degToRad = 0.017453289f;
    int i;

    for (i = 0; i < 37; i++) {
        xglSinDegTable[i] = sinf((float) (i * 10) * degToRad);
    }
}

/* Small .sdata flags and handles, in address order: the table-ready flags of xglCosDeg/xglSinDeg, the oLib handle,
 * then recordUpdatePaused (used by other TUs, so it stays a global). */
int xglCosDegTableReady = 0;
int xglSinDegTableReady = 0;
void *oLibHandle = 0;
int recordUpdatePaused = 0;
