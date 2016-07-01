/*
 * OMG codes, really so dumb and dirty codes.
 */

#include <linux/compat.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/fb.h>

#include "tw_pglines.h"

struct pgl_vertex pgls[] = {
  [0] = { .x = 0, .y = 0 },
  [1] = { .x = 0, .y = 0 },
  [2] = { .x = 0, .y = 0 },
  [3] = { .x = 0, .y = 0 },
  [4] = { .x = 0, .y = 0 },
  [5] = { .x = 0, .y = 0 },
  [6] = { .x = 0, .y = 0 },
  [7] = { .x = 0, .y = 0 },
};

//struct pgl_vertex default_pglines[] = {
//  [0] = { .x = 404, .y = 166 },
//  [1] = { .x = 410, .y = 166 },
//  [2] = { .x = 170, .y = 600 },
//  [3] = { .x = 187, .y = 600 },
//  [4] = { .x = 614, .y = 166 },
//  [5] = { .x = 620, .y = 166 },
//  [6] = { .x = 837, .y = 600 },
//  [7] = { .x = 854, .y = 600 },
//};
struct pgl_vertex default_pglines[] = {
  [0] = { .x = 316, .y = 132 },
  [1] = { .x = 322, .y = 132 },
  [2] = { .x = 132, .y = 480 },
  [3] = { .x = 147, .y = 480 },
  [4] = { .x = 483, .y = 132 },
  [5] = { .x = 489, .y = 132 },
  [6] = { .x = 667, .y = 480 },
  [7] = { .x = 682, .y = 480 },
};

//struct pgl_vertex default_pglines_virtualfb[] = {
//  [0] = { .x = 936, .y = 505 },
//  [1] = { .x = 940, .y = 505 },
//  [2] = { .x = 558, .y = 915 },
//  [3] = { .x = 580, .y = 915 },
//  [4] = { .x = 1108, .y = 505 },
//  [5] = { .x = 1112, .y = 505 },
//  [6] = { .x = 1468, .y = 915 },
//  [7] = { .x = 1490, .y = 915 },
//};

struct pgl_vertex default_pglines_virtualfb[] = {
  [0] = { .x = 729, .y = 404 },
  [1] = { .x = 733, .y = 404 },
  [2] = { .x = 427, .y = 732 },
  [3] = { .x = 445, .y = 732 },


  [4] = { .x = 867, .y = 404 },
  [5] = { .x = 871, .y = 404 },
  [6] = { .x = 1155, .y = 732 },
  [7] = { .x = 1173, .y = 732 },
};




#define PGL_GREEN_COLOR		(PGL_RGB(0x64, 0xFD, 0x0))
#define PGL_ORANGE_COLOR	(PGL_RGB(0xFF, 0xA2, 0x0))
#define PGL_RED_COLOR		(PGL_RGB(0xFF, 0x0, 0x2A))

/*
 * all values tested based on the original vertices coordinations
 * 
 *  #1---#2
 *   |    |
 *  #3---#4
 *
 * 1. needs to check a possible division by zero
 * 2. calc the minimum size of a unit figure
 *
 */
bool is_pglines_valid(const struct pgl_vertex *v1, const struct pgl_vertex *v2, 
                      const struct pgl_vertex *v3, const struct pgl_vertex *v4) 
{
  int i;
  const unsigned int test[] = { abs((v2->x) - (v1->x)),
                                abs((v4->x) - (v3->x)), 
                                ((v2->x) - (v1->x)),
                                ((v4->x) - (v3->x)) };

//  printk("v1->x %d\n", v1->x);
//  printk("v1->y %d\n", v1->y);
  
//  printk("v2->x %d\n", v2->x);
//  printk("v2->y %d\n", v2->y);
  
//  printk("v3->x %d\n", v3->x);
//  printk("v3->y %d\n", v3->y);
  
//  printk("v4->x %d\n", v4->x);
//  printk("v4->y %d\n", v4->y);



  if (v2->x > MAX_XRES_VIRTUAL || v2->y > MAX_YRES_VIRTUAL ||
      v4->x > MAX_XRES_VIRTUAL || v4->y > MAX_YRES_VIRTUAL ||
      v1->x < 0 || v1->y < 0 ||
      v3->x < 0 || v3->y < 0)
    return false;

  if ((v3->y - v1->y) < 8) /* magic : MAX DISPLACEMENT offset is 12 */
    return false;

  if ((v2->x - v1->x) < 2) /* magic : points of pixels left v1, v2 */
    return false;

  if (v3->x == v4->x || v1->y == v4->y)
    return false;

  for (i = 0; i < 4; i++) {
    if (test[i] == 0)
      return false;
  }

  return true;
}

