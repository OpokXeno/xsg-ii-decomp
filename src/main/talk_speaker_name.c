#include "common.h"
#include "main/talk_speaker_name.h"

/* Two leading bytes of the text, stashed into talkPrefixStash by talkStashPrefix. */
extern signed char talkPrefixStash[2];

/* Moves the two leading bytes of the text into talkPrefixStash and blanks them in the text; the second only when it is non-zero. */
void talkStashPrefix(MsgSlot *box)
{
    MsgSlotText *body;
    signed char *text;

    /* The original clears both stash bytes with one halfword store (a single sh of $zero). Plain
       talkPrefixStash[0] = 0; talkPrefixStash[1] = 0; compiles to two sb and changes the stash addressing
       (26 differing rows). */
    *(short *)talkPrefixStash = 0;
    body = &box->u.text; /* body address kept in a register, as in the original */
    text = (signed char *)body->reveal;
    talkPrefixStash[0] = text[0];
    text[0] = 0;
    if (text[1] != 0) {
        talkPrefixStash[1] = text[1];
        text[1] = 0;
    }
}

/* Puts the stashed leading bytes back into the text; the second byte only when it is non-zero. */
void talkRestorePrefix(MsgSlot *box)
{
    unsigned char *text;

    text = (unsigned char *) box->u.text.reveal; /* reveal is const char * for readers; the stashed prefix is written back through it */
    text[0] = talkPrefixStash[0];
    if (talkPrefixStash[1] != 0) {
        text[1] = talkPrefixStash[1];
    }
}

/* Copies the speaker name into buffer up to NUL or ']' and terminates it. */
void talkCopySpeakerName(unsigned char *buffer, MsgSlot *box)
{
    const char *src;
    int ch;
    int next;
    unsigned char out;

    src = box->u.text.speaker_name;
    if (src != 0) {
        ch = *src;
        out = *src;
        if (ch != 0 && ch != ']') {
            do {
                *buffer = out;
                buffer += 1;
                src += 1;
                next = *src;
                out = *src;
            } while (next != 0 && next != ']');
        }
        *buffer = 0;
    }
}

/* Scaffold data of this file (.sdata), in address order. The string items are read by the other main TUs
 * (main/debt_box.c and others) through their own extern declarations. */
signed char talkPrefixStash[2] = { 0, 0 };
