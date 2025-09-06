#include "weather_display.h"
#include "weather_applet.h"
#include "weather_conditions.h"
#include "day_night.h"
#include "weather_provider.h"
#include "logger.h"
#include <string.h>
#include <time.h>

void update_display(WeatherApplet *weather_applet) {
    log_info("update_display called - current_weather=%p, temp=%.1f",
            weather_applet->current_weather,
            weather_applet->current_weather ? weather_applet->current_weather->temperature : -999.0);
    
    g_mutex_lock(&weather_applet->data_mutex);
    
    if (weather_applet->current_weather && weather_applet->current_weather->temperature != -999.0) {
        char temp_str[32];
        char unit_str[8];
        
        log_info("Display: Have valid data, temp=%.1f, condition='%s'",
                weather_applet->current_weather->temperature,
                weather_applet->current_weather->condition_text ? weather_applet->current_weather->condition_text : "NULL");
        
        // Set weather emoji on label using time-appropriate version
        const char *emoji = get_time_appropriate_emoji(weather_applet->current_weather);
        log_info("Display: Setting emoji to '%s' for condition %d (day/night adjusted)", 
                emoji, weather_applet->current_weather->condition);
        gtk_label_set_text(GTK_LABEL(weather_applet->weather_icon), emoji);
        
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
        
        // Build detailed tooltip with rain data
        GString *tooltip_str = g_string_new("");
        g_string_append_printf(tooltip_str,
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
        
        // Add rain/precipitation data if available
        WeatherData *wd = weather_applet->current_weather;
        
        // Always show rain probability if available
        if (wd->precipitation_probability >= 0) {
            g_string_append_printf(tooltip_str, "\nRain chance: %.0f%%", wd->precipitation_probability);
        }
        
        // Only show non-zero intensity values
        if (wd->rain_intensity > 0) {
            g_string_append_printf(tooltip_str, "\nRain: %.1f mm/hr", wd->rain_intensity);
        }
        
        if (wd->snow_intensity > 0) {
            g_string_append_printf(tooltip_str, "\nSnow: %.1f mm/hr", wd->snow_intensity);
        }
        
        if (wd->sleet_intensity > 0) {
            g_string_append_printf(tooltip_str, "\nSleet: %.1f mm/hr", wd->sleet_intensity);
        }
        
        if (wd->freezing_rain_intensity > 0) {
            g_string_append_printf(tooltip_str, "\nFreezing rain: %.1f mm/hr", wd->freezing_rain_intensity);
        }
        
        if (wd->precipitation_accumulation > 0) {
            g_string_append_printf(tooltip_str, "\nPrecipitation: %.1f mm", wd->precipitation_accumulation);
        }
        
        gtk_widget_set_tooltip_text(weather_applet->container, tooltip_str->str);
        g_string_free(tooltip_str, TRUE);
        log_info("Display: Updated successfully with weather icon");
    } else {
        log_info("Display: No valid data or temp=-999, showing error state");
        gtk_label_set_text(GTK_LABEL(weather_applet->weather_icon), "⚠️");
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