//void set_pglines_bg(struct fb_info *fb)
//{
//  struct fb_fix_screeninfo *fix = &fb->fix;
//  struct fb_var_screeninfo *var = &fb->var;
//  u32 height = var->yres;
//  u32 line = fix->line_length;
//  u32 i, j;
//
//  for (i = 0; i < height; i++) {
//    for (j = 0; j < var->xres; j++) {
//      memset(fb->screen_base + i * line + j * 4 + 0, 0x64, 1);
//      memset(fb->screen_base + i * line + j * 4 + 1, 0x78, 1);
//      memset(fb->screen_base + i * line + j * 4 + 2, 0x78, 1);
//    }
//  }
//}
//EXPORT_SYMBOL(set_pglines_bg);
//


inline static void set_pixel(struct backward_camera_fb_info *fb, 
                             const int x, const int y, 
                             const unsigned int color)
{

    int temp_x;
    int temp_y;


    if(x > 800+400) return;
    if(x < 400) return;
    if(y > 480+240) return;
    if(y < 240) return ;



    temp_x = x - 400;
    temp_y = y - 240;

    if(temp_x <0) temp_x =0;
    if(temp_y <0) temp_y =0;



    u32* pbuffer = fb->mem;  
    pbuffer[temp_y* fb->width +temp_x] = color;


}






inline static void set_hline(struct backward_camera_fb_info *fb, 
                             const int x0, const int x1, const int y, 
                             const unsigned int color)
{
  int i;

  if (x0 < 0 || x1 > MAX_XRES_VIRTUAL || y > MAX_YRES_VIRTUAL)
    return;

  if (x1 > x0) {
    for (i = 0; i < (x1 - x0); i++) {
      set_pixel(fb, x0 + i, y, color);
    }
  }

  if (x0 > x1) {
    for (i = 0; i < (x0 - x1); i++) {
      set_pixel(fb, x0 - i, y, color);
    }
  }

}

void connect_hline(struct backward_camera_fb_info *fb,
                          const int x0, const int x1, const int y,
                          const unsigned int color, const unsigned int tick)
{
  int i;

  for (i = 0; i < tick; i++)
    set_hline(fb, x0, x1, y + i, color);
}

void fill_trapezoidal(struct backward_camera_fb_info *fb,
                      const struct pgl_vertex *v1, const struct pgl_vertex *v2,
                      const struct pgl_vertex *v3, const struct pgl_vertex *v4,
                      const enum pgl_line_type ltype, struct pgl_section_info sections[])
{
  const unsigned int DISPLACEMENT = HORIZONTAL_LINE_DISPLACEMENT;
  const bool isLeft = (ltype == PGL_LEFT);

  int dy, dx1, dx2;
  int p1, p2;
  int m1, m2;
  int unit_1, unit_2;
  int const11, const12, const21, const22;
  int y, x1, x2;

  unsigned int yBegin;
  unsigned int yEnd;

  int color_step;
  unsigned int steps;
  unsigned int begin;

  if (ltype == PGL_LEFT)
    dy = v2->y - v1->y;
  else 
    dy = v4->y - v3->y;

  dx1 = abs((v2->x) - (v1->x)) % dy;
  dx2 = abs((v4->x) - (v3->x)) % dy;
  m1 = ((v2->x) - (v1->x)) / dy;
  m2 = ((v4->x) - (v3->x)) / dy;

  const11 = 2 * dx1;
  const12 = 2 * (dx1 - dy);
  const21 = 2 * dx2;
  const22 = 2 * (dx2 - dy);

  p1 = (2 * dx1) - dy;
  p2 = (2 * dx2) - dy;

  x1 = v1->x;
  x2 = v3->x;

  if (v2->x > v1->x) {
    unit_1 = 1;
  }

  if (v2->x < v1->x) {
    unit_1 = -1;
  }

  if (v4->x > v3->x) {
    unit_2 = 1;
  }

  if (v4->x < v3->x) {
    unit_2 = -1;
  }

  color_step = 0;
  begin = 0;

  if (ltype == PGL_LEFT) {
    yBegin = v1->y;
    yEnd = v2->y;
    steps = ((v2->y - v1->y) ? (v2->y - v1->y) : (v2->y - v1->y) - 1) / 3;
  } else {
    yBegin = v3->y;
    yEnd = v4->y;
    steps = ((v4->y - v3->y) ? (v4->y - v3->y) : (v4->y - v3->y) - 1) / 3;
  }

  for (y = yBegin; y < yEnd ; y++) {

    if (p1 < 0) {
      p1 += const11;
      x1 += m1;
    } else {
      p1 += const12;
      x1 += m1 + unit_1;
    }

    if (p2 < 0) {
      p2 += const21;
      x2 += m2;
    } else {
      p2 += const22;
      x2 += m2 + unit_2;
    }

    if (color_step == 0) { 
      set_hline(fb, x1, x2, y, PGL_GREEN_COLOR);

      if (begin == 0) {
        sections[color_step].x1 = isLeft ? x2 : x1;
        sections[color_step].y1 = y;
      }

      if (begin == DISPLACEMENT - 4) { // this should be DISPLACEMENT, though.
        sections[color_step].x2 = isLeft ? x2 : x1;
        sections[color_step].y2 = y;
      }

    }

    if (color_step == 1) {
      set_hline(fb, x1, x2, y, PGL_ORANGE_COLOR);

      if (begin == 0) {
        sections[color_step].x1 = isLeft ? x2 : x1;
        sections[color_step].y1 = y;
      }

      if (begin == DISPLACEMENT) {
        sections[color_step].x2 = isLeft ? x2 : x1;
        sections[color_step].y2 = y;
      }
    }

    if (color_step == 2) {
      set_hline(fb, x1, x2, y, PGL_RED_COLOR);

      if (begin == 0) {
        sections[color_step].x1 = isLeft ? x2 : x1;
        sections[color_step].y1 = y;
      }

      if (begin == DISPLACEMENT + 4) {
        sections[color_step].x2 = isLeft ? x2 : x1;
        sections[color_step].y2 = y;
      }
    }

    begin += 1;

    if (begin == steps) {

      if (begin < DISPLACEMENT) {
        sections[color_step].x2 = isLeft ? x2 : x1;
        sections[color_step].y2 = y;
      }

      begin = 0;
      color_step += 1;
    }

  } // end, for

}


