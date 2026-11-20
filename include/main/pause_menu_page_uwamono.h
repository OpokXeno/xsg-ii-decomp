#ifndef INCLUDE_MAIN_PAUSE_MENU_PAGE_UWAMONO_H
#define INCLUDE_MAIN_PAUSE_MENU_PAGE_UWAMONO_H

#include "shared.h"

/* Functions of src/main/pause_menu_page_uwamono.c that other files call: the uwamono debug outline drawers.
 * Their bodies only build the outline and draw nothing; the colour or mode argument is not read. */

void DispCollUwamono(void);
void DispPillar(Vector4 *source, float radius, float lift, int color);
void uwamonoDrawPointPath(int slotIndex);
void uwamonoDrawStateLinks(int slotIndex);
void uwamonoDrawPursuitDebug(int slotIndex);
void uwamonoDrawLine(const Vector4 *first, const Vector4 *second, int mode);
void uwamonoDrawFanEdge(const Vector4 *from, float angle, float radius, int mode);
void uwamonoDrawSegmentBox(const Vector4 *from, const Vector4 *to, const int color[4]);
void uwamonoDrawPointAxes(const Vector4 *position);

#endif /* INCLUDE_MAIN_PAUSE_MENU_PAGE_UWAMONO_H */
