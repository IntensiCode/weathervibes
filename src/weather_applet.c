#include <mate-panel-applet.h>
#include <gtk/gtk.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "app.h"
#include "weather_applet.h"
#include "weather_fetcher.h"
#include "config.h"
#include "weather_provider.h"
#include "weather_conditions.h"
#include "weather_details_dialog.h"
#include "preferences_dialog.h"
#include "about_dialog.h"
#include "weather_display.h"
#include "weather_update.h"
#include "network.h"
#include "logger.h"
#include "weather_resume.h"

// Global app context
AppContext *g_app_context = NULL;

static void destroy_applet(WeatherApplet *weather_applet) {
    if (weather_applet->update_timer) {
        weather_resume_timer_stop(weather_applet->update_timer);
    }
    if (weather_applet->fetch_cancellable) {
        g_cancellable_cancel(weather_applet->fetch_cancellable);
        g_object_unref(weather_applet->fetch_cancellable);
    }
    if (weather_applet->details_window && GTK_IS_WINDOW(weather_applet->details_window)) {
        gtk_widget_destroy(weather_applet->details_window);
        weather_applet->details_window = NULL;
    }
    if (weather_applet->config) {
        g_free(weather_applet->config->city);
        g_free(weather_applet->config->openweather_api_key);
        g_free(weather_applet->config->tomorrow_api_key);
        g_free(weather_applet->config);
    }
    if (weather_applet->current_weather) {
        weather_fetcher_free_data(weather_applet->current_weather);
    }
    if (g_app_context) {
        // Lock before accessing shared data during cleanup
        g_mutex_lock(&g_app_context->data_mutex);
        
        // Free cached city
        if (g_app_context->cached_city) {
            g_free(g_app_context->cached_city);
            g_app_context->cached_city = NULL;
        }
        
        // Free all provider caches
        for (int i = 0; i < PROVIDER_COUNT; i++) {
            if (g_app_context->provider_cache[i]) {
                weather_fetcher_free_data(g_app_context->provider_cache[i]);
                g_app_context->provider_cache[i] = NULL;
            }
        }
        
        // Free global weather data
        if (g_app_context->weather_data) {
            weather_fetcher_free_data(g_app_context->weather_data);
            g_app_context->weather_data = NULL;
        }
        
        g_mutex_unlock(&g_app_context->data_mutex);
    }
    g_mutex_clear(&weather_applet->data_mutex);
    network_cleanup();
    logger_cleanup();
    g_free(weather_applet);
}

// Removed display functions - now in weather_display.c

static gboolean on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    WeatherApplet *weather_applet = (WeatherApplet *)data;
    
    // Handle left mouse button click - toggle details window
    if (event->button == 1) {  // Left button
        if (weather_applet->details_window && GTK_IS_WINDOW(weather_applet->details_window)) {
            log_debug("Left click detected - closing existing weather details window");
            gtk_widget_destroy(weather_applet->details_window);
            weather_applet->details_window = NULL;
        } else {
            log_debug("Left click detected - showing weather details");
            show_weather_details_popup(weather_applet);
        }
        return TRUE;  // Event handled
    }
    
    // Let right-click go through for the menu
    return FALSE;  // Let other handlers process this event
}

// Removed update_weather function - now in weather_update.c

static const GtkActionEntry menu_actions[] = {
    {"Preferences", "preferences-system", "_Preferences", NULL, 
     "Configure the weather applet", G_CALLBACK(show_preferences_dialog)},
    {"About", "help-about", "_About", NULL, 
     "About this applet", G_CALLBACK(show_about_dialog)}
};

static const char *menu_xml = 
    "<menuitem name=\"Preferences\" action=\"Preferences\"/>"
    "<menuitem name=\"About\" action=\"About\"/>";

