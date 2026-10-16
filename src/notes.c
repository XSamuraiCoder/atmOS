/* notes.c - built-in Notes app for the TI-84 Plus CE shell
 *
 * List of notes (pointer-first, like Files) plus a keyboard text editor.
 * Stored in the AppVar SHNOTES (RAM on every change, archived on exit).
 */
#include <graphx.h>
#include <fileioc.h>
#include <keypadc.h>
#include <ti/tokens.h>
#include <ti/vars.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "notes.h"
#include "settings.h"
#include "ui_text.h"
#include "debug_stack.h"

/* palette indices (same as main.c / files.c) */
enum {
    PAL_BAR = 16, PAL_BAR_LINE = 17, PAL_DOCK = 18,
    PAL_TEXT = 19, PAL_LABEL = 20, PAL_ACCENT = 21,
    PAL_ICON0 = 22                      /* red,yl,grn,blu,pur,cyn,org,gray */
};

#define WIN_W       304
#define CONTENT_H   180

#define NOTES_MAX   5
#define NOTE_CAP    160                 /* bytes of text per note (RAM is tight) */
#define STORE_NAME  "SHNOTES"

/* list layout */
#define ROW_H_MIN   20

/* rows grow with the active font so a tall custom font never overlaps */
static int row_h(void)
{
    int h = (int)ui_text_height() + 10;
    return h < ROW_H_MIN ? ROW_H_MIN : h;
}
static int row_step(void)
{
    return row_h() + 2;
}

#define ROW_Y0      4
#define ROW_X       6
#define ROW_W       (WIN_W - 12)

/* editor layout: 8px monospace cells, 10px lines */
#define BOX_X       6
#define BOX_Y       6
#define BOX_W       (WIN_W - 12)
#define BOX_H       140
#define CELL_W      8
#define LINE_H      10
#define COLS        ((BOX_W - 8) / CELL_W)
#define ROWS        ((BOX_H - 8) / LINE_H)

#define PROGRAM_ASM   0
#define PROGRAM_C     1
#define PROGRAM_BASIC 2
#define PROGRAM_ICE   3

typedef struct {
    uint16_t len;
    char     text[NOTE_CAP];
} note_t;

static note_t  notes[NOTES_MAX];
static uint8_t note_count;
static bool    loaded;
static bool    changed;                 /* RAM copy differs from the AppVar */
static bool    archive_pending;

enum { MODE_LIST, MODE_EDIT };
static int  mode = MODE_LIST;
static int  cur;                        /* note being edited */
static int  caret, top;                 /* editor caret index / first visible line */
static int  hover_row = -1;
static int  del_row = -1;               /* list row awaiting delete confirmation */
static bool alpha_on, lower_on;
static uint16_t prev[8];
static uint8_t  held[5];                /* repeat counters: del, up, down, left, right */

#define NEW(row, key) ((kb_Data[row] & (key)) && !(prev[row] & (key)))

/* ---------- storage ---------- */
static void load_notes(void)
{
    uint8_t h = ti_Open(STORE_NAME, "r");
    uint8_t count = 0, i;

    loaded = true;
    note_count = 0;
    if (!h) return;
    if (ti_Read(&count, 1, 1, h) == 1) {
        for (i = 0; i < count && i < NOTES_MAX; i++) {
            uint16_t len, keep;
            if (ti_Read(&len, sizeof(len), 1, h) != 1) break;
            keep = len < NOTE_CAP ? len : NOTE_CAP - 1;      /* older, longer notes are trimmed */
            if (keep && ti_Read(notes[i].text, 1, keep, h) != keep) break;
            if (len > keep) ti_Seek(len - keep, SEEK_CUR, h);
            notes[i].len = keep;
            note_count = (uint8_t)(i + 1);
        }
    }
    ti_Close(h);
}

static void store_notes(void)
{
    uint8_t h, i;

    if (!changed) return;
    h = ti_Open(STORE_NAME, "w");
    if (!h) return;
    ti_Write(&note_count, 1, 1, h);
    for (i = 0; i < note_count; i++) {
        ti_Write(&notes[i].len, sizeof(uint16_t), 1, h);
        if (notes[i].len) ti_Write(notes[i].text, 1, notes[i].len, h);
    }
    ti_Close(h);
    changed = false;
    archive_pending = true;
}

