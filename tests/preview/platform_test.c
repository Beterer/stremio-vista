/* Verify desktop controls and settings persistence independently of a display. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "platform/plat.h"
#include "core/keys.h"
#include "core/store.h"

void applog_start(const char *dir) { assert(!strcmp(dir, plat_data_dir())); }
static int key(SDL_Keycode code) { SDL_KeyboardEvent e = { 0 }; e.keysym.sym = code; return plat_map_key(&e); }
int main(void) {
  char temp[] = "/tmp/vista-preview-test-XXXXXX", profile[256], path[300];
  assert(mkdtemp(temp)); snprintf(profile, sizeof profile, "%s/profile's settings", temp);
  setenv("SB_DATA_DIR", profile, 1); setenv("SB_ASSETS", "assets", 1);
  plat_pre_init("tv.stremiovista.app");
  assert(!strcmp(plat_data_dir(), profile) && !strcmp(plat_asset_dir(), "assets"));
  store_init(plat_data_dir()); store_set("uiSound", "false"); store_flush();
  store_set("uiSound", "true"); store_init(plat_data_dir());
  assert(!strcmp(store_get("uiSound", "true"), "false"));

  assert(key(SDLK_LEFT) == K_LEFT && key(SDLK_RIGHT) == K_RIGHT);
  assert(key(SDLK_UP) == K_UP && key(SDLK_DOWN) == K_DOWN);
  assert(key(SDLK_RETURN) == K_OK && key(SDLK_KP_ENTER) == K_OK);
  assert(key(SDLK_ESCAPE) == K_BACK && key(SDLK_BACKSPACE) == K_BACK);
  assert(key(SDLK_F1) == K_RED && key(SDLK_SPACE) == K_PLAYPAUSE && key(SDLK_a) == K_NONE);
  int w, h; Uint32 flags; unsetenv("SB_WINDOW"); plat_window_size(&w, &h, &flags);
  assert(w == 1280 && h == 720 && (flags & SDL_WINDOW_OPENGL) && !(flags & SDL_WINDOW_HIDDEN));
  setenv("SB_WINDOW", "960x540", 1); plat_window_size(&w, &h, &flags); assert(w == 960 && h == 540);
  setenv("SB_WINDOW", "invalid", 1); plat_window_size(&w, &h, &flags); assert(w == 1280 && h == 720);
  SDL_Event e = { 0 }; e.type = SDL_WINDOWEVENT; e.window.event = SDL_WINDOWEVENT_MINIMIZED; assert(plat_lifecycle(&e) == -1);
  e.window.event = SDL_WINDOWEVENT_RESTORED; assert(plat_lifecycle(&e) == 1);
  e.window.event = SDL_WINDOWEVENT_FOCUS_GAINED; assert(plat_lifecycle(&e) == 0);

  snprintf(path, sizeof path, "%s/state.json", profile); assert(!unlink(path));
  snprintf(path, sizeof path, "%s/cache", profile); assert(!rmdir(path));
  assert(!rmdir(profile) && !rmdir(temp));
  puts("Linux preview: remote keys, lifecycle, visible sizing and saved settings passed");
  return 0;
}
