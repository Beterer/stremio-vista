/* Exercise the production update lookup without network, rendering or TV storage. */
#include <assert.h>
#include "../../src/core/update.c"

static int requests;
static char requested[1024];
static NetCb reply;
static void *replyData;

double app_now(void) { return 10; }
int player_open(void) { return 0; }
const char *plat_asset_dir(void) { return "/unused"; }
const char *plat_data_dir(void) { return "/unused"; }
const char *store_get(const char *key, const char *def) { (void)key; return def; }
void store_set(const char *key, const char *value) { (void)key; (void)value; }
void store_remove(const char *key) { (void)key; }
const char *T(const char *en) { return en; }
void toast(const char *msg, int ms, const char *key) { (void)msg; (void)ms; (void)key; }
unsigned net_get_timeout(const char *url, int timeout, NetCb cb, void *ud) {
  assert(timeout == 15000); requests++;
  snprintf(requested, sizeof requested, "%s", url); reply = cb; replyData = ud;
  return (unsigned)requests;
}
static void respond(int status, const char *body) {
  NetCb cb = reply; void *ud = replyData; reply = NULL;
  assert(cb); cb(status, (const unsigned char *)body, strlen(body), ud);
}
static void start(void) {
  memset(&U, 0, sizeof U); U.read = 1; snprintf(U.cur, sizeof U.cur, "1.0.0");
  requests = 0; reply = NULL; reached = 0; update_check(1);
  assert(requests == 1);
  assert(!strcmp(requested, "https://api.github.com/repos/Beterer/stremio-vista/releases?per_page=8"));
}
int main(void) {
  unsetenv("SB_UPD_GH"); unsetenv("SB_UPD_MAN");

  /* An empty fork must not check the inherited upstream Homebrew manifest. */
  start(); respond(200, "[]");
  assert(requests == 1 && !reply && update_state() == UPD_NONE);

  start(); respond(0, "");
  assert(requests == 1 && !reply && update_state() == UPD_ERROR);

  setenv("SB_UPD_MAN", "", 1);
  start(); respond(200, "[]");
  assert(requests == 1 && !reply && update_state() == UPD_NONE);

  /* Explicit development manifests remain available when deliberately set. */
  setenv("SB_UPD_MAN", "http://127.0.0.1:8080/manifest.json", 1);
  start(); respond(200, "[]");
  assert(requests == 2 && !strcmp(requested, "http://127.0.0.1:8080/manifest.json"));
  respond(200, "{\"version\":\"1.1.0\",\"ipkUrl\":\"https://example.invalid/vista_arm.ipk\","
               "\"ipkHash\":{\"sha256\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"}}");
  assert(update_state() == UPD_AVAIL && !strcmp(update_avail(), "1.1.0"));
  unsetenv("SB_UPD_MAN");

  /* A compatible Vista release still resolves directly, without a manifest. */
  start(); respond(200, "[{\"assets\":[{\"name\":\"tv.stremiovista.app_1.2.0_arm.ipk\","
    "\"digest\":\"sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\","
    "\"browser_download_url\":\"https://github.com/Beterer/stremio-vista/releases/download/v1.2.0/tv.stremiovista.app_1.2.0_arm.ipk\"}]}]");
  assert(requests == 1 && !reply && update_state() == UPD_AVAIL && !strcmp(update_avail(), "1.2.0"));
  /* Origin packages must never be offered to the separate Vista installation. */
  start(); respond(200, "[{\"assets\":[{\"name\":\"tv.stremioorigin.app_9.0.0_arm.ipk\","
    "\"digest\":\"sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\","
    "\"browser_download_url\":\"https://example.invalid/tv.stremioorigin.app_9.0.0_arm.ipk\"}]}]");
  assert(requests == 1 && !reply && update_state() == UPD_NONE);
  puts("update lookup: fork-only releases, empty list, network error and explicit manifest passed");
  return 0;
}
