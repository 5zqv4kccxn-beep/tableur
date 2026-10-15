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

static void fill(eadk_rect_t r, eadk_color_t c) { eadk_display_push_rect_uniform(r, c); }

static bool starts_with(const char *s, const char *p) {
  while (*p) { if (*s != *p) return false; s++; p++; }
  return true;
}

static bool down(eadk_keyboard_state_t k, eadk_key_t key) {
  return eadk_keyboard_key_down(k, key);
}

static void release(void) {
  while (eadk_keyboard_scan()) eadk_timing_msleep(10);
}

/* ---------- Word wrap ---------- */

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
  line_count = 0; part_count = 0;
  while (*s) {
    while (*s == '\n' || *s == '\r' || *s == ' ') s++;
    if (!*s) break;

    const char *start = s;
    while (*s && *s != '\n' && *s != '\r') s++;
    unsigned len = (unsigned)(s - start);

    if (len > 0 && start[0] == '#') {
      start++; len--;
      while (len && *start == ' ') { start++; len--; }
      if (part_count < MAX_PARTS) part_line[part_count++] = line_count;
      wrap_paragraph(start, len, true, false, WRAP_CHARS);
    } else if (len >= 4 && starts_with(start, "Def:")) {
      start += 4; len -= 4;
      while (len && *start == ' ') { start++; len--; }
      wrap_paragraph(start, len, false, true, WRAP_CHARS - 1);
    } else {
      wrap_paragraph(start, len, false, false, WRAP_CHARS);
    }
  }
}

/* ---------- Affichage ---------- */

static void draw_title_bar(const char *title) {
  fill((eadk_rect_t){0, 0, 320, 24}, BAR_BLUE);
  int len = 0;
  while (title[len]) len++;
  int x = (320 - len * 9) / 2;
  if (x < 6) x = 6;
  draw_text(title, x, 5, false, BG, BAR_BLUE);
  fill((eadk_rect_t){0, 24, 320, 2}, LINE_GREY);
}

static void draw_reader(void) {
  clear_screen();
  draw_title_bar(course_title);
  unsigned max = line_count > VISIBLE_LINES ? line_count - VISIBLE_LINES : 0;
  if (page > max) page = max;
  for (unsigned i = 0; i < VISIBLE_LINES && page + i < line_count; i++) {
    line_t *L = &lines[page + i];
    int y = 30 + (int)i * 17;
    if (L->is_title) {
      draw_text(L->text, 6, y, true, TITLE_BLUE, BG);
    } else if (L->is_def) {
      fill((eadk_rect_t){2, y - 2, 316, 16}, DEF_BG);
      fill((eadk_rect_t){2, y - 2, 3, 16}, DEF_BAR);
      draw_text(L->text, 10, y, false, FG, DEF_BG);
    } else {
      draw_text(L->text, 6, y, false, FG, BG);
    }
  }
  draw_text("Haut/Bas : defiler   Retour : sommaire", 6, 202, false, FG, BG);
}

static void draw_summary(void) {
  clear_screen();
  draw_title_bar("Sommaire");
  draw_text("0. Lire tout le cours", 12, 30, false, FG, BG);
  for (unsigned i = 0; i < part_count; i++) {
    unsigned idx = part_line[i];
    char buf[64]; unsigned n = 0;
    buf[n++] = (char)('1' + i);
    buf[n++] = ' ';
    for (unsigned j = 0; lines[idx].text[j] && n < 62; j++) buf[n++] = lines[idx].text[j];
    buf[n] = 0;
    unsigned sel = i + 1 == index_sel;
    draw_text(buf, 12, 30 + (int)(i + 1) * 20, false, sel ? TITLE_BLUE : FG, BG);
  }
  draw_text("Haut/Bas : choisir   OK : ouvrir   Retour", 6, 202, false, FG, BG);
}

static void draw_sheet(void) {
  clear_screen();
  draw_title_bar("Mon tableur");
  for (int c = 0; c < 3; c++) { char s[2] = {(char)('A' + c), 0}; draw_text(s, 42 + c * 82, 25, false, FG, BG); }
  for (int r = 0; r < 3; r++) { char s[2] = {(char)('1' + r), 0}; draw_text(s, 8, 43 + r * 34, false, FG, BG); }
  for (int c = 0; c < 4; c++) fill((eadk_rect_t){30 + (uint16_t)c * 82, 20, 1, 130}, FG);
  for (int r = 0; r < 4; r++) fill((eadk_rect_t){30, 38 + (uint16_t)r * 34, 250, 1}, FG);
  fill((eadk_rect_t){31 + col * 82, 39 + row * 34, 80, 32}, SEL_FILL);
  char cell[3] = {(char)('A' + col), (char)('1' + row), 0};
  draw_text(cell, 38 + col * 82, 48 + row * 34, false, FG, SEL_FILL);
  draw_text("Fleches : se deplacer", 6, 178, false, FG, BG);
}

static void draw_list(const char *title, const char * const *items, unsigned count, unsigned selected, const char *hint) {
  clear_screen();
  draw_title_bar(title);
  for (unsigned i = 0; i < count; i++) {
    draw_text(items[i], 12, 30 + (int)i * 24, false, i == selected ? TITLE_BLUE : FG, BG);
  }
  draw_text(hint, 6, 202, false, FG, BG);
}

/* ---------- Gestion des touches ---------- */

static void sheet_key(eadk_keyboard_state_t k) {
  if (down(k, eadk_key_left) && col) col--;
  else if (down(k, eadk_key_right) && col < 2) col++;
  else if (down(k, eadk_key_up) && row) row--;
  else if (down(k, eadk_key_down) && row < 2) row++;
  else if (down(k, eadk_key_two)) code_pos = code_pos == 0 ? 1 : 0;
  else if (down(k, eadk_key_zero)) code_pos = code_pos == 1 ? 2 : 0;
  else if (down(k, eadk_key_one)) {
    if (code_pos == 2) code_pos = 3;
    else if (code_pos == 3) { code_pos = 0; screen = MENU; menu_sel = 0; }
    else code_pos = 0;
  }
}

static void select_course(unsigned which) {
  if (which == 0) { course_text = course_maths; course_title = "Maths"; }
  else if (which == 1) { course_text = course_pc1; course_title = "Physique-Chimie 1"; }
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
    if (screen == SHEET) {
      if (need_redraw) { draw_sheet(); need_redraw = false; }
    } else if (need_redraw) {
      if (screen == MENU) draw_list("Menu des cours", menu, 3, menu_sel,
                                    "Haut/Bas : choisir   OK : ouvrir   Retour");
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
