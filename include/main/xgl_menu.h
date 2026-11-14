#ifndef INCLUDE_MAIN_XGL_MENU_H
#define INCLUDE_MAIN_XGL_MENU_H

/* Menu descriptions and the functions of src/main/xgl_menu.c that other files call. */

typedef struct XglMenuNode XglMenuNode;

/*
 * The row's link word is either the child menu node pointer or, for a row that
 * ends the menu, a plain result value; xgl_menu.c reads it through both views.
 */
typedef union XglMenuRowLink {
    XglMenuNode *node;
    int result;
} XglMenuRowLink;

typedef struct XglMenuRow {
    const char *text;
    XglMenuRowLink next;
} XglMenuRow;

struct XglMenuNode {
    unsigned char item_count;
    struct { signed char initial_selected_item; unsigned char flags; unsigned char row_spacing; } initial;
    signed char selected_item;
    unsigned char unmodeled_05[3];
    int enter_result;
    int cancel_result;
    unsigned char unmodeled_10[8];
    XglMenuRow *rows;
};

/* xglMenuOpen's request: what to open, where, and where the result goes. */
typedef struct XglMenuParams {
    unsigned char type;
    unsigned char select_flags;
    unsigned char unmodeled_02[2];
    short x;
    short y;
    short z;
    unsigned char unmodeled_0a[2];
    int default_result;
    XglMenuNode *root;
    int *result_out;
} XglMenuParams;

/* xglMenuOpen slot: the first inactive one. */
#define XGL_MENU_SLOT_ANY (-1)

void xglMenuInitial(void);
int xglMenuOpen(int slot, XglMenuParams *params);
void xglMenuDraw(void);

#endif /* INCLUDE_MAIN_XGL_MENU_H */
