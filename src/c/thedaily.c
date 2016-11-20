#include <pebble.h>
#include "composition.h"
#include "event.h"
#include "main_menu.h"
#include "pin_window.h"

void init(void) {
  APP_LOG(APP_LOG_LEVEL_INFO, "01");
  composition_preinit();
  composition_init();
  APP_LOG(APP_LOG_LEVEL_INFO, "02");

  event_init();
  APP_LOG(APP_LOG_LEVEL_INFO, "03");

  // Now do launch reason-specific action
  switch (launch_reason())
  {
    case APP_LAUNCH_WAKEUP:
      // Launch the question.
      break;
    case APP_LAUNCH_USER:
    default:
      // Launch the menu.
      main_menu_show();
      break;
  }
  APP_LOG(APP_LOG_LEVEL_INFO, "04");
  composition_postinit();
  APP_LOG(APP_LOG_LEVEL_INFO, "05");
}

void deinit(void) {
  main_menu_hide();
  event_deinit();
  composition_deinit();
}

int main( void ) {
	init();
	app_event_loop();
	deinit();
}