static void delete_note(int idx)
{
    int i;
    for (i = idx; i + 1 < note_count; i++) notes[i] = notes[i + 1];
    if (note_count) note_count--;
    changed = true;
    store_notes();
}

/* ---------- text layout (hard wrap at COLS, '\n' breaks) ---------- */
static void pos_of(const note_t *n, int idx, int *line, int *col)
{
    int l = 0, c = 0, i;
    for (i = 0; i < idx; i++) {
        if (n->text[i] == '\n') { l++; c = 0; }
        else if (++c == COLS)   { l++; c = 0; }
    }
    *line = l;
    *col = c;
}

/* index of (line, col), clamped to the end of that line; -1 if no such line */
static int index_at(const note_t *n, int line, int col)
{
    int l = 0, c = 0, i, best = -1;
    for (i = 0;; i++) {
        if (l == line) {
            if (c <= col) best = i; else break;
        } else if (l > line) {
            break;
        }
        if (i >= n->len) break;
        if (n->text[i] == '\n') { l++; c = 0; }
        else if (++c == COLS)   { l++; c = 0; }
    }
    return best;
}

static void scroll_to_caret(void)
{
    int l, c;
    pos_of(&notes[cur], caret, &l, &c);
    if (l < top) top = l;
    if (l >= top + ROWS) top = l - ROWS + 1;
}

static bool insert_char(char ch)
{
    note_t *n = &notes[cur];
    if (n->len >= NOTE_CAP - 1) return false;
    memmove(&n->text[caret + 1], &n->text[caret], (size_t)(n->len - caret));
    n->text[caret++] = ch;
    n->len++;
    changed = true;
    scroll_to_caret();
    return true;
}

static bool backspace(void)
{
    note_t *n = &notes[cur];
    if (caret == 0) return false;
    memmove(&n->text[caret - 1], &n->text[caret], (size_t)(n->len - caret));
    caret--;
    n->len--;
    changed = true;
    scroll_to_caret();
    return true;
}

static void leave_editor(void)
{
    if (mode != MODE_EDIT) return;
    if (notes[cur].len == 0) delete_note(cur);   /* empty notes are dropped */
    else store_notes();
    mode = MODE_LIST;
}

/* ---------- public: lifecycle ---------- */
void notes_open(void)
{
    int i;
    if (!loaded) load_notes();
    if (mode == MODE_EDIT) leave_editor();
    mode = MODE_LIST;
    hover_row = del_row = -1;
    for (i = 0; i < 8; i++) prev[i] = kb_Data[i];   /* don't replay the launch click */
}

void notes_flush(void)
{
    uint8_t h;
    if (!loaded) return;
    if (mode == MODE_EDIT) leave_editor();
    store_notes();
    if (archive_pending) {
        h = ti_Open(STORE_NAME, "r");
        if (h) { ti_SetArchiveStatus(true, h); ti_Close(h); }
        archive_pending = false;
    }
}

bool notes_cancel(void)
{
    if (mode != MODE_EDIT) return false;
    leave_editor();
    hover_row = del_row = -1;
    return true;
}

/* ---------- public: NOTES program detection ---------- */
bool notes_find_program(const char **name, uint8_t *var_type, uint8_t *program_kind)
{
    static const char prgm_name[9] = "NOTES";          /* zero padded: asm copies 8 bytes */
    static const uint8_t types[2] = { OS_TYPE_PRGM, OS_TYPE_PROT_PRGM };
    int i;

    for (i = 0; i < 2; i++) {
        uint8_t h = ti_OpenVar(prgm_name, "r", types[i]);
        if (h) {
            uint8_t sig[3];
            uint8_t kind = PROGRAM_BASIC;
            if (ti_Read(sig, 1, sizeof(sig), h) == sizeof(sig) &&
                sig[0] == OS_TOK_2BYTE_EXT &&
                (sig[1] == OS_TOK_ASM_84CE_PRGM_CMP ||
                 sig[1] == OS_TOK_ASM_84C_PRGM_CMP)) {
                if (sig[2] == 0)         kind = PROGRAM_C;
                else if (sig[2] == 0x7F) kind = PROGRAM_ICE;
                else                     kind = PROGRAM_ASM;
            }
            ti_Close(h);
            *name = prgm_name;
            *var_type = types[i];
            *program_kind = kind;
            return true;
        }
    }
    return false;
}

