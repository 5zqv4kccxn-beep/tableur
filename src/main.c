#include <eadk.h>
#include <stdint.h>
#include <stdbool.h>

const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "Cours";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;

extern const char course_maths[];
extern const char course_pc1[];
extern const char course_pc2[];

#define BG       eadk_color_white
#define FG       eadk_color_black
#define BLUE     ((eadk_color_t)0x0AB5)
#define BAR      ((eadk_color_t)0x3D1F)
#define DEF_BG   ((eadk_color_t)0xE7F6)
#define DEF_BAR  ((eadk_color_t)0x07E0)
#define SEL      ((eadk_color_t)0xDEDF)

#define MAX_LINES 300
#define VISIBLE 10
#define LINE_H 20
#define WRAP 36

typedef struct { char text[96]; bool is_title; bool is_def; } line_t;
static line_t lines[MAX_LINES];
static unsigned line_count;
static unsigned page;
static unsigned menu_sel;
static const char *course_title;
static bool in_reader = false;
static bool need_redraw = true;

static void fill(eadk_rect_t r, eadk_color_t c) {
  eadk_display_push_rect_uniform(r, c);
}

static void parse_course(const char *src) {
  line_count = 0;
  const char *p = src;
  while (*p && line_count < MAX_LINES) {
    line_t *L = &lines[line_count];
    L->is_title = false;
    L->is_def = false;
    unsigned n = 0;
    if (*p == '#') { L->is_title = true; p++; if (*p == ' ') p++; }
    else if (*p == '!') { L->is_def = true; p++; if (*p == ' ') p++; }
    while (*p && *p != '\n' && n < 94) L->text[n++] = *p++;
    L->text[n] = 0;
    if (*p == '\n') p++;
    line_count++;
  }
}

/* Dessine une ligne : texte normal, et *MOTS* en gros et en couleur */
static void draw_line(const char *s, int x, int y, eadk_color_t bg, bool title) {
  bool big = title;
  char buf[2] = {0, 0};
  for (; *s; s++) {
    if (*s == '*') { big = !big; continue; }
    buf[0] = *s;
    if (big) {
      if (buf[0] >= 'a' && buf[0] <= 'z') buf[0] -= 32;
      eadk_display_draw_string(buf, (eadk_point_t){x, y}, true, BLUE, bg);
      x += 10;
    } else {
      eadk_display_draw_string(buf, (eadk_point_t){x, y + 2}, false, FG, bg);
      x += 7;
    }
    if (x > 305) break;
  }
}

static void draw_menu(void) {
  fill((eadk_rect_t){0, 0, 320, 240}, BG);
  fill((eadk_rect_t){0, 0, 320, 24}, BAR);
  eadk_display_draw_string("MES COURS", (eadk_point_t){100, 4}, true, eadk_color_white, BAR);
  const char *names[3] = {"MATHS", "PC 1", "PC 2"};
  for (unsigned i = 0; i < 3; i++) {
    unsigned y = 50 + i * 50;
    if (i == menu_sel) fill((eadk_rect_t){20, y - 6, 280, 36}, SEL);
    eadk_display_draw_string(names[i], (eadk_point_t){40, y}, true, BLUE, i == menu_sel ? SEL : BG);
  }
}

static void draw_reader(void) {
  fill((eadk_rect_t){0, 0, 320, 240}, BG);
  fill((eadk_rect_t){0, 0, 320, 22}, BAR);
  eadk_display_draw_string(course_title, (eadk_point_t){8, 4}, true, eadk_color_white, BAR);
  for (unsigned i = 0; i < VISIBLE; i++) {
    unsigned idx = page + i;
    if (idx >= line_count) break;
    int y = 28 + i * LINE_H;
    line_t *L = &lines[idx];
    eadk_color_t bg = BG;
    if (L->is_def) {
      bg = DEF_BG;
      fill((eadk_rect_t){0, y - 2, 320, LINE_H}, DEF_BG);
      fill((eadk_rect_t){0, y - 2, 4, LINE_H}, DEF_BAR);
    }
    draw_line(L->text, 8, y, bg, L->is_title);
  }
}

static bool down(uint64_t k, eadk_key_t key) { return eadk_keyboard_key_down(k, key); }

static void select_course(unsigned i) {
  const char *src = i == 0 ? course_maths : (i == 1 ? course_pc1 : course_pc2);
  course_title = i == 0 ? "MATHS" : (i == 1 ? "PC 1" : "PC 2");
  parse_course(src);
  page = 0;
  in_reader = true;
}

int main(int argc, char *argv[]) {
  uint64_t old = 0;
  while (1) {
    if (need_redraw) {
      if (in_reader) draw_reader(); else draw_menu();
      need_redraw = false;
    }
    uint64_t k = eadk_keyboard_scan();
    if (k && !old) {
      if (!in_reader) {
        if (down(k, eadk_key_up) && menu_sel > 0) menu_sel--;
        else if (down(k, eadk_key_down) && menu_sel < 2) menu_sel++;
        else if (down(k, eadk_key_ok) || down(k, eadk_key_exe)) select_course(menu_sel);
      } else {
        unsigned max = line_count > VISIBLE ? line_count - VISIBLE : 0;
        if (down(k, eadk_key_up) && page > 0) page--;
        else if (down(k, eadk_key_down) && page < max) page++;
        else if (down(k, eadk_key_back)) in_reader = false;
      }
      need_redraw = true;
    }
    old = k;
    eadk_timing_msleep(25);
  }
}
