/* Exercise the production board's row construction and asynchronous catalog replies. */
#include <assert.h>
#include "../../src/screens/board.c"

Scr SCR = SCR_HOME;
TopNav topNav;
static int cwCount, upcomingCount, discCount = 2, pendingCount;
static struct { CatalogDef def; void (*cb)(ItemList *, int, void *); void *ud; } pending[256];
double app_now(void) { return 10; }
unsigned tw_ver(void) { return 0; }
unsigned lib_version(void) { return (unsigned)cwCount; }
void focus_pop(FocusPop *f) { (void)f; }
void itemlist_clear(ItemList *l) { free(l->items); memset(l, 0, sizeof *l); }
void itemlist_free(ItemList *l) { itemlist_clear(l); free(l); }
int lib_cw_items(ItemList *out) {
  memset(out, 0, sizeof *out); out->n = cwCount;
  out->items = cwCount ? calloc((size_t)cwCount, sizeof(Item)) : NULL;
  for (int i = 0; i < cwCount; i++) snprintf(out->items[i].id, sizeof out->items[i].id, "resume%d", i);
  return cwCount;
}
int sb_home_defs(CatalogDef *out, int max) {
  if (!out) return discCount;
  int n = discCount < max ? discCount : max;
  for (int i = 0; i < n; i++) { memset(&out[i], 0, sizeof out[i]); snprintf(out[i].title, sizeof out[i].title, "Discover %d", i); out[i].skip = 1; }
  return n;
}
void sb_catalog(const CatalogDef *def, void (*cb)(ItemList *, int, void *), void *ud) {
  assert(pendingCount < 256); pending[pendingCount].def = *def; pending[pendingCount].cb = cb; pending[pendingCount++].ud = ud;
}
int notify_upcoming(ItemList *out) {
  memset(out, 0, sizeof *out); out->n = upcomingCount;
  if (upcomingCount) { out->items = calloc(1, sizeof(Item)); snprintf(out->items[0].id, sizeof out->items[0].id, "upcoming"); }
  return out->n;
}
int live_band(ItemList *out) { memset(out, 0, sizeof *out); return 0; }
int live_catalog_listed(const char *url) { (void)url; return 0; }
int live_type(const char *type) { (void)type; return 0; }
int sb_disc_types(char (*out)[16], int max) { (void)out; (void)max; return 0; }
int sb_disc_cats(const char *type, CatalogDef *out, char (*addon)[64], int max) { (void)type; (void)out; (void)addon; (void)max; return 0; }
int sb_disc_cat_genres(const CatalogDef *d, char (*out)[48], int max, int *required) { (void)d; (void)out; (void)max; *required = 0; return 0; }
void sb_disc_with_genre(CatalogDef *d, const char *genre) { (void)d; (void)genre; }
void sb_catalog_url(const char *url, const char *type, int n, void (*cb)(ItemList *, int, void *), void *ud) { (void)url; (void)type; (void)n; (void)cb; (void)ud; }
void sb_catalog_page(const CatalogDef *d, int skip, void (*cb)(ItemList *, int, void *), void *ud) { (void)d; (void)skip; (void)cb; (void)ud; }
const char *sb_logo_of(const Item *it, char *out, size_t cap) { (void)it; if (cap) out[0] = 0; return out; }
const char *sb_poster_of(const Item *it, char *out, size_t cap) { return sb_logo_of(it, out, cap); }
void accent_tint_fetch(const char *id, const char *poster, const char *logo) { (void)id; (void)poster; (void)logo; }
static void reply(int request, const char *id) {
  ItemList *l = calloc(1, sizeof *l); l->n = l->total = id ? 1 : 0; l->items = calloc(1, sizeof(Item));
  if (id) snprintf(l->items[0].id, sizeof l->items[0].id, "%s", id);
  pending[request].cb(l, 0, pending[request].ud);
}
static void reset(void) {
  for (int i = 0; i < boards[0].n; i++) row_clear(&boards[0].rows[i]);
  free(boards[0].rows);
  memset(&boards[0], 0, sizeof boards[0]); pendingCount = 0;
}
static void library_refresh_keeps_catalog_page(void) {
  Board *b = &boards[0];
  cwCount = 1; discCount = 2; board_update_rows(b);
  reply(0, "first"); reply(1, "next-row");
  Row *r = &b->rows[1];
  ItemList *page = calloc(1, sizeof *page);
  page->n = page->total = 10;
  page->items = calloc((size_t)page->n, sizeof *page->items);
  for (int i = 0; i < page->n; i++) snprintf(page->items[i].id, sizeof page->items[i].id, "page-%d", i);
  r->paging = 1;
  on_page(page, 1, tok(b, 1));
  b->rowIdx = 1; r->col = 9;
  slide_row(r); tween_set(&r->off, r->off.to);
  float offset = r->off.v;

  SCR = SCR_DETAIL;
  cwCount = 2;
  board_refresh_quiet();
  SCR = SCR_HOME;
  board_update_rows(b);
  const Item *selected = focused_of(b);
  assert(selected && !strcmp(selected->id, "page-8"));
  assert(r->off.v == offset && r->items.n == 11);
  assert(!strcmp(r->items.items[10].id, "page-9") && !b->rows[2].hidden);
  assert(b->rows[0].items.n == 2);
  reset();
}
static void dynamic_row_keeps_catalog_page(void) {
  Board *b = &boards[0]; cwCount = 1; discCount = 2; board_update_rows(b);
  reply(0, "catalog"); b->rowIdx = 1;
  Row *r = &b->rows[1]; r->skipAt = 32; r->paged = 1;
  upcomingCount = 1; board_refresh_quiet();
  assert(b->rowIdx == 2 && b->rows[1].items.n == 1);
  assert(!strcmp(focused_of(b)->id, "catalog") && b->rows[2].skipAt == 32 && b->rows[2].paged);
  upcomingCount = 0; board_refresh_quiet();
  assert(b->rows[1].closing && !strcmp(focused_of(b)->id, "catalog"));
  reset();
}
int main(void) {
  library_refresh_keeps_catalog_page();
  dynamic_row_keeps_catalog_page();
  Board *b = &boards[0];
  cwCount = 2; load_catalog_rows(b, 0, 0);
  assert(b->n == 3 && b->rows[0].wide && b->rows[0].items.n == 2);
  assert(!strcmp(b->rows[0].title, "Continue Watching"));
  assert(!strcmp(b->rows[1].title, "Discover 0") && !strcmp(b->rows[2].title, "Discover 1"));
  assert(pendingCount == 2 && !strcmp(pending[0].def.title, "Discover 0"));
  reply(0, "movie");
  assert(!strcmp(b->rows[1].items.items[0].id, "movie"));
  assert(!strcmp(b->rows[0].items.items[0].id, "resume0"));
  assert(b->rows[1].more && !b->rows[1].hidden);

  /* A reply sent before Continue Watching disappears must not land in a shifted row. */
  cwCount = 0; refresh_cw(b);
  reply(1, "next"); assert(!strcmp(b->rows[2].items.items[0].id, "next"));
  assert(b->rows[0].hidden && b->rowIdx == 1);
  assert(!strcmp(b->rows[1].items.items[0].id, "movie"));

  /* A reply from a replaced catalog generation is still ignored. */
  load_catalog_rows(b, 0, 0);
  assert(b->n == 3 && b->rows[0].hidden && !strcmp(b->rows[1].title, "Discover 0"));
  reply(1, "stale"); assert(b->rows[2].items.n == 0);
  reply(2, "fresh"); assert(!strcmp(b->rows[1].items.items[0].id, "fresh"));

  reset(); cwCount = 1; discCount = 12; load_catalog_rows(b, 0, 0);
  assert(b->n == 13 && pendingCount == 4);
  for (int i = 0; i < 4; i++) reply(i, "ready");
  b->rowIdx = 12; catalogs_ahead(b); assert(pendingCount == 7);
  reply(4, "last"); assert(!strcmp(b->rows[12].items.items[0].id, "last"));
  reset(); cwCount = 1; discCount = 100; load_catalog_rows(b, 0, 0);
  assert(b->n == 101);
  assert(pendingCount == 4); for (int i = 0; i < 4; i++) reply(i, "ready");
  b->rowIdx = 88; catalogs_ahead(b); assert(pendingCount == 8);
  assert(!strcmp(pending[4].def.title, "Discover 87"));
  reply(4, "high-row"); assert(!strcmp(b->rows[88].items.items[0].id, "high-row"));
  assert(item_at(b, 88, 0) == &b->rows[88].items.items[0]);
  load_catalog_rows(b, 0, 1); assert(b->rowIdx == 88 && pendingCount == 12);
  assert(browse_next_row(b, 87, -1) == -2);
  reply(9, "before-one"); assert(b->rowIdx == 88);
  reply(10, "before-two"); reply(8, "restored");
  assert(b->rowIdx == 88 && browse_next_row(b, 87, -1) == 87);
  reset(); cwCount = 1; discCount = 2; load_catalog_rows(b, 0, 0);
  cwCount = 0; refresh_cw(b); assert(b->rows[0].closing);
  b->rows[0].closing = 0; b->rows[0].hidden = 1; /* the response arrives after the close animation */
  reply(0, "after-history"); assert(b->rowIdx == 1);
  reset(); cwCount = 0; discCount = 2; load_catalog_rows(b, 0, 0);
  reply(1, "second-first"); assert(b->rowIdx == 1); b->heroPending = 0;
  reply(0, NULL); assert(b->rowIdx == 2 && b->heroPending);
  reset(); cwCount = 0; discCount = 2; board_update_rows(b);
  reply(0, NULL); reply(1, NULL);
  cwCount = 1; board_refresh_quiet();
  const Item *selected = focused_of(b);
  assert(selected && !strcmp(selected->id, "resume0"));
  reset(); discCount = 100; load_catalog_rows(b, 0, 0);
  for (int i = 0; i < 4; i++) reply(i, "ready"); b->rowIdx = 88; load_catalog_rows(b, 0, 1);
  reply(4, "restored"); reply(5, NULL); reply(6, NULL); catalogs_ahead(b);
  assert(!strcmp(pending[8].def.title, "Discover 84"));
  reply(8, "previous"); assert(browse_next_row(b, 87, -1) == 85);
  reset(); puts("board rows: catalog focus after library changes, account order, empty history, stale replies and 100 catalogs passed");
  return 0;
}
