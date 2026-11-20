#include "common.h"
#include "shared.h"
#include "gs.h"
#include "main/xgl_render.h"
#include "main/pxy_debug_draw.h"
#include "main/pxy_packet_state.h"

extern u8 *pxyGlyphDigits[];
extern u8 *pxyGlyphUpper[];
extern u8 *pxyGlyphLower[];
extern u8 pxyGlyphUnderscore[];
extern u8 pxyGlyphPeriod[];
extern u8 pxyGlyphSlash[];
extern u8 pxyGlyphParenOpen[];
extern u8 pxyGlyphParenClose[];
extern u16 sRenderDepthBase;

u8 pxyDebugDrawChar(u8 ch, int x, int y, float scale, u32 color, u32 z, int flush) {
    u8 advance = 0;
    u8 *glyph = 0;
    int code = ch;

    if (code >= 48 && code < 58) {
        glyph = pxyGlyphDigits[code - 48];
    } else if (code >= 65 && code < 91) {
        glyph = pxyGlyphUpper[code - 65];
    } else if (code >= 97 && code < 123) {
        glyph = pxyGlyphLower[code - 97];
    } else if (code == 46) {
        glyph = pxyGlyphPeriod;
    } else if (code == 47) {
        glyph = pxyGlyphSlash;
    } else if (code == 95) {
        glyph = pxyGlyphUnderscore;
    } else if (code == 40) {
        glyph = pxyGlyphParenOpen;
    } else if (code == 41) {
        glyph = pxyGlyphParenClose;
    } else if (code == 32) {
        return 7;
    }
    if (glyph == 0) {
        return advance;
    }
    advance = *glyph++;

    if (flush != 0) {
        u64 *cursor;
        u64 *packetStart;

        nmlPacketGsInit();
        cursor = &xglDirectQueueBuffer[xglDirectQueueCount].data;
        packetStart = cursor;
        cursor[0] = 0x50000;
        cursor[1] = 0x47;
        cursor += 2;
        GS_PUT(cursor, GS_ZBUF_1, GS_SET_ZBUF(sRenderDepthBase, 1, 0));
        GS_PUT(cursor, GS_RGBAQ, GS_SET_RGBAQ(color & 0xFF, (color >> 8) & 0xFF, (color >> 16) & 0xFF, color >> 24, 0x3F800000));
        GS_PUT(cursor, GS_PRIM, 2);
        while (*glyph != 0xFF) {
            if (*glyph == 0xFE) {
                GS_PUT(cursor, GS_PRIM, 2);
            } else {
                GS_PUT(cursor, GS_XYZF2, GS_SET_XYZ(((int)((float)glyph[0] * scale) + x + 0x700) << 4, ((int)((float)glyph[1] * scale) + y + 0x720) << 4, z));
            }
            glyph += 2;
        }
        xglDirectQueueCount += ((u32)cursor - (u32)packetStart) >> 4;
        xglDirectQueueSubmitRef();
    } else {
        u64 *cursor;
        u64 *packetStart;

        nmlPacketGsInit();
        cursor = &xglDirectQueueBuffer[xglDirectQueueCount].data;
        packetStart = cursor;
        cursor[0] = 0x50000;
        cursor[1] = 0x47;
        cursor += 2;
        GS_PUT(cursor, GS_ZBUF_1, GS_SET_ZBUF(sRenderDepthBase, 1, 0));
        GS_PUT(cursor, GS_RGBAQ, GS_SET_RGBAQ(color & 0xFF, (color >> 8) & 0xFF, (color >> 16) & 0xFF, color >> 24, 0x3F800000));
        GS_PUT(cursor, GS_PRIM, 2);
        while (*glyph != 0xFF) {
            if (*glyph == 0xFE) {
                GS_PUT(cursor, GS_PRIM, 2);
            } else {
                GS_PUT(cursor, GS_XYZF2, GS_SET_XYZ(((int)((float)glyph[0] * scale) + x + 0x700) << 4, ((int)((float)glyph[1] * scale) + y + 0x720) << 4, z));
            }
            glyph += 2;
        }
        xglDirectQueueCount += ((u32)cursor - (u32)packetStart) >> 4;
        xglDirectQueueDefer(0, 10);
    }
    return advance;
}

