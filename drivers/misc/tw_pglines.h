/*
 * a parking guidelines header file
 */

#ifndef __TW_PGLINES_H
#define __TW_PGLINES_H __FILE__

struct pgl_vertex {
  int x;
  int y;
};

struct pgl_section_info {
  int x1;
  int y1;
  int x2;
  int y2;
};


struct backward_camera_fb_info {
 
    
    u32 width;
    u32 height;
    void* mem;

};


enum pgl_line_type { PGL_LEFT = 0, PGL_RIGHT }; // intentionally either case only.


#define MAX_XRES_VIRTUAL	1600	/* offset (left,right == 800) */
#define MAX_YRES_VIRTUAL	960 /* offset (top,bottom = 480) */
#define XOFFSET_PGLINES		400
#define YOFFSET_PGLINES		240 /* for now it became 0, which needs to be '300' */
#define HORIZONTAL_LINE_DISPLACEMENT	8

#define PGLMAX(x,y) ((x) > (y) ? (x) : (y))
#define PGLMIN(x,y) ((x) < (y) ? (x) : (y))

#define PGL_RGB(r, g, b) (((r & 0xff) << 16) | ((g & 0xff) << 8) | ((b & 0xff) << 0) | 0xff000000)

extern bool is_pglines_valid(const struct pgl_vertex *vl, const struct pgl_vertex *v2, 
                             const struct pgl_vertex *v3, const struct pgl_vertex *v4);

extern void set_pglines_bg(struct backward_camera_fb_info *fb);

/* unused currently */
extern void connect_hline(struct backward_camera_fb_info *fb,
                          const int x0, const int x1, const int y,
                          const unsigned int color, const unsigned int tick);

extern void fill_trapezoidal(struct backward_camera_fb_info *fb,
                             const struct pgl_vertex *p1, const struct pgl_vertex *p2,
                             const struct pgl_vertex *p3, const struct pgl_vertex *p4,
                             const enum pgl_line_type ltype, struct pgl_section_info sections[]);

extern void fill_full_trapezoid_color(struct backward_camera_fb_info *fb,
                                      const struct pgl_vertex *p1, const struct pgl_vertex *p2,
                                      const struct pgl_vertex *p3, const struct pgl_vertex *p4,
                                      const unsigned int color);

extern void draw_pglines(struct backward_camera_fb_info *fb);
//extern void draw_pglines(struct fb_info *fb);

extern struct pgl_vertex default_pglines[];
extern struct pgl_vertex default_pglines_virtualfb[];

#endif /* _TW_PGLINES_H_ */
