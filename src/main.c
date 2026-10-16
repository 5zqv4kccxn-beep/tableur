#include <eadk.h>
#include <stdint.h>
#include <stdbool.h>

const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "Tableur";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;

extern const char course_maths[];
extern const char course_pc1[];
extern const char course_pc2[];
extern const char course_de[];

/* ---------- Couleurs (RGB565) ---------- */
#define WHITE    ((eadk_color_t)0xFFFF)
#define BLACK    ((eadk_color_t)0x0000)
#define GREEN    ((eadk_color_t)0x2589)
#define GREEN_L  ((eadk_color_t)0xD7FA)
#define GREY     ((eadk_color_t)0xEF7D)
#define GREY_D   ((eadk_color_t)0x8C71)
#define BLUE     ((eadk_color_t)0x0AB5)
#define BLUE_L   ((eadk_color_t)0xDF1F)
#define DEF_BG   ((eadk_color_t)0xE7F6)
#define DEF_BAR  ((eadk_color_t)0x07E0)
#define ACCENT   ((eadk_color_t)0xF9A0)

#define SCREEN_W 320
#define SCREEN_H 240

typedef enum { MODE_SHEET, MODE_MENU, MODE_READER } mode_t_;

static mode_t_ mode = MODE_SHEET;
static bool need_redraw = true;

/* ---------- Utilitaires ---------- */
static void fill(int x, int y, int w, int h, eadk_color_t c) {
  eadk_display_push_rect_uniform((eadk_rect_t){x, y, w, h}, c);
}

static void text(const char *s, int x, int y, bool large, eadk_color_t fg, eadk_color_t bg) {
  eadk_display_draw_string(s, (eadk_point_t){x, y}, large, fg, bg);
}

static bool kd(eadk_keyboard_state_t k, eadk_key_t key) {
  return eadk_keyboard_key_down(k, key);
}

/* ---------- TABLEUR ---------- */
#define COLS 3
#define ROWS 3
static int cur_c = 0, cur_r = 0;
static bool touched = false;
static int code_pos = 0;
static const char code_seq[4] = {'2', '0', '1', '1'};

static void draw_sheet(void) {
  fill(0, 0, SCREEN_W, SCREEN_H, WHITE);
  fill(0, 0, SCREEN_W, 28, GREEN);
  text("Tableur", 10, 6, true, WHITE, GREEN);
  fill(0, 28, SCREEN_W, 22, GREY);
  text("fx", 8, 32, false, GREY_D, GREY);
  char ref[3] = {(char)('A' + cur_c), (char)('1' + cur_r), 0};
  text(ref, 40, 32, false, BLACK, GREY);

  int x0 = 40, y0 = 60, cw = 80, ch = 40;
  /* en-tetes colonnes */
  for (int c = 0; c < COLS; c++) {
    char s[2] = {(char)('A' + c), 0};
    fill(x0 + c * cw, y0, cw - 1, 24, c == cur_c ? GREEN_L : GREY);
    text(s, x0 + c * cw + cw / 2 - 5, y0 + 5, true, BLACK, c == cur_c ? GREEN_L : GREY);
  }
  /* lignes */
  for (int r = 0; r < ROWS; r++) {
    char s[2] = {(char)('1' + r), 0};
    int y = y0 + 26 + r * ch;
    fill(10, y, 28, ch - 1, r == cur_r ? GREEN_L : GREY);
    text(s, 20, y + 12, true, BLACK, r == cur_r ? GREEN_L : GREY);
    for (int c = 0; c < COLS; c++) {
      int x = x0 + c * cw;
      bool sel = (c == cur_c && r == cur_r);
      fill(x, y, cw - 1, ch - 1, WHITE);
      /* contour */
      fill(x, y, cw - 1, 1, sel ? GREEN : GREY);
      fill(x, y + ch - 2, cw - 1, 1, sel ? GREEN : GREY);
      fill(x, y, 1, ch - 1, sel ? GREEN : GREY);
      fill(x + cw - 2, y, 1, ch - 1, sel ? GREEN : GREY);
      if (sel) {
        fill(x + 1, y + 1, cw - 3, 2, GREEN);
        fill(x + 1, y + ch - 4, cw - 3, 2, GREEN);
        fill(x + 1, y + 1, 2, ch - 3, GREEN);
        fill(x + cw - 4, y + 1, 2, ch - 3, GREEN);
      }
    }
  }
}

