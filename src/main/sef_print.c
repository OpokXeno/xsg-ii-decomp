#include "common.h"
#include "shared.h"
#include "main/xgl_thread.h"
#include "main/pad.h"
#include "main/sef_print.h"
#include "main/xgl_font.h"
#include "main/xgl_render.h"

extern u8 *buffer;

void tracePrint(const char *format, ...);

#define va_start(ap, last) ((ap) = (va_list)__builtin_next_arg(last) - (8 - __builtin_args_info(2)) * 8)
#define va_end(ap) ((void)0)

typedef char *va_list;

extern const char tracePrintFormat[];
extern int printf(const char *format, ...);
extern int vsprintf(char *buffer, const char *format, va_list args);
extern const char tracePrintFormat[8];

/* The reset message resetWaitForPad shows. */

/* Word of sefUpdateEntryTransforms (sef.c). */
int sefFallbackVectorFlag = 0;

/* Shows the reset message until SELECT and START are held together. */
void resetWaitForPad(void)
{
    while ((PadData[0].heldAndPressed & (PAD_SELECT | PAD_START)) != (PAD_SELECT | PAD_START)) {
        xglFontDebugPrintf(8, 8, "RESET");
        xglRenderClearFrame();
        xglSleep();
    }
}

void tracePrint(const char *format, ...) {
    va_list args;
    char buffer[0x100];

    va_start(args, format);
    vsprintf(buffer, format, args);
    printf(tracePrintFormat, buffer);
    va_end(args);
}

/* Log call sites use this variadic no-op (lightSetLogDump passes a vector, a format and an index): it spills
 * its arguments and returns. */
void LOG(const void *data, const char *format, ...)
{
}

/* Scene flags of the sv line and image functions. */
unsigned int svSceneFlags = 0;