void fill_full_trapezoid_color(struct backward_camera_fb_info *fb,
                               const struct pgl_vertex *v1, const struct pgl_vertex *v2,
                               const struct pgl_vertex *v3, const struct pgl_vertex *v4,
                               const unsigned color)
{
  int dy, dx1, dx2;
  int p1, p2;
  int m1, m2;
  int unit_1, unit_2;
  int const11, const12, const21, const22;
  int y, x1, x2;

  dy = v2->y - (v1->y);

  dx1 = abs((v2->x) - (v1->x)) % dy;
  dx2 = abs((v4->x) - (v3->x)) % dy;
  m1 = ((v2->x) - (v1->x)) / dy;
  m2 = ((v4->x) - (v3->x)) / dy;

  const11 = 2 * dx1;
  const12 = 2 * (dx1 - dy);
  const21 = 2 * dx2;
  const22 = 2 * (dx2 - dy);

  p1 = (2 * dx1) - dy;
  p2 = (2 * dx2) - dy;

  x1 = v1->x;
  x2 = v3->x;

  if (v2->x > v1->x) {
    unit_1 = 1;
  }

  if (v2->x < v1->x) {
    unit_1 = -1;
  }

  if (v4->x > v3->x) {
    unit_2 = 1;
  }

  if (v4->x < v3->x) {
    unit_2 = -1;
  }

  for (y = v1->y; y < v2->y ; y++) {

    if (p1 < 0) {
      p1 += const11;
      x1 += m1;
    } else {
      p1 += const12;
      x1 += m1 + unit_1;
    }

    if (p2 < 0) {
      p2 += const21;
      x2 += m2;
    } else {
      p2 += const22;
      x2 += m2 + unit_2;
    }

    set_hline(fb, x1, x2, y, color);

  } // end, for

}


