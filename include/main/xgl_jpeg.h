#ifndef INCLUDE_MAIN_XGL_JPEG_H
#define INCLUDE_MAIN_XGL_JPEG_H

/* The request of src/main/xgl_jpeg.c's decoder (xglJpegDecode, StartOfScan). */

typedef struct JpegImageDimensions {
    short width;
    short height;
} JpegImageDimensions;

/* Chroma subsampling of the first component (JPEG SOF sampling factors, H << 4 | V). */
typedef enum JpegSamplingMode {
    JPEG_SAMPLING_444,
    JPEG_SAMPLING_422,
    JPEG_SAMPLING_420
} JpegSamplingMode;

/* The encoder request of xglJpegEncode: it stores the GS frame region at frameBufferBase /
 * frameBufferWidth (origin originX / originY, width x height) as a JPEG in outputBuffer and sets
 * outputSize to the encoded size. samplingMode is a JpegSamplingMode. */
typedef struct JpegEncodeRequest {
    u8 unmodeled_00[4];
    u8 *outputBuffer;
    int outputSize;
    short frameBufferBase;  /* +0x0C: sceGsSetDefStoreImage fbp */
    short frameBufferWidth; /* +0x0E: fbw */
    short originX;
    short originY;
    short width;
    short height;
    u8 luminanceQuality;
    u8 chrominanceQuality;
    u8 samplingMode;
    u8 unmodeled_1b;
    u8 unmodeled_1c[4]; /* the callers' request block is 0x20 bytes, cleared whole */
} JpegEncodeRequest;

/* The 32-byte decode request. With destination 0 the image is uploaded to
 * the GS frame buffer at frameBufferBase / frameBufferWidth, origin
 * originX / originY; otherwise it is written to destination. */
typedef struct JpegDecodeControl {
    u8 *source;
    u8 *destination;
    int consumedSize; /* +0x08: bytes of source parsed up to the RST0/EOI marker */
    short frameBufferBase;
    short frameBufferWidth;
    u16 originX;
    u16 originY;
    JpegImageDimensions imageLimits; /* +0x14 */
    u8 luminancePercent;
    u8 chrominancePercent;
    u8 scaleMode; /* +0x1a */
    u8 pixelFormat;
    u8 redPercent;
    u8 greenPercent;
    u8 bluePercent;
    u8 unmodeled_1f;
} JpegDecodeControl;

int xglJpegDecode(JpegDecodeControl *request);
int xglJpegEncode(JpegEncodeRequest *request);

#endif /* INCLUDE_MAIN_XGL_JPEG_H */
