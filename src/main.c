#include <eadk.h>
#include <stdint.h>
#include <stdbool.h>

const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "Tableur";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;

extern const char course_maths[];
extern const char course_pc1[];
extern const char course_pc2[];

#define BG         eadk_color_white
#define FG         eadk_color_black
#define BAR_BLUE   ((eadk_color_t)0x3D1F)
#define HEAD_GREY  ((eadk_color_t)0xD6BA)
#define LINE_GREY  ((eadk_color_t)0xBDF7)
#define SEL_FILL   ((eadk_color_t)0xDEDF)
#define TITLE_BLUE ((eadk_color_t)0x0AB5)
#define DEF_BG     ((eadk_color_t)0xE7F6)
#define DEF_BAR    ((eadk_color_t)0x07E0)

#define VISIBLE_LINES 11
#define MAX_LINES 300
#define MAX_PARTS 24
#define WRAP_CHARS 38
#define TITLE_WRAP 26

typedef struct { char text[64]; bool is_title; bool is_def; } line_t;
static line_t lines[MAX_LINES];
static unsigned line_count;
static unsigned part_line[MAX_PARTS];
static unsigned part_count;

typedef enum { SHEET, MENU, INDEX, READER } screen_t;
static screen_t screen = SHEET;
static unsigned col, row, code_pos, menu_sel, index_sel, page;
static const char *course_text;
static const char *course_title;
static bool need_redraw = true;

/* ---------- Utilitaires ---------- */

static void fill(eadk_rect_t r, eadk_color_t c) {
  eadk_display_push_rect_uniform(r, c);
}

static bool starts_with(const char *s, const char *p) {
  while (*p) {
    if (*s != *p) return false;
    s++; p++;
  }
  return true;
}

static bool down(eadk_keyboard_state_t k, eadk_key_t key) {
  return eadk_keyboard_key_down(k, key);
}

static void release(void) {
  eadk_timing_msleep(150);
}

/* ---------- Police agrandie 5x7 ---------- */

static const uint8_t font5x7[][5] = {
  {0x00,0x00,0x00,0x00,0x00}, /* espace */
  {0x00,0x00,0x5F,0x00,0x00}, /* ! */
  {0x00,0x60,0x60,0x00,0x00}, /* . */
  {0x00,0x50,0x30,0x00,0x00}, /* , */
  {0x00,0x36,0x36,0x00,0x00}, /* : */
  {0x08,0x08,0x08,0x08,0x08}, /* - */
  {0x3E,0x51,0x49,0x45,0x3E}, /* 0 */
  {0x00,0x42,0x7F,0x40,0x00}, /* 1 */
  {0x42,0x61,0x51,0x49,0x46}, /* 2 */
  {0x21,0x41,0x45,0x4B,0x31}, /* 3 */
  {0x18,0x14,0x12,0x7F,0x10}, /* 4 */
  {0x27,0x45,0x45,0x45,0x39}, /* 5 */
  {0x3C,0x4A,0x49,0x49,0x30}, /* 6 */
  {0x01,0x71,0x09,0x05,0x03}, /* 7 */
  {0x36,0x49,0x49,0x49,0x36}, /* 8 */
  {0x06,0x49,0x49,0x29,0x1E}, /* 9 */
  {0x7E,0x11,0x11,0x11,0x7E}, /* A */
  {0x7F,0x49,0x49,0x49,0x36}, /* B */
  {0x3E,0x41,0x41,0x41,0x22}, /* C */
  {0x7F,0x41,0x41,0x22,0x1C}, /* D */
  {0x7F,0x49,0x49,0x49,0x41}, /* E */
  {0x7F,0x09,0x09,0x09,0x01}, /* F */
  {0x3E,0x41,0x49,0x49,0x7A}, /* G */
  {0x7F,0x08,0x08,0x08,0x7F}, /* H */
  {0x00,0x41,0x7F,0x41,0x00}, /* I */
  {0x20,0x40,0x41,0x3F,0x01}, /* J */
  {0x7F,0x08,0x14,0x22,0x41}, /* K */
  {0x7F,0x40,0x40,0x40,0x40}, /* L */
  {0x7F,0x02,0x0C,0x02,0x7F}, /* M */
  {0x7F,0x04,0x08,0x10,0x7F}, /* N */
  {0x3E,0x41,0x41,0x41,0x3E}, /* O */
  {0x7F,0x09,0x09,0x09,0x06}, /* P */
  {0x3E,0x41,0x51,0x21,0x5E}, /* Q */
  {0x7F,0x09,0x19,0x29,0x46}, /* R */
  {0x46,0x49,0x49,0x49,0x31}, /* S */
  {0x01,0x01,0x7F,0x01,0x01}, /* T */
  {0x3F,0x40,0x40,0x40,0x3F}, /* U */
  {0x1F,0x20,0x40,0x20,0x1F}, /* V */
  {0x3F,0x40,0x38,0x40,0x3F}, /* W */
  {0x63,0x14,0x08,0x14,0x63}, /* X */
  {0x07,0x08,0x70,0x08,0x07}, /* Y */
  {0x61,0x51,0x49,0x45,0x43}, /* Z */
};

