#include <mate-panel-applet.h>
#include <gtk/gtk.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "app.h"
#include "weather_fetcher.h"
#include "config.h"
#include "weather_provider.h"
#include "logger.h"

// Global app context
AppContext *g_app_context = NULL;

// Forward declarations
static gboolean update_weather(gpointer data);

typedef struct {
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
} WeatherApplet;

static void destroy_applet(WeatherApplet *weather_applet) {
    if (weather_applet->update_timer) {
        g_source_remove(weather_applet->update_timer);
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
    if (g_app_context && g_app_context->cached_city) {
        g_free(g_app_context->cached_city);
        g_app_context->cached_city = NULL;
    }
    g_mutex_clear(&weather_applet->data_mutex);
    logger_cleanup();
    g_free(weather_applet);
}

// Map weather conditions to icon names (using GNOME/freedesktop standard names)
static const char* get_weather_icon_name(const char *condition_text) {
    if (!condition_text) return "weather-severe-alert";
    
    // Convert to lowercase for comparison
    char lower[256];
    strncpy(lower, condition_text, sizeof(lower)-1);
    for (int i = 0; lower[i]; i++) {
        lower[i] = tolower(lower[i]);
    }
    
    // Map conditions to standard weather icons from gnome theme
    if (strstr(lower, "clear") || strstr(lower, "sunny")) {
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        if (tm->tm_hour >= 6 && tm->tm_hour < 20) {
            return "weather-clear";
        } else {
            return "weather-clear-night";
        }
    } else if (strstr(lower, "partly cloudy") || strstr(lower, "few clouds")) {
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        if (tm->tm_hour >= 6 && tm->tm_hour < 20) {
            return "weather-few-clouds";
        } else {
            return "weather-few-clouds-night";
        }
    } else if (strstr(lower, "overcast") || strstr(lower, "cloudy")) {
        return "weather-overcast";
    } else if (strstr(lower, "shower") || strstr(lower, "drizzle") || strstr(lower, "light rain")) {
        return "weather-showers-scattered";
    } else if (strstr(lower, "rain")) {
        return "weather-showers";
    } else if (strstr(lower, "snow")) {
        return "weather-snow";
    } else if (strstr(lower, "storm") || strstr(lower, "thunder")) {
        return "weather-storm";
    } else if (strstr(lower, "fog") || strstr(lower, "mist") || strstr(lower, "haze")) {
        return "weather-fog";
    }
    
    // If no match found or generic "Current Weather", use a general weather icon
    if (strstr(lower, "current weather")) {
        // For generic "Current Weather" from AnsiWeather, show partly cloudy as neutral
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        if (tm->tm_hour >= 6 && tm->tm_hour < 20) {
            return "weather-few-clouds";
        } else {
            return "weather-few-clouds-night";
        }
    }
    
    // For truly unknown/unrecognized conditions, show alert
    return "weather-severe-alert";
}

static void update_display(WeatherApplet *weather_applet) {
    g_mutex_lock(&weather_applet->data_mutex);
    
    if (weather_applet->current_weather && weather_applet->current_weather->temperature != -999.0) {
        char temp_str[32];
        char unit_str[8];
        char tooltip[512];
        
        // Set weather icon from theme
        const char *icon_name = get_weather_icon_name(weather_applet->current_weather->condition_text);
        gtk_image_set_from_icon_name(GTK_IMAGE(weather_applet->weather_icon), 
                                     icon_name, GTK_ICON_SIZE_LARGE_TOOLBAR);
        
        // Set temperature
        double temp = weather_applet->current_weather->temperature;
        double feels_like = weather_applet->current_weather->feels_like;
        if (!weather_applet->config->use_celsius) {
            temp = temp * 9.0/5.0 + 32.0; // Convert to Fahrenheit
            feels_like = feels_like * 9.0/5.0 + 32.0;
        }
        snprintf(temp_str, sizeof(temp_str), "%.0f", temp);
        gtk_label_set_text(GTK_LABEL(weather_applet->temp_label), temp_str);
        
        // Set unit
        strcpy(unit_str, weather_applet->config->use_celsius ? "°C" : "°F");
        gtk_label_set_text(GTK_LABEL(weather_applet->unit_label), unit_str);
        
        // Set detailed tooltip
        snprintf(tooltip, sizeof(tooltip),
                "%s\n"
                "%s\n"
                "Temperature: %.0f%s\n"
                "Feels like: %.0f%s\n"
                "Humidity: %d%%\n"
                "Wind: %.1f m/s %s\n"
                "UV Index: %.1f",
                weather_applet->current_weather->city ? weather_applet->current_weather->city : "Unknown",
                weather_applet->current_weather->condition_text ? weather_applet->current_weather->condition_text : "Unknown",
                temp, unit_str,
                feels_like, unit_str,
                weather_applet->current_weather->humidity,
                weather_applet->current_weather->wind_speed,
                weather_applet->current_weather->wind_direction ? weather_applet->current_weather->wind_direction : "",
                weather_applet->current_weather->uvi);
        
        gtk_widget_set_tooltip_text(weather_applet->container, tooltip);
    } else {
        gtk_image_set_from_icon_name(GTK_IMAGE(weather_applet->weather_icon), 
                                     "weather-severe-alert", GTK_ICON_SIZE_LARGE_TOOLBAR);
        gtk_label_set_text(GTK_LABEL(weather_applet->temp_label), "--");
        gtk_label_set_text(GTK_LABEL(weather_applet->unit_label), "");
        
        // Check if we have an error message from the fetcher
        char *error_msg;
        if (weather_applet->current_weather && weather_applet->current_weather->error_message) {
            error_msg = g_strdup(weather_applet->current_weather->error_message);
        } else {
            error_msg = g_strdup_printf(
                "Weather data unavailable\n\n"
                "City: %s\n"
                "Provider: %s\n\n"
                "Click to open preferences and check settings",
                weather_applet->config->city ? weather_applet->config->city : "Not set",
                weather_provider_get_name(weather_applet->config->provider)
            );
        }
        gtk_widget_set_tooltip_text(weather_applet->container, error_msg);
        g_free(error_msg);
    }
    
    g_mutex_unlock(&weather_applet->data_mutex);
}

static void on_popup_destroy(GtkWidget *widget, gpointer data) {
    WeatherApplet *weather_applet = (WeatherApplet *)data;
    // Clear the window reference in the applet
    weather_applet->details_window = NULL;
}

static void show_weather_details_popup(WeatherApplet *weather_applet) {
    GtkWidget *window;
    GtkWidget *vbox;
    GtkWidget *label;
    GtkWidget *button;
    char *text;
    
    // Check if a window is already open
    if (weather_applet->details_window != NULL && GTK_IS_WINDOW(weather_applet->details_window)) {
        // Window already exists - just present it (bring to front and focus)
        gtk_window_present(GTK_WINDOW(weather_applet->details_window));
        return;
    }
    
    // Create a popup window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    weather_applet->details_window = window;  // Store reference
    gtk_window_set_title(GTK_WINDOW(window), "Weather Details");
    gtk_window_set_default_size(GTK_WINDOW(window), 350, 400);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_MOUSE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(window), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(window), TRUE);
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);
    
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    
    if (weather_applet->current_weather) {
        WeatherData *data = weather_applet->current_weather;
        
        // Title with city and condition
        text = g_strdup_printf("<b><big>%s</big></b>\n<big>%s %s</big>",
                               data->city ? data->city : "Unknown",
                               data->condition_icon ? data->condition_icon : "",
                               data->condition_text ? data->condition_text : "");
        label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(label), text);
        gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 10);
        g_free(text);
        
        // Temperature
        text = g_strdup_printf("<b>Temperature:</b> %.1f°%c\n<b>Feels like:</b> %.1f°%c",
                               data->temperature,
                               weather_applet->config->use_celsius ? 'C' : 'F',
                               data->feels_like,
                               weather_applet->config->use_celsius ? 'C' : 'F');
        label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(label), text);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0);
        gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 5);
        g_free(text);
        
        // Wind
        text = g_strdup_printf("<b>Wind:</b> %.1f m/s %s",
                               data->wind_speed,
                               data->wind_direction ? data->wind_direction : "");
        label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(label), text);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0);
        gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 5);
        g_free(text);
        
        // Humidity & Pressure
        text = g_strdup_printf("<b>Humidity:</b> %d%%\n<b>Pressure:</b> %d hPa\n<b>UV Index:</b> %.1f",
                               data->humidity, data->pressure, data->uvi);
        label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(label), text);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0);
        gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 5);
        g_free(text);
        
        // Sunrise/Sunset if available
        if (data->sunrise || data->sunset) {
            GString *sunrise_sunset = g_string_new("");
            if (data->sunrise) {
                g_string_append_printf(sunrise_sunset, "<b>Sunrise:</b> %s", data->sunrise);
            }
            if (data->sunset) {
                if (data->sunrise) g_string_append(sunrise_sunset, "\n");
                g_string_append_printf(sunrise_sunset, "<b>Sunset:</b> %s", data->sunset);
            }
            text = g_string_free(sunrise_sunset, FALSE);
            label = gtk_label_new(NULL);
            gtk_label_set_markup(GTK_LABEL(label), text);
            gtk_label_set_xalign(GTK_LABEL(label), 0.0);
            gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 5);
            g_free(text);
        }
        
        // Provider info
        text = g_strdup_printf("<small><i>Provider: %s</i></small>",
                               weather_provider_get_name(weather_applet->config->provider));
        label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(label), text);
        gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 5);
        g_free(text);
        
        // Last update
        char time_str[100];
        struct tm *tm_info = localtime(&data->last_update);
        strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);
        text = g_strdup_printf("<small><i>Last update: %s</i></small>", time_str);
        label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(label), text);
        gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 5);
        g_free(text);
    } else {
        label = gtk_label_new("No weather data available");
        gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
    }
    
    // Clean up when window is destroyed
    g_signal_connect(window, "destroy", G_CALLBACK(on_popup_destroy), weather_applet);
    
    // Close button
    button = gtk_button_new_with_label("Close");
    g_signal_connect_swapped(button, "clicked", 
                            G_CALLBACK(gtk_widget_destroy), window);
    gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 5);
    
    gtk_widget_show_all(window);
}

