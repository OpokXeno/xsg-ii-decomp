#ifndef INCLUDE_OV01_DATA_BOX_H
#define INCLUDE_OV01_DATA_BOX_H

/* Functions of src/ov01/data_box.c that other files call. */

/* Data box access: the quantity of item id in category 1 to 4 (dataBoxPtrGet answers NULL for any other category). */
u16 *dataBoxPtrGet(int category);
int dataBoxChk(int category, int id);
int dataBoxInc(int category, int id);
int dataBoxDec(int category, int id);

/* Per-category wrappers of dataBoxChk, dataBoxInc and dataBoxDec for categories 1 to 4. */
int dataBoxChkCategory1(int id);
int dataBoxIncCategory1(int id);
int dataBoxDecCategory1(int id);
int dataBoxChkCategory2(int id);
int dataBoxIncCategory2(int id);
int dataBoxDecCategory2(int id);
int dataBoxChkCategory3(int id);
int dataBoxIncCategory3(int id);
int dataBoxDecCategory3(int id);
int dataBoxChkCategory4(int id);
int dataBoxIncCategory4(int id);
int dataBoxDecCategory4(int id);
void dataBoxClearAll(void);

#endif /* INCLUDE_OV01_DATA_BOX_H */
