#ifndef WEATHER_APPLET_H
#define WEATHER_APPLET_H

#include <mate-panel-applet.h>
#include <gtk/gtk.h>
#include <gio/gio.h>
#include "config.h"
#include "weather_provider.h"

// WeatherApplet structure definition
typedef struct _WeatherApplet {
    MatePanelApplet *applet;
    GtkWidget *container;
    GtkWidget *weather_icon;
    GtkWidget *temp_label;
    GtkWidget *unit_label;
    GtkWidget *details_window;  // Track the details popup window
    
    AppConfig *config;
    WeatherData *current_weather;
    guint update_timer;
    GMutex data_mutex;
    GCancellable *fetch_cancellable;  // For cancelling in-flight fetches
    GtkActionGroup *action_group;     // For updating menu item sensitivity
} WeatherApplet;

// Function declarations that ui_dialogs.c needs
gboolean update_weather(gpointer data);

#endif // WEATHER_APPLET_H