static gboolean on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    WeatherApplet *weather_applet = (WeatherApplet *)data;
    
    // Handle left mouse button click
    if (event->button == 1) {  // Left button
        g_message("Left click detected - showing weather details");
        show_weather_details_popup(weather_applet);
        return TRUE;  // Event handled
    }
    
    // Let right-click go through for the menu
    return FALSE;  // Let other handlers process this event
}

static gboolean update_weather(gpointer data) {
    WeatherApplet *weather_applet = (WeatherApplet *)data;
    
    if (weather_applet->config && weather_applet->config->city) {
        g_message("Updating weather for %s with provider %d", 
                  weather_applet->config->city, weather_applet->config->provider);
        
        gboolean success = weather_fetcher_update(weather_applet->config->city, weather_applet->config->provider);
        
        g_message("Update result: %s, have data: %s", 
                  success ? "success" : "failed",
                  (g_app_context && g_app_context->weather_data) ? "yes" : "no");
        
        // Check if we have data to display
        if (g_app_context && g_app_context->weather_data) {
            g_mutex_lock(&weather_applet->data_mutex);
            if (weather_applet->current_weather) {
                weather_fetcher_free_data(weather_applet->current_weather);
            }
            weather_applet->current_weather = weather_fetcher_copy_data(g_app_context->weather_data);
            g_mutex_unlock(&weather_applet->data_mutex);
            
            update_display(weather_applet);
        } else if (!success) {
            // Fetch failed and no cached data - show error state
            g_mutex_lock(&weather_applet->data_mutex);
            if (weather_applet->current_weather) {
                weather_fetcher_free_data(weather_applet->current_weather);
                weather_applet->current_weather = NULL;
            }
            g_mutex_unlock(&weather_applet->data_mutex);
            
            update_display(weather_applet);
        }
        // If success but no data, it means cache said "don't fetch" but there's no data
        // This shouldn't happen with our fixed caching logic
    }
    
    return TRUE; // Continue timer
}

