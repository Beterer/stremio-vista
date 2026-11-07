#include "core/httpd.h"
#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

uint32_t ACC = 0x7654cc, ACC2 = 0x9876ee;
const char *T(const char *s) { return s; }
const char *i18n_lang(void) { return "en"; }
int i18n_rtl(void) { return 0; }

static int tmdb_key(const char *s) { return !strcmp(s, "test-tmdb"); }
static int trakt_key(const char *s) { return !strcmp(s, "test-trakt"); }
static void get(const char *url, char *out, size_t cap) {
  char ip[64], path[600]; int port;
  assert(sscanf(url, "http://%63[^:]:%d%599s", ip, &port, path) == 3);
  int fd = socket(AF_INET, SOCK_STREAM, 0); assert(fd >= 0);
  struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(port) };
  assert(inet_pton(AF_INET, ip, &a.sin_addr) == 1);
  assert(connect(fd, (struct sockaddr *)&a, sizeof a) == 0);
  char req[800]; int n = snprintf(req, sizeof req, "GET %s HTTP/1.1\r\nHost: %s:%d\r\n\r\n", path, ip, port);
  assert(send(fd, req, n, 0) == n);
  size_t used = 0; ssize_t got;
  while (used + 1 < cap && (got = recv(fd, out + used, cap - used - 1, 0)) > 0) used += got;
  out[used] = 0; close(fd);
}
int main(int argc, char **argv) {
  int occupied = argc > 1 && !strcmp(argv[1], "occupied");
  int configured = argc > 1 && !strcmp(argv[1], "configured");
  if (configured) setenv("SB_PHONE_PORT", "8251", 1);
  if (occupied) {
    int fd = socket(AF_INET, SOCK_STREAM, 0); assert(fd >= 0);
    int one = 1; setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(8099), .sin_addr.s_addr = htonl(INADDR_ANY) };
    assert(bind(fd, (struct sockaddr *)&a, sizeof a) == 0); assert(listen(fd, 1) == 0);
  }
  setenv("SB_PHONE_TOKEN", "0123456789abcdef", 1);
  httpd_set_key_handler(tmdb_key); httpd_set_trakt_handler(trakt_key);
  char url[600], body[16000], submit[700];
  assert(httpd_url("/tmdb", url, sizeof url));
#if defined(SB_LINUX)
  assert(!strncmp(url, "http://127.0.0.1:", 17));
#endif
  if (occupied) assert(strstr(url, ":8100/t/0123456789abcdef/tmdb"));
  if (configured) assert(strstr(url, ":8251/t/0123456789abcdef/tmdb"));
  char shown[600]; httpd_url_shown(url, shown, sizeof shown);
  get(shown, body, sizeof body); assert(strstr(body, "200 OK")); assert(strstr(body, "TMDB API key"));
  snprintf(submit, sizeof submit, "%s/set?key=test-tmdb", url);
  get(submit, body, sizeof body); assert(strstr(body, "204 No Content"));
  snprintf(submit, sizeof submit, "%s/set?key=test-trakt", url);
  get(submit, body, sizeof body); assert(strstr(body, "400 Bad Request"));
  assert(httpd_url("/trakt", url, sizeof url));
  get(url, body, sizeof body); assert(strstr(body, "200 OK")); assert(strstr(body, "Trakt Client ID"));
  snprintf(submit, sizeof submit, "%s/set?key=test-trakt", url);
  get(submit, body, sizeof body); assert(strstr(body, "204 No Content"));
  assert(httpd_hits() == 2);
  snprintf(submit, sizeof submit, "%s", url); char *t = strstr(submit, "/t/"); assert(t); t[3] = 'f';
  get(submit, body, sizeof body); assert(strstr(body, "404 Not Found"));
  puts("Phone key pages: reachable address, both providers, validation and token protection passed");
  fflush(stdout);
  if (argc > 1 && !strcmp(argv[1], "serve")) for (;;) pause(); /* Docker publication integration check */
  return 0;
}
