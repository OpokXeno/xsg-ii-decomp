#ifndef INCLUDE_MAIN_BATTLE_RESULT_H
#define INCLUDE_MAIN_BATTLE_RESULT_H

/* Functions of src/main/battle_result.c that other files call. */

/* Label texts of the battle result and choice menus (8-byte .sdata strings, declared without a size so that
 * their users address them as externs, not as gp-relative small data). */
extern char statusLabelText[];
extern char wEtherLabelText[];
extern char specialLabelText[];
extern char doubleLabelText[];
extern char emptyLabelText[];
extern const char explanationLabelText[];

/* Runs the battle result scene. Both callers pass 0 in mode; the body does not read it. */
void battleResultScene(int mode);

#endif /* INCLUDE_MAIN_BATTLE_RESULT_H */
