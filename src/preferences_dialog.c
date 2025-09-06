#include "preferences_dialog.h"
#include "weather_applet.h"
#include "weather_provider.h"
#include "preferences_handler.h"
#include "logger.h"
#include <string.h>

// Provider callback data structure
typedef struct {
    GtkWidget *api_key_label;
    GtkWidget *api_key_entry;
    WeatherApplet *weather_applet;
} ProviderCallbackData;

// Helper function to create location entry widget
static GtkWidget* create_location_entry(AppConfig *config) {
    GtkWidget *entry = gtk_entry_new();
    if (config && config->city) {
        gtk_entry_set_text(GTK_ENTRY(entry), config->city);
    }
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 30);
    return entry;
}

// Helper function to create provider combo box
static GtkWidget* create_provider_combo(int current_provider) {
    GtkWidget *combo = gtk_combo_box_text_new();
    weather_provider_init();
    for (int i = 0; i < PROVIDER_COUNT; i++) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), 
                                       weather_provider_get_name((WeatherProvider)i));
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), current_provider);
    return combo;
}

// Helper function to create API key entry field
static GtkWidget* create_api_key_entry(AppConfig *config, int provider) {
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 40);
    
    if (provider == PROVIDER_OPENWEATHER) {
        gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Enter OpenWeather API key");
        if (config->openweather_api_key) {
            gtk_entry_set_text(GTK_ENTRY(entry), config->openweather_api_key);
        }
    } else if (provider == PROVIDER_TOMORROW) {
        gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Enter Tomorrow.io API key");
        if (config->tomorrow_api_key) {
            gtk_entry_set_text(GTK_ENTRY(entry), config->tomorrow_api_key);
        }
    }
    
    return entry;
}

// Helper function to create temperature unit combo box
static GtkWidget* create_unit_combo(gboolean use_celsius) {
    GtkWidget *combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "Celsius");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "Fahrenheit");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), use_celsius ? 0 : 1);
    return combo;
}

// Helper function to create update interval combo box
static GtkWidget* create_interval_combo(int current_interval_minutes) {
    GtkWidget *combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "5 minutes");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "10 minutes");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "15 minutes");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "30 minutes");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "1 hour");
    
    int active_index = 1; // Default to 10 minutes
    switch (current_interval_minutes) {
        case 5:  active_index = 0; break;
        case 10: active_index = 1; break;
        case 15: active_index = 2; break;
        case 30: active_index = 3; break;
        case 60: active_index = 4; break;
        default: active_index = 1; break;
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), active_index);
    return combo;
}

// Helper function to create preferences grid
static GtkWidget* create_preferences_grid(WeatherApplet *weather_applet,
                                         GtkWidget **location_entry_out,
                                         GtkWidget **provider_combo_out,
                                         GtkWidget **api_key_label_out,
                                         GtkWidget **api_key_entry_out,
                                         GtkWidget **unit_combo_out,
                                         GtkWidget **interval_combo_out) {
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);
    
    // Location
    GtkWidget *location_label = gtk_label_new("Location:");
    gtk_widget_set_halign(location_label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), location_label, 0, 0, 1, 1);
    
    *location_entry_out = create_location_entry(weather_applet->config);
    gtk_grid_attach(GTK_GRID(grid), *location_entry_out, 1, 0, 1, 1);
    
    // Weather provider
    GtkWidget *provider_label = gtk_label_new("Weather Provider:");
    gtk_widget_set_halign(provider_label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), provider_label, 0, 1, 1, 1);
    
    *provider_combo_out = create_provider_combo(weather_applet->config->provider);
    gtk_grid_attach(GTK_GRID(grid), *provider_combo_out, 1, 1, 1, 1);
    
    // API Key
    *api_key_label_out = gtk_label_new("API Key:");
    gtk_widget_set_halign(*api_key_label_out, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), *api_key_label_out, 0, 2, 1, 1);
    
    int current_provider = gtk_combo_box_get_active(GTK_COMBO_BOX(*provider_combo_out));
    *api_key_entry_out = create_api_key_entry(weather_applet->config, current_provider);
    gtk_grid_attach(GTK_GRID(grid), *api_key_entry_out, 1, 2, 1, 1);
    
    // Hide API key fields if not needed
    if (current_provider != PROVIDER_OPENWEATHER && current_provider != PROVIDER_TOMORROW) {
        gtk_widget_hide(*api_key_label_out);
        gtk_widget_hide(*api_key_entry_out);
    }
    
    // Temperature unit
    GtkWidget *unit_label = gtk_label_new("Temperature Unit:");
    gtk_widget_set_halign(unit_label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), unit_label, 0, 3, 1, 1);
    
    *unit_combo_out = create_unit_combo(weather_applet->config->use_celsius);
    gtk_grid_attach(GTK_GRID(grid), *unit_combo_out, 1, 3, 1, 1);
    
    // Update interval
    GtkWidget *interval_label = gtk_label_new("Update Interval:");
    gtk_widget_set_halign(interval_label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), interval_label, 0, 4, 1, 1);
    
    *interval_combo_out = create_interval_combo(weather_applet->config->update_interval_minutes);
    gtk_grid_attach(GTK_GRID(grid), *interval_combo_out, 1, 4, 1, 1);
    
    return grid;
}

