#include "common.h"
#include "main/assert_fail.h"

int printf(const char *format, ...);
/* Exit path shared by the assertion and error reporters: prints "> Exit Program", then traps (break). */ void assertExit(void);
extern const char assertBannerTop[];
extern const char assertFormat[];
extern const char assertBannerBottom[];

/* Assertion failure report: prints the banner, "ASSERTION <file, line> :: message", then exits. */
void assertFailed(const char *message, const char *file, int line) {
    printf(assertBannerTop);
    printf(assertFormat, file, line, message);
    printf(assertBannerBottom);
    assertExit();
}

#include "debug_break.h"

extern const char exitProgramBanner[];

/* Exit path shared by the assertion and error reporters: prints "> Exit Program", then traps (break). */
void assertExit(void) {
    printf(exitProgramBanner);
    DEBUG_BREAK();
}

typedef char *va_list;

#define va_start(ap, last) ((ap) = (va_list)__builtin_next_arg(last) - (8 - __builtin_args_info(2)) * 8)

extern const char errorBanner[];
int vprintf(const char *format, va_list args);

/* Fatal error with a printf-style message: prints "> Error!!", the formatted message, then exits. */
void errorFatal(const char *format, ...) {
    va_list args;

    printf(errorBanner);
    va_start(args, format);
    vprintf(format, args);
    printf(format);
    assertExit();
}

const char assertBannerTop[64] = "_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_\n\n";

const char assertFormat[32] = "ASSERTION <%s, %d> :: %s\n\n";

const char assertBannerBottom[64] = "_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_\n";

const char exitProgramBanner[64] = "> Exit Program ==========================================\n";

const char errorBanner[16] = "> Error!!\n";
