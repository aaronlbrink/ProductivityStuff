#pragma once

#include "pebble-v/v.h"

typedef uint8_t EventKey;

extern void event_init();
extern void event_deinit();

extern V event_get_launches();
extern void event_destroy_launches(V *launches);
