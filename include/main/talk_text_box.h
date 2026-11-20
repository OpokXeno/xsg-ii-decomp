#ifndef INCLUDE_MAIN_TALK_TEXT_BOX_H
#define INCLUDE_MAIN_TALK_TEXT_BOX_H

#include "main/timecode_counter.h"

/* Functions of src/main/talk_text_box.c. The talk text box is a MsgSlot (timecode_counter.h): x/y/z are the slot origin
 * and depth, u.text.start the panel height test, u.text.reveal the text, u.text.speaker_name the name plate and
 * u.text.args the text arguments. talkGetBoxLayoutX reads u.timecode.cursor_x (its caller draws a timecode slot). */

int talkGetBoxLayoutX(MsgSlot *box);
int talkGetBoxLayoutY(MsgSlot *box);
void talkDrawTextBox(MsgSlot *box);

#endif /* INCLUDE_MAIN_TALK_TEXT_BOX_H */