/* ---------- MENU ---------- */
static const char *menu_items[4] = {"Maths", "Physique-Chimie 1", "Physique-Chimie 2", "Allemand"};
static int menu_sel = 0;

static void draw_menu(void) {
  fill(0, 0, SCREEN_W, SCREEN_H, WHITE);
  fill(0, 0, SCREEN_W, 36, BLUE);
  text("Mes cours", 12, 9, true, WHITE, BLUE);
  for (int i = 0; i < 4; i++) {
    int y = 50 + i * 42;
    bool s = (i == menu_sel);
    fill(20, y, 280, 36, s ? BLUE_L : GREY);
    if (s) fill(20, y, 6, 36, BLUE);
    text(menu_items[i], 40, y + 10, false, BLACK, s ? BLUE_L : GREY);
  }
}

/* ---------- LECTEUR ---------- */
#define MAX_LINES 450
#define CHARS_PER_LINE 34
#define VISIBLE 8
#define LINE_H 24

typedef struct {
  bool is_title;
  bool is_def;
  char text[CHARS_PER_LINE + 12];
} line_t;

static line_t lines[MAX_LINES];
static int line_count = 0;
static int page = 0;
static const char *course_title = "";

static void parse_course(const char *src) {
  line_count = 0;
  int i = 0;
  static char full[400];
  while (src[i] && line_count < MAX_LINES) {
    bool title = false, def = false;
    if (src[i] == '#') { title = true; i++; if (src[i] == ' ') i++; }
    else if (src[i] == '!') { def = true; i++; if (src[i] == ' ') i++; }
    int n = 0;
    while (src[i] && src[i] != '\n' && n < 399) full[n++] = src[i++];
    full[n] = 0;
    if (src[i] == '\n') i++;
    if (n == 0) continue;

    int p = 0;
    do {
      line_t *L = &lines[line_count];
      L->is_title = title; L->is_def = def;
      int vis = 0, k = 0, last_space = -1, last_space_k = -1;
      int start = p;
      while (full[p] && vis < CHARS_PER_LINE) {
        if (full[p] == '*') { L->text[k++] = full[p++]; continue; }
        if (full[p] == ' ') { last_space = p; last_space_k = k; }
        L->text[k++] = full[p++];
        vis++;
      }
      if (full[p] && last_space > start) { p = last_space + 1; k = last_space_k; }
      L->text[k] = 0;
      line_count++;
    } while (full[p] && line_count < MAX_LINES);
  }
}

static void select_course(int i) {
  const char *src = i == 0 ? course_maths : (i == 1 ? course_pc1 : (i == 2 ? course_pc2 : course_de));
  course_title = i == 0 ? "MATHS" : (i == 1 ? "PC chap. 1" : (i == 2 ? "PC chap. 2" : "ALLEMAND"));
  parse_course(src);
  page = 0;
  mode = MODE_READER;
}

/* dessine une ligne avec *MOTS* en gros (rouge), le reste en petit */
static void draw_rich(const char *s, int x, int y, eadk_color_t bg) {
  bool big = false;
  char buf[48];
  int b = 0;
  for (int i = 0;; i++) {
    char c = s[i];
    if (c == '*' || c == 0) {
      if (b > 0) {
        buf[b] = 0;
        text(buf, x, y, big, big ? ACCENT : BLACK, bg);
        x += b * (big ? 10 : 7);
        b = 0;
      }
      if (c == 0) break;
      big = !big;
    } else if (b < 46) {
      buf[b++] = c;
    }
  }
}

