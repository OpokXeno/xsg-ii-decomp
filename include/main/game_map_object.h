#ifndef INCLUDE_MAIN_GAME_MAP_OBJECT_H
#define INCLUDE_MAIN_GAME_MAP_OBJECT_H

#include "main/game_data.h"

/* Functions and data of src/main/game_map_object.c that other files use. */

void mapResourceSwitch(int id, signed char mode);
int DispMapObject(void);
void mapObjectsDraw(void);

/* The map object resources, loaded by mapObjectResourcesLoad in the order of mapObjectResourceNames: four data files, then
 * the (model, texture) pair of every extra-object type (see MAP_MODEL_SLOT in game_map_object.c). */
#define MAP_RESOURCE_COUNT 54
enum {
    MAP_RESOURCE_KOWAREMONO = 0,    /* kowaremono.dat: the 48-byte object name rows */
    MAP_RESOURCE_TREASURE_BOX = 1,  /* treasurebox.dat: the uwamono event table */
    MAP_RESOURCE_ENEMY_ORDER = 2,   /* enemyorder.dat: the uwamono key rows */
    MAP_RESOURCE_SPLINE = 3         /* spline.dat */
};
extern void *mapObjectResources[MAP_RESOURCE_COUNT];

/* Bit n of the hide masks is set while map block entry n is hidden: the main object and the second pack (mapObjectTable); the
 * second pack's part mask is a separate word array. */
extern MapBits mapMainHideMask;
extern MapBits mapSecondHideMask;
extern int mapSecondPartMask[10];
/* Frames the pause-search wait still counts (game_camera.c counts it down, -1 once it ran out). */
extern int uwamonoSearchWaitCount;

#endif /* INCLUDE_MAIN_GAME_MAP_OBJECT_H */
