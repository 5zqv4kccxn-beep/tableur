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

#define BG eadk_color_white
#define FG eadk_color_black
#define BLUE ((eadk_color_t)0x001F)
#define VISIBLE_LINES 10
static char lines[100][64];
static unsigned line_count;

typedef enum { SHEET, MENU, INDEX, READER } screen_t;
static screen_t screen = SHEET;
static unsigned col, row, code_pos, menu_sel, index_sel, page;
static const char *course_text;
static const char * const *course_sections;
static unsigned section_count;

static void clear_screen(void) { eadk_display_push_rect_uniform(eadk_screen_rect, BG); }
static void draw_text(const char *s, int x, int y, bool large, eadk_color_t fg, eadk_color_t bg) {
  eadk_display_draw_string(s, (eadk_point_t){(uint16_t)x, (uint16_t)y}, large, fg, bg);
}
static bool down(eadk_keyboard_state_t k, eadk_key_t key) { return eadk_keyboard_key_down(k, key); }
static void release(void) { eadk_keyboard_state_t k; do { k=eadk_keyboard_scan(); eadk_timing_msleep(18); } while (k); }

static void draw_sheet(void) {
  clear_screen(); draw_text("Tableur", 6, 4, false, FG, BG);
  for (int c=0;c<3;c++) { char s[2]={(char)('A'+c),0}; draw_text(s,42+c*82,25,false,FG,BG); }
  for (int r=0;r<3;r++) { char s[2]={(char)('1'+r),0}; draw_text(s,8,43+r*34,false,FG,BG); }
  for (int c=0;c<4;c++) eadk_display_push_rect_uniform((eadk_rect_t){30+(uint16_t)c*82,20,1,130},FG);
  for (int r=0;r<4;r++) eadk_display_push_rect_uniform((eadk_rect_t){30,38+(uint16_t)r*34,250,1},FG);
  eadk_display_push_rect_uniform((eadk_rect_t){31+col*82,39+row*34,80,32},BLUE);
  char cell[3]={(char)('A'+col),(char)('1'+row),0}; draw_text(cell,38+col*82,48+row*34,false,FG,BLUE);
  draw_text("Fleches: naviguer   Code: 2 0 1 1",6,178,false,FG,BG);
}
static void draw_list(const char *title, const char * const *items, unsigned count, unsigned selected) {
  clear_screen(); draw_text(title,6,4,false,FG,BG);
  for (unsigned i=0;i<count;i++) draw_text(items[i],12,30+(int)i*24,false,FG,i==selected?BLUE:BG);
  draw_text("Haut/Bas  OK  Back",6,202,false,FG,BG);
}
static void make_lines(const char *s) {
  line_count=0;
  while (*s && line_count<100) {
    while (*s==' ' || *s=='\n' || *s=='\r' || *s=='\t') s++;
    if (!*s) break;
    unsigned n=0;
    while (*s && *s!='\n' && *s!='\r' && n<62) {
      lines[line_count][n++]=*s++;
      if (n>=48 && *s==' ') break;
    }
    while (n && lines[line_count][n-1]==' ') n--;
    lines[line_count][n]=0; line_count++;
    while (*s && *s!=' ' && *s!='\n' && *s!='\r' && n<62) lines[line_count-1][n++]=*s++;
    lines[line_count-1][n]=0;
  }
}
static void draw_reader(void) {
  make_lines(course_text); clear_screen(); draw_text("Cours",6,4,false,FG,BG);
  unsigned maxpage=line_count>VISIBLE_LINES?line_count-VISIBLE_LINES:0;
  if (page>maxpage) page=maxpage;
  for (unsigned i=0;i<VISIBLE_LINES && page+i<line_count;i++) draw_text(lines[page+i],6,24+(int)i*17,false,FG,BG);
  draw_text("H/B defiler  G/D page  Back",6,202,false,FG,BG);
}
static void select_course(unsigned which) {
  if (which==0) {course_text=course_maths;course_sections=course_maths_sections;section_count=course_maths_section_count;}
  else if (which==1) {course_text=course_pc1;course_sections=course_pc1_sections;section_count=course_pc1_section_count;}
  else {course_text=course_pc2;course_sections=course_pc2_sections;section_count=course_pc2_section_count;}
  index_sel=0; page=0; screen=INDEX;
}
static void sheet_key(eadk_keyboard_state_t k) {
  if (down(k,eadk_key_left) && col) col--; else if (down(k,eadk_key_right) && col<2) col++;
  else if (down(k,eadk_key_up) && row) row--; else if (down(k,eadk_key_down) && row<2) row++;
  /* The unlock sequence is intentionally not shown as a UI state. */
  else if (down(k,eadk_key_two)) code_pos=code_pos==0?1:0;
  else if (down(k,eadk_key_zero)) code_pos=code_pos==1?2:0;
  else if (down(k,eadk_key_one)) { if (code_pos==2) code_pos=3; else if (code_pos==3) {code_pos=4;screen=MENU;menu_sel=0;} else code_pos=0; }
}
int main(int argc, char **argv) {
  (void)argc; (void)argv; eadk_keyboard_state_t old=0;
  const char *menu[]={"Maths","Physique-Chimie Chap. 1","Physique-Chimie Chap. 2"};
  for (;;) {
    if (screen==SHEET) draw_sheet();
    else if (screen==MENU) draw_list("Menu secret",menu,3,menu_sel);
    else if (screen==INDEX) { const char *items[19]; items[0]="Cours complet"; for(unsigned i=0;i<section_count && i<18;i++) items[i+1]=course_sections[i]; draw_list("Sommaire",items,section_count+1,index_sel); }
    else draw_reader();
    eadk_keyboard_state_t k=eadk_keyboard_scan();
    if (k && !old) {
      if (screen==SHEET) sheet_key(k);
      else if (screen==MENU) { if(down(k,eadk_key_up)&&menu_sel)menu_sel--; else if(down(k,eadk_key_down)&&menu_sel<2)menu_sel++; else if(down(k,eadk_key_ok))select_course(menu_sel); else if(down(k,eadk_key_back))screen=SHEET; }
      else if (screen==INDEX) { if(down(k,eadk_key_up)&&index_sel)index_sel--; else if(down(k,eadk_key_down)&&index_sel<section_count)index_sel++; else if(down(k,eadk_key_ok)){page=0;screen=READER;} else if(down(k,eadk_key_back))screen=MENU; }
      else { if(down(k,eadk_key_up)&&page)page--; else if(down(k,eadk_key_down)){unsigned max=line_count>VISIBLE_LINES?line_count-VISIBLE_LINES:0;if(page<max)page++;} else if(down(k,eadk_key_left))page=page>10?page-10:0; else if(down(k,eadk_key_right)){unsigned max=line_count>VISIBLE_LINES?line_count-VISIBLE_LINES:0;page=page+10<max?page+10:max;} else if(down(k,eadk_key_back))screen=INDEX; }
      release(); old=0;
    } else old=k;
    eadk_timing_msleep(25);
  }
}
