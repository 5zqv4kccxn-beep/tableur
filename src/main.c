#include <eadk.h>
#include <stdint.h>
#include <stdbool.h>

const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "Tableur";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;

extern const char course_maths[];
extern const char course_pc1[];
extern const char course_pc2[];

/* ---------- Couleurs (RGB565) ---------- */
#define WHITE    ((eadk_color_t)0xFFFF)
#define BLACK    ((eadk_color_t)0x0000)
#define GREEN    ((eadk_color_t)0x2589)  /* vert tableur */
#define GREEN_L  ((eadk_color_t)0xD7FA)  /* vert clair selection */
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

static int str_len(const char *s) { int n = 0; while (s[n]) n++; return n; }

static char to_upper(char c) { return (c >= 'a' && c <= 'z') ? c - 32 : c; }

static void text(const char *s, int x, int y, bool large, eadk_color_t fg, eadk_color_t bg) {
  eadk_display_draw_string(s, (eadk_point_t){x, y}, large, fg, bg);
}

/* ---------- TABLEUR ---------- */
#define COLS 3
#define ROWS 3
static int cur_c = 0, cur_r = 0;
static bool touched = false;     /* a-t-on deja bouge ? */
static int code_pos = 0;         /* progression dans 2011 */
static const char code_seq[4] = {'2', '0', '1', '1'};

static void draw_sheet(void) {
  fill(0, 0, SCREEN_W, SCREEN_H, WHITE);
  /* barre de titre */
  fill(0, 0, SCREEN_W, 28, GREEN);
  text("Tableur", 10, 6, true, WHITE, GREEN);
  /* barre de formule */
  fill(0, 28, SCREEN_W, 22, GREY);
  text("fx", 8, 32, false, GREY_D, GREY);
  /* en-tetes colonnes */
  int x0 = 40, y0 = 56, cw = 90, ch = 40;
  fill(0, y0, x0, 20, GREY);
  for (int c = 0; c < COLS; c++) {
    char h[2] = {'A' + c, 0};
    fill(x0 + c * cw, y0, cw - 1, 20, (c == cur_c) ? GREEN_L : GREY);
    text(h, x0 + c * cw + cw / 2 - 5, y0 + 3, false, BLACK, (c == cur_c) ? GREEN_L : GREY);
  }
  /* lignes */
  for (int r = 0; r < ROWS; r++) {
    int y = y0 + 22 + r * ch;
    char n[2] = {'1' + r, 0};
    fill(0, y, x0 - 1, ch - 1, (r == cur_r) ? GREEN_L : GREY);
    text(n, 16, y + 12, false, BLACK, (r == cur_r) ? GREEN_L : GREY);
    for (int c = 0; c < COLS; c++) {
      bool s = (c == cur_c && r == cur_r);
      fill(x0 + c * cw, y, cw - 1, ch - 1, s ? GREEN_L : WHITE);
      if (s) {
        fill(x0 + c * cw, y, cw - 1, 2, GREEN);
        fill(x0 + c * cw, y + ch - 3, cw - 1, 2, GREEN);
        fill(x0 + c * cw, y, 2, ch - 1, GREEN);
        fill(x0 + c * cw + cw - 3, y, 2, ch - 1, GREEN);
      }
    }
  }
}

/* ---------- MENU ---------- */
static int menu_sel = 0;
static const char *menu_items[3] = {"Maths", "Physique-Chimie 1", "Physique-Chimie 2"};

static void draw_menu(void) {
  fill(0, 0, SCREEN_W, SCREEN_H, WHITE);
  fill(0, 0, SCREEN_W, 40, BLUE);
  text("MES COURS", 90, 10, true, WHITE, BLUE);
  for (int i = 0; i < 3; i++) {
    int y = 60 + i * 50;
    bool s = (i == menu_sel);
    fill(20, y, 280, 40, s ? BLUE_L : GREY);
    if (s) fill(20, y, 6, 40, BLUE);
    text(menu_items[i], 40, y + 12, false, BLACK, s ? BLUE_L : GREY);
  }
  text("OK: ouvrir   Retour: tableur", 40, 220, false, GREY_D, WHITE);
}

/* ---------- LECTEUR ---------- */
#define MAX_LINES 300
#define VISIBLE 8
#define LINE_H 24
typedef struct { char text[96]; bool is_title; bool is_def; } line_t;
static line_t lines[MAX_LINES];
static int line_count = 0;
static int page = 0;
static const char *course_title = "";

