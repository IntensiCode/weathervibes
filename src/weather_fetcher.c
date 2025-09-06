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
    
    // Check if city has changed - must be done inside mutex
    gboolean city_changed = FALSE;
    g_mutex_lock(&g_app_context->data_mutex);
    if (g_app_context->cached_city == NULL || 
        g_strcmp0(g_app_context->cached_city, city) != 0) {
        city_changed = TRUE;
        log_debug("City changed from '%s' to '%s' - clearing all provider caches", 
                  g_app_context->cached_city ? g_app_context->cached_city : "NULL", city);
        
        // Clear all provider caches when city changes
        for (int i = 0; i < PROVIDER_COUNT; i++) {
            if (g_app_context->provider_cache[i]) {
                weather_data_free(g_app_context->provider_cache[i]);
                g_app_context->provider_cache[i] = NULL;
            }
            g_app_context->last_fetch_time[i] = 0;
        }
    }
    g_mutex_unlock(&g_app_context->data_mutex);
    
    if (!city_changed && last_fetch > 0 && (now - last_fetch) < 60) {
        // Check if we have cached data for THIS provider - must lock for safe access
        g_mutex_lock(&g_app_context->data_mutex);
        if (g_app_context->provider_cache[provider] != NULL) {
            log_debug("Using cached data for provider %d - less than 1 minute old (%ld seconds)",
                      provider, (now - last_fetch));
            
            // Update global weather_data from provider cache
            if (g_app_context->weather_data) {
                weather_data_free(g_app_context->weather_data);
            }
            g_app_context->weather_data = weather_fetcher_copy_data(g_app_context->provider_cache[provider]);
            g_app_context->cached_data_provider = provider;
            g_mutex_unlock(&g_app_context->data_mutex);
            
            return TRUE;  // Return success with cached data
        } else {
            g_mutex_unlock(&g_app_context->data_mutex);
            log_debug("Cache time valid but no cached data for provider %d - fetching", provider);
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
        
        // Update provider-specific cache
        if (g_app_context->provider_cache[provider]) {
            weather_data_free(g_app_context->provider_cache[provider]);
        }
        g_app_context->provider_cache[provider] = weather_fetcher_copy_data(new_data);
        
        // Update global display data
        if (g_app_context->weather_data) {
            weather_data_free(g_app_context->weather_data);
        }
        g_app_context->weather_data = new_data;
        
        // Update cache metadata - MUST be inside mutex lock
        g_app_context->last_fetch_time[provider] = now;
        g_app_context->cached_data_provider = provider;
        
        // Update cached city - MUST be inside mutex lock
        g_free(g_app_context->cached_city);
        g_app_context->cached_city = g_strdup(city);
        
        g_mutex_unlock(&g_app_context->data_mutex);
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
    copy->condition = data->condition;
    
    // Copy rain/precipitation data
    copy->precipitation_probability = data->precipitation_probability;
    copy->rain_intensity = data->rain_intensity;
    copy->snow_intensity = data->snow_intensity;
    copy->sleet_intensity = data->sleet_intensity;
    copy->freezing_rain_intensity = data->freezing_rain_intensity;
    copy->precipitation_accumulation = data->precipitation_accumulation;
    
    if (data->condition_text) copy->condition_text = g_strdup(data->condition_text);
    if (data->city) copy->city = g_strdup(data->city);
    if (data->wind_direction) copy->wind_direction = g_strdup(data->wind_direction);
    if (data->sunrise) copy->sunrise = g_strdup(data->sunrise);
    if (data->sunset) copy->sunset = g_strdup(data->sunset);
    if (data->raw_output) copy->raw_output = g_strdup(data->raw_output);
    if (data->error_message) copy->error_message = g_strdup(data->error_message);
    
    // Copy forecast data
    if (data->forecast && data->forecast_days > 0) {
        copy->forecast_days = data->forecast_days;
        copy->forecast = g_new0(ForecastDay, data->forecast_days);
        for (int i = 0; i < data->forecast_days; i++) {
            copy->forecast[i].date = data->forecast[i].date;
            copy->forecast[i].temp_min = data->forecast[i].temp_min;
            copy->forecast[i].temp_max = data->forecast[i].temp_max;
            copy->forecast[i].condition = data->forecast[i].condition;
            copy->forecast[i].precipitation_probability = data->forecast[i].precipitation_probability;
            copy->forecast[i].precipitation_amount = data->forecast[i].precipitation_amount;
            if (data->forecast[i].condition_text) {
                copy->forecast[i].condition_text = g_strdup(data->forecast[i].condition_text);
            }
        }
    }
    
    // Copy hourly forecast data
    if (data->hourly_forecast && data->hourly_count > 0) {
        copy->hourly_count = data->hourly_count;
        copy->hourly_forecast = g_new0(HourlyData, data->hourly_count);
        for (int i = 0; i < data->hourly_count; i++) {
            copy->hourly_forecast[i] = data->hourly_forecast[i];
        }
    }
    
    return copy;
}