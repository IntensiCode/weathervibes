#include "weather_update.h"
#include "weather_applet.h"
#include "weather_display.h"
#include "weather_fetcher.h"
#include "async_fetch.h"
#include "app.h"
#include "logger.h"
#include <gtk/gtk.h>

// External global context
extern AppContext *g_app_context;

// Callback for when async weather fetch completes
static void on_weather_fetch_complete(GObject *source_object,
                                     GAsyncResult *result,
                                     gpointer user_data) {
    WeatherApplet *weather_applet = (WeatherApplet *)user_data;
    GError *error = NULL;
    
    // Check if applet still exists (might have been destroyed)
    if (!weather_applet || !GTK_IS_WIDGET(weather_applet->applet)) {
        log_warn("Weather applet destroyed before async fetch completed");
        return;
    }
    
    // Get the result
    WeatherData *weather_data = async_fetch_weather_finish(result, &error);
    
    if (error) {
        if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            log_info("Weather fetch was cancelled");
        } else {
            log_error("Weather fetch failed: %s", error->message);
            
            // Clear display to show error
            g_mutex_lock(&weather_applet->data_mutex);
            if (weather_applet->current_weather) {
                weather_fetcher_free_data(weather_applet->current_weather);
                weather_applet->current_weather = NULL;
            }
            g_mutex_unlock(&weather_applet->data_mutex);
            
            // Update display on main thread
            g_idle_add((GSourceFunc)update_display, weather_applet);
        }
        g_error_free(error);
        
        // Clear the cancellable reference
        if (weather_applet->fetch_cancellable) {
            g_object_unref(weather_applet->fetch_cancellable);
            weather_applet->fetch_cancellable = NULL;
        }
        return;
    }
    
    if (weather_data) {
        log_info("Async weather fetch successful: temp=%.1f, city=%s",
                weather_data->temperature,
                weather_data->city ? weather_data->city : "Unknown");
        
        // Update the applet's weather data
        g_mutex_lock(&weather_applet->data_mutex);
        if (weather_applet->current_weather) {
            weather_fetcher_free_data(weather_applet->current_weather);
        }
        weather_applet->current_weather = weather_data;
        
        // Also update global context if available
        if (g_app_context) {
            g_mutex_lock(&g_app_context->data_mutex);
            if (g_app_context->weather_data) {
                weather_fetcher_free_data(g_app_context->weather_data);
            }
            g_app_context->weather_data = weather_fetcher_copy_data(weather_data);
            g_app_context->last_fetch_time[weather_applet->config->provider] = time(NULL);
            g_mutex_unlock(&g_app_context->data_mutex);
        }
        g_mutex_unlock(&weather_applet->data_mutex);
        
        // Update display on main thread
        g_idle_add((GSourceFunc)update_display, weather_applet);
    }
    
    // Clear the cancellable reference
    if (weather_applet->fetch_cancellable) {
        g_object_unref(weather_applet->fetch_cancellable);
        weather_applet->fetch_cancellable = NULL;
    }
}

gboolean update_weather(gpointer data) {
    WeatherApplet *weather_applet = (WeatherApplet *)data;
    
    log_info("=== update_weather (async) called, weather_applet=%p ===", weather_applet);
    
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
    
    // Cancel any existing fetch
    if (weather_applet->fetch_cancellable) {
        log_info("Cancelling existing weather fetch");
        g_cancellable_cancel(weather_applet->fetch_cancellable);
        g_object_unref(weather_applet->fetch_cancellable);
    }
    
    // Note: Cache checking is handled by weather_fetcher_update_with_provider()
    // which uses WEATHER_CACHE_TIMEOUT_SECONDS (60 seconds)
    
    log_info("Starting async weather fetch for %s with provider %d", 
             weather_applet->config->city, weather_applet->config->provider);
    
    // Create new cancellable for this fetch
    weather_applet->fetch_cancellable = g_cancellable_new();
    
    // Get the appropriate API key
    const char *api_key = NULL;
    if (weather_applet->config->provider == PROVIDER_OPENWEATHER) {
        api_key = weather_applet->config->openweather_api_key;
    } else if (weather_applet->config->provider == PROVIDER_TOMORROW) {
        api_key = weather_applet->config->tomorrow_api_key;
    }
    
    // Start async fetch
    async_fetch_weather(weather_applet->config->provider,
                       weather_applet->config->city,
                       api_key,
                       weather_applet->fetch_cancellable,
                       on_weather_fetch_complete,
                       weather_applet);
    
    return TRUE; // Continue timer
}