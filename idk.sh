cd ./src &&  python3 - <<'EOF'
# ---------------- settings.h / settings.c ----------------
def edit(path, pairs):
    s = open(path, newline='').read()
    crlf = '\r\n' in s
    N = (lambda t: t.replace('\n', '\r\n')) if crlf else (lambda t: t)
    for label, old, new in pairs:
        old, new = N(old), N(new)
        assert s.count(old) == 1, (path, label, s.count(old))
        s = s.replace(old, new)
    open(path, 'w', newline='').write(s)

edit('settings.h', [
 ('decl', '''void settings_draw(int x, int y);
void settings_click(int x, int y);
bool settings_hover(int x, int y);
''', '''void settings_draw(int x, int y);
void settings_draw_region(int x, int y, int x0, int y0, int x1, int y1);   /* only what touches the region */
void settings_click(int x, int y);
bool settings_hover(int x, int y);
/* Screen rectangles (x0, y0, x1, y1; x1/y1 exclusive) that changed since the last call, so the
 * shell repaints only those. Returns the count, or -1 when the whole window should be repainted. */
int settings_dirty(int x, int y, int r[][4], int max);
'''),
])

edit('settings.c', [
 ('state', 'static int     hover = -1;              /* button under the pointer, -1 = none */\n',
  'static int     hover = -1;              /* button under the pointer, -1 = none */\nstatic uint16_t dirty_mask;             /* buttons whose look changed since settings_dirty() */\n'),
 ('bit', '#define BTN_COUNT   (ID_RESET + 1)\n', '#define BTN_COUNT   (ID_RESET + 1)\n#define BIT(id)     ((uint16_t)1u << (id))\n'),
 ('draw', '''void settings_draw(int x, int y)
{
    int id;

    gfx_SetTextScale(1, 1);
    text_color(PAL_TEXT);
    gfx_PrintStringXY("Theme",         x + BTN_X0, y + 6);
    gfx_PrintStringXY("Pointer speed", x + BTN_X0, y + 76);
    gfx_PrintStringXY("Clock",         x + BTN_X0, y + 118);

    for (id = 0; id < BTN_COUNT; id++)
        draw_button(x, y, id);

    text_color(PAL_ICON0 + 7);
    gfx_PrintStringXY("Changes apply right away and are saved.",
                      x + BTN_X0, y + CONTENT_H - 10);
}
''', '''static bool touches(int ax0, int ay0, int ax1, int ay1,
                    int bx0, int by0, int bx1, int by1)
{
    return ax0 < bx1 && bx0 < ax1 && ay0 < by1 && by0 < ay1;
}

/* draws only the labels and buttons that overlap [x0,x1) x [y0,y1) (screen coordinates) */
void settings_draw_region(int x, int y, int x0, int y0, int x1, int y1)
{
    int id;

    gfx_SetTextScale(1, 1);

    if (touches(x0, y0, x1, y1, x, y + 6, x + WIN_W, y + 14) ||
        touches(x0, y0, x1, y1, x, y + 76, x + WIN_W, y + 84) ||
        touches(x0, y0, x1, y1, x, y + 118, x + WIN_W, y + 126)) {
        text_color(PAL_TEXT);
        gfx_PrintStringXY("Theme",         x + BTN_X0, y + 6);
        gfx_PrintStringXY("Pointer speed", x + BTN_X0, y + 76);
        gfx_PrintStringXY("Clock",         x + BTN_X0, y + 118);
    }

    for (id = 0; id < BTN_COUNT; id++) {
        rect_t r = btn_rect(id);
        if (touches(x0, y0, x1, y1, x + r.x, y + r.y, x + r.x + r.w, y + r.y + r.h))
            draw_button(x, y, id);
    }

    if (touches(x0, y0, x1, y1, x, y + CONTENT_H - 10, x + WIN_W, y + CONTENT_H - 2)) {
        text_color(PAL_ICON0 + 7);
        gfx_PrintStringXY("Changes apply right away and are saved.",
                          x + BTN_X0, y + CONTENT_H - 10);
    }
}

void settings_draw(int x, int y)
{
    settings_draw_region(x, y, x, y, x + WIN_W, y + CONTENT_H);
}
'''),
 ('open', '''void settings_open(void)
{
    hover = -1;
}
''', '''void settings_open(void)
{
    hover = -1;
    dirty_mask = 0;
}
'''),
 ('hover', '''    int id = btn_at(x, y);
    if (id == hover) return false;
    hover = id;
    return true;
}
''', '''    int id = btn_at(x, y);
    if (id == hover) return false;
    if (hover >= 0) dirty_mask |= BIT(hover);       /* un-highlight the old button */
    if (id >= 0)    dirty_mask |= BIT(id);          /* highlight the new one */
    hover = id;
    return true;
}

int settings_dirty(int x, int y, int r[][4], int max)
{
    uint16_t m = dirty_mask;
    int id, n = 0;

    dirty_mask = 0;
    for (id = 0; id < BTN_COUNT; id++) {
        rect_t b;
        if (!(m & BIT(id))) continue;
        if (n >= max) return -1;                    /* too many pieces: repaint the window */
        b = btn_rect(id);
        r[n][0] = x + b.x;
        r[n][1] = y + b.y;
        r[n][2] = x + b.x + b.w;
        r[n][3] = y + b.y + b.h;
        n++;
    }
    return n;
}
'''),
 ('apply', '''    if (next->theme == prefs.theme && next->speed == prefs.speed &&
        next->clock24 == prefs.clock24)
        return;                         /* nothing changed: skip the flash write */
    prefs = *next;
    changed = true;
''', '''    bool before[BTN_COUNT];
    int id;

    if (next->theme == prefs.theme && next->speed == prefs.speed &&
        next->clock24 == prefs.clock24)
        return;                         /* nothing changed: skip the flash write */
    for (id = 0; id < BTN_COUNT; id++) before[id] = btn_selected(id);
    prefs = *next;
    for (id = 0; id < BTN_COUNT; id++)  /* only buttons whose selection flipped need repainting */
        if (btn_selected(id) != before[id]) dirty_mask |= BIT(id);
    changed = true;
'''),
])
print("settings ok")
EOF