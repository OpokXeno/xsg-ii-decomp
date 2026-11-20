#ifndef INCLUDE_MAIN_SAVE_PARTY_INIT_H
#define INCLUDE_MAIN_SAVE_PARTY_INIT_H

#include "main/pt_member.h"

/* Functions of src/main/save_party_init.c that other files call. */

void uwamonoDetachCurrent(void);

void saveDataSetDefaults(void);
void menuRequestLoad(void);
void menuLoadTxds(void);
void ptReset(void);
PartyMember *ptMemberRecordAt(int index);
PartyMember *ptMemberRecordFindById(int id);
void uwamonoAttachCurrent(unsigned int kind, int runSplit);

#endif /* INCLUDE_MAIN_SAVE_PARTY_INIT_H */