void draw_pglines(struct backward_camera_fb_info *fb)
{
  struct pgl_vertex _v1;
  struct pgl_vertex _v2;
  struct pgl_vertex _v3;
  struct pgl_vertex _v4;

  int i;

  struct pgl_section_info lefts[] = { { .x1 = -1, .y1 = -1, .x2 = -1, .y2 = -1 },
                                      { .x1 = -1, .y1 = -1, .x2 = -1, .y2 = -1 },
                                      { .x1 = -1, .y1 = -1, .x2 = -1, .y2 = -1 } };
  struct pgl_section_info rights[] = { { .x1 = -1, .y1 = -1, .x2 = -1, .y2 = -1 },
                                       { .x1 = -1, .y1 = -1, .x2 = -1, .y2 = -1 },
                                       { .x1 = -1, .y1 = -1, .x2 = -1, .y2 = -1 } };




  memset(fb->mem, 0 , fb->width * fb->height*4);




  /*
   * struct pgl_vertex vl1 = { 397, 174 };
   * struct pgl_vertex vl2 = { 86, 580 };
   * struct pgl_vertex vl3 = { 402, 173 };
   * struct pgl_vertex vl4 = { 113, 575 };

   * struct pgl_vertex vr1 = { 626, 174 };
   * struct pgl_vertex vr2 = { 937, 580 };
   * struct pgl_vertex vr3 = { 621, 173 };
   * struct pgl_vertex vr4 = { 910, 575 };
   *
   * matrix
   *
   *  1 2
   *  3 4
   *
   * 1 -> 2 -> 3 -> 4
   * 1 -> 3 -> 2 -> 4
   *
   * fill_trapezoidal(fbdev->fb[j], &vl1, &vl2, &vl3, &vl4);
   * fill_trapezoidal(fbdev->fb[j], &vr1, &vr2, &vr3, &vr4);
   *
   *
   * pgls[0].x = 936; vl1.x
   * pgls[0].y = 505; vl1.y
   * pgls[1].x = 940;  vl2.x
   * pgls[1].y = 505; vl2.y
   * pgls[2].x = 558; vl3.x
   * pgls[2].y = 915; vl3.y
   * pgls[3].x = 580; vl4.x
   * pgls[3].y = 915; vl4.y
   *
   * pgls[4].x = 1108; vr1.x
   * pgls[4].y = 505; vr1.y
   * pgls[5].x = 1112; vr2.x
   * pgls[5].y = 505; vr2.y
   * pgls[6].x = 1468; vr3.x
   * pgls[6].y = 915; vr3.y
   * pgls[7].x = 1490; vr4.x
   * pgls[7].y = 915; vr4.y
   *
   * fill_trapezoidal(fbdev->fb[j], &pgls[0], &pgls[1], &pgls[2], &pgls[3]);
   * fill_trapezoidal(fbdev->fb[j], &pgls[4], &pgls[5], &pgls[6], &pgls[7]);
   *
   *
   */

#ifdef PGLINES_VERTICES_DEBUG
  printk("[DEBUG/pglines]\n" 
         "vl1: %d, %d \n"
         "vl2: %d, %d \n"
         "vl3: %d, %d \n"
         "vl4: %d, %d \n"
         "vr1: %d, %d \n"
         "vr2: %d, %d \n"
         "vr3: %d, %d \n"
         "vr4: %d, %d \n", 
         pgls[0], pgls[1], pgls[2], pgls[3], pgls[4], pgls[5], pgls[6], pgls[7]);
#endif

  const bool left_ok = is_pglines_valid(&pgls[0], &pgls[1], &pgls[2], &pgls[3]) == true;
  const bool right_ok = is_pglines_valid(&pgls[4], &pgls[5], &pgls[6], &pgls[7]) == true;

  if (left_ok && right_ok) {
    fill_trapezoidal(fb, &pgls[0], &pgls[2], &pgls[1], &pgls[3], PGL_LEFT, lefts);
    fill_trapezoidal(fb, &pgls[4], &pgls[6], &pgls[5], &pgls[7], PGL_RIGHT, rights);
  } else
 {
    fill_trapezoidal(fb,
                     &default_pglines_virtualfb[0], &default_pglines_virtualfb[2],
                     &default_pglines_virtualfb[1], &default_pglines_virtualfb[3],
                     PGL_LEFT, lefts);
    fill_trapezoidal(fb,
                     &default_pglines_virtualfb[4], &default_pglines_virtualfb[6],
                     &default_pglines_virtualfb[5], &default_pglines_virtualfb[7],
                     PGL_RIGHT, rights);
  }

#ifdef PGLINES_VERTICES_DEBUG
  printk("\n");
  for (i = 0; i < 3; i++) {
    printk("-l:%d %d %d %d\n", lefts[i].x1, lefts[i].y1, lefts[i].x2, lefts[i].y2);
    printk("-r:%d %d %d %d\n", rights[i].x1, rights[i].y1, rights[i].x2, rights[i].y2);
    printk("\n");
  }
#endif

  for (i = 0; i < 3; i++) {
    _v1.x = lefts[i].x1;
    _v1.y = lefts[i].y1;
    _v2.x = rights[i].x1;
    _v2.y = rights[i].y1;
    _v3.x = lefts[i].x2;
    _v3.y = lefts[i].y2;
    _v4.x = rights[i].x2;
    _v4.y = rights[i].y2;

    if (i == 0)
      fill_full_trapezoid_color(fb, &_v1, &_v3, &_v2, &_v4, PGL_RGB(0x64, 0xFD, 0x0));

    if (i == 1)
      fill_full_trapezoid_color(fb, &_v1, &_v3, &_v2, &_v4, PGL_RGB(0xFF, 0xA2, 0x0));

    if (i == 2)
      fill_full_trapezoid_color(fb, &_v1, &_v3, &_v2, &_v4, PGL_RGB(0xFF, 0x0, 0x2A));
  }

}