static const uint8_t *glyph(char c) {
  static const uint8_t blank[5] = {0,0,0,0,0};
  if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
  switch (c) {
    case ' ': return font5x7[0];
    case '!': return font5x7[1];
    case '.': return font5x7[2];
    case ',': return font5x7[3];
    case ':': return font5x7[4];
    case '-': return font5x7[5];
  }
  if (c >= '0' && c <= '9') return font5x7[6 + (c - '0')];
  if (c >= 'A' && c <= 'Z') return font5x7[16 + (c - 'A')];
  return blank;
}

/* Texte agrandi : scale = 1, 2 ou 3 */
static void draw_big_text(const char *s, int x, int y, unsigned scale,
                          eadk_color_t fg, eadk_color_t bg) {
  int cx = x;
  for (; *s; s++) {
    const uint8_t *g = glyph(*s);
    for (int c = 0; c < 5; c++) {
      for (int r = 0; r < 7; r++) {
        eadk_color_t color = (g[c] & (1 << r)) ? fg : bg;
        fill((eadk_rect_t){(uint16_t)(cx + c * scale), (uint16_t)(y + r * scale),
                           (uint16_t)scale, (uint16_t)scale}, color);
      }
    }
    cx += 6 * scale;
  }
}

/* ---------- Affichage des ecrans ---------- */

static void clear_screen(void) {
  fill((eadk_rect_t){0, 0, 320, 222}, BG);
}

static void draw_title_bar(const char *title) {
  fill((eadk_rect_t){0, 0, 320, 26}, BAR_BLUE);
  draw_big_text(title, 6, 6, 1, BG, BAR_BLUE);
}

static void draw_sheet(void) {
  clear_screen();
  fill((eadk_rect_t){0, 0, 320, 26}, HEAD_GREY);
  draw_big_text("TABLEUR", 6, 6, 2, FG, HEAD_GREY);
  for (unsigned c = 0; c < 3; c++) {
    char cell[3] = {(char)('A' + c), 0, 0};
    draw_big_text(cell, 38 + c * 82, 40, 1, FG, BG);
  }
  for (unsigned r = 0; r < 3; r++) {
    char num[2] = {(char)('1' + r), 0};
    draw_big_text(num, 10, 70 + r * 34, 1, FG, BG);
  }
  for (unsigned r = 0; r < 3; r++) {
    for (unsigned c = 0; c < 3; c++) {
      eadk_rect_t box = {(uint16_t)(30 + c * 82), (uint16_t)(60 + r * 34), 80, 32};
      eadk_color_t border = (r == row && c == col) ? BAR_BLUE : LINE_GREY;
      fill((eadk_rect_t){box.x, box.y, box.width, 2}, border);
      fill((eadk_rect_t){box.x, box.y + 30, box.width, 2}, border);
      fill((eadk_rect_t){box.x, box.y, 2, box.height}, border);
      fill((eadk_rect_t){box.x + 78, box.y, 2, box.height}, border);
    }
  }
  draw_big_text("FLECHES: NAVIGUER", 6, 186, 1, FG, BG);
}

