#include <eadk.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "Tableur";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;

extern const char course_maths[];
extern const char course_pc1[];
extern const char course_pc2[];
extern const char * const course_maths_sections[];
extern const char * const course_pc1_sections[];
extern const char * const course_pc2_sections[];
extern const unsigned course_maths_section_count, course_pc1_section_count, course_pc2_section_count;

#define BG        eadk_color_white
#define FG        eadk_color_black
#define BAR_BLUE  ((eadk_color_t)0x3D1F)
#define HEAD_GREY ((eadk_color_t)0xD6BA)
#define LINE_GREY ((eadk_color_t)0xBDF7)
#define SEL_FILL  ((eadk_color_t)0xDEDF)
#define VISIBLE_LINES 10

static char lines[100][64];
static unsigned line_count;

typedef enum { SHEET, MENU, INDEX, READER } screen_t;
static screen_t screen = SHEET;
static unsigned col, row, code_pos, menu_sel, index_sel, page;
static const char *course_text;
static const char * const *course_sections;
static unsigned section_count;

static unsigned prev_col = 0, prev_row = 0;
static bool sheet_dirty = true;

static void fill(eadk_rect_t r, eadk_color_t c) { eadk_display_push_rect_uniform(r, c); }

static void draw_text(const char *s, int x, int y, bool large,
                      eadk_color_t fg, eadk_color_t bg) {
  eadk_display_draw_string(s, (eadk_point_t){(uint16_t)x, (uint16_t)y}, large, fg, bg);
}

static bool down(eadk_keyboard_state_t k, eadk_key_t key) {
  return eadk_keyboard_key_down(k, key);
}

static void release(void) {
  eadk_keyboard_state_t k;
  do { k = eadk_keyboard_scan(); eadk_timing_msleep(18); } while (k);
}

/* ---------- Tableur ---------- */

static void draw_sheet_frame(void) {
  fill(eadk_screen_rect, BG);
  /* Barre de titre */
  fill((eadk_rect_t){0, 0, 320, 18}, BAR_BLUE);
  draw_text("Tableur", 6, 2, false, BG, BAR_BLUE);
  /* En-tetes colonnes et lignes */
  fill((eadk_rect_t){0, 18, 320, 20}, HEAD_GREY);
  fill((eadk_rect_t){0, 38, 30, 136}, HEAD_GREY);
  for (int c = 0; c < 3; c++) {
    char s[2] = {(char)('A' + c), 0};
    draw_text(s, 42 + c * 82, 22, false, FG, HEAD_GREY);
  }
  for (int r = 0; r < 3; r++) {
    char s[2] = {(char)('1' + r), 0};
    draw_text(s, 8, 46 + r * 34, false, FG, HEAD_GREY);
  }
  /* Grille fine */
  for (int c = 0; c <= 3; c++)
    fill((eadk_rect_t){30 + (uint16_t)c * 82, 38, 1, 136}, LINE_GREY);
  for (int r = 0; r <= 3; r++)
    fill((eadk_rect_t){30, 38 + (uint16_t)r * 34, 250, 1}, LINE_GREY);
  /* Barre d'etat */
  fill((eadk_rect_t){0, 202, 320, 20}, HEAD_GREY);
  draw_text("Fleches : naviguer", 6, 204, false, FG, HEAD_GREY);
}

static void draw_cell(unsigned c, unsigned r, bool selected) {
  eadk_rect_t box = {31 + (uint16_t)c * 82, 39 + (uint16_t)r * 34, 80, 32};
  fill(box, selected ? SEL_FILL : BG);
  if (selected) {
    fill((eadk_rect_t){box.x, box.y, 80, 2}, BAR_BLUE);
    fill((eadk_rect_t){box.x, box.y + 30, 80, 2}, BAR_BLUE);
    char cell[3] = {(char)('A' + c), (char)('1' + r), 0};
    draw_text(cell, box.x + 4, box.y + 8, false, BAR_BLUE, SEL_FILL);
  }
}

static void draw_sheet_update(void) {
  if (sheet_dirty) {
    draw_sheet_frame();
    for (unsigned c = 0; c < 3; c++)
      for (unsigned r = 0; r < 3; r++)
        draw_cell(c, r, c == col && r == row);
    sheet_dirty = false;
    prev_col = col; prev_row = row;
    return;
  }
  if (prev_col != col || prev_row != row) {
    draw_cell(prev_col, prev_row, false);
    draw_cell(col, row, true);
    prev_col = col; prev_row = row;
  }
}

/* ---------- Menus ---------- */

