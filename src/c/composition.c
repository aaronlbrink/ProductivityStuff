#include <inttypes.h>
#include <pebble.h>
#include "composition.h"
#include "def.h"
#include "pebble-v/v.h"
#include "pebble-events/pebble-events.h"

static V message_handlers;
static EventHandle event_handle;

typedef struct {
  uint32_t key;
  AppMessageInboxReceived handler;
  void *context;
} MessageHandler;

static int cmp_message_handler_key(MessageHandler *a, MessageHandler *b) {
  return ((int)a->key) - b->key;
}
static int cast_cmp_message_handler_key(void *a, void *b) {
  return cmp_message_handler_key(a, b);
}

static void inbox_received_handler(DictionaryIterator *iterator, void *context)
{
  APP_LOG(APP_LOG_LEVEL_INFO, "inbox_received_handler 01");
  Tuple *tuple = dict_find(iterator, MESSAGE_KEY_TYPE);
  if (!tuple) {
    APP_LOG(APP_LOG_LEVEL_INFO, "inbox_received_handler 02");
    return;
  }
  APP_LOG(APP_LOG_LEVEL_INFO, "inbox_received_handler 03");
  MessageHandler handler_search = { .key = tuple->value->int32, };
  int i = v_find(&message_handlers, cast_cmp_message_handler_key, &handler_search);
  APP_LOG(APP_LOG_LEVEL_INFO, "inbox_received_handler 04");
  if (i < 0) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Received messaged for unsupported type: %" PRId32, tuple->value->int32);
    return;
  }
  APP_LOG(APP_LOG_LEVEL_INFO, "inbox_received_handler 05");
  MessageHandler *handler_found = v_get(&message_handlers, i);
  handler_found->handler(iterator, handler_found->context);
  APP_LOG(APP_LOG_LEVEL_INFO, "inbox_received_handler 06");
}

void composition_preinit() {
  v_init(&message_handlers, sizeof(MessageHandler));
}

void composition_init()
{
}

void composition_postinit() {
  event_handle = events_app_message_register_inbox_received(inbox_received_handler, NULL);
  v_compact(&message_handlers);
}

void composition_deinit()
{
  events_app_message_unsubscribe(event_handle);
  v_deinit(&message_handlers);
}

void composition_app_message_register_handler(uint32_t key, uint32_t inbox_size_request, uint32_t outbox_size_request, AppMessageInboxReceived handler, void *context)
{
  if (inbox_size_request) {
    events_app_message_request_inbox_size(inbox_size_request);
  }
  if (outbox_size_request) {
    events_app_message_request_outbox_size(outbox_size_request);
  }

  MessageHandler buf = {
    .key = key,
    .handler = handler,
    .context = context,
  };
  if (v_add(&message_handlers, &buf)) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Unable to add message handler for key %lu", key);
  }
}

void composition_app_message_unregister_handler(uint32_t key)
{
  MessageHandler search = { .key = key, };
  int i = v_find(&message_handlers, cast_cmp_message_handler_key, &search);
  if (i != -1) {
    v_remove(&message_handlers, i);
  }
}
