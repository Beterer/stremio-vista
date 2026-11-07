/* Exercise the production board's row construction and asynchronous catalog replies. */
#include <assert.h>
#include "../../src/screens/board.c"

Scr SCR = SCR_HOME;
TopNav topNav;
static int cwCount, discCount = 2, pendingCount;
static struct { CatalogDef def; void (*cb)(ItemList *, int, void *); void *ud; } pending[256];
double app_now(void) { return 10; }
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
  if (!max) return 0;
  memset(out, 0, sizeof *out); snprintf(out->title, sizeof out->title, "Old Home catalog"); return 1;
}
int sb_disc_defs(CatalogDef *out, int max) {
  if (!out) return discCount;
  int n = discCount < max ? discCount : max;
  for (int i = 0; i < n; i++) { memset(&out[i], 0, sizeof out[i]); snprintf(out[i].title, sizeof out[i].title, "Discover %d", i); out[i].skip = 1; }
  return n;
}
void sb_catalog(const CatalogDef *def, void (*cb)(ItemList *, int, void *), void *ud) {
  assert(pendingCount < 256); pending[pendingCount].def = *def; pending[pendingCount].cb = cb; pending[pendingCount++].ud = ud;
}
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
int main(void) {
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
  cwCount = 0; load_catalog_rows(b, 0, 0);
  assert(b->n == 2 && !strcmp(b->rows[0].title, "Discover 0"));
  reply(1, "stale"); assert(b->rows[0].items.n == 0 && b->rows[1].items.n == 0);
  reply(2, "fresh"); assert(!strcmp(b->rows[0].items.items[0].id, "fresh"));

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
  cwCount = 0; load_catalog_rows(b, 0, 1); assert(b->rows[0].closing);
  b->rows[0].closing = 0; b->rows[0].hidden = 1; /* the response arrives after the close animation */
  reply(2, "after-history"); assert(b->rowIdx == 1);
  reset(); cwCount = 0; discCount = 2; load_catalog_rows(b, 0, 0);
  reply(1, "second-first"); assert(b->rowIdx == 0); b->heroPending = 0;
  reply(0, NULL); assert(b->rowIdx == 1 && b->heroPending);
  reset(); discCount = 100; load_catalog_rows(b, 0, 0);
  for (int i = 0; i < 4; i++) reply(i, "ready"); b->rowIdx = 88; load_catalog_rows(b, 0, 1);
  reply(4, "restored"); reply(5, NULL); reply(6, NULL); catalogs_ahead(b);
  assert(!strcmp(pending[8].def.title, "Discover 85"));
  reply(8, "previous"); assert(browse_next_row(b, 87, -1) == 85);
  reset(); puts("board rows: Continue Watching first, account order, empty history, stale replies and 100 catalogs passed");
  return 0;
}
