#pragma once
#include <pebble.h>

extern void composition_preinit();
extern void composition_init();
extern void composition_postinit();
extern void composition_deinit();

extern void composition_app_message_register_handler(uint32_t key, uint32_t inbox_size_request, uint32_t outbox_size_request, AppMessageInboxReceived handler, void *context);
extern void composition_app_message_unregister_handler(uint32_t key);