/* ---------- drawing ---------- */
static void text_color(uint8_t fg)
{
    ui_text_color(fg);
    gfx_SetTextBGColor(255);
    gfx_SetTextTransparentColor(255);
}

static void print_clipped(const char *s, int len, int x, int y, int max_w)
{
    char buf[48];
    if (len > (int)sizeof(buf) - 1) len = (int)sizeof(buf) - 1;
    memcpy(buf, s, (size_t)len);
    buf[len] = 0;
    while (len > 0 && (int)ui_text_width(buf) > max_w) buf[--len] = 0;
    ui_print(buf, x, y);
}

static void draw_row(int x, int y, int r, bool hot)
{
    int ry = y + ROW_Y0 + r * row_step();
    int rx = x + ROW_X;
    bool warn = (r > 0 && r == del_row);

    if (r == 0) {
        bool full = note_count >= NOTES_MAX;
        gfx_SetColor(full ? PAL_BAR_LINE : PAL_ACCENT);
        gfx_FillRectangle(rx, ry, ROW_W, row_h());
        text_color(PAL_LABEL);
        ui_print(full ? "Notes full - delete one" : "+  New note", rx + 8, ry + (row_h() - 8) / 2);
        if (hot && !full) {
            gfx_SetColor(PAL_LABEL);
            gfx_Rectangle(rx, ry, ROW_W, row_h());
        }
        return;
    }

    gfx_SetColor(warn ? PAL_ICON0 : (hot ? PAL_DOCK : PAL_LABEL));
    gfx_FillRectangle(rx, ry, ROW_W, row_h());
    gfx_SetColor(PAL_BAR_LINE);
    gfx_Rectangle(rx, ry, ROW_W, row_h());

    {
        const note_t *n = &notes[r - 1];
        int tl = 0;
        char cnt[12];
        int v = n->len, k = 0, w;

        if (warn) {
            text_color(PAL_LABEL);
            ui_print("Press del again to delete", rx + 8, ry + (row_h() - 8) / 2);
            return;
        }
        while (tl < n->len && n->text[tl] != '\n') tl++;
        text_color(PAL_TEXT);
        if (tl == 0) ui_print("(empty)", rx + 8, ry + (row_h() - 8) / 2);
        else print_clipped(n->text, tl, rx + 8, ry + (row_h() - 8) / 2, ROW_W - 70);

        /* "<len>c" right-aligned, gray */
        {
            char tmp[8];
            int t = 0;
            do { tmp[t++] = (char)('0' + v % 10); v /= 10; } while (v && t < 6);
            while (t) cnt[k++] = tmp[--t];
            cnt[k++] = 'c';
            cnt[k] = 0;
        }
        w = ui_text_width(cnt);
        text_color(PAL_ICON0 + 7);
        ui_print(cnt, rx + ROW_W - 8 - w, ry + (row_h() - 8) / 2);
    }
}

static void draw_list(int x, int y)
{
    int r;
    for (r = 0; r <= note_count; r++) draw_row(x, y, r, r == hover_row);

    text_color(PAL_TEXT);
    ui_print(note_count ? "del: delete hovered note" : "No notes yet",
                      x + ROW_X, y + CONTENT_H - 16);
}

