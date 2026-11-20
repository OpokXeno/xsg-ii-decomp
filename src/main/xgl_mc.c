#include "common.h"
#include "main/xgl_mc.h"
#include "shared.h"
#include "main/xgl_mc_request.h"

extern u8 *buffer;

/* sceMcOpen modes (SCE_RDONLY, SCE_WRONLY | SCE_CREAT). */
#define MC_OPEN_READ 1
#define MC_OPEN_WRITE_CREATE 0x202

int sceMcOpen(int port, int slot, const char *name, int mode);
int sceMcClose(int fd);
int sceMcRead(int fd, void *buffer, int size);
int sceMcWrite(int fd, const void *buffer, int size);
int sceMcSync(int mode, int *command, int *result);
int sceMcGetInfo(int port, int slot, int *type, int *free, int *format);

int xglMcEasyLoad(const char *path, void *buffer, int size)
{
    int card_type;
    int command;
    int result;
    int descriptor;
    int read_result;

    sceMcGetInfo(0, 0, 0, 0, &card_type);
    sceMcSync(0, &command, &result);
    if (card_type == 0)
    {
        return -1;
    }
    if (result < -1)
    {
        return result;
    }

    sceMcOpen(0, 0, path, MC_OPEN_READ);
    sceMcSync(0, &command, &result);
    descriptor = result;
    if (descriptor < 0)
    {
        return descriptor;
    }

    sceMcRead(descriptor, buffer, size);
    sceMcSync(0, &command, &result);
    read_result = result;
    if (read_result < 0)
    {
        return read_result;
    }

    sceMcClose(descriptor);
    sceMcSync(0, &command, &result);
    if (result < 0)
    {
        return result;
    }
    return read_result;
}

int xglMcEasySave(const char *path, const void *buffer, int size)
{
    int card_type;
    int command;
    int result;
    int descriptor;

    sceMcGetInfo(0, 0, 0, 0, &card_type);
    sceMcSync(0, &command, &result);
    if (result < -1 || card_type == 0)
    {
        return -1;
    }

    sceMcOpen(0, 0, path, MC_OPEN_WRITE_CREATE);
    sceMcSync(0, &command, &result);
    descriptor = result;
    if (descriptor < 0)
    {
        return -1;
    }

    sceMcWrite(descriptor, buffer, size);
    sceMcSync(0, &command, &result);
    if (result < 0)
    {
        return -1;
    }

    sceMcClose(descriptor);
    sceMcSync(0, &command, &result);
    return result >= 0 ? 0 : -1;
}

extern void *memset(void *destination, int value, unsigned int count);
void sceMcInit(void);

/* Memory-card init: the SDK init, the request reset (xglMcReset), then the save buffer clear. Called from InitializeSystem. */
void xglMcInitial(void)
{
    sceMcInit();
    xglMcReset();
    memset(&SaveData, 0, sizeof(SaveData));
}
