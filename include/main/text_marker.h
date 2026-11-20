#ifndef INCLUDE_MAIN_TEXT_MARKER_H
#define INCLUDE_MAIN_TEXT_MARKER_H

/* Functions of src/main/text_marker.c that other files call. */

/* The stats the ANALYZE panel shows for an enemy (analyzePanelStart copies the 40 bytes). */
typedef struct AnalyzeStats {
    int hp;                /* +0x00: negative when unknown */
    int maxHp;             /* +0x04 */
    int ep;                /* +0x08 */
    int maxEp;             /* +0x0C */
    int exp;               /* +0x10 */
    int skillPoints;       /* +0x14 */
    int classPoints;       /* +0x18 */
    int rareItem;          /* +0x1C: 0 none, negative unknown */
    int item;              /* +0x20 */
    signed char attackType;    /* +0x24: weak attack type, 0 none */
    unsigned char attributes;  /* +0x25: bit i set when attribute i is a weakness */
    signed char rareItemKind;  /* +0x26: 2 item, otherwise a hit action */
    signed char itemKind;      /* +0x27: same for item */
} AnalyzeStats;

/* Labels of the ANALYZE panel's item rows; choice_menu.c uses them as text styles. */
extern char itemLabelText[];  /* 8 bytes in .sdata; the size is left open so users do not address it gp-relative */
extern char etherLabelText[];

int analyzePanelIsActive(void);
void analyzePanelClear(void);
void analyzePanelStart(int x, int y, const AnalyzeStats *stats);
void textUpdate(void);

#endif /* INCLUDE_MAIN_TEXT_MARKER_H */