static void draw_editor(int x, int y)
{
    const note_t *n = &notes[cur];
    int bx = x + BOX_X, by = y + BOX_Y;
    int i, l = 0, c = 0, cl, cc;
    char ch[2] = { 0, 0 };
    char info[24];

    gfx_SetColor(PAL_LABEL);
    gfx_FillRectangle(bx, by, BOX_W, BOX_H);
    gfx_SetColor(PAL_BAR_LINE);
    gfx_Rectangle(bx, by, BOX_W, BOX_H);

    gfx_SetMonospaceFont(CELL_W);
    text_color(PAL_TEXT);
    for (i = 0; i < n->len; i++) {
        char t = n->text[i];
        if (t == '\n') { l++; c = 0; continue; }
        if (l >= top && l < top + ROWS) {
            ch[0] = t;
            gfx_PrintStringXY(ch, bx + 4 + c * CELL_W, by + 4 + (l - top) * LINE_H);
        }
        if (++c == COLS) { l++; c = 0; }
        if (l >= top + ROWS) break;
    }
    settings_apply_font();      /* back to the font chosen in Settings */

    pos_of(n, caret, &cl, &cc);
    if (cl >= top && cl < top + ROWS) {
        gfx_SetColor(PAL_ACCENT);
        gfx_FillRectangle(bx + 4 + cc * CELL_W, by + 3 + (cl - top) * LINE_H, 2, 10);
    }

    /* status line: input mode on the left, usage on the right */
    text_color(PAL_TEXT);
    ui_print(alpha_on ? (lower_on ? "ALPHA abc" : "ALPHA ABC") : "123", x + BOX_X, y + BOX_Y + BOX_H + 6);
    {
        int v = n->len, k = 0, t = 0, w;
        char tmp[8];
        do { tmp[t++] = (char)('0' + v % 10); v /= 10; } while (v && t < 6);
        while (t) info[k++] = tmp[--t];
        info[k++] = '/';
        v = NOTE_CAP - 1; t = 0;
        do { tmp[t++] = (char)('0' + v % 10); v /= 10; } while (v && t < 6);
        while (t) info[k++] = tmp[--t];
        info[k] = 0;
        w = ui_text_width(info);
        ui_print(info, x + WIN_W - BOX_X - w, y + BOX_Y + BOX_H + 6);
    }
    text_color(PAL_ICON0 + 7);
    ui_print("alpha:letters  2nd:case  del:erase  clear:back",
                      x + BOX_X, y + CONTENT_H - 10);
}

void notes_draw(int x, int y)
{
    NOTE_STACK();
    gfx_SetTextScale(1, 1);
    if (mode == MODE_EDIT) draw_editor(x, y);
    else draw_list(x, y);
}

/* ---------- list interaction ---------- */
static int row_at(int x, int y)
{
    int r;
    if (x < ROW_X || x >= ROW_X + ROW_W || y < ROW_Y0) return -1;
    r = (y - ROW_Y0) / row_step();
    if ((y - ROW_Y0) % row_step() >= row_h()) return -1;
    return r <= note_count ? r : -1;
}

bool notes_hover(int x, int y)
{
    int r;
    if (mode != MODE_LIST) return false;
    r = row_at(x, y);
    if (r == hover_row) return false;
    hover_row = r;
    del_row = -1;                       /* moving away cancels a pending delete */
    return true;
}

static void open_note(int idx)
{
    cur = idx;
    caret = notes[idx].len;
    top = 0;
    alpha_on = false;
    mode = MODE_EDIT;
    scroll_to_caret();
}

void notes_click(int x, int y)
{
    int r;
    if (mode != MODE_LIST) return;
    r = row_at(x, y);
    del_row = -1;
    if (r < 0) return;
    if (r == 0) {
        if (note_count >= NOTES_MAX) return;
        notes[note_count].len = 0;
        note_count++;
        open_note(note_count - 1);
        return;
    }
    open_note(r - 1);
}