unsigned int strlen(const char *);

int pxyDebugDrawString(const char *str, int x, int y, float scale, u32 color, u32 depth, int flush) {
    int i;
    int advanceTotal = 0;

    for (i = 0; i < strlen(str); i++) {
        advanceTotal += pxyDebugDrawChar(str[i], (int)((float)x + (float)advanceTotal * scale), y, scale, color, depth, flush);
    }
    return advanceTotal;
}

int pxyDebugDrawStringShadow(const char *str, int x, int y, float scale, u32 color, u32 depth, int flush) {
    int i;
    int advanceTotal = 0;

    for (i = 0; i < strlen(str); i++) {
        advanceTotal += pxyDebugDrawChar(str[i], (int)((float)(x + 1) + (float)advanceTotal * scale), y + 1, scale, 0, depth, flush);
    }
    advanceTotal = 0;
    for (i = 0; i < strlen(str); i++) {
        advanceTotal += pxyDebugDrawChar(str[i], (int)((float)x + (float)advanceTotal * scale), y, scale, color, depth, flush);
    }
    return advanceTotal;
}

/* pxyTextColor and pxyTextScale are read by pxyDebugPrintf, which passes them on to pxyDebugDrawString. */
int pxyTextColor = -1;
float pxyTextScale = 1.0f;
int pxyTextDepth = 0xFFFFFF;

void pxyDebugSetTextStyle(int color, float scale, int depth) {
    pxyTextColor = color;
    pxyTextScale = scale;
    pxyTextDepth = depth;
}

typedef char *va_list;

extern int vsprintf(char *buffer, const char *format, va_list args);

void pxyDebugPrintf(int x, int y, const char *format, ...) {
    char buffer[512];
    va_list args;

    args = (va_list)__builtin_next_arg(format) - (8 - 3) * 8;
    vsprintf(buffer, format, args);
    pxyDebugDrawString(buffer, x, y, pxyTextScale, pxyTextColor, pxyTextDepth, 1);
}

