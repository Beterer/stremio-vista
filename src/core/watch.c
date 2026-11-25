#include "core/watch.h"
#include "core/app.h"
#include "core/away.h"
#include "core/httpd.h"
#include "core/i18n.h"
#include "core/net.h"
#include "core/store.h"
#include "core/video.h"
#include "ui/qrcard.h"
#include "cJSON.h"
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

enum { PREPARE = 1, PAUSED, PLAYING };
static struct {
  char endpoint[WATCH_ORIGIN_CAP + 24], code[11], token[49];
  unsigned generation, request, input, sentInput;
  int mode, rev, ready, watchers, wasPlaying, startedRev;
  double pos, at, sentAt, nextPoll, lastReply, duration, pauseAt, seekAt;
  double offset, bestRtt, clockAt;
  WatchAction action;
  int actionRev;
  double actionPos;
} W;

static unsigned configSeen;
static int config_check(void *ud) {
  (void)ud;
  char origin[WATCH_ORIGIN_CAP];
  unsigned seq = httpd_watch_origin(origin, sizeof origin);
  if (seq == configSeen) return 0;
  configSeen = seq;
  store_set("watchRelay", origin);
  return 1;
}

static void config_closed(void *ud) { config_check(ud); }

void watch_configure(void) {
  char url[600], origin[WATCH_ORIGIN_CAP];
  httpd_url("/watch", url, sizeof url);
  configSeen = httpd_watch_origin(origin, sizeof origin);
  QrCardOpts opts = { .title = T("Watch Together relay"), .sub = T("Open this address on your home network and enter the relay’s HTTPS address."),
    .url = url, .check = config_check, .doneTitle = T("Relay saved"), .doneHold = 1.3f, .onClose = config_closed };
  qrcard_show(&opts);
}

static void add_release(cJSON *releases, const char *text) {
  const char *end = text + strcspn(text, "\r\n");
  while (end > text && isspace((unsigned char)end[-1])) end--;
  if (end == text || text + strcspn(text, "\\/:?&@") < end) return;
  const char *extensions[] = { ".mkv", ".mp4", ".avi", ".mov", ".webm", ".m4v", ".ts" };
  for (size_t i = 0; i < sizeof extensions / sizeof *extensions; i++) {
    size_t n = strlen(extensions[i]);
    if ((size_t)(end - text) >= n && !strncasecmp(end - n, extensions[i], n)) { end -= n; break; }
  }
  uint64_t hash = UINT64_C(0xcbf29ce484222325);
  int count = 0;
  for (const char *p = text; p < end; p++) {
    unsigned c = (unsigned char)*p;
    if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) { hash = (hash ^ c) * UINT64_C(0x100000001b3); count++; }
  }
  if (count < 8 || count > 160) return;
  char key[17]; snprintf(key, sizeof key, "%016llx", (unsigned long long)hash);
  cJSON *item;
  cJSON_ArrayForEach(item, releases) if (!strcmp(cJSON_GetStringValue(item), key)) return;
  cJSON_AddItemToArray(releases, cJSON_CreateString(key));
}

static void add_torrent(cJSON *media, const char *url) {
  if (strncmp(url, "p2p://", 6) || strspn(url + 6, "0123456789abcdefABCDEF") != 40 || url[46] != '/') return;
  char hash[41]; memcpy(hash, url + 6, 40); hash[40] = 0;
  cJSON_AddStringToObject(media, "hash", hash);
  const char *idx = url + 47;
  unsigned file = 0;
  if (*idx < '0' || *idx > '9') return;
  for (; *idx >= '0' && *idx <= '9'; idx++) {
    file = file * 10 + (unsigned)(*idx - '0');
    if (file > 65535) return;
  }
  if (!*idx || *idx == '?') cJSON_AddNumberToObject(media, "file", file);
}

static const char *string(const cJSON *obj, const char *key) {
  const char *s = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(obj, key));
  return s ? s : "";
}