static gboolean weather_applet_fill(MatePanelApplet *applet) {
    WeatherApplet *weather_applet;
    GtkActionGroup *action_group;
    
    // Initialize logging first
    logger_init();
    log_info("Weather applet starting up");
    
    // Set applet flags
    mate_panel_applet_set_flags(applet, MATE_PANEL_APPLET_EXPAND_MINOR);
    
    // Create weather applet structure
    weather_applet = g_new0(WeatherApplet, 1);
    weather_applet->applet = applet;
    
    // Initialize mutex
    g_mutex_init(&weather_applet->data_mutex);
    
    // Initialize global context if needed
    if (!g_app_context) {
        g_app_context = g_new0(AppContext, 1);
        g_app_context->config = g_new0(AppConfig, 1);
        g_mutex_init(&g_app_context->data_mutex);
        config_init();
        config_load();
    }
    
    // Create local config copy
    weather_applet->config = g_new0(AppConfig, 1);
    weather_applet->config->city = g_strdup(g_app_context->config->city ? g_app_context->config->city : "London");
    weather_applet->config->use_celsius = g_app_context->config->use_celsius;
    weather_applet->config->provider = g_app_context->config->provider;
    weather_applet->config->openweather_api_key = g_strdup(g_app_context->config->openweather_api_key);
    weather_applet->config->tomorrow_api_key = g_strdup(g_app_context->config->tomorrow_api_key);
    weather_applet->config->update_interval_minutes = g_app_context->config->update_interval_minutes ? 
                                                      g_app_context->config->update_interval_minutes : 30;
    weather_applet->current_weather = NULL;
    weather_applet->fetch_cancellable = NULL;
    
    // Create container
    weather_applet->container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_container_add(GTK_CONTAINER(applet), weather_applet->container);
    
    // Create weather emoji label
    weather_applet->weather_icon = gtk_label_new("🌤️");
    PangoAttrList *emoji_attrs = pango_attr_list_new();
    PangoAttribute *emoji_size = pango_attr_size_new(16 * PANGO_SCALE);
    pango_attr_list_insert(emoji_attrs, emoji_size);
    gtk_label_set_attributes(GTK_LABEL(weather_applet->weather_icon), emoji_attrs);
    pango_attr_list_unref(emoji_attrs);
    gtk_box_pack_start(GTK_BOX(weather_applet->container), 
                       weather_applet->weather_icon, FALSE, FALSE, 0);
    
    // Create temperature label with larger font
    weather_applet->temp_label = gtk_label_new("--");
    PangoAttrList *attrs = pango_attr_list_new();
    PangoAttribute *size_attr = pango_attr_size_new(12 * PANGO_SCALE);
    pango_attr_list_insert(attrs, size_attr);
    gtk_label_set_attributes(GTK_LABEL(weather_applet->temp_label), attrs);
    pango_attr_list_unref(attrs);
    gtk_box_pack_start(GTK_BOX(weather_applet->container), 
                       weather_applet->temp_label, FALSE, FALSE, 2);
    
    // Create unit label
    weather_applet->unit_label = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(weather_applet->container), 
                       weather_applet->unit_label, FALSE, FALSE, 0);
    
    // Show all widgets
    gtk_widget_show_all(GTK_WIDGET(applet));
    
    // Setup right-click menu
    action_group = gtk_action_group_new("WeatherAppletActions");
    gtk_action_group_add_actions(action_group, menu_actions, 
                                 G_N_ELEMENTS(menu_actions), weather_applet);
    mate_panel_applet_setup_menu(applet, menu_xml, action_group);
    g_object_unref(action_group);
    
    // Connect destroy signal
    g_signal_connect_swapped(applet, "destroy", 
                            G_CALLBACK(destroy_applet), weather_applet);
    
    // Connect button press event for left-click handling
    g_signal_connect(G_OBJECT(applet), "button-press-event",
                     G_CALLBACK(on_button_press), weather_applet);
    
    // Start weather updates
    log_info("About to call update_weather from weather_applet_fill");
    update_weather(weather_applet);
    log_info("Returned from update_weather, setting up resume-aware timer");
    weather_applet->update_timer = weather_resume_timer_start(
        weather_applet,
        weather_applet->config->update_interval_minutes);
    
    return TRUE;
}

static gboolean weather_applet_factory(MatePanelApplet *applet,
                                       const char *iid,
                                       gpointer data) {
    if (strcmp(iid, "WeatherVibes") == 0) {
        return weather_applet_fill(applet);
    }
    return FALSE;
}

MATE_PANEL_APPLET_OUT_PROCESS_FACTORY("WeatherVibesFactory",
                                      PANEL_TYPE_APPLET,
                                      "WeatherVibes",
                                      weather_applet_factory,
                                      NULL)