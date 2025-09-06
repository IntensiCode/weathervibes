#ifndef PREFERENCES_DIALOG_H
#define PREFERENCES_DIALOG_H

#include <gtk/gtk.h>
#include "weather_applet.h"

// Show preferences dialog
void show_preferences_dialog(GtkAction *action, WeatherApplet *weather_applet);

// Callback for provider changes (exposed for use in dialog)
void on_provider_changed(GtkComboBox *combo, gpointer user_data);

#endif // PREFERENCES_DIALOG_H