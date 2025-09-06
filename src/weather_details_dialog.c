#include "weather_details_dialog.h"
#include "weather_applet.h"
#include "weather_provider.h"
#include "weather_conditions.h"
#include "day_night.h"
#include "config.h"
#include <time.h>
#include <string.h>

// Callback for when popup window is destroyed
static void on_popup_destroy(GtkWidget *widget, gpointer data) {
    (void)widget; // Unused
    WeatherApplet *weather_applet = (WeatherApplet *)data;
    // Clear the window reference in the applet
    weather_applet->details_window = NULL;
}

// Callback for when popup window loses focus
static gboolean on_focus_out(GtkWidget *widget, GdkEventFocus *event, gpointer data) {
    (void)event; // Unused
    WeatherApplet *weather_applet = (WeatherApplet *)data;
    // Close the window when it loses focus
    if (weather_applet->details_window && GTK_IS_WINDOW(weather_applet->details_window)) {
        gtk_widget_destroy(weather_applet->details_window);
        weather_applet->details_window = NULL;
    }
    return FALSE;
}

// Helper function to populate weather details grid
static void populate_weather_details(GtkWidget *vbox, WeatherData *data, AppConfig *config) {
    if (!data) {
        GtkWidget *label = gtk_label_new("No weather data available");
        gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
        return;
    }
    
    char *text;
    GtkWidget *label;
    GtkWidget *hbox;
    
    // Top row: City/condition on left, provider/update on right
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 10);
    
    // Left side: Title with city and condition
    text = g_strdup_printf("<b><big>%s</big></b>\n<big>%s %s</big>",
                          data->city ? data->city : "Unknown",
                          get_time_appropriate_emoji(data),
                          data->condition_text ? data->condition_text : "");
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), text);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
    g_free(text);
    
    // Right side: Provider and last update
    char time_str[100];
    struct tm *tm_info = localtime(&data->last_update);
    strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);
    text = g_strdup_printf("<small><i>Provider: %s\nLast update: %s</i></small>",
                          weather_provider_get_name(config->provider),
                          time_str);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), text);
    gtk_label_set_xalign(GTK_LABEL(label), 1.0);
    gtk_label_set_yalign(GTK_LABEL(label), 1.0);
    gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    g_free(text);
    
    // Create a horizontal box for the two-column layout
    GtkWidget *details_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    gtk_box_pack_start(GTK_BOX(vbox), details_hbox, FALSE, FALSE, 10);
    
    // Left column: Temperature and Sunrise/Sunset
    GtkWidget *left_column = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(details_hbox), left_column, TRUE, TRUE, 0);
    
    // Temperature
    char temp_str[64], feels_str[64];
    double temp = data->temperature;
    double feels_like = data->feels_like;
    if (!config->use_celsius) {
        temp = temp * 9.0/5.0 + 32.0;  // Convert to Fahrenheit
        feels_like = feels_like * 9.0/5.0 + 32.0;
    }
    snprintf(temp_str, sizeof(temp_str), "%.1f°%c", 
             temp, config->use_celsius ? 'C' : 'F');
    snprintf(feels_str, sizeof(feels_str), "%.1f°%c", 
             feels_like, config->use_celsius ? 'C' : 'F');
    
    text = g_strdup_printf("<b>Temperature:</b> %s\n<b>Feels like:</b> %s", temp_str, feels_str);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), text);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    gtk_box_pack_start(GTK_BOX(left_column), label, FALSE, FALSE, 5);
    g_free(text);
    
    // Wind
    text = g_strdup_printf("<b>Wind:</b> %.1f m/s %s",
                          data->wind_speed,
                          data->wind_direction ? data->wind_direction : "");
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), text);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    gtk_box_pack_start(GTK_BOX(left_column), label, FALSE, FALSE, 5);
    g_free(text);
    
    // Right column: Sunrise/Sunset, Humidity, Pressure, UV Index
    GtkWidget *right_column = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(details_hbox), right_column, TRUE, TRUE, 0);
    
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
        gtk_box_pack_start(GTK_BOX(right_column), label, FALSE, FALSE, 5);
        g_free(text);
    }
    
    // Rain/precipitation section with padding above - LEFT COLUMN
    GString *rain_str = g_string_new("");
    gboolean has_rain_data = FALSE;
    
    // Always show rain probability if available (including 0%)
    if (data->precipitation_probability >= 0) {
        g_string_append_printf(rain_str, "<b>Rain chance:</b> %.0f%%", data->precipitation_probability);
        has_rain_data = TRUE;
    }
    
    // Add non-zero precipitation amounts
    if (data->rain_intensity > 0) {
        if (has_rain_data) g_string_append(rain_str, "\n");
        g_string_append_printf(rain_str, "<b>Rain:</b> %.1f mm/hr", data->rain_intensity);
        has_rain_data = TRUE;
    }
    if (data->snow_intensity > 0) {
        if (has_rain_data) g_string_append(rain_str, "\n");
        g_string_append_printf(rain_str, "<b>Snow:</b> %.1f mm/hr", data->snow_intensity);
        has_rain_data = TRUE;
    }
    if (data->sleet_intensity > 0) {
        if (has_rain_data) g_string_append(rain_str, "\n");
        g_string_append_printf(rain_str, "<b>Sleet:</b> %.1f mm/hr", data->sleet_intensity);
        has_rain_data = TRUE;
    }
    if (data->freezing_rain_intensity > 0) {
        if (has_rain_data) g_string_append(rain_str, "\n");
        g_string_append_printf(rain_str, "<b>Freezing rain:</b> %.1f mm/hr", data->freezing_rain_intensity);
        has_rain_data = TRUE;
    }
    if (data->precipitation_accumulation > 0) {
        if (has_rain_data) g_string_append(rain_str, "\n");
        g_string_append_printf(rain_str, "<b>Precipitation:</b> %.1f mm", data->precipitation_accumulation);
        has_rain_data = TRUE;
    }
    
    // Add rain section to LEFT column if we have any rain data
    if (has_rain_data) {
        text = g_string_free(rain_str, FALSE);
        label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(label), text);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0);
        gtk_box_pack_start(GTK_BOX(left_column), label, FALSE, FALSE, 5);
        g_free(text);
    } else {
        g_string_free(rain_str, TRUE);
    }
    
    // Combined Humidity, Pressure, UV Index - RIGHT COLUMN
    text = g_strdup_printf("<b>Humidity:</b> %d%%\n<b>Pressure:</b> %d hPa\n<b>UV Index:</b> %.1f",
                          data->humidity, data->pressure, data->uvi);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), text);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    gtk_box_pack_start(GTK_BOX(right_column), label, FALSE, FALSE, 5);
    g_free(text);
    
    // 5-day forecast at the bottom
    if (data->forecast && data->forecast_days > 0) {
        // Add separator
        GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
        gtk_box_pack_start(GTK_BOX(vbox), separator, FALSE, FALSE, 5);
        
        // Forecast title
        label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(label), "<b>5-Day Forecast</b>");
        gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 3);
        
        // Horizontal box for forecast days
        GtkWidget *forecast_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
        gtk_box_set_homogeneous(GTK_BOX(forecast_box), TRUE);
        gtk_box_pack_start(GTK_BOX(vbox), forecast_box, FALSE, FALSE, 5);
        
        for (int i = 0; i < data->forecast_days && i < 5; i++) {
            GtkWidget *day_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
            
            // Day name
            struct tm *tm_info = localtime(&data->forecast[i].date);
            char day_name[20];
            strftime(day_name, sizeof(day_name), "%a", tm_info);
            label = gtk_label_new(day_name);
            gtk_widget_set_halign(label, GTK_ALIGN_CENTER);
            gtk_box_pack_start(GTK_BOX(day_box), label, FALSE, FALSE, 0);
            
            // Weather emoji
            const char *emoji = weather_condition_get_emoji(data->forecast[i].condition);
            label = gtk_label_new(NULL);
            text = g_strdup_printf("<span size='x-large'>%s</span>", emoji);
            gtk_label_set_markup(GTK_LABEL(label), text);
            gtk_widget_set_halign(label, GTK_ALIGN_CENTER);
            gtk_box_pack_start(GTK_BOX(day_box), label, FALSE, FALSE, 0);
            g_free(text);
            
            // Temperature (max/min)
            double temp_max = data->forecast[i].temp_max;
            double temp_min = data->forecast[i].temp_min;
            if (!config->use_celsius) {
                temp_max = temp_max * 9.0/5.0 + 32.0;
                temp_min = temp_min * 9.0/5.0 + 32.0;
            }
            text = g_strdup_printf("%.0f°/%.0f°", temp_max, temp_min);
            label = gtk_label_new(text);
            gtk_widget_set_halign(label, GTK_ALIGN_CENTER);
            gtk_box_pack_start(GTK_BOX(day_box), label, FALSE, FALSE, 0);
            g_free(text);
            
            // Rain probability if available and > 0
            if (data->forecast[i].precipitation_probability > 0) {
                text = g_strdup_printf("<small>%.0f%%</small>", data->forecast[i].precipitation_probability);
                label = gtk_label_new(NULL);
                gtk_label_set_markup(GTK_LABEL(label), text);
                gtk_widget_set_halign(label, GTK_ALIGN_CENTER);
                gtk_box_pack_start(GTK_BOX(day_box), label, FALSE, FALSE, 0);
                g_free(text);
            }
            
            // Precipitation amount if significant
            if (data->forecast[i].precipitation_amount > 0.1) {
                text = g_strdup_printf("<small>%.1fmm</small>", data->forecast[i].precipitation_amount);
                label = gtk_label_new(NULL);
                gtk_label_set_markup(GTK_LABEL(label), text);
                gtk_widget_set_halign(label, GTK_ALIGN_CENTER);
                gtk_box_pack_start(GTK_BOX(day_box), label, FALSE, FALSE, 0);
                g_free(text);
            }
            
            gtk_box_pack_start(GTK_BOX(forecast_box), day_box, TRUE, TRUE, 0);
        }
    }
}

void show_weather_details_popup(WeatherApplet *weather_applet) {
    GtkWidget *window;
    GtkWidget *vbox;
    
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
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 380);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_MOUSE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(window), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(window), TRUE);
    gtk_container_set_border_width(GTK_CONTAINER(window), 8);
    
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    
    // Populate weather details
    populate_weather_details(vbox, weather_applet->current_weather, weather_applet->config);
    
    // Clean up when window is destroyed
    g_signal_connect(window, "destroy", G_CALLBACK(on_popup_destroy), weather_applet);
    
    // Close window when it loses focus
    g_signal_connect(window, "focus-out-event", G_CALLBACK(on_focus_out), weather_applet);
    
    gtk_widget_show_all(window);
}