static void parse_course(const char *src) {
  line_count = 0;
  int i = 0;
  while (src[i] && line_count < MAX_LINES) {
    line_t *L = &lines[line_count];
    L->is_title = false; L->is_def = false;
    if (src[i] == '#') { L->is_title = true; i++; if (src[i] == ' ') i++; }
    else if (src[i] == '!') { L->is_def = true; i++; if (src[i] == ' ') i++; }
    int n = 0;
    while (src[i] && src[i] != '\n' && n < 95) L->text[n++] = src[i++];
    L->text[n] = 0;
    if (src[i] == '\n') i++;
    line_count++;
  }
}

/* dessine une ligne avec les *MOTS* en gros, couleur et majuscules */
static void draw_line(const char *s, int x, int y, eadk_color_t bg, bool title) {
  char buf[96];
  int i = 0;
  bool star = false;
  while (s[i]) {
    int n = 0;
    while (s[i] && s[i] != '*' && n < 90) buf[n++] = s[i++];
    buf[n] = 0;
    if (n > 0) {
      if (star) {
        for (int k = 0; k < n; k++) buf[k] = to_upper(buf[k]);
        text(buf, x, y - 2, true, ACCENT, bg);
        x += n * 14;
      } else {
        text(buf, x, y + 2, false, title ? BLUE : BLACK, bg);
        x += n * 8;
      }
    }
    if (s[i] == '*') { star = !star; i++; }
  }
}

static void draw_reader(void) {
  fill(0, 0, SCREEN_W, SCREEN_H, WHITE);
  fill(0, 0, SCREEN_W, 28, BLUE);
  text(course_title, 10, 6, true, WHITE, BLUE);
  for (int v = 0; v < VISIBLE; v++) {
    int idx = page + v;
    if (idx >= line_count) break;
    line_t *L = &lines[idx];
    int y = 36 + v * LINE_H;
    eadk_color_t bg = WHITE;
    if (L->is_def) {
      bg = DEF_BG;
      fill(0, y - 2, SCREEN_W, LINE_H, DEF_BG);
      fill(0, y - 2, 4, LINE_H, DEF_BAR);
    }
    draw_line(L->text, 10, y, bg, L->is_title);
  }
  /* indicateur de position */
  if (line_count > VISIBLE) {
    int h = 190 * VISIBLE / line_count;
    int max = line_count - VISIBLE;
    int y = 36 + (190 - h) * page / (max > 0 ? max : 1);
    fill(314, 36, 4, 190, GREY);
    fill(314, y, 4, h, BLUE);
  }
}

static void select_course(int i) {
  const char *src = i == 0 ? course_maths : (i == 1 ? course_pc1 : course_pc2);
  course_title = i == 0 ? "MATHS" : (i == 1 ? "PC chap. 1" : "PC chap. 2");
  parse_course(src);
  page = 0;
  mode = MODE_READER;
}

/* ---------- Touches ---------- */
static bool kd(uint64_t k, eadk_key_t key) { return eadk_keyboard_key_down(k, key); }

static char digit_pressed(uint64_t k) {
  if (kd(k, eadk_key_zero))  return '0';
  if (kd(k, eadk_key_one))   return '1';
  if (kd(k, eadk_key_two))   return '2';
  if (kd(k, eadk_key_three)) return '3';
  if (kd(k, eadk_key_four))  return '4';
  if (kd(k, eadk_key_five))  return '5';
  if (kd(k, eadk_key_six))   return '6';
  if (kd(k, eadk_key_seven)) return '7';
  if (kd(k, eadk_key_eight)) return '8';
  if (kd(k, eadk_key_nine))  return '9';
  return 0;
}

int main(int argc, char *argv[]) {
  uint64_t old = 0;
  while (1) {
    if (need_redraw) {
      if (mode == MODE_SHEET) draw_sheet();
      else if (mode == MODE_MENU) draw_menu();
      else draw_reader();
      need_redraw = false;
    }
    uint64_t k = eadk_keyboard_scan();
    if (k && !old) {
      if (mode == MODE_SHEET) {
        char d = digit_pressed(k);
        if (d && !touched) {
          /* code secret : invisible, uniquement si on n'a pas encore bouge */
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
        else if (kd(k, eadk_key_down) && menu_sel < 2) menu_sel++;
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
