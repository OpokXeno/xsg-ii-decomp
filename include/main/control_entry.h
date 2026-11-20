#ifndef INCLUDE_MAIN_CONTROL_ENTRY_H
#define INCLUDE_MAIN_CONTROL_ENTRY_H

#include "shared.h"

/* Data and functions of src/main/control_entry.c that other files use. */

/* The end of the work heap: InitializeSystem sets it to the start of the heap and the carving
 * code (sound driver, LAN, resource loaders) advances it. */
extern u8 *WorkEnd;
/* Nonzero while the uwamono (map object) drawing is switched off. */
extern signed char uwamonoDrawDisabled;


#endif /* INCLUDE_MAIN_CONTROL_ENTRY_H */
