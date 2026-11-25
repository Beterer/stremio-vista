#include "core/httpd.h"
#include "core/net.h"
#include "core/video.h"
#include "core/watch.h"
#include "ui/qrcard.h"
#include "cJSON.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static double now = 100, position = 42.5;
static double bufferedEnd = 50;
static VidState video = VID_PLAYING;
static int playCount, settling, toastCount, behind;
static cJSON *sent;
static NetCb reply;
static void *context;

double app_now(void) { return now; }
int app_behind(void) { return behind; }
const char *T(const char *s) { return s; }
void toast(const char *message, int ms, const char *key) { (void)message; (void)ms; (void)key; toastCount++; }
const char *store_get(const char *key, const char *fallback) { (void)fallback; assert(!strcmp(key, "watchRelay")); return "https://relay.example"; }
void store_set(const char *key, const char *value) { (void)key; (void)value; assert(0); }
int watch_origin(const char *input, char out[WATCH_ORIGIN_CAP]) { snprintf(out, WATCH_ORIGIN_CAP, "%s", input); return 1; }
int httpd_url(const char *suffix, char *out, size_t cap) { (void)suffix; (void)out; (void)cap; assert(0); return 0; }
unsigned httpd_watch_origin(char *out, size_t cap) { (void)out; (void)cap; assert(0); return 0; }
void qrcard_show(const QrCardOpts *opts) { (void)opts; assert(0); }
VidState video_state(void) { return video; }
double video_time(void) { return position; }
double video_buffered_end(void) { return bufferedEnd; }
int video_settling(void) { return settling; }
void video_play(void) { assert(!settling); video = VID_PLAYING; playCount++; }
void video_pause(void) { video = VID_PAUSED; }
void video_seek(double pos) { position = pos; settling = 1; }
cJSON *net_json(const unsigned char *body) { return cJSON_Parse((const char *)body); }
void net_json_free(cJSON *object) { cJSON_Delete(object); }
void net_cancel(unsigned id) { assert(id == 1); reply = NULL; }

unsigned net_post_json_direct(const char *url, const char *body, int timeout, NetCb cb, void *ud) {
  assert(!strcmp(url, "https://relay.example/watch-together/v1") && timeout > 0);
  assert(!reply);
  cJSON_Delete(sent);
  sent = cJSON_Parse(body);
  assert(sent);
  reply = cb;
  context = ud;
  return 1;
}

static const char *str(const cJSON *object, const char *key) { return cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(object, key)); }
static int num(const cJSON *object, const char *key) { return cJSON_GetObjectItemCaseSensitive(object, key)->valueint; }
static void deliver(const char *mode, int rev, double pos, int at, int create) {
  assert(reply);
  char body[512];
  snprintf(body, sizeof body, "{%s\"state\":{\"mode\":\"%s\",\"rev\":%d,\"pos\":%.3f,\"at\":%d,\"now\":%.3f,\"watchers\":2}}",
    create ? "\"token\":\"0123456789abcdef0123456789abcdef0123456789abcdef\",\"code\":\"ABCDE12345\"," : "",
    mode, rev, pos, at, now * 1000);
  NetCb cb = reply;
  reply = NULL;
  cb(200, (const unsigned char *)body, strlen(body), context);
}

int main(void) {
  Stream stream = { .url = "p2p://0123456789abcdef0123456789abcdef01234567/3?tr=private",
    .filename = "Example.Episode.1080p.mkv", .hdrs = "Authorization: private" };
  watch_start("series", "tt123", "tt123:1:2", &stream, 2800);
  assert(watch_active() && video == VID_PAUSED && playCount == 0);
  cJSON *media = cJSON_GetObjectItemCaseSensitive(sent, "media");
  assert(!strcmp(str(media, "hash"), "0123456789abcdef0123456789abcdef01234567"));
  assert(num(media, "file") == 3 && !str(media, "url") && !str(media, "hdrs"));
  assert(!strcmp(cJSON_GetStringValue(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(media, "release"), 0)), "9b171d1d9bfb903d"));
  deliver("prepare", 1, 42.5, 0, 1);
  watch_tick();
  assert(num(sent, "ready") == 1);
  deliver("playing", 2, 42.5, 101500, 0);
  now = 101.49;
  watch_tick();
  assert(video == VID_PAUSED && playCount == 0);
  deliver("playing", 2, 42.5, 101500, 0);
  now = 101.51;
  behind = 1;
  watch_tick();
  assert(video == VID_PAUSED && playCount == 0);
  behind = 0;
  watch_tick();
  assert(video == VID_PLAYING && playCount == 1);

  now = 101.90;
  video = VID_BUFFERING;
  watch_tick();
  deliver("paused", 3, 42.8, 0, 0);
  watch_tick();
  assert(video == VID_PAUSED);
  assert(watch_control(WATCH_SEEK, 0));
  watch_tick();
  assert(!strcmp(str(sent, "action"), "seek") && num(sent, "rev") == 3 && num(sent, "pos") == 0);
  deliver("paused", 4, 0, 0, 0);
  now = 102.21;
  watch_tick();
  assert(position == 0 && settling);
  deliver("paused", 4, 0, 0, 0);
  watch_control(WATCH_PLAY, 0);
  watch_tick();
  assert(!strcmp(str(sent, "action"), "play"));
  deliver("prepare", 5, 0, 0, 0);
  now = 102.51;
  watch_tick();
  assert(num(sent, "ready") == 0);
  deliver("prepare", 5, 0, 0, 0);
  settling = 0;
  bufferedEnd = 0.1;
  watch_tick();
  assert(num(sent, "ready") == 0);
  bufferedEnd = 50;
  watch_tick();
  assert(num(sent, "ready") == 5);
  watch_leave();
  assert(!watch_active() && !strcmp(str(sent, "op"), "leave"));
  assert(playCount == 1 && toastCount == 0);
  cJSON_Delete(sent);
  puts("Watch Together native timeline: buffering, timed start, background pause, shared controls and leave passed");
  return 0;
}
