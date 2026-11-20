#ifndef INCLUDE_OV02_TITLE_H
#define INCLUDE_OV02_TITLE_H

/* The title screen of the ov02 overlay. Returns the chosen screen: the low 24 bits are its entry address, the top byte
 * its overlay (control_entry.c splits it); 0 means stay on the title. */
unsigned int Title(void);

#endif /* INCLUDE_OV02_TITLE_H */
