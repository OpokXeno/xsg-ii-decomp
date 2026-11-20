#include "common.h"
#include "shared.h"
#include "ov01/data_box.h"
/* Category values as X1 names them. This overlay's dataBoxPtrGet answers only 1 to 4 (NULL for 0 and 10), so these names are not confirmed here. */
#define ITEM_CATEGORY_ITEM 0
#define ITEM_CATEGORY_WEAPON 1
#define ITEM_CATEGORY_BULLET 2
#define ITEM_CATEGORY_ACCESSORY 3
#define ITEM_CATEGORY_EVENT 10 /* event items: names come from EvtItemTbl */
/* Each category keeps DATA_BOX_SLOTS quantities of at most DATA_BOX_QUANTITY_MAX. */
#define DATA_BOX_SLOTS 255
#define DATA_BOX_QUANTITY_MAX 99

/* X1 box table names. This overlay's code reaches the tables through dataBoxPtrGet and uses none of these. */
extern u16 accBox[DATA_BOX_SLOTS];
extern u16 bltBox[DATA_BOX_SLOTS];
extern u16 evtBox[DATA_BOX_SLOTS];
extern u16 itmBox[DATA_BOX_SLOTS];
extern u16 wpnBox[DATA_BOX_SLOTS];
extern void *memset(void *destination, int value, unsigned int count);
extern u16 dataBoxQuantityTable1[DATA_BOX_SLOTS];
extern u16 dataBoxQuantityTable2[DATA_BOX_SLOTS];
extern u16 dataBoxQuantityTable3[DATA_BOX_SLOTS];
extern u16 dataBoxQuantityTable4[DATA_BOX_SLOTS];

/* Zeroes the four quantity tables. ov01InitAll (ov01 choice_menu.c) calls it among its setup calls. */
void dataBoxClearAll(void) {
    memset(dataBoxQuantityTable1, 0, sizeof dataBoxQuantityTable1);
    memset(dataBoxQuantityTable2, 0, sizeof dataBoxQuantityTable2);
    memset(dataBoxQuantityTable3, 0, sizeof dataBoxQuantityTable3);
    memset(dataBoxQuantityTable4, 0, sizeof dataBoxQuantityTable4);
}

/* Quantity table of a category: dataBoxQuantityTable1 for 1, dataBoxQuantityTable2 for 2, dataBoxQuantityTable3 for 3, dataBoxQuantityTable4 for 4; NULL for any other value. */
u16 *dataBoxPtrGet(int category)
{
    switch (category) {
    case 1:
        return dataBoxQuantityTable1;
    case 2:
        return dataBoxQuantityTable2;
    case 3:
        return dataBoxQuantityTable3;
    case 4:
        return dataBoxQuantityTable4;
    }
    return 0;
}

int dataBoxChk(int category, int id)
{
    return dataBoxPtrGet(category)[id - 1];
}

int dataBoxInc(int category, int id)
{
    unsigned short *quantity;

    quantity = dataBoxPtrGet(category) + id - 1;
    if (*quantity < DATA_BOX_QUANTITY_MAX) {
        *quantity += 1;
        return 1;
    }
    return 0;
}

int dataBoxDec(int category, int id)
{
    unsigned short *quantity;

    quantity = dataBoxPtrGet(category) + id - 1;
    if (*quantity != 0) {
        *quantity -= 1;
        return 1;
    }
    return 0;
}

/* Per-category wrappers for categories 1 to 4: Chk, Inc and Dec act on the quantity of one id. The original tail-calls
 * each one and returns its value. calcItmEnableChk (ov01 calc.c) reads the value of dataBoxChkCategory1. */
int dataBoxChkCategory1(int id) {
    return dataBoxChk(1, id);
}

int dataBoxIncCategory1(int id) {
    return dataBoxInc(1, id);
}

int dataBoxDecCategory1(int id) {
    return dataBoxDec(1, id);
}

int dataBoxChkCategory2(int id) {
    return dataBoxChk(2, id);
}

int dataBoxIncCategory2(int id) {
    return dataBoxInc(2, id);
}

int dataBoxDecCategory2(int id) {
    return dataBoxDec(2, id);
}

int dataBoxChkCategory3(int id) {
    return dataBoxChk(3, id);
}

int dataBoxIncCategory3(int id) {
    return dataBoxInc(3, id);
}

int dataBoxDecCategory3(int id) {
    return dataBoxDec(3, id);
}

int dataBoxChkCategory4(int id) {
    return dataBoxChk(4, id);
}

int dataBoxIncCategory4(int id) {
    return dataBoxInc(4, id);
}

int dataBoxDecCategory4(int id) {
    return dataBoxDec(4, id);
}