// Callback for provider combo box changes
static void on_provider_changed(GtkComboBox *combo, gpointer user_data) {
    typedef struct {
        GtkWidget *api_key_label;
        GtkWidget *api_key_entry;
        WeatherApplet *weather_applet;
    } ProviderCallbackData;
    
    ProviderCallbackData *data = (ProviderCallbackData*)user_data;
    WeatherProvider selected_provider = (WeatherProvider)gtk_combo_box_get_active(combo);
    
    // Save current API key before switching
    const char *current_key = gtk_entry_get_text(GTK_ENTRY(data->api_key_entry));
    int previous_provider = data->weather_applet->config->provider;
    
    if (previous_provider == PROVIDER_OPENWEATHER && current_key && strlen(current_key) > 0) {
        g_free(data->weather_applet->config->openweather_api_key);
        data->weather_applet->config->openweather_api_key = g_strdup(current_key);
    } else if (previous_provider == PROVIDER_TOMORROW && current_key && strlen(current_key) > 0) {
        g_free(data->weather_applet->config->tomorrow_api_key);
        data->weather_applet->config->tomorrow_api_key = g_strdup(current_key);
    }
    
    // Update provider in config
    data->weather_applet->config->provider = selected_provider;
    
    // Update API key field based on new provider
    if (selected_provider == PROVIDER_OPENWEATHER) {
        gtk_entry_set_placeholder_text(GTK_ENTRY(data->api_key_entry), "Enter OpenWeather API key");
        gtk_entry_set_text(GTK_ENTRY(data->api_key_entry), 
                          data->weather_applet->config->openweather_api_key ? 
                          data->weather_applet->config->openweather_api_key : "");
        gtk_widget_show(data->api_key_label);
        gtk_widget_show(data->api_key_entry);
    } else if (selected_provider == PROVIDER_TOMORROW) {
        gtk_entry_set_placeholder_text(GTK_ENTRY(data->api_key_entry), "Enter Tomorrow.io API key");
        gtk_entry_set_text(GTK_ENTRY(data->api_key_entry), 
                          data->weather_applet->config->tomorrow_api_key ? 
                          data->weather_applet->config->tomorrow_api_key : "");
        gtk_widget_show(data->api_key_label);
        gtk_widget_show(data->api_key_entry);
    } else {
        // Hide API key field for providers that don't need it
        gtk_widget_hide(data->api_key_label);
        gtk_widget_hide(data->api_key_entry);
    }
    
    // Force dialog to resize to fit new content
    GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(combo));
    if (GTK_IS_WINDOW(dialog)) {
        gtk_window_resize(GTK_WINDOW(dialog), 1, 1);  // Shrink to minimum
    }
    
    log_debug("Provider changed to %d (%s), API key field: %s", 
            selected_provider, weather_provider_get_name(selected_provider), 
            (selected_provider == PROVIDER_OPENWEATHER || selected_provider == PROVIDER_TOMORROW) ? "visible" : "hidden");
}

