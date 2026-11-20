#ifndef INCLUDE_MAIN_XGL_CAMERA_H
#define INCLUDE_MAIN_XGL_CAMERA_H

#include "shared.h"

/* Functions of src/main/xgl_camera.c that other files call. */

/* The VIF1 packet builder of the xgl renderer, owned by xgl_packet.c (xglPacketInit sets up the two
 * halves of the packet buffer, packetSourceFirst and packetSourceSecond). */
typedef struct XglPacket XglPacket;

/*
 * The VIF1 direct-mode packet builder object. xglPacketInit sets up two of
 * them, packetSourceFirst and packetSourceSecond (xgl_packet.c). The record is 0x30 bytes: buffer and bufferSize
 * at +0x28 and +0x2c are part of it. Only the members the functions of
 * this TU touch are named; everything the sceVif1Pk* library alone reads or
 * writes stays unmodeled.
 */
struct XglPacket {
    /* xglFontReloadTexture decrements this forward cursor by four words. */
    u32 *write_cursor;
    /*
     * +0x04: the address xglPacketSend hands to xglDmaDirectSrcChain as the
     * DMA source-chain start whenever a packet is pending (main
     * 0x0022c520-0x0022c548). No function of this TU writes it -- neither
     * xglPacketInit nor xglPacketMove touches +0x04 -- so it is set by the
     * sceVif1Pk* library: sceVif1PkInit (main 0x0020ac68) stores its base
     * argument at +0x00 and +0x04, and xglPacketInit passes it each entry's
     * buffer base (0x00c00000 and 0x00e00000).
     */
    u32 start;
    /*
 * +0x08 and +0x0c: xglPrimAddGifTagDirect (main 0x00222c00 lw v0,8(s0);
 * main 0x00222c0c lw v0,0xc(s0)) tests both for non-zero to decide
 * whether a direct-mode block the sceVif1Pk* library opened is still
 * open, so it appends to that block instead of resetting/opening/
 * closing a new one (compare SGsSendPacket, src/main/sv_get_image_
 * item.c, which always resets/opens/closes without this check).
 * Neither word is written by any function of this TU, so both are set
 * by the sceVif1Pk* library.
 */
    u32 directCode;
    u32 *directState;
    unsigned char unmodeled_10[0x10];
    /*
     * +0x20: the packet's fixed top-of-buffer address. xglPacketInit sets it
     * once per entry after both sceVif1PkInit calls (main 0x0022c49c for the
     * first entry, value 0x00e00000; main 0x0022c4a0 for the second, value
     * 0x01000000): the end of the buffer whose base went to sceVif1PkInit.
     */
    u32 limit;
    /*
     * +0x24: the write cursor. xglPacketInit seeds it equal to `limit` (main
     * 0x0022c48c/0x0022c490) and xglPacketMove reloads `limit` into it at
     * the start of every frame (main 0x0022c4e4/0x0022c4f0), so writers that
     * fill the packet backward from the top always restart from the same
     * address. main/pxy_packet_state.c's nmlPacketSetAttributeData and
     * nmlPacketSetAttributeAlloc16N read it, decrement it by the size being
     * requested and store it back, then use the new value as the memcpy
     * destination or return it as the allocation (main 0x00238e28-0x00238e3c
     * and 0x00238fe0-0x00238ff4),
     * which is why the member is byte-addressed instead of scaled.
     */
    u8 *cursor;
    /* +0x28, +0x2C: the packet's half of the buffer, set by xglPacketInit. */
    u32 buffer;
    u32 bufferSize;
};

/* The xgl camera (0x600 bytes; xgl_studio.c keeps eight per studio). Earlier files called it StudioCamera:
 * nearClip/farClip are screen.clipRange, fov is travel.fov, the 0x60 rectangle is screen.window and the view
 * matrix _CurMatrixSet loads is volume[0].viewMatrix. */
typedef struct XglCamera XglCamera;
typedef struct XglCameraScreen XglCameraScreen;

/* The four frustum vectors of a travel view; xglCameraViewTravel copies them as one matrix into the camera volume. */
typedef union CameraFrustum {
    Vector4 corner[4];
    Matrix4 matrix;
} CameraFrustum;

typedef struct CameraTravel {
    float focus;
    float fov;
    float fov_velocity;
    float scale;
    Vector4 rotation;
    Vector4 angle_velocity;
    Vector4 angle_delta;
    Vector4 position;
    Vector4 place_velocity;
    Vector4 place_delta;
    float chase_yaw;
    float chase_distance;
    int unknown_78;
    unsigned char unmodeled_7c[4];
    Vector4 chase_reference;
    Vector4 chase_position;
    CameraFrustum frustum;
    /* +0xE0 and +0x120 of the travel view: the camera transform pair saved by xglCameraViewTravel. */
    Matrix4 transform;
    Matrix4 inverseTransform;
} CameraTravel;

struct XglCameraScreen {
    unsigned int depthMax;
    unsigned int depthMin;
    unsigned char unmodeled_08[8];
    Vector4Elements clipRange; /* x, y = near, far */
    Vector4 portEffect;
    Vector4 resolution;
    Vector4 projection;
    Vector4 window;
    Vector4 screenOffset;
    Vector4 screenScale;
};

typedef struct CameraVolume {
    Vector4Elements vertex[16];
    Vector4Elements plane[12];
    Matrix4 frustum;
    Matrix4 viewMatrix;
    unsigned char unmodeled_240[0x40];
    float planeSet[4][4][4];
} CameraVolume;

struct XglCamera {
    unsigned int active;
    unsigned int state;
    int padPort;                     /* +0x8: PadData port read by the pad updates */
    unsigned char unmodeled_0c[4];
    XglCameraScreen screen;
    CameraTravel travel;
    unsigned char unmodeled_1f0[0x80];
    CameraVolume volume[1];
    int lookAtLocked;                /* +0x5F0: toggled by button 0x800 */
    float unknown_5f4;               /* +0x5F4: set to 1.0f by xglCameraInit */
    unsigned char unmodeled_5f8[8];  /* the studio keeps cameras at a 0x600 stride */
};

/* The culling map table of face_point.c's checks (14 rows, the last one the terminator; data of xgl_camera.c) and the name
 * word that stands for an empty map (xgl_render.c). */
extern const unsigned char cullingMapDataTable[1584];
extern int xglCullingMapEmptyName;

void xglCameraInit(XglCamera *camera);
void xglCameraMove(XglCamera *camera);
void xglCameraSetPadPort(XglCamera *camera, int port);
void xglCameraSetWindow(XglCamera *camera, int x0, int y0, int x1, int y1);
void xglCameraTravelProc(XglCamera *camera);

#endif /* INCLUDE_MAIN_XGL_CAMERA_H */
