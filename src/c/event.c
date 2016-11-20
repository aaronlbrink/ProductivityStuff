#include <pebble.h>

#include "composition.h"
#include "event.h"
#include "question.h"
#include "storage_keys.h"
#include "strdup.h"
#include "pebble-v/v.h"

typedef struct {
  char *label;
} LaunchEventDefinition;

// Using bits: http://stackoverflow.com/a/1631857/429091
typedef struct {
  EventKey next;
  QuestionKey question_id;
  // This is a launch event
  int type_launch : 1;
  // The question should be launched when this event starts.
  int launch : 1;
} EventHeader;

/*
 * At its core, an event just specifies a question to
 * be asked and indicates when it should be asked.
 */
typedef struct {
  EventHeader header;
  union {
    LaunchEventDefinition launch;
  } definition;
} Event;

typedef struct {
  // The index of the first launch event. In an initial state or when
  // there are no launch events, the storage key referred to may not
  // exist or may refer to an event that is not a launch event. This
  // must be detected upon trying to read the value and treated as
  // and empty list of launch events.
  uint8_t first_launch;
  // Similar for schedule events, etc.
  uint8_t first_wakeup;
} EventInfo;

static EventInfo info;

// Not sure yet if copying makes sense, probably want to have events
// either be short lived or commonly referenced ones (e.g., launch)
// live in an updated list instead of having copies.
static void event_copy(Event *dst, Event *src) {
  *dst = *src;
  if (dst->header.type_launch) {
    dst->definition.launch.label = strdup(dst->definition.launch.label);
  }
}

static void event_destroy(Event *e) {
  if (e->header.type_launch) {
    free(e->definition.launch.label);
  }
}

static int event_write(Event *e, EventKey key) {
  // Pack it
  uint8_t buf[PERSIST_DATA_MAX_LENGTH];
  memcpy(buf, &e->header, sizeof(e->header));
  DictionaryIterator iterator;
  dict_write_begin(&iterator, &buf[sizeof(e->header)], sizeof(buf) - sizeof(e->header));
  if (e->header.type_launch) {
    if (DICT_OK != dict_write_cstring(&iterator, MESSAGE_KEY_EVENT_LAUNCH_LABEL, e->definition.launch.label)) {
      return 1;
    }
  }
  // Store
  if (0 < persist_write_data(STORAGE_KEY_EVENT_BASE + key, buf, sizeof(e->header) + dict_write_end(&iterator))) {
    return 0;
  }
  return 1;
}

static void inbox_received_handler(DictionaryIterator *iterator, void *context) {
  EventKey key = 0;
  Event e = { .header = { .next = key, }, };
  short delete = 0;

  Tuple *tuple = dict_read_first(iterator);
  while (tuple) {
    // Note that for most event types, the type can be inferred. Until
    // we have an event type that can’t be inferred, no reason to send
    // it as explicit data over the wire.
    //
    // Data types are those suported by PebbleKit JS:
    // https://developer.pebble.com/guides/communication/using-pebblekit-js/#type-conversion
    if (tuple->key == MESSAGE_KEY_EVENT_KEY) {
      key = tuple->value->int32;
    } else if (tuple->key == MESSAGE_KEY_EVENT_DELETE) {
      delete = tuple->value->int16;
    } else if (tuple->key == MESSAGE_KEY_EVENT_NEXT) {
      e.header.next = tuple->value->int32;
    } else if (tuple->key == MESSAGE_KEY_EVENT_LAUNCH_LABEL) {
      // Found a launch event!
      e.header.type_launch = 1;
      e.definition.launch.label = tuple->value->cstring;
    }
    tuple = dict_read_next(iterator);
  }

  short success = 0;
  if (delete) {
    persist_delete(STORAGE_KEY_EVENT_BASE + key);
    success = 1;
  } else {
    success = !event_write(&e, key);
  }
  if (APP_MSG_OK == app_message_outbox_begin(&iterator)) {
    dict_write_uint8(iterator, MESSAGE_KEY_TYPE, success ? MESSAGE_KEY_ACK : MESSAGE_KEY_NACK);
    app_message_outbox_send();
  }
}

void event_init()
{
  // Rely on static initialization if the key doesn’t exist ^^.
  persist_read_data(STORAGE_KEY_EVENT, &info, sizeof(info));

  composition_app_message_register_handler(MESSAGE_KEY_EVENT, 512, 512, inbox_received_handler, NULL);
}

void event_deinit()
{
}

V event_get_launches() {
  V launches;
  v_init(&launches, sizeof(Event));
  return launches;
}

void event_destroy_launches(V *launches) {
  for (int i = 0; i < v_count(launches); i++) {
    Event *e = v_get(launches, i);
    event_destroy(e);
  }
  v_deinit(launches);
}
