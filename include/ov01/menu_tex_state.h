#ifndef INCLUDE_OV01_MENU_TEX_STATE_H
#define INCLUDE_OV01_MENU_TEX_STATE_H

#include "ov01/gr_gp_init.h"

/* Functions of src/ov01/menu_tex_state.c that other files call. */

/* Texture flush for the menu (waits on dataStatBufGet, then one GS_TEXFLUSH write). */
void menuTexFlush(void);
/* The cell index (0 to 12) of a unit key in the menu cell texture tables. */
int menuTexCellIndex(int key);
/* Emits one 16-byte texture state record into the packet. */
void grPacketTexState(GrPacket *packet, void *record);
void menuTexStateByKey(GrPacket *packet, int key);
/* The same record chosen by key from the alternate table. */
void menuTexStateAltByKey(GrPacket *packet, int key);
void menuSmallCellTexState(GrPacket *packet);
void menuNumTexState(GrPacket *packet);
void menuIconTexState(GrPacket *packet);
void dataObjSpriteTexState(GrPacket *packet);
void menuUnitSlotTexState(GrPacket *packet);
void menuHighlightTexState(GrPacket *packet);
void menuRowCursorTexState(GrPacket *packet);
void menuNumStyle1TexState(GrPacket *packet);
void menuPanelTexState(GrPacket *packet);
void menuStatusIconsTexState(GrPacket *packet);
void menuGaugeTexState(GrPacket *packet);
/* Texture state of a function of the number at 0xAB6A88 (called from src/ov01/menu.c and act_step_icon.c). */
void menuStepIconTexState(GrPacket *packet);

#endif /* INCLUDE_OV01_MENU_TEX_STATE_H */