static double number(const cJSON *obj, const char *key) {
  return cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(obj, key));
}

static unsigned post(cJSON *j, NetCb cb) {
  char *body = cJSON_PrintUnformatted(j);
  unsigned id = body ? net_post_json_direct(W.endpoint, body, 2300, cb, (void *)(uintptr_t)W.generation) : 0;
  cJSON_free(body);
  cJSON_Delete(j);
  return id;
}

int watch_active(void) { return W.token[0] || W.request; }

void watch_leave(void) {
  if (W.request) net_cancel(W.request);
  if (W.token[0]) {
    cJSON *j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "op", "leave");
    cJSON_AddStringToObject(j, "token", W.token);
    post(j, NULL);
  }
  unsigned generation = W.generation + 1;
  memset(&W, 0, sizeof W);
  W.generation = generation;
}

static void on_reply(int status, const unsigned char *body, size_t len, void *ud) {
  if (W.generation != (unsigned)(uintptr_t)ud) return;
  W.request = 0;
  double now = app_now();
  W.nextPoll = now + 0.28;
  cJSON *j = status == 200 && len ? net_json(body) : NULL;
  const cJSON *state = cJSON_GetObjectItemCaseSensitive(j, "state");
  const char *mode = string(state, "mode");
  int nextMode = !strcmp(mode, "prepare") ? PREPARE : !strcmp(mode, "paused") ? PAUSED : !strcmp(mode, "playing") ? PLAYING : 0;
  double rev = number(state, "rev"), serverNow = number(state, "now") / 1000, at = number(state, "at") / 1000, pos = number(state, "pos");
  int creating = !W.token[0];
  const char *token = string(j, "token"), *code = string(j, "code");
  int valid = nextMode && rev >= 1 && rev <= 2147483647 && isfinite(serverNow) &&
    isfinite(at) && pos >= 0 && pos <= W.duration && (!creating || (strlen(token) == 48 && strlen(code) == 10));
  if (valid) {
    if (creating) {
      snprintf(W.token, sizeof W.token, "%s", token);
      snprintf(W.code, sizeof W.code, "%s", code);
    }
    double rtt = now - W.sentAt;
    if (!W.clockAt || now - W.clockAt > 20 || rtt < W.bestRtt) {
      W.bestRtt = rtt; W.clockAt = now;
      W.offset = serverNow - (W.sentAt + now) / 2;
    }
    W.lastReply = now;
    W.rev = (int)rev; W.mode = nextMode; W.pos = pos; W.at = at;
    W.watchers = number(state, "watchers") == 2 ? 2 : 1;
    if (W.input == W.sentInput) W.action = 0;
  } else if (creating || !strcmp(string(j, "error"), "room-ended")) {
    int resume = creating && W.wasPlaying;
    watch_leave();
    if (resume) video_play();
    toast(T(creating ? "Couldn’t create a Watch Together room" : "Watch Together room ended"), 4000, NULL);
  }
  net_json_free(j);
}

void watch_start(const char *type, const char *id, const char *vid, const Stream *stream, double duration) {
  char origin[WATCH_ORIGIN_CAP];
  if (!watch_origin(store_get("watchRelay", ""), origin)) {
    toast(T("Set up Watch Together in Settings → Playback"), 4200, NULL);
    return;
  }
  snprintf(W.endpoint, sizeof W.endpoint, "%s/watch-together/v1", origin);
  cJSON *j = cJSON_CreateObject(), *media = cJSON_AddObjectToObject(j, "media");
  cJSON_AddStringToObject(j, "op", "create");
  cJSON_AddStringToObject(media, "type", type);
  cJSON_AddStringToObject(media, "id", id);
  cJSON_AddStringToObject(media, "vid", vid);
  cJSON_AddNumberToObject(media, "duration", duration);
  add_torrent(media, stream->url);
  if (strlen(stream->hash) == 16 && strspn(stream->hash, "0123456789abcdefABCDEF") == 16) cJSON_AddStringToObject(media, "videoHash", stream->hash);
  cJSON *releases = cJSON_AddArrayToObject(media, "release");
  if (releases) { add_release(releases, stream->filename); add_release(releases, stream->title); }
  double pos = video_time();
  cJSON_AddNumberToObject(j, "pos", pos);
  W.duration = duration;
  W.sentAt = app_now();
  W.wasPlaying = video_state() != VID_PAUSED;
  W.request = post(j, on_reply);
  if (W.request && W.wasPlaying) video_pause();
}

