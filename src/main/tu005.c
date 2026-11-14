#include "common.h"
#include "debug_break.h"
#include "main/tu005.h"

int printf(const char *format, ...);

void func_00194D08(void) {

}

/* Debug stubs: each passes a fixed "Don't use ..." message to printf. */
int xglDebFileSize(const char *name)
{
    printf("Don't use xglDebFileSize()\n");
    DEBUG_BREAK();
    return -1;
}

int xglDebFileRead(const char *name, void *buffer)
{
    printf("Don't use xglDebFileRead()\n");
    DEBUG_BREAK();
    return -1;
}

int xglDebFileWrite(const char *name, const void *buffer)
{
    printf("Don't use xglDebFileWrite()\n");
    DEBUG_BREAK();
    return -1;
}