static void draw_list(const char *title, const char * const *items,
                      unsigned count, unsigned selected, const char *footer) {
  clear_screen();
  draw_title_bar(title);
  for (unsigned i = 0; i < count; i++) {
    unsigned y = 40 + i * 34;
    eadk_color_t bg = (i == selected) ? SEL_FILL : BG;
    fill((eadk_rect_t){10, (uint16_t)y, 300, 28}, bg);
    draw_big_text(items[i], 18, (int)y + 8, 1, FG, bg);
  }
  draw_big_text(footer, 6, 200, 1, FG, BG);
}

/* ---------- Decoupage des cours (word wrap) ---------- */

static void push_line(const char *text, unsigned len, bool title, bool def) {
  if (line_count >= MAX_LINES) return;
  line_t *L = &lines[line_count];
  L->is_title = title;
  L->is_def = def;
  if (len > 63) len = 63;
  for (unsigned i = 0; i < len; i++) L->text[i] = text[i];
  L->text[len] = 0;
  line_count++;
}

static void wrap_paragraph(const char *s, unsigned len, bool title, bool def, unsigned width) {
  unsigned start = 0;
  while (start < len) {
    if (len - start <= width) {
      push_line(s + start, len - start, title, def);
      return;
    }
    unsigned end = start + width;
    unsigned cut = end;
    while (cut > start && s[cut] != ' ') cut--;
    if (cut == start) cut = end;
    push_line(s + start, cut - start, title, def);
    start = cut;
    while (start < len && s[start] == ' ') start++;
  }
}

static void parse_course(const char *s) {
  line_count = 0;
  part_count = 0;
  while (*s) {
    while (*s == '\n' || *s == '\r' || *s == ' ') s++;
    if (!*s) break;
    const char *start = s;
    while (*s && *s != '\n' && *s != '\r') s++;
    unsigned len = (unsigned)(s - start);
    bool title = false, def = false;

    if (len > 0 && start[0] == '#') {
      title = true;
      start++; len--;
      while (len && *start == ' ') { start++; len--; }
      if (part_count < MAX_PARTS) part_line[part_count++] = line_count;
      wrap_paragraph(start, len, title, def, TITLE_WRAP);
    } else if (len >= 4 && starts_with(start, "Def:")) {
      def = true;
      start += 4; len -= 4;
      while (len && *start == ' ') { start++; len--; }
      wrap_paragraph(start, len, title, def, WRAP_CHARS - 1);
    } else {
      wrap_paragraph(start, len, title, def, WRAP_CHARS);
    }
  }
}

/* ---------- Sommaire et lecteur ---------- */

static void draw_summary(void) {
  clear_screen();
  draw_title_bar(course_title);
  const char *items[MAX_PARTS + 1];
  unsigned n = 0;
  items[n++] = "Cours complet";
  for (unsigned i = 0; i < part_count && n < MAX_PARTS + 1; i++) {
    items[n++] = lines[part_line[i]].text;
  }
  for (unsigned i = 0; i < n && i < 5; i++) {
    unsigned y = 40 + i * 30;
    eadk_color_t bg = (i == index_sel) ? SEL_FILL : BG;
    fill((eadk_rect_t){10, (uint16_t)y, 300, 26}, bg);
    draw_big_text(items[i], 18, (int)y + 7, 1, FG, bg);
  }
  draw_big_text("HAUT/BAS  OK  RETOUR", 6, 200, 1, FG, BG);
}

