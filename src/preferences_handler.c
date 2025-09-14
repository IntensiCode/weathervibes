#include "preferences_handler.h"
#include "weather_applet.h"
#include "applet_config.h"
#include "weather_fetcher.h"
#include "weather_update.h"
#include "weather_display.h"
#include "app.h"
#include "logger.h"
#include "weather_resume.h"

// External global context
extern AppContext *g_app_context;

// Handle provider change - clear data and invalidate cache
static void handle_provider_change(WeatherApplet *weather_applet, int old_provider, int new_provider) {
    if (old_provider == new_provider) {
        return;
    }
    
    // Clear current weather data
    g_mutex_lock(&weather_applet->data_mutex);
    if (weather_applet->current_weather) {
        weather_fetcher_free_data(weather_applet->current_weather);
        weather_applet->current_weather = NULL;
    }
    g_mutex_unlock(&weather_applet->data_mutex);
    
    // Clear global weather data and invalidate cache for the old provider
    if (g_app_context) {
        g_mutex_lock(&g_app_context->data_mutex);
        if (g_app_context->weather_data) {
            weather_fetcher_free_data(g_app_context->weather_data);
            g_app_context->weather_data = NULL;
        }
        // Don't clear cached_city - it's still the same city, just different provider
        // Clear the old provider's cache to prevent stale data issues  
        if (g_app_context->provider_cache[old_provider]) {
            weather_fetcher_free_data(g_app_context->provider_cache[old_provider]);
            g_app_context->provider_cache[old_provider] = NULL;
        }
        g_app_context->last_fetch_time[old_provider] = 0;
        g_mutex_unlock(&g_app_context->data_mutex);
    }
    
    // Update display to show new state
    update_display(weather_applet);
}

// Handle timer restart with new interval
static void restart_update_timer(WeatherApplet *weather_applet, int interval_minutes) {
    if (weather_applet->update_timer) {
        weather_resume_timer_stop(weather_applet->update_timer);
    }
    
    weather_applet->update_timer = weather_resume_timer_start(
        weather_applet,
        interval_minutes);
}

void handle_preferences_save(WeatherApplet *weather_applet, 
                            const char *location,
                            int provider,
                            const char *api_key,
                            gboolean use_celsius,
                            int interval_index) {
    
    // Map combo box index to minutes
    int interval_minutes[] = {5, 10, 15, 30, 60};
    int new_interval = interval_minutes[interval_index];
    
    int old_provider = weather_applet->config->provider;
    
    // Update the applet's configuration
    applet_config_update(weather_applet->config, location, provider, 
                         api_key, use_celsius, new_interval);
    
    // Sync with global configuration
    applet_config_sync_to_global(weather_applet->config);
    
    // Handle provider change if needed
    handle_provider_change(weather_applet, old_provider, provider);
    
    // Restart timer if interval changed
    if (weather_applet->config->update_interval_minutes != new_interval) {
        weather_applet->config->update_interval_minutes = new_interval;
        restart_update_timer(weather_applet, new_interval);
    }
    
    // Trigger immediate update
    update_weather(weather_applet);
}