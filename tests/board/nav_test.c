/* Run the production top bar's layout with deterministic text metrics. */
#include <assert.h>
#include "../../src/core/app.c"

static int rtl, discoverLabels;
const char *T(const char *s) { return s; }
int i18n_rtl(void) { return rtl; }
int update_pill_focused(void) { return 0; }
float text_width(const char *s, int px, int weight, float tracking) {
  (void)weight; (void)tracking; if (!strcmp(s, "Discover")) discoverLabels++;
  return (float)strlen(s) * px * 0.5f;
}
int main(void) {
  for (rtl = 0; rtl <= 1; rtl++) {
    discoverLabels = 0; topNav.idx = NAV_HOME; topNav.active = 1; SCR = SCR_HOME;
    topnav_layout();
    assert(discoverLabels == 0);
    assert(chipScr(NAV_SET) == SCR_SET && chipScr(NAV_SEARCH) == SCR_SEARCH);
    for (int i = NAV_SEARCH + 1; i <= NAV_LIB; i++) assert(chipScr(i) == SCR_HOME || chipScr(i) == SCR_LIB);
    assert(chipScr(NAV_HOME) == SCR_HOME && chipScr(NAV_LIB) == SCR_LIB);
    float hx = topNav.chipX[NAV_HOME], lx = topNav.chipX[NAV_LIB];
    assert(rtl ? hx > lx : hx < lx);
    float left = fminf(hx, lx), right = fmaxf(hx + topNav.chipW[NAV_HOME], lx + topNav.chipW[NAV_LIB]);
    assert(fabsf((left + right) / 2 - SB_W / 2) < 0.01f);
  }
  puts("top navigation: Home and Library only, centered in LTR and RTL");
  return 0;
}
