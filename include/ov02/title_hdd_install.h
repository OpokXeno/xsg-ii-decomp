#ifndef INCLUDE_OV02_TITLE_HDD_INSTALL_H
#define INCLUDE_OV02_TITLE_HDD_INSTALL_H

/* Functions of src/ov02/title_hdd_install.c that other files call. */

/* The install screens: the command TitleHddInstall takes (the title passes it for the state of the hard disk). */
#define HDD_SCREEN_INSTALL 0
#define HDD_SCREEN_UNINSTALL 1
#define HDD_SCREEN_MESSAGE_FIRST 2
#define HDD_SCREEN_MESSAGE_LAST 4
#define HDD_SCREEN_REACTIVATE 5

/* The HDD install dispatcher: shows the install or uninstall arm, or the message screen of command. */
void TitleHddInstall(int command);

#endif /* INCLUDE_OV02_TITLE_HDD_INSTALL_H */