/* ---------- keyboard ---------- */
static const struct { uint8_t row, mask; char plain, alpha; } keymap[] = {
    {2, kb_Math, 0, 'A'},  {3, kb_Apps, 0, 'B'},  {4, kb_Prgm, 0, 'C'},
    {2, kb_Recip, 0, 'D'}, {3, kb_Sin, 0, 'E'},   {4, kb_Cos, 0, 'F'},
    {5, kb_Tan, 0, 'G'},   {6, kb_Power, '^', 'H'},{2, kb_Square, 0, 'I'},
    {3, kb_Comma, ',', 'J'},{4, kb_LParen, '(', 'K'},{5, kb_RParen, ')', 'L'},
    {6, kb_Div, '/', 'M'}, {2, kb_Log, 0, 'N'},   {3, kb_7, '7', 'O'},
    {4, kb_8, '8', 'P'},   {5, kb_9, '9', 'Q'},   {6, kb_Mul, '*', 'R'},
    {2, kb_Ln, 0, 'S'},    {3, kb_4, '4', 'T'},   {4, kb_5, '5', 'U'},
    {5, kb_6, '6', 'V'},   {6, kb_Sub, '-', 'W'}, {2, kb_Sto, 0, 'X'},
    {3, kb_1, '1', 'Y'},   {4, kb_2, '2', 'Z'},   {3, kb_0, '0', ' '},
    {4, kb_DecPnt, '.', ':'},{5, kb_Chs, '_', '?'},{5, kb_3, '3', '!'},
    {6, kb_Add, '+', '"'}
};

/* held-key repeat: fires on press, then after ~20 frames every 3 frames */
static bool repeat(uint8_t *h, bool down)
{
    if (!down) { *h = 0; return false; }
    if (*h < 255) (*h)++;
    return *h == 1 || (*h > 20 && (*h % 3) == 0);
}

static bool edit_keys(void)
{
    bool did = false;
    bool arrows = (kb_Data[7] & (kb_Up | kb_Down | kb_Left | kb_Right)) != 0;
    size_t i;
    int l, c, idx;

    if (NEW(2, kb_Alpha)) { alpha_on = !alpha_on; did = true; }

    for (i = 0; i < sizeof(keymap) / sizeof(keymap[0]); i++) {
        char ch;
        if (!NEW(keymap[i].row, keymap[i].mask)) continue;
        ch = alpha_on ? keymap[i].alpha : keymap[i].plain;
        if (!ch) continue;
        if (alpha_on && lower_on && ch >= 'A' && ch <= 'Z') ch = (char)(ch + 32);
        if (insert_char(ch)) did = true;
    }

    if (repeat(&held[0], (kb_Data[1] & kb_Del) != 0) && backspace()) did = true;

    if (repeat(&held[3], (kb_Data[7] & kb_Left) != 0) && caret > 0) { caret--; scroll_to_caret(); did = true; }
    if (repeat(&held[4], (kb_Data[7] & kb_Right) != 0) && caret < notes[cur].len) { caret++; scroll_to_caret(); did = true; }
    if (repeat(&held[1], (kb_Data[7] & kb_Up) != 0)) {
        pos_of(&notes[cur], caret, &l, &c);
        idx = l > 0 ? index_at(&notes[cur], l - 1, c) : -1;
        if (idx >= 0) { caret = idx; scroll_to_caret(); did = true; }
    }
    if (repeat(&held[2], (kb_Data[7] & kb_Down) != 0)) {
        pos_of(&notes[cur], caret, &l, &c);
        idx = index_at(&notes[cur], l + 1, c);
        if (idx >= 0) { caret = idx; scroll_to_caret(); did = true; }
    }

    return did || arrows;               /* arrows never move the pointer in the editor */
}

static bool list_keys(void)
{
    if (NEW(1, kb_Del) && hover_row > 0) {
        if (del_row == hover_row) {
            delete_note(hover_row - 1);
            del_row = -1;
            if (hover_row > note_count) hover_row = -1;
        } else {
            del_row = hover_row;
        }
        return true;
    }
    return false;                       /* pointer-first: arrows move the pointer */
}

bool notes_keys(void)
{
    bool used;
    int i;

    used = (mode == MODE_EDIT) ? edit_keys() : list_keys();
    for (i = 0; i < 8; i++) prev[i] = kb_Data[i];
    return used;
}

bool notes_press(void)
{
    if (mode != MODE_EDIT) return false;        /* list: Enter clicks the hovered row */
    if (kb_Data[6] & kb_Enter) insert_char('\n');
    else lower_on = !lower_on;                  /* 2nd toggles letter case */
    return true;
}