#include <assert.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h>
#include <stdint.h>

static struct ifaddrs interfaces[3];
static struct sockaddr_in addresses[3];
static const char *route;
static int fixture_interfaces(struct ifaddrs **out) { *out = interfaces; return 0; }
static void fixture_free(struct ifaddrs *p) { (void)p; }
static int fixture_route(int fd, struct sockaddr *out, socklen_t *len) {
  (void)fd; (void)len;
  if (!route) return -1;
  struct sockaddr_in *a = (struct sockaddr_in *)out; a->sin_family = AF_INET;
  return inet_pton(AF_INET, route, &a->sin_addr) == 1 ? 0 : -1;
}
static int fixture_connect(int fd, const struct sockaddr *a, socklen_t len) {
  (void)fd; assert(len == sizeof(struct sockaddr_in));
  const struct sockaddr_in *v = (const struct sockaddr_in *)a;
  assert(v->sin_family == AF_INET && ntohs(v->sin_port) == 53);
  return 0; /* route tests must also run without internet access */
}
#define getifaddrs fixture_interfaces
#define freeifaddrs fixture_free
#define getsockname fixture_route
#define connect fixture_connect
#include "../../src/core/httpd.c"

uint32_t ACC, ACC2;
const char *T(const char *s) { return s; }
const char *i18n_lang(void) { return "en"; }
int i18n_rtl(void) { return 0; }
static void interface(int n, const char *ip, unsigned flags) {
  interfaces[n].ifa_addr = (struct sockaddr *)&addresses[n];
  interfaces[n].ifa_flags = flags;
  interfaces[n].ifa_next = n < 2 ? &interfaces[n + 1] : NULL;
  addresses[n].sin_family = AF_INET; assert(inet_pton(AF_INET, ip, &addresses[n].sin_addr));
}
int main(void) {
  char ip[64];
  interface(0, "10.0.0.1", IFF_UP);
  interface(1, "192.168.1.50", IFF_UP);
  interface(2, "127.0.0.1", IFF_UP | IFF_LOOPBACK);
  route = "192.168.1.50";
  assert(lan_ip(ip, sizeof ip) && !strcmp(ip, "192.168.1.50"));
  interface(0, "192.0.2.1", IFF_UP);
  route = NULL; /* still find the LAN when internet routing is unavailable */
  assert(lan_ip(ip, sizeof ip) && !strcmp(ip, "192.168.1.50"));
  interface(0, "10.8.0.2", IFF_UP | IFF_POINTOPOINT); route = "10.8.0.2";
  assert(lan_ip(ip, sizeof ip) && !strcmp(ip, "192.168.1.50"));
  interface(0, "169.254.3.4", IFF_UP); interface(1, "192.168.1.50", 0);
  assert(!lan_ip(ip, sizeof ip));
  puts("Phone address: active route, private LAN fallback, VPN and offline interfaces passed");
  return 0;
}
