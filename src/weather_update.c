#include "weather_update.h"
#include "weather_applet.h"
#include "weather_display.h"
#include "weather_fetcher.h"
#include "app.h"
#include "logger.h"

// External global context
extern AppContext *g_app_context;

gboolean update_weather(gpointer data) {
    WeatherApplet *weather_applet = (WeatherApplet *)data;
    
    log_info("=== update_weather called, weather_applet=%p ===", weather_applet);
    
    if (!weather_applet) {
        log_error("update_weather: weather_applet is NULL!");
        return TRUE;
    }
    
    if (!weather_applet->config) {
        log_error("update_weather: config is NULL!");
        return TRUE;
    }
    
    if (!weather_applet->config->city) {
        log_error("update_weather: city is NULL!");
        return TRUE;
    }
    
    if (weather_applet->config && weather_applet->config->city) {
        log_info("Updating weather for %s with provider %d", 
                 weather_applet->config->city, weather_applet->config->provider);
        
        gboolean success = weather_fetcher_update(weather_applet->config->city, weather_applet->config->provider);
        
        log_info("Update result: %s, have data: %s", 
                 success ? "success" : "failed",
                 (g_app_context && g_app_context->weather_data) ? "yes" : "no");
        
        // Update current_weather only when there's new data or on explicit failure
        if (g_app_context && g_app_context->weather_data) {
            // We have fresh data - update our display copy
            g_mutex_lock(&weather_applet->data_mutex);
            if (weather_applet->current_weather) {
                weather_fetcher_free_data(weather_applet->current_weather);
            }
            weather_applet->current_weather = weather_fetcher_copy_data(g_app_context->weather_data);
            g_mutex_unlock(&weather_applet->data_mutex);
            
            log_info("Updated current_weather: temp=%.1f, city=%s, condition=%s",
                    weather_applet->current_weather ? weather_applet->current_weather->temperature : -999.0,
                    weather_applet->current_weather && weather_applet->current_weather->city ? 
                        weather_applet->current_weather->city : "NULL",
                    weather_applet->current_weather && weather_applet->current_weather->condition_text ? 
                        weather_applet->current_weather->condition_text : "NULL");
            
            update_display(weather_applet);
        } else if (!success) {
            // Fetch explicitly failed - clear display to show error
            g_mutex_lock(&weather_applet->data_mutex);
            if (weather_applet->current_weather) {
                weather_fetcher_free_data(weather_applet->current_weather);
                weather_applet->current_weather = NULL;
            }
            g_mutex_unlock(&weather_applet->data_mutex);
            
            log_info("Fetch failed - clearing current_weather");
            update_display(weather_applet);
        } else {
            // If success but no g_app_context->weather_data, it's a cache hit saying "too soon"
            // Keep the current display as-is (don't call update_display)
            log_info("Cache hit - keeping existing display (temp=%.1f)", 
                    weather_applet->current_weather ? weather_applet->current_weather->temperature : -999.0);
        }
    }
    
    return TRUE; // Continue timer
}