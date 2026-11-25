/* Installed catalog enumeration must survive large collections and configured URLs. */
#include <assert.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/stremio.h"
#include "core/net.h"

static cJSON *addons;
static char requestUrl[SB_CATALOG_URL_MAX], readKey[SB_CATALOG_URL_MAX + 8], written[8][SB_CATALOG_URL_MAX + 8];
static int nwritten, answers; static NetCb networkReply; static void *networkData;
unsigned SDL_GetTicks(void) { return 0; }
unsigned char *cache_read(const char *key, long age, size_t *len) { (void)age; (void)len; snprintf(readKey, sizeof readKey, "%s", key); return NULL; }
int cache_file(const char *key, long age, char *path, size_t cap) { (void)key; (void)age; (void)path; (void)cap; return 0; }
void cache_write(const char *key, const unsigned char *data, size_t len) { (void)data; (void)len; assert(nwritten < 8); snprintf(written[nwritten++], sizeof written[0], "%s", key); }
unsigned net_get(const char *url, NetCb cb, void *ud) { snprintf(requestUrl, sizeof requestUrl, "%s", url); networkReply = cb; networkData = ud; return 1; }
cJSON *net_json(const unsigned char *body) { return cJSON_Parse((const char *)body); }
void net_json_free(cJSON *json) { cJSON_Delete(json); }
static void answer(ItemList *list, int cached, void *ud) { (void)cached; (void)ud; assert(list->n == 1); answers++; itemlist_free(list); }
static void page(ItemList *list, int more, void *ud) { (void)more; answer(list, 0, ud); }
static void respond(void) {
  const char *body = "{\"metas\":[{\"id\":\"tt123\",\"name\":\"A title\"}],\"cacheMaxAge\":120}";
  networkReply(200, (const unsigned char *)body, strlen(body), networkData);
}
const char *T(const char *s) { return s; }
const char *store_get(const char *key, const char *fallback) { return !strcmp(key, "adOn_disabled") ? "false" : fallback; }
cJSON *store_node(const char *key) { return !strcmp(key, "addons") ? addons : NULL; }
void plat_thread_background(int nice_by) { (void)nice_by; }
static pthread_mutex_t pageLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t pageReady = PTHREAD_COND_INITIALIZER;
static NetCb pageReply;
static void *pageData;
unsigned net_file(const char *path, NetCb cb, void *ud) {
  (void)path; pthread_mutex_lock(&pageLock);
  pageReply = cb; pageData = ud;
  pthread_cond_signal(&pageReady); pthread_mutex_unlock(&pageLock);
  return 1;
}
static void finish_page(void) {
  if (answers == 3) return; /* the fallback can finish synchronously */
  struct timespec until; clock_gettime(CLOCK_REALTIME, &until); until.tv_sec += 5;
  pthread_mutex_lock(&pageLock);
  while (!pageReply) assert(pthread_cond_timedwait(&pageReady, &pageLock, &until) == 0);
  pthread_mutex_unlock(&pageLock);
  pageReply(0, NULL, 0, pageData);
}
unsigned net_get_timeout(const char *url, int ms, NetCb cb, void *ud) { (void)ms; return net_get(url, cb, ud); }
int traktrows_is(const char *url) { (void)url; return 0; }
void traktrows_fetch(const char *url, NetCb cb, void *ud) { (void)url; (void)cb; (void)ud; assert(0); }
int traktrows_defs(CatalogDef *out, int max) { (void)out; (void)max; return 0; }

int main(void) {
  addons = cJSON_CreateArray();
  cJSON *ad = cJSON_CreateObject(); cJSON_AddItemToArray(addons, ad);
  char transport[6200]; memset(transport, 'x', sizeof transport);
  memcpy(transport, "https://example.invalid/", 24); strcpy(transport + 5600, "/manifest.json");
  cJSON_AddStringToObject(ad, "transportUrl", transport);
  cJSON *man = cJSON_AddObjectToObject(ad, "manifest"); cJSON_AddStringToObject(man, "id", "lists");
  cJSON *cats = cJSON_AddArrayToObject(man, "catalogs");
  for (int i = 0; i < 70; i++) {
    cJSON *cat = cJSON_CreateObject(); char id[32]; snprintf(id, sizeof id, "list%d", i);
    cJSON_AddStringToObject(cat, "id", id); cJSON_AddStringToObject(cat, "name", id);
    cJSON_AddStringToObject(cat, "type", i % 2 ? "series" : "movie"); cJSON_AddItemToArray(cats, cat);
  }
  assert(sb_home_defs(NULL, 0) == 70);
  CatalogDef *defs = calloc(70, sizeof *defs); assert(defs);
  assert(sb_home_defs(defs, 70) == 70);
  assert(!strcmp(defs[0].title, "list0 · Movies") && !strcmp(defs[69].title, "list69 · Series"));
  assert(strlen(defs[0].url) > 5600 && strstr(defs[0].url, "/catalog/movie/list0.json"));
  assert(strstr(defs[69].url, "/catalog/series/list69.json") && strcmp(defs[0].url, defs[69].url));
  sb_catalog(&defs[0], answer, NULL); assert(!strcmp(requestUrl, defs[0].url)); respond();
  assert(!strcmp(readKey + 4, defs[0].url) && !strcmp(written[0] + 4, defs[0].url));
  assert(!strcmp(written[1] + 4, defs[0].url));
  sb_catalog(&defs[69], answer, NULL); respond();
  assert(strcmp(written[0], written[2]) && !strcmp(written[2] + 4, defs[69].url));
  sb_catalog_page(&defs[0], 16, page, NULL); assert(strlen(requestUrl) > 5600 && strstr(requestUrl, "/skip=16.json")); respond();
  finish_page(); assert(answers == 3);
  cJSON *search = cJSON_AddArrayToObject(cJSON_GetArrayItem(cats, 0), "extra");
  cJSON *filter = cJSON_CreateObject(); cJSON_AddStringToObject(filter, "name", "search"); cJSON_AddItemToArray(search, filter);
  assert(sb_search_defs("a title", defs, 70) == 1);
  assert(strlen(defs[0].url) > 5600 && strstr(defs[0].url, "/search=a%20title.json"));
  assert(sb_home_defs(defs, 3) == 3 && !strcmp(defs[2].title, "list2 · Movies"));
  cJSON_Delete(addons); addons = NULL;
  assert(sb_home_defs(NULL, 0) == 4 && sb_home_defs(defs, 70) == 4);
  assert(!strcmp(defs[3].title, "Top Rated · Series"));
  free(defs); puts("catalog definitions: all installed rows, account order, long URLs and movie/series fallback passed");
}