static void draw_list(const char *title, const char * const *items,
                      unsigned count, unsigned selected) {
  fill(eadk_screen_rect, BG);
  fill((eadk_rect_t){0, 0, 320, 22}, BAR_BLUE);
  draw_text(title, 6, 4, false, BG, BAR_BLUE);
  for (unsigned i = 0; i < count; i++) {
    bool sel = (i == selected);
    eadk_rect_t row_box = {10, 32 + (uint16_t)i * 26, 300, 22};
    fill(row_box, sel ? SEL_FILL : BG);
    draw_text(items[i], 16, 36 + (int)i * 26, false, sel ? BAR_BLUE : FG,
              sel ? SEL_FILL : BG);
  }
  fill((eadk_rect_t){0, 202, 320, 20}, HEAD_GREY);
  draw_text("Haut/Bas  OK  Retour", 6, 204, false, FG, HEAD_GREY);
}

/* ---------- Lecteur de cours ---------- */

static void make_lines(const char *s) {
  line_count = 0;
  while (*s && line_count < 100) {
    while (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t') s++;
    if (!*s) break;
    unsigned n = 0;
    while (*s && *s != '\n' && *s != '\r' && n < 62) {
      lines[line_count][n++] = *s++;
      if (n >= 48 && *s == ' ') break;
    }
    while (n && lines[line_count][n - 1] == ' ') n--;
    lines[line_count][n] = 0;
    line_count++;
    while (*s && *s != ' ' && *s != '\n' && *s != '\r' && n < 62)
      lines[line_count - 1][n++] = *s++;
    lines[line_count - 1][n] = 0;
  }
}

static void draw_reader(void) {
  make_lines(course_text);
  fill(eadk_screen_rect, BG);
  fill((eadk_rect_t){0, 0, 320, 22}, BAR_BLUE);
  draw_text("Cours", 6, 4, false, BG, BAR_BLUE);
  unsigned maxpage = line_count > VISIBLE_LINES ? line_count - VISIBLE_LINES : 0;
  if (page > maxpage) page = maxpage;
  for (unsigned i = 0; i < VISIBLE_LINES && page + i < line_count; i++)
    draw_text(lines[page + i], 6, 28 + (int)i * 17, false, FG, BG);
  fill((eadk_rect_t){0, 202, 320, 20}, HEAD_GREY);
  draw_text("Haut/Bas defiler  Retour", 6, 204, false, FG, HEAD_GREY);
}

/* ---------- Logique ---------- */

static void select_course(unsigned which) {
  if (which == 0) {
    course_text = course_maths; course_sections = course_maths_sections;
    section_count = course_maths_section_count;
  } else if (which == 1) {
    course_text = course_pc1; course_sections = course_pc1_sections;
    section_count = course_pc1_section_count;
  } else {
    course_text = course_pc2; course_sections = course_pc2_sections;
    section_count = course_pc2_section_count;
  }
  index_sel = 0; page = 0; screen = INDEX;
}

static void sheet_key(eadk_keyboard_state_t k) {
  if (down(k, eadk_key_left) && col) col--;
  else if (down(k, eadk_key_right) && col < 2) col++;
  else if (down(k, eadk_key_up) && row) row--;
  else if (down(k, eadk_key_down) && row < 2) row++;
  /* Sequence secrete 2-0-1-1 : aucun etat n'est affiche */
  else if (down(k, eadk_key_two)) code_pos = code_pos == 0 ? 1 : 0;
  else if (down(k, eadk_key_zero)) code_pos = code_pos == 1 ? 2 : 0;
  else if (down(k, eadk_key_one)) {
    if (code_pos == 2) code_pos = 3;
    else if (code_pos == 3) { code_pos = 0; screen = MENU; menu_sel = 0; }
    else code_pos = 0;
  }
}

int main(int argc, char **argv) {
  (void)argc; (void)argv;
  eadk_keyboard_state_t old = 0;
  const char *menu[] = {"Maths", "Physique-Chimie Chap. 1", "Physique-Chimie Chap. 2"};
  bool need_redraw = true;

  for (;;) {
    if (screen == SHEET) {
      draw_sheet_update();
    } else if (need_redraw) {
      if (screen == MENU) {
        draw_list("Menu", menu, 3, menu_sel);
      } else if (screen == INDEX) {
        const char *items[19];
        items[0] = "Cours complet";
        for (unsigned i = 0; i < section_count && i < 18; i++)
          items[i + 1] = course_sections[i];
        draw_list("Sommaire", items, section_count + 1, index_sel);
      } else {
        draw_reader();
      }
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
        else if (down(k, eadk_key_back)) { screen = SHEET; sheet_dirty = true; }
      } else if (screen == INDEX) {
        if (down(k, eadk_key_up) && index_sel) index_sel--;
        else if (down(k, eadk_key_down) && index_sel < section_count) index_sel++;
        else if (down(k, eadk_key_ok)) { page = 0; screen = READER; }
        else if (down(k, eadk_key_back)) screen = MENU;
      } else {
        unsigned max = line_count > VISIBLE_LINES ? line_count - VISIBLE_LINES : 0;
        if (down(k, eadk_key_up) && page) page--;
        else if (down(k, eadk_key_down) && page < max) page++;
        else if (down(k, eadk_key_left)) page = page > 10 ? page - 10 : 0;
        else if (down(k, eadk_key_right)) page = page + 10 < max ? page + 10 : max;
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