void on_provider_changed(GtkComboBox *combo, gpointer user_data) {
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

void show_preferences_dialog(GtkAction *action, WeatherApplet *weather_applet) {
    (void)action; // Unused
    GtkWidget *dialog;
    GtkWidget *content_area;
    GtkWidget *grid;
    GtkWidget *location_entry;
    GtkWidget *provider_combo;
    GtkWidget *api_key_label, *api_key_entry;
    GtkWidget *unit_combo;
    GtkWidget *interval_combo;
    
    dialog = gtk_dialog_new_with_buttons("Weather Preferences",
                                         NULL,
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         "_Cancel", GTK_RESPONSE_CANCEL,
                                         "_OK", GTK_RESPONSE_OK,
                                         NULL);
    
    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    
    // Create preferences grid with all widgets
    grid = create_preferences_grid(weather_applet,
                                  &location_entry,
                                  &provider_combo,
                                  &api_key_label,
                                  &api_key_entry,
                                  &unit_combo,
                                  &interval_combo);
    gtk_container_add(GTK_CONTAINER(content_area), grid);
    
    // Add callback to update API key field when provider changes
    ProviderCallbackData *callback_data = g_new0(ProviderCallbackData, 1);
    callback_data->api_key_label = api_key_label;
    callback_data->api_key_entry = api_key_entry;
    callback_data->weather_applet = weather_applet;
    
    g_signal_connect_data(provider_combo, "changed", 
                         G_CALLBACK(on_provider_changed), callback_data,
                         (GClosureNotify)g_free, 0);
    
    gtk_widget_show_all(dialog);
    
    // After showing all widgets, re-apply the API key field visibility
    int current_provider = gtk_combo_box_get_active(GTK_COMBO_BOX(provider_combo));
    if (current_provider != PROVIDER_OPENWEATHER && current_provider != PROVIDER_TOMORROW) {
        gtk_widget_hide(api_key_label);
        gtk_widget_hide(api_key_entry);
    }
    
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
        // Get values from UI widgets
        const char *new_location = gtk_entry_get_text(GTK_ENTRY(location_entry));
        int new_provider = gtk_combo_box_get_active(GTK_COMBO_BOX(provider_combo));
        const char *api_key = gtk_entry_get_text(GTK_ENTRY(api_key_entry));
        gboolean use_celsius = gtk_combo_box_get_active(GTK_COMBO_BOX(unit_combo)) == 0;
        int interval_index = gtk_combo_box_get_active(GTK_COMBO_BOX(interval_combo));
        
        // Save all preferences using preferences handler
        handle_preferences_save(weather_applet, new_location, new_provider, api_key, 
                               use_celsius, interval_index);
    }
    
    gtk_widget_destroy(dialog);
}