static void show_about_dialog(GtkAction *action, WeatherApplet *weather_applet) {
    GtkWidget *dialog;
    
    dialog = gtk_about_dialog_new();
    
    gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(dialog), "Weather Vibes");
    gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), "1.0");
    gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dialog), 
                                  "A vibrant weather applet for MATE Panel\n\n"
                                  "Multiple weather providers with geocoding support\n\n"
                                  "Vibe-Coded with ♥\n"
                                  "Feel the weather vibes!");
    gtk_about_dialog_set_logo_icon_name(GTK_ABOUT_DIALOG(dialog), "weather-clear");
    gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(dialog), "https://github.com/fcambus/ansiweather");
    gtk_about_dialog_set_website_label(GTK_ABOUT_DIALOG(dialog), "AnsiWeather on GitHub");
    
    // Don't set authors to avoid Credits button
    
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static void show_preferences_dialog(GtkAction *action, WeatherApplet *weather_applet) {
    GtkWidget *dialog;
    GtkWidget *content_area;
    GtkWidget *grid;
    GtkWidget *location_label, *location_entry;
    GtkWidget *provider_label, *provider_combo;
    GtkWidget *api_key_label, *api_key_entry;
    GtkWidget *unit_label, *unit_combo;
    GtkWidget *interval_label, *interval_combo;
    
    dialog = gtk_dialog_new_with_buttons("Weather Preferences",
                                         NULL,
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         "_Cancel", GTK_RESPONSE_CANCEL,
                                         "_OK", GTK_RESPONSE_OK,
                                         NULL);
    
    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    
    grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);
    gtk_container_add(GTK_CONTAINER(content_area), grid);
    
    // Location
    location_label = gtk_label_new("Location:");
    gtk_widget_set_halign(location_label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), location_label, 0, 0, 1, 1);
    
    location_entry = gtk_entry_new();
    if (weather_applet->config && weather_applet->config->city) {
        gtk_entry_set_text(GTK_ENTRY(location_entry), weather_applet->config->city);
    }
    gtk_entry_set_width_chars(GTK_ENTRY(location_entry), 30);
    gtk_grid_attach(GTK_GRID(grid), location_entry, 1, 0, 1, 1);
    
    // Weather provider
    provider_label = gtk_label_new("Weather Provider:");
    gtk_widget_set_halign(provider_label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), provider_label, 0, 1, 1, 1);
    
    provider_combo = gtk_combo_box_text_new();
    // Initialize providers if not done already
    weather_provider_init();
    // Populate provider combo box
    for (int i = 0; i < PROVIDER_COUNT; i++) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(provider_combo), 
                                       weather_provider_get_name((WeatherProvider)i));
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(provider_combo), weather_applet->config->provider);
    gtk_grid_attach(GTK_GRID(grid), provider_combo, 1, 1, 1, 1);
    
    // API Key (single field that updates based on provider)
    api_key_label = gtk_label_new("API Key:");
    gtk_widget_set_halign(api_key_label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), api_key_label, 0, 2, 1, 1);
    
    api_key_entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), api_key_entry, 1, 2, 1, 1);
    
    // Set initial API key based on selected provider
    int current_provider = gtk_combo_box_get_active(GTK_COMBO_BOX(provider_combo));
    if (current_provider == PROVIDER_OPENWEATHER) {
        gtk_entry_set_placeholder_text(GTK_ENTRY(api_key_entry), "Enter OpenWeather API key");
        if (weather_applet->config->openweather_api_key) {
            gtk_entry_set_text(GTK_ENTRY(api_key_entry), weather_applet->config->openweather_api_key);
        }
        gtk_widget_show(api_key_label);
        gtk_widget_show(api_key_entry);
    } else if (current_provider == PROVIDER_TOMORROW) {
        gtk_entry_set_placeholder_text(GTK_ENTRY(api_key_entry), "Enter Tomorrow.io API key");
        if (weather_applet->config->tomorrow_api_key) {
            gtk_entry_set_text(GTK_ENTRY(api_key_entry), weather_applet->config->tomorrow_api_key);
        }
        gtk_widget_show(api_key_label);
        gtk_widget_show(api_key_entry);
    } else {
        // Hide API key field for providers that don't need it
        gtk_widget_hide(api_key_label);
        gtk_widget_hide(api_key_entry);
    }
    
    // Add callback to update API key field when provider changes
    typedef struct {
        GtkWidget *api_key_label;
        GtkWidget *api_key_entry;
        WeatherApplet *weather_applet;
    } ProviderCallbackData;
    
    ProviderCallbackData *callback_data = g_new0(ProviderCallbackData, 1);
    callback_data->api_key_label = api_key_label;
    callback_data->api_key_entry = api_key_entry;
    callback_data->weather_applet = weather_applet;
    
    g_signal_connect_data(provider_combo, "changed", 
                         G_CALLBACK(on_provider_changed), callback_data,
                         (GClosureNotify)g_free, 0);
    
    // Temperature unit
    unit_label = gtk_label_new("Temperature Unit:");
    gtk_widget_set_halign(unit_label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), unit_label, 0, 3, 1, 1);
    
    unit_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(unit_combo), "Celsius");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(unit_combo), "Fahrenheit");
    gtk_combo_box_set_active(GTK_COMBO_BOX(unit_combo), 
                             weather_applet->config->use_celsius ? 0 : 1);
    gtk_grid_attach(GTK_GRID(grid), unit_combo, 1, 3, 1, 1);
    
    // Update interval
    interval_label = gtk_label_new("Update Interval:");
    gtk_widget_set_halign(interval_label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), interval_label, 0, 4, 1, 1);
    
    interval_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(interval_combo), "5 minutes");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(interval_combo), "10 minutes");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(interval_combo), "15 minutes");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(interval_combo), "30 minutes");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(interval_combo), "1 hour");
    
    // Set active based on current interval
    int active_index = 1; // Default to 10 minutes
    switch (weather_applet->config->update_interval_minutes) {
        case 5:  active_index = 0; break;
        case 10: active_index = 1; break;
        case 15: active_index = 2; break;
        case 30: active_index = 3; break;
        case 60: active_index = 4; break;
        default: active_index = 1; break; // Default to 10 minutes for any other value
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(interval_combo), active_index);
    gtk_grid_attach(GTK_GRID(grid), interval_combo, 1, 4, 1, 1);
    
    gtk_widget_show_all(dialog);
    
    // After showing all widgets, re-apply the API key field visibility
    // (gtk_widget_show_all overrides the previous visibility settings)
    if (current_provider != PROVIDER_OPENWEATHER && current_provider != PROVIDER_TOMORROW) {
        gtk_widget_hide(api_key_label);
        gtk_widget_hide(api_key_entry);
    }
    
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
        // Update config
        const char *new_location = gtk_entry_get_text(GTK_ENTRY(location_entry));
        g_free(weather_applet->config->city);
        weather_applet->config->city = g_strdup(new_location);
        
        int old_provider = weather_applet->config->provider;
        weather_applet->config->provider = 
            gtk_combo_box_get_active(GTK_COMBO_BOX(provider_combo));
        
        // Update API key for the current provider
        const char *api_key = gtk_entry_get_text(GTK_ENTRY(api_key_entry));
        
        if (weather_applet->config->provider == PROVIDER_OPENWEATHER) {
            g_free(weather_applet->config->openweather_api_key);
            weather_applet->config->openweather_api_key = g_strdup(api_key);
            if (weather_applet->config->openweather_api_key) {
                g_strstrip(weather_applet->config->openweather_api_key);  // Remove any whitespace/newlines
            }
            log_debug("OpenWeather API key saved: '%s' (length: %zu)", 
                      weather_applet->config->openweather_api_key ? weather_applet->config->openweather_api_key : "NULL",
                      weather_applet->config->openweather_api_key ? strlen(weather_applet->config->openweather_api_key) : 0);
        } else if (weather_applet->config->provider == PROVIDER_TOMORROW) {
            g_free(weather_applet->config->tomorrow_api_key);
            weather_applet->config->tomorrow_api_key = g_strdup(api_key);
            if (weather_applet->config->tomorrow_api_key) {
                g_strstrip(weather_applet->config->tomorrow_api_key);  // Remove any whitespace/newlines
            }
            log_debug("Tomorrow.io API key saved: '%s' (length: %zu)", 
                      weather_applet->config->tomorrow_api_key ? weather_applet->config->tomorrow_api_key : "NULL",
                      weather_applet->config->tomorrow_api_key ? strlen(weather_applet->config->tomorrow_api_key) : 0);
        }
        
        // Clear current weather data if provider changed
        if (old_provider != weather_applet->config->provider) {
            g_mutex_lock(&weather_applet->data_mutex);
            if (weather_applet->current_weather) {
                weather_fetcher_free_data(weather_applet->current_weather);
                weather_applet->current_weather = NULL;
            }
            g_mutex_unlock(&weather_applet->data_mutex);
            
            // Also clear the global weather data and invalidate cache for the old provider
            if (g_app_context) {
                g_mutex_lock(&g_app_context->data_mutex);
                if (g_app_context->weather_data) {
                    weather_fetcher_free_data(g_app_context->weather_data);
                    g_app_context->weather_data = NULL;
                }
                // Invalidate the cache for the old provider so next fetch will work
                g_app_context->last_fetch_time[old_provider] = 0;
                g_mutex_unlock(&g_app_context->data_mutex);
            }
            
            // Update display immediately to show "--" while fetching
            update_display(weather_applet);
        }
        
        weather_applet->config->use_celsius = 
            gtk_combo_box_get_active(GTK_COMBO_BOX(unit_combo)) == 0;
        
        // Map combo box index to minutes
        int interval_index = gtk_combo_box_get_active(GTK_COMBO_BOX(interval_combo));
        int interval_minutes[] = {5, 10, 15, 30, 60};
        weather_applet->config->update_interval_minutes = interval_minutes[interval_index];
        
        // Save config to g_app_context and update display
        if (g_app_context) {
            g_free(g_app_context->config->city);
            g_app_context->config->city = g_strdup(weather_applet->config->city);
            g_app_context->config->provider = weather_applet->config->provider;
            g_free(g_app_context->config->openweather_api_key);
            g_app_context->config->openweather_api_key = g_strdup(weather_applet->config->openweather_api_key);
            g_free(g_app_context->config->tomorrow_api_key);
            g_app_context->config->tomorrow_api_key = g_strdup(weather_applet->config->tomorrow_api_key);
            g_app_context->config->use_celsius = weather_applet->config->use_celsius;
            g_app_context->config->update_interval_minutes = weather_applet->config->update_interval_minutes;
            config_save();
        }
        
        // Restart timer with new interval
        if (weather_applet->update_timer) {
            g_source_remove(weather_applet->update_timer);
        }
        weather_applet->update_timer = g_timeout_add_seconds(
            weather_applet->config->update_interval_minutes * 60,
            update_weather, weather_applet);
        
        // Update immediately
        update_weather(weather_applet);
    }
    
    gtk_widget_destroy(dialog);
}

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
    
    // Create container
    weather_applet->container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_container_add(GTK_CONTAINER(applet), weather_applet->container);
    
    // Create weather icon image - use GNOME theme as fallback
    GtkIconTheme *icon_theme = gtk_icon_theme_get_default();
    gtk_icon_theme_append_search_path(icon_theme, "/usr/share/icons/gnome");
    
    weather_applet->weather_icon = gtk_image_new_from_icon_name("weather-severe-alert", 
                                                                GTK_ICON_SIZE_LARGE_TOOLBAR);
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
    update_weather(weather_applet);
    weather_applet->update_timer = g_timeout_add_seconds(
        weather_applet->config->update_interval_minutes * 60,
        update_weather, weather_applet);
    
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