#ifndef SB_WATCH_H
#define SB_WATCH_H

#include "core/stremio.h"
#include <stddef.h>

void watch_configure(void);
void watch_start(const char *type, const char *id, const char *vid, const Stream *stream, double duration);
void watch_leave(void);
int watch_active(void);
void watch_tick(void);
int watch_status(int controls, char *out, size_t cap);

typedef enum { WATCH_PLAY = 1, WATCH_PAUSE, WATCH_SEEK, WATCH_RESTART } WatchAction;
int watch_control(WatchAction action, double pos); /* consumed by the room */

#endif
