/* Check the production metadata parser and rendered field order without GL. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ui/widgets.h"
#include "ui/text.h"
#include "ui/gfx.h"

static char drawn[12][256]; static int ndrawn, dots, rtl; static float firstX;
const char *T(const char *s) { return s; }
const char *T1(const char *s, const char *key, const char *value) {
  (void)key; static char b[64]; snprintf(b, sizeof b, "%s%s", value, strstr(s, "min") ? "min" : "h"); return b;
}
const char *T2(const char *s, const char *k1, const char *v1, const char *k2, const char *v2) {
  (void)s; (void)k1; (void)k2; static char b[64]; snprintf(b, sizeof b, "%sh %smin", v1, v2); return b;
}
unsigned i18n_gen(void) { return 1; }
int app_rtl(void) { return rtl; }
int text_line_height(int px) { return px + 4; }
float text_width(const char *s, int px, int wt, float track) { (void)px; (void)wt; (void)track; return (float)strlen(s) * 10; }
float text_draw(const char *s, float x, float y, int px, int wt, uint32_t c, float track, float maxW, int align) {
  (void)y; (void)c; (void)maxW; (void)align;
  assert(ndrawn < 12); if (!ndrawn) firstX = x;
  snprintf(drawn[ndrawn++], sizeof drawn[0], "%s", s); return text_width(s, px, wt, track);
}
void gfx_circle(float x, float y, float r, uint32_t c) { (void)x; (void)y; (void)r; (void)c; dots++; }
void gfx_rrect(float x, float y, float w, float h, float r, uint32_t c) { (void)x; (void)y; (void)w; (void)h; (void)r; (void)c; }
static void render(const MetaParts *p, int align) {
  ndrawn = dots = 0;
  assert(fabsf(meta_draw_p(p, 1000, 50, WHITE_A(255), align) - p->w) < 0.01f);
}
int main(void) {
  MetaParts p;
  meta_prep(&p, "2024 1h 50min IMDb 7.2", "Horror, Thriller", NULL, 25);
  assert(!strcmp(p.rt, "110min"));
  render(&p, ALIGN_LEFT);
  assert(ndrawn == 5 && dots == 3);
  assert(!strcmp(drawn[0], "110min") && !strcmp(drawn[1], "2024"));
  assert(!strcmp(drawn[2], "Horror, Thriller") && !strcmp(drawn[3], "IMDb") && !strcmp(drawn[4], "7.2"));
  assert(firstX == 1000);
  rtl = 1; render(&p, ALIGN_START); assert(fabsf(firstX - (1000 - p.w)) < 0.01f); rtl = 0;

  meta_prep(&p, "2019-2021 65 min IMDb 8.1", "Comedy", NULL, 25);
  assert(!strcmp(p.rt, "65min") && !strcmp(p.year, "2019-2021"));
  render(&p, ALIGN_LEFT); assert(!strcmp(drawn[0], "65min") && !strcmp(drawn[1], "2019-2021"));

  meta_prep(&p, "2020", "Drama", NULL, 25); render(&p, ALIGN_LEFT);
  assert(ndrawn == 2 && dots == 1 && !strcmp(drawn[0], "2020") && !strcmp(drawn[1], "Drama"));
  meta_prep(&p, "95 minutes", "", NULL, 25); render(&p, ALIGN_LEFT);
  assert(ndrawn == 1 && !dots && !strcmp(drawn[0], "95min"));
  meta_prep(&p, "IMDb 6.0", "", NULL, 25); render(&p, ALIGN_LEFT);
  assert(ndrawn == 2 && !dots && !strcmp(drawn[0], "IMDb"));
  meta_prep(&p, "", "", NULL, 25); render(&p, ALIGN_LEFT); assert(!ndrawn && !dots && !p.w);
  puts("metadata row: minutes, field order, missing fields, measurement and RTL alignment passed");
  return 0;
}