/* Glyph stroke data of the lower-case and digit-tail blocks, with the lower-case pointer table. */
u8 pxyGlyphUnderscore[] = {
    7, 0, 7, 6, 7, 0, 7, 255
};
u8 pxyGlyphPeriod[16] = {
    7, 2, 6, 3, 6, 3, 7, 2,
    7, 2, 6, 255, 0, 0, 0, 0
};
u8 pxyGlyphSlash[] = {
    7, 5, 1, 1, 7, 5, 1, 255
};
u8 pxyGlyphParenOpen[16] = {
    7, 4, 1, 3, 2, 3, 6, 4,
    7, 3, 6, 255, 0, 0, 0, 0
};
u8 pxyGlyphParenClose[16] = {
    7, 2, 1, 3, 2, 3, 6, 2,
    7, 3, 6, 255, 0, 0, 0, 0
};
u8 pxyGlyphUpperA[24] = {
    7, 1, 7, 1, 3, 3, 1, 5,
    3, 5, 7, 5, 5, 1, 5, 5,
    5, 255, 0, 0, 0, 0, 0, 0
};
u8 pxyGlyphUpperB[32] = {
    7, 1, 7, 1, 1, 4, 1, 5,
    2, 5, 3, 4, 4, 1, 4, 4,
    4, 5, 5, 5, 6, 4, 7, 1,
    7, 255, 0, 0, 0, 0, 0, 0
};
u8 pxyGlyphUpperC[24] = {
    7, 5, 2, 4, 1, 2, 1, 1,
    2, 1, 6, 2, 7, 4, 7, 5,
    6, 4, 7, 255, 0, 0, 0, 0
};
u8 pxyGlyphUpperD[24] = {
    7, 1, 7, 1, 1, 4, 1, 5,
    2, 5, 6, 4, 7, 1, 7, 4,
    7, 255, 0, 0, 0, 0, 0, 0
};
u8 pxyGlyphUpperE[24] = {
    7, 5, 1, 1, 1, 1, 7, 5,
    7, 1, 7, 254, 254, 1, 4, 4,
    4, 1, 4, 255, 0, 0, 0, 0
};
u8 pxyGlyphUpperF[16] = {
    7, 5, 1, 1, 1, 1, 7, 1,
    4, 4, 4, 1, 4, 255, 0, 0
};
u8 pxyGlyphUpperG[24] = {
    7, 5, 2, 4, 1, 2, 1, 1,
    2, 1, 6, 2, 7, 4, 7, 5,
    6, 5, 4, 4, 4, 5, 4, 255
};
u8 pxyGlyphUpperH[16] = {
    7, 1, 1, 1, 7, 1, 4, 5,
    4, 5, 1, 5, 7, 5, 1, 255
};
u8 pxyGlyphUpperI[16] = {
    7, 2, 1, 4, 1, 3, 1, 3,
    7, 4, 7, 2, 7, 4, 7, 255
};
u8 pxyGlyphUpperJ[16] = {
    7, 3, 1, 5, 1, 4, 1, 4,
    6, 3, 7, 2, 7, 1, 6, 255
};
u8 pxyGlyphUpperK[24] = {
    7, 1, 1, 1, 7, 1, 4, 2,
    4, 5, 1, 2, 4, 5, 7, 2,
    4, 255, 0, 0, 0, 0, 0, 0
};
u8 pxyGlyphUpperL[16] = {
    7, 1, 1, 1, 7, 5, 7, 1,
    7, 255, 0, 0, 0, 0, 0, 0
};
u8 pxyGlyphUpperM[16] = {
    7, 1, 7, 1, 1, 3, 7, 5,
    1, 5, 7, 5, 1, 255, 0, 0
};
u8 pxyGlyphUpperN[16] = {
    7, 1, 7, 1, 1, 5, 7, 5,
    1, 5, 7, 255, 0, 0, 0, 0
};
u8 pxyGlyphUpperO[24] = {
    7, 1, 2, 1, 6, 2, 7, 4,
    7, 5, 6, 5, 2, 4, 1, 2,
    1, 1, 2, 255, 0, 0, 0, 0
};
u8 pxyGlyphUpperP[24] = {
    7, 1, 7, 1, 1, 4, 1, 5,
    2, 5, 3, 4, 4, 1, 4, 4,
    4, 255, 0, 0, 0, 0, 0, 0
};
u8 pxyGlyphUpperQ[32] = {
    7, 1, 2, 1, 6, 2, 7, 4,
    7, 5, 6, 5, 2, 4, 1, 2,
    1, 1, 2, 254, 254, 3, 5, 5,
    7, 3, 5, 255, 0, 0, 0, 0
};
u8 pxyGlyphUpperR[32] = {
    7, 1, 7, 1, 1, 4, 1, 5,
    2, 5, 3, 4, 4, 1, 4, 4,
    4, 254, 254, 2, 4, 5, 7, 2,
    4, 255, 0, 0, 0, 0, 0, 0
};
u8 pxyGlyphUpperS[32] = {
    7, 5, 2, 4, 1, 2, 1, 1,
    2, 1, 3, 2, 4, 4, 4, 5,
    5, 5, 6, 4, 7, 2, 7, 1,
    6, 2, 7, 255, 0, 0, 0, 0
};
u8 pxyGlyphUpperT[16] = {
    7, 1, 1, 5, 1, 3, 1, 3,
    7, 3, 1, 255, 0, 0, 0, 0
};
u8 pxyGlyphUpperU[16] = {
    7, 1, 1, 1, 6, 2, 7, 4,
    7, 5, 6, 5, 1, 5, 6, 255
};
u8 pxyGlyphUpperV[16] = {
    7, 1, 1, 3, 7, 5, 1, 3,
    7, 255, 0, 0, 0, 0, 0, 0
};
u8 pxyGlyphUpperW[16] = {
    7, 1, 1, 2, 7, 3, 1, 4,
    7, 5, 1, 4, 7, 255, 0, 0
};
u8 pxyGlyphUpperX[16] = {
    7, 1, 1, 5, 7, 1, 1, 254,
    254, 1, 7, 5, 1, 1, 7, 255
};
u8 pxyGlyphUpperY[16] = {
    7, 1, 1, 3, 4, 3, 7, 3,
    4, 5, 1, 3, 4, 255, 0, 0
};
u8 pxyGlyphUpperZ[16] = {
    7, 1, 1, 5, 1, 1, 7, 5,
    7, 1, 7, 255, 0, 0, 0, 0
};
u8 *pxyGlyphDigits[10] = {
    pxyGlyphDigitTable + 4, pxyGlyphDigitTable + 36, pxyGlyphDigitTable + 52, pxyGlyphDigitTable + 76,
    pxyGlyphDigitTable + 108, pxyGlyphDigitTable + 124, pxyGlyphDigitTable + 148, pxyGlyphDigitTable + 172,
    pxyGlyphDigitTable + 188, pxyGlyphDigitTable + 228
};
u8 *pxyGlyphUpper[26] = {
    pxyGlyphUpperA, pxyGlyphUpperB, pxyGlyphUpperC, pxyGlyphUpperD,
    pxyGlyphUpperE, pxyGlyphUpperF, pxyGlyphUpperG, pxyGlyphUpperH,
    pxyGlyphUpperI, pxyGlyphUpperJ, pxyGlyphUpperK, pxyGlyphUpperL,
    pxyGlyphUpperM, pxyGlyphUpperN, pxyGlyphUpperO, pxyGlyphUpperP,
    pxyGlyphUpperQ, pxyGlyphUpperR, pxyGlyphUpperS, pxyGlyphUpperT,
    pxyGlyphUpperU, pxyGlyphUpperV, pxyGlyphUpperW, pxyGlyphUpperX,
    pxyGlyphUpperY, pxyGlyphUpperZ
};
u8 pxyGlyphLowerA[24] = { 7, 2, 2, 4, 2, 5, 3, 5, 7, 2, 7, 1, 6, 1, 5, 2, 4, 5, 4, 255, 0, 0, 0, 0 };
u8 pxyGlyphLowerB[16] = { 7, 1, 1, 1, 7, 4, 7, 5, 6, 5, 5, 4, 4, 1, 4, 255 };
u8 pxyGlyphLowerC[24] = { 7, 5, 4, 4, 3, 2, 3, 1, 4, 1, 6, 2, 7, 4, 7, 5, 6, 4, 7, 255, 0, 0, 0, 0 };
u8 pxyGlyphLowerD[16] = { 7, 5, 1, 5, 7, 2, 7, 1, 6, 1, 5, 2, 4, 5, 4, 255 };
u8 pxyGlyphLowerE[24] = { 7, 1, 5, 5, 5, 5, 4, 4, 3, 2, 3, 1, 4, 1, 6, 2, 7, 5, 7, 2, 7, 255, 0, 0 };
u8 pxyGlyphLowerF[24] = { 7, 5, 1, 4, 1, 3, 2, 3, 7, 3, 3, 1, 3, 5, 3, 4, 3, 255, 0, 0, 0, 0, 0, 0 };
u8 pxyGlyphLowerG[24] = { 7, 2, 8, 4, 8, 5, 7, 5, 4, 4, 3, 2, 3, 1, 4, 1, 5, 2, 6, 5, 6, 255, 0, 0 };
u8 pxyGlyphLowerH[16] = { 7, 1, 0, 1, 7, 1, 3, 4, 3, 5, 4, 5, 7, 5, 4, 255 };
u8 pxyGlyphLowerI[16] = { 7, 3, 7, 3, 4, 3, 7, 254, 254, 3, 2, 3, 1, 255, 0, 0 };
u8 pxyGlyphLowerJ[24] = { 7, 1, 7, 2, 7, 3, 6, 3, 4, 3, 6, 254, 254, 3, 2, 3, 1, 255, 0, 0, 0, 0, 0, 0 };
u8 pxyGlyphLowerK[32] = { 7, 1, 1, 1, 7, 1, 5, 2, 5, 3, 4, 4, 4, 5, 3, 4, 4, 254, 254, 2, 5, 3, 6, 4, 6, 5, 7, 4, 6, 255, 0, 0 };
u8 pxyGlyphLowerL[16] = { 7, 3, 1, 3, 6, 4, 7, 3, 6, 255, 0, 0, 0, 0, 0, 0 };
u8 pxyGlyphLowerM[32] = { 7, 1, 7, 1, 3, 2, 3, 3, 4, 4, 3, 5, 4, 5, 7, 5, 6, 254, 254, 3, 4, 3, 7, 3, 4, 255, 0, 0, 0, 0, 0, 0 };
u8 pxyGlyphLowerN[16] = { 7, 1, 7, 1, 3, 4, 3, 5, 4, 5, 7, 5, 4, 255, 0, 0 };
u8 pxyGlyphLowerO[24] = { 7, 1, 6, 2, 7, 4, 7, 5, 6, 5, 4, 4, 3, 2, 3, 1, 4, 1, 6, 255, 0, 0, 0, 0 };
u8 pxyGlyphLowerP[16] = { 7, 1, 8, 1, 3, 4, 3, 5, 4, 5, 5, 4, 6, 1, 6, 255 };
u8 pxyGlyphLowerQ[16] = { 7, 5, 8, 5, 3, 2, 3, 1, 4, 1, 5, 2, 6, 5, 6, 255 };
u8 pxyGlyphLowerR[16] = { 7, 1, 3, 1, 7, 1, 5, 3, 3, 5, 3, 3, 3, 255, 0, 0 };
u8 pxyGlyphLowerS[24] = { 7, 5, 3, 2, 3, 1, 4, 2, 5, 4, 5, 5, 6, 4, 7, 1, 7, 4, 7, 255, 0, 0, 0, 0 };
u8 pxyGlyphLowerT[16] = { 7, 3, 1, 3, 7, 3, 3, 1, 3, 5, 3, 1, 3, 255, 0, 0 };
u8 pxyGlyphLowerU[16] = { 7, 1, 3, 1, 6, 2, 7, 5, 7, 5, 3, 5, 7, 255, 0, 0 };
u8 pxyGlyphLowerV[16] = { 7, 1, 3, 1, 5, 3, 7, 5, 5, 5, 3, 5, 5, 255, 0, 0 };
u8 pxyGlyphLowerW[32] = { 7, 1, 3, 1, 6, 2, 7, 3, 6, 4, 7, 5, 6, 5, 3, 5, 4, 254, 254, 3, 3, 3, 6, 3, 3, 255, 0, 0, 0, 0, 0, 0 };
u8 pxyGlyphLowerX[16] = { 7, 1, 3, 5, 7, 1, 3, 254, 254, 1, 7, 5, 3, 1, 7, 255 };
u8 pxyGlyphLowerY[16] = { 7, 1, 3, 3, 7, 5, 3, 3, 7, 2, 8, 1, 8, 2, 8, 255 };
u8 pxyGlyphLowerZ[16] = { 7, 1, 3, 5, 3, 4, 4, 1, 7, 5, 7, 1, 7, 255, 0, 0 };
u8 *pxyGlyphLower[28] = { pxyGlyphLowerA, pxyGlyphLowerB, pxyGlyphLowerC, pxyGlyphLowerD, pxyGlyphLowerE, pxyGlyphLowerF, pxyGlyphLowerG, pxyGlyphLowerH, pxyGlyphLowerI, pxyGlyphLowerJ, pxyGlyphLowerK, pxyGlyphLowerL, pxyGlyphLowerM, pxyGlyphLowerN, pxyGlyphLowerO, pxyGlyphLowerP, pxyGlyphLowerQ, pxyGlyphLowerR, pxyGlyphLowerS, pxyGlyphLowerT, pxyGlyphLowerU, pxyGlyphLowerV, pxyGlyphLowerW, pxyGlyphLowerX, pxyGlyphLowerY, pxyGlyphLowerZ, 0, 0 };