static void draw_reader(void) {
  clear_screen();
  draw_title_bar(course_title);
  unsigned y = 34;
  for (unsigned i = page; i < line_count && y < 200; i++) {
    line_t *L = &lines[i];
    eadk_color_t bg = BG;
    if (L->is_def) {
      fill((eadk_rect_t){4, (uint16_t)(y - 2), 312, 16}, DEF_BG);
      fill((eadk_rect_t){4, (uint16_t)(y - 2), 4, 16}, DEF_BAR);
      bg = DEF_BG;
    }
    if (L->is_title) {
      draw_big_text(L->text, 6, (int)y, 2, TITLE_BLUE, BG);
      y += 16;
    } else {
      draw_big_text(L->text, 14, (int)y, 1, FG, bg);
      y += 15;
    }
  }
}

/* ---------- Tableur et code secret ---------- */

static void sheet_key(eadk_keyboard_state_t k) {
  if (down(k, eadk_key_left) && col) col--;
  else if (down(k, eadk_key_right) && col < 2) col++;
  else if (down(k, eadk_key_up) && row) row--;
  else if (down(k, eadk_key_down) && row < 2) row++;
  else if (down(k, eadk_key_two)) code_pos = (code_pos == 0) ? 1 : 0;
  else if (down(k, eadk_key_zero)) code_pos = (code_pos == 1) ? 2 : 0;
  else if (down(k, eadk_key_one)) {
    if (code_pos == 2) code_pos = 3;
    else if (code_pos == 3) {
      code_pos = 4;
      screen = MENU;
      menu_sel = 0;
    } else {
      code_pos = 0;
    }
  }
}

static void select_course(unsigned i) {
  if (i == 0) { course_text = course_maths; course_title = "Maths"; }
  else if (i == 1) { course_text = course_pc1; course_title = "Physique-Chimie 1"; }
  else { course_text = course_pc2; course_title = "Physique-Chimie 2"; }
  parse_course(course_text);
  screen = INDEX;
  index_sel = 0;
  page = 0;
}

/* ---------- Boucle principale ---------- */

int main(int argc, char **argv) {
  (void)argc; (void)argv;
  eadk_keyboard_state_t old = 0;
  const char *menu[] = {"Maths", "Physique-Chimie Chap. 1", "Physique-Chimie Chap. 2"};

  for (;;) {
    if (need_redraw) {
      if (screen == SHEET) draw_sheet();
      else if (screen == MENU) draw_list("Menu des cours", menu, 3, menu_sel,
                                         "HAUT/BAS  OK  RETOUR");
      else if (screen == INDEX) draw_summary();
      else draw_reader();
      need_redraw = false;
    }

    eadk_keyboard_state_t k = eadk_keyboard_scan();
    if (k && !old) {
      if (screen == SHEET) {
        sheet_key(k);
      } else if (screen == MENU) {
        if (down(k, eadk_key_up) && menu_sel) menu_sel--;
        else if (down(k, eadk_key_down) && menu_sel < 2) menu_sel++;
        else if (down(k, eadk_key_ok)) select_course(menu_sel);
        else if (down(k, eadk_key_back)) screen = SHEET;
      } else if (screen == INDEX) {
        if (down(k, eadk_key_up) && index_sel) index_sel--;
        else if (down(k, eadk_key_down) && index_sel < part_count) index_sel++;
        else if (down(k, eadk_key_ok)) {
          page = (index_sel == 0) ? 0 : part_line[index_sel - 1];
          screen = READER;
        }
        else if (down(k, eadk_key_back)) screen = MENU;
      } else {
        unsigned max = line_count > VISIBLE_LINES ? line_count - VISIBLE_LINES : 0;
        if (down(k, eadk_key_up) && page) page--;
        else if (down(k, eadk_key_down) && page < max) page++;
        else if (down(k, eadk_key_back)) screen = INDEX;
      }
      need_redraw = true;
      release();
      old = 0;
    } else {
      old = k;
    }
    eadk_timing_msleep(25);
  }
}
