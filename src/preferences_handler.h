#ifndef PREFERENCES_HANDLER_H
#define PREFERENCES_HANDLER_H

#include <glib.h>
#include "weather_applet.h"

// Handle saving preferences from the preferences dialog
void handle_preferences_save(WeatherApplet *weather_applet, 
                            const char *location,
                            int provider,
                            const char *api_key,
                            gboolean use_celsius,
                            int interval_index);

#endif // PREFERENCES_HANDLER_H