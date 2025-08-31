#include "weather_fetcher.h"
#include "weather_provider.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

gboolean weather_fetcher_update_with_provider(const char *city, WeatherProvider provider) {
    if (!g_app_context) return FALSE;
    
    // Initialize providers if not done already
    static gboolean providers_initialized = FALSE;
    if (!providers_initialized) {
        weather_provider_init();
        providers_initialized = TRUE;
    }
    
    // Check cache: don't fetch if we've fetched this provider within the last minute
    // AND we actually have weather data stored
    // AND the city hasn't changed
    time_t now = time(NULL);
    time_t last_fetch = g_app_context->last_fetch_time[provider];
    
    // Check if city has changed
    gboolean city_changed = FALSE;
    if (g_app_context->cached_city == NULL || 
        g_strcmp0(g_app_context->cached_city, city) != 0) {
        city_changed = TRUE;
        log_debug("City changed from '%s' to '%s' - forcing refresh", 
                  g_app_context->cached_city ? g_app_context->cached_city : "NULL", city);
    }
    
    if (!city_changed && last_fetch > 0 && (now - last_fetch) < 60) {
        // Only skip fetch if we actually have data AND it's from this provider AND same city
        if (g_app_context->weather_data != NULL && 
            g_app_context->cached_data_provider == provider) {
            log_debug("Skipping fetch for provider %d - cached data is less than 1 minute old (%ld seconds)",
                      provider, (now - last_fetch));
            return TRUE;  // Return success but don't fetch
        } else {
            log_debug("Cache time valid but no data for provider %d (current data from provider %d) - fetching anyway", 
                      provider, g_app_context->cached_data_provider);
        }
    }
    
    // Get the provider interface
    const WeatherProviderInterface* provider_interface = weather_provider_get(provider);
    if (!provider_interface || !provider_interface->fetch_weather) {
        g_warning("Invalid provider %d or no fetch function", provider);
        return FALSE;
    }
    
    log_info("weather_fetcher: Using provider %d (%s) for city %s", 
            provider, weather_provider_get_name(provider), city);
    if (provider == PROVIDER_OPENWEATHER && g_app_context && g_app_context->config) {
        log_debug("weather_fetcher: OpenWeather API key configured: %s", 
                g_app_context->config->openweather_api_key ? "yes" : "no");
    }
    
    // Fetch weather data using the provider
    WeatherData *new_data = NULL;
    gboolean success = provider_interface->fetch_weather(city, &new_data);
    
    if (success && new_data) {
        g_mutex_lock(&g_app_context->data_mutex);
        if (g_app_context->weather_data) {
            weather_data_free(g_app_context->weather_data);
        }
        g_app_context->weather_data = new_data;
        g_mutex_unlock(&g_app_context->data_mutex);
        
        // Update cache timestamp, provider tracking, and cached city on successful fetch
        g_app_context->last_fetch_time[provider] = now;
        g_app_context->cached_data_provider = provider;
        
        // Update cached city
        g_free(g_app_context->cached_city);
        g_app_context->cached_city = g_strdup(city);
    }
    
    return success;
}

gboolean weather_fetcher_update(const char *city, WeatherProvider provider) {
    log_info("weather_fetcher_update: Using provider %d for city %s", provider, city);
    return weather_fetcher_update_with_provider(city, provider);
}

void weather_fetcher_free_data(WeatherData *data) {
    weather_data_free(data);
}

WeatherData* weather_fetcher_copy_data(const WeatherData *data) {
    if (!data) return NULL;
    
    WeatherData *copy = weather_data_new();
    copy->temperature = data->temperature;
    copy->feels_like = data->feels_like;
    copy->uvi = data->uvi;
    copy->wind_speed = data->wind_speed;
    copy->humidity = data->humidity;
    copy->pressure = data->pressure;
    copy->last_update = data->last_update;
    
    if (data->condition_icon) copy->condition_icon = g_strdup(data->condition_icon);
    if (data->condition_text) copy->condition_text = g_strdup(data->condition_text);
    if (data->city) copy->city = g_strdup(data->city);
    if (data->wind_direction) copy->wind_direction = g_strdup(data->wind_direction);
    if (data->sunrise) copy->sunrise = g_strdup(data->sunrise);
    if (data->sunset) copy->sunset = g_strdup(data->sunset);
    if (data->raw_output) copy->raw_output = g_strdup(data->raw_output);
    if (data->error_message) copy->error_message = g_strdup(data->error_message);
    
    return copy;
}