static void draw_reader(void) {
  fill(0, 0, SCREEN_W, SCREEN_H, WHITE);
  fill(0, 0, SCREEN_W, 28, BLUE);
  text(course_title, 10, 6, true, WHITE, BLUE);
  for (int i = 0; i < VISIBLE; i++) {
    int idx = page + i;
    if (idx >= line_count) break;
    int y = 34 + i * LINE_H;
    line_t *L = &lines[idx];
    if (L->is_title) {
      fill(0, y, SCREEN_W, LINE_H - 2, BLUE_L);
      text(L->text, 8, y + 4, true, BLUE, BLUE_L);
    } else if (L->is_def) {
      fill(0, y, 5, LINE_H - 2, DEF_BAR);
      fill(5, y, SCREEN_W - 5, LINE_H - 2, DEF_BG);
      draw_rich(L->text, 10, y + 5, DEF_BG);
    } else {
      draw_rich(L->text, 10, y + 5, WHITE);
    }
  }
  /* barre de defilement */
  if (line_count > VISIBLE) {
    int h = SCREEN_H - 34;
    int bh = h * VISIBLE / line_count; if (bh < 10) bh = 10;
    int by = 34 + (h - bh) * page / (line_count - VISIBLE);
    fill(SCREEN_W - 5, 34, 5, h, GREY);
    fill(SCREEN_W - 5, by, 5, bh, BLUE);
  }
}

/* ---------- Touches chiffres ---------- */
static char digit_pressed(eadk_keyboard_state_t k) {
  if (kd(k, eadk_key_zero)) return '0';
  if (kd(k, eadk_key_one)) return '1';
  if (kd(k, eadk_key_two)) return '2';
  if (kd(k, eadk_key_three)) return '3';
  if (kd(k, eadk_key_four)) return '4';
  if (kd(k, eadk_key_five)) return '5';
  if (kd(k, eadk_key_six)) return '6';
  if (kd(k, eadk_key_seven)) return '7';
  if (kd(k, eadk_key_eight)) return '8';
  if (kd(k, eadk_key_nine)) return '9';
  return 0;
}

/* ---------- MAIN ---------- */
int main(int argc, char *argv[]) {
  (void)argc; (void)argv;
  eadk_keyboard_state_t old = 0;
  for (;;) {
    if (need_redraw) {
      if (mode == MODE_SHEET) draw_sheet();
      else if (mode == MODE_MENU) draw_menu();
      else draw_reader();
      need_redraw = false;
    }
    eadk_keyboard_state_t k = eadk_keyboard_scan();
    if (k && !old) {
      if (mode == MODE_SHEET) {
        char d = digit_pressed(k);
        if (d && !touched) {
          if (d == code_seq[code_pos]) {
            code_pos++;
            if (code_pos == 4) { code_pos = 0; mode = MODE_MENU; menu_sel = 0; need_redraw = true; }
          } else {
            code_pos = (d == code_seq[0]) ? 1 : 0;
          }
        } else {
          if (kd(k, eadk_key_left)  && cur_c > 0)        { cur_c--; touched = true; need_redraw = true; }
          if (kd(k, eadk_key_right) && cur_c < COLS - 1) { cur_c++; touched = true; need_redraw = true; }
          if (kd(k, eadk_key_up)    && cur_r > 0)        { cur_r--; touched = true; need_redraw = true; }
          if (kd(k, eadk_key_down)  && cur_r < ROWS - 1) { cur_r++; touched = true; need_redraw = true; }
          if (touched) code_pos = 0;
        }
      } else if (mode == MODE_MENU) {
        if (kd(k, eadk_key_up) && menu_sel > 0) menu_sel--;
        else if (kd(k, eadk_key_down) && menu_sel < 3) menu_sel++;
        else if (kd(k, eadk_key_ok) || kd(k, eadk_key_exe)) select_course(menu_sel);
        else if (kd(k, eadk_key_back)) { mode = MODE_SHEET; cur_c = 0; cur_r = 0; touched = false; code_pos = 0; }
        need_redraw = true;
      } else {
        int max = line_count > VISIBLE ? line_count - VISIBLE : 0;
        if (kd(k, eadk_key_up) && page > 0) page--;
        else if (kd(k, eadk_key_down) && page < max) page++;
        else if (kd(k, eadk_key_back)) mode = MODE_MENU;
        need_redraw = true;
      }
    }
    old = k;
    eadk_timing_msleep(25);
  }
}
