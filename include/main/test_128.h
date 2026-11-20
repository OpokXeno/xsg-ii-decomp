#ifndef INCLUDE_MAIN_TEST_128_H
#define INCLUDE_MAIN_TEST_128_H

/* Functions of src/main/test_128.c that other files call. */

struct RgHeap;

struct RgHeap *InstanceOfRgHeapById(unsigned int heapId);
void InitRgHeap(struct RgHeap *pHeap, void *pMemory, int size);
/* A block of size bytes from the heap heapId, stamped with the calling source file and line; 0 when it does not fit. */
void *RgHeapAllocById(unsigned int heapId, unsigned int size, const char *pSourceFile, int line);
void RgHeapFree(void *pPtr, const char *pSourceFile, int line);

#endif /* INCLUDE_MAIN_TEST_128_H */
