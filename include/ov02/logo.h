#ifndef INCLUDE_OV02_LOGO_H
#define INCLUDE_OV02_LOGO_H

/* Functions of src/ov02/logo.c that other files call. */

/* The logo screens and the memory card check that follows them. */
void LogoFirst(void);
/* The credit roll: loads credit.bin and draws it in bands. */
void logoShowSlides(void);

#endif /* INCLUDE_OV02_LOGO_H */
