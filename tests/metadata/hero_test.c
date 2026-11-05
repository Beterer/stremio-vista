/* Check metadata-to-hero wiring and invalidation as focused content changes. */
#include <assert.h>
#include "../../src/screens/board.c"

static cJSON *metadata;
static TmdbLoc localized;
static int useLocalized;
static char genresShown[256];
double app_now(void) { return 10; }
const char *T(const char *s) { return s; }
unsigned i18n_gen(void) { return 1; }
const TmdbLoc *tmdb_loc(const char *type, const char *id, int overview) { (void)type; (void)id; (void)overview; return useLocalized ? &localized : NULL; }
cJSON *sb_meta_cached(const char *type, const char *id) { (void)type; return !strcmp(id, "movie1") ? metadata : NULL; }
const char *sb_logo_of(const Item *it, char *out, size_t cap) { (void)it; (void)cap; out[0] = 0; return out; }
int img_failed(const char *url) { (void)url; return 0; }
int img_loaded(const char *url) { (void)url; return 0; }
GLuint img_get(const char *url, int w, int h, float *aspect) { (void)url; (void)w; (void)h; (void)aspect; return 0; }
int text_wrap_count(const char *s, float w, int lines, int px, int wt, float track) { (void)s; (void)w; (void)lines; (void)px; (void)wt; (void)track; return 1; }
void meta_prep(MetaParts *p, const char *line, const char *genres, const char *name, int px) {
  (void)p; (void)line; (void)name; (void)px; snprintf(genresShown, sizeof genresShown, "%s", genres ? genres : "");
}
static void prepare(Board *b) { hero_view(b, &b->view); b->viewVer++; hero_lay(b); }
int main(void) {
  Board b = { 0 }; snprintf(b.hero.id, sizeof b.hero.id, "movie1");
  metadata = cJSON_Parse("{\"genres\":[\"Horror\",\"Thriller\"]}"); assert(metadata);
  prepare(&b); assert(!strcmp(genresShown, "Horror, Thriller")); unsigned first = b.view.sig;
  cJSON_Delete(metadata); metadata = cJSON_Parse("{\"genres\":[\"Comedy\"]}");
  prepare(&b); assert(!strcmp(genresShown, "Comedy") && b.view.sig != first);
  snprintf(b.hero.id, sizeof b.hero.id, "missing"); prepare(&b); assert(!genresShown[0]);
  useLocalized = 1; localized.ngenres = 1; localized.genres[0] = "Localized drama";
  prepare(&b); assert(!strcmp(genresShown, "Localized drama"));
  cJSON_Delete(metadata); puts("browse hero: genres, late metadata, focus changes and localized genres passed");
  return 0;
}
