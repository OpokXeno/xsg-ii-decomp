#include "common.h"
#include "main/talk_text_box.h"
#include "main/talk_speaker_name.h"
#include "main/timecode_counter.h"
#include "main/debt_box.h"
#include "main/xgl_font.h"


/* Returns talkBoxContentX(box) plus the box's bias. */
int talkGetBoxLayoutX(MsgSlot *box)
{
    MsgSlotTimecode *body;

    body = &box->u.timecode; /* the original keeps the body address (slot + 0x28) in a register */
    return talkBoxContentX(box) + body->cursor_x;
}


/* Returns talkBoxContentY(box), the vertical counterpart of talkGetBoxLayoutX. */
int talkGetBoxLayoutY(MsgSlot *box)
{
    return talkBoxContentY(box);
}


#define TALK_NAME_BUFFER_WORDS 0x10 /* room for the copied speaker name */
#define TALK_TEXT_INDENT 0x20       /* x of the text and of the name plate, from the box origin */
#define TALK_NAME_RISE 0x12         /* the name plate sits this far above the box origin */
#define TALK_TEXT_Z_BIAS 7          /* depth of the text relative to box->z */

/*
 * Draws one talk text box. When it is visible: the speaker name plate first
 * (only when a speaker name is present), then the text and a flush of the
 * font batch.
 */
void talkDrawTextBox(MsgSlot *box)
{
    MsgSlotText *body;
    unsigned int buffer[TALK_NAME_BUFFER_WORDS]; /* word buffer because drawShadowedText takes unsigned int *; filled bytewise */
    int row_y;

    body = &box->u.text; /* the original keeps the body address (slot + 0x28) in a register */
    if (body->start != 0)
    {
        if (body->speaker_name != 0)
        {
            talkCopySpeakerName((unsigned char *) buffer, box);
            xglFontPrintExtFunc(box->z, talkBoxScissorCallback, (void *) 1);
            drawShadowedText(box->x + TALK_TEXT_INDENT, box->y - TALK_NAME_RISE, box->z + TALK_TEXT_Z_BIAS, &buffer[0]);
        }
        xglFontPrintExtFunc(box->z, talkBoxScissorCallback, box);
        row_y = textSlotFirstRowY(box);
        talkStashPrefix(box);
        fontPrintIntRow(box->x + TALK_TEXT_INDENT, row_y, box->z + TALK_TEXT_Z_BIAS, body->start, body->args);
        talkRestorePrefix(box);
        xglFontFlush();
        xglFontPrintExtFunc(0, talkBoxScissorCallback, (void *) 1);
    }
}
