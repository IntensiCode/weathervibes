#ifndef APPLET_CONFIG_H
#define APPLET_CONFIG_H

#include <glib.h>
#include "config.h"
#include "weather_provider.h"

// Create a new applet config from global config
AppConfig* applet_config_new_from_global(void);

// Update applet configuration
void applet_config_update(AppConfig *config,
                          const char *location,
                          int provider,
                          const char *api_key,
                          gboolean use_celsius,
                          int interval_minutes);

// Sync applet config to global config and save
void applet_config_sync_to_global(AppConfig *config);

// Free applet config
void applet_config_free(AppConfig *config);

#endif // APPLET_CONFIG_H