int watch_control(WatchAction action, double pos) {
  if (!watch_active()) return 0;
  if (W.token[0]) {
    W.action = action;
    W.actionRev = W.rev;
    W.actionPos = pos;
    W.input++;
    W.nextPoll = app_now();
  }
  return 1;
}

static int align(double pos, double tolerance, double now, double retry) {
  if (video_settling()) return 0;
  if (fabs(video_time() - pos) <= tolerance) return 1;
  if (now >= W.seekAt) { video_seek(pos); W.seekAt = now + retry; }
  return 0;
}

void watch_tick(void) {
  if (!W.token[0]) return;
  double now = app_now();
  if (now - W.lastReply > 10) {
    watch_leave();
    toast(T("Watch Together lost its connection"), 4000, NULL);
    return;
  }
  VidState video = video_state();
  int ready = 0;
  if (!app_behind() && (video == VID_PLAYING || video == VID_PAUSED || video == VID_BUFFERING)) {
    double elapsed = now + W.offset - W.at;
    if (W.mode != PLAYING || elapsed < 0) {
      if (video != VID_PAUSED && now >= W.pauseAt) { video_pause(); W.pauseAt = now + 0.15; }
      if (video == VID_PAUSED && now >= W.pauseAt && align(W.pos, 0.24, now, 1.8) && W.mode == PREPARE) {
        double end = video_buffered_end();
        if (end <= 0 || end >= fmin(W.pos + 0.25, W.duration - 0.05)) ready = W.rev;
      }
    } else {
      double target = fmin(W.pos + elapsed, W.duration - 0.1);
      if (video == VID_PAUSED && (W.startedRev == W.rev || (elapsed < 0.45 && !video_settling()) || align(target, 0.75, now, 1.8))) {
        video_play(); W.startedRev = W.rev;
      } else if (video == VID_PLAYING && elapsed > 2) {
        align(target, 1.25, now, 7);
      }
    }
  }
  if (W.ready != ready && !W.request) W.nextPoll = now;
  W.ready = ready;
  if (W.request || now < W.nextPoll) return;
  cJSON *j = cJSON_CreateObject();
  cJSON_AddStringToObject(j, "op", "tick");
  cJSON_AddStringToObject(j, "token", W.token);
  cJSON_AddNumberToObject(j, "ready", ready);
  if (W.action) {
    static const char *const actions[] = { "", "play", "pause", "seek", "restart" };
    cJSON_AddStringToObject(j, "action", actions[W.action]);
    cJSON_AddNumberToObject(j, "rev", W.actionRev);
    cJSON_AddNumberToObject(j, "pos", W.actionPos);
  }
  W.sentAt = now; W.sentInput = W.input;
  W.request = post(j, on_reply);
  W.nextPoll = now + 0.28;
}

int watch_status(int controls, char *out, size_t cap) {
  if (!watch_active() || (W.mode == PLAYING && !controls && app_now() + W.offset >= W.at)) return 0;
  const char *status = !W.token[0] ? T("Creating room…") : W.watchers == 1 ? T("Waiting for your friend") :
    W.mode == PAUSED ? T("Room paused") : W.mode == PREPARE ? T("Getting both players ready") :
    app_now() + W.offset < W.at ? T("Starting together…") : T("Watching together");
  snprintf(out, cap, "%s%s%s", status, W.code[0] ? "  ·  " : "", W.code);
  return 1;
}
