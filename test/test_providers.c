#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "../src/app.h"
#include "../src/weather_fetcher.h"
#include "../src/config.h"
#include "../src/weather_provider.h"
#include "../src/weather_conditions.h"
#include "../src/logger.h"

// Global app context
AppContext *g_app_context = NULL;

void print_weather_data(WeatherData *data, const char *provider_name) {
    printf("\n=== %s ===\n", provider_name);
    if (!data) {
        printf("No data received!\n");
        return;
    }
    
    printf("City: %s\n", data->city ? data->city : "Unknown");
    printf("Temperature: %.1f°C\n", data->temperature);
    printf("Feels like: %.1f°C\n", data->feels_like);
    printf("Condition: %s %s (enum: %d)\n", 
           weather_condition_get_emoji(data->condition),
           data->condition_text ? data->condition_text : "Unknown",
           data->condition);
    printf("Humidity: %d%%\n", data->humidity);
    printf("Wind: %.1f m/s %s\n", data->wind_speed, 
           data->wind_direction ? data->wind_direction : "");
    printf("Pressure: %d hPa\n", data->pressure);
    printf("UV Index: %.1f\n", data->uvi);
    
    // Always display rain probability if available
    if (data->precipitation_probability >= 0) {
        printf("Rain chance: %.0f%%\n", data->precipitation_probability);
    }
    // Only display non-zero intensity values
    if (data->rain_intensity > 0) {
        printf("Rain: %.1f mm/hr\n", data->rain_intensity);
    }
    if (data->snow_intensity > 0) {
        printf("Snow: %.1f mm/hr\n", data->snow_intensity);
    }
    if (data->sleet_intensity > 0) {
        printf("Sleet: %.1f mm/hr\n", data->sleet_intensity);
    }
    if (data->freezing_rain_intensity > 0) {
        printf("Freezing rain: %.1f mm/hr\n", data->freezing_rain_intensity);
    }
    if (data->precipitation_accumulation > 0) {
        printf("Precipitation: %.1f mm\n", data->precipitation_accumulation);
    }
    
    if (data->sunrise) printf("Sunrise: %s\n", data->sunrise);
    if (data->sunset) printf("Sunset: %s\n", data->sunset);
    
    printf("Last update: %s", ctime(&data->last_update));
}

void test_provider(const char *city, WeatherProvider provider) {
    printf("\nTesting %s for city: %s\n", weather_provider_get_name(provider), city);
    printf("Fetching weather data...\n");
    
    gboolean success = weather_fetcher_update_with_provider(city, provider);
    
    if (success && g_app_context->weather_data) {
        print_weather_data(g_app_context->weather_data, weather_provider_get_name(provider));
    } else {
        printf("Failed to fetch weather data from %s\n", weather_provider_get_name(provider));
    }
    
    // Clean up for next test
    if (g_app_context->weather_data) {
        weather_fetcher_free_data(g_app_context->weather_data);
        g_app_context->weather_data = NULL;
    }
}

int main(int argc, char *argv[]) {
    const char *city = "Berlin";
    
    if (argc > 1) {
        city = argv[1];
    }
    
    // Initialize logging
    logger_init();
    log_info("Weather provider test program started");
    
    printf("Weather Provider Test Program\n");
    printf("==============================\n");
    printf("Testing weather providers for: %s\n", city);
    printf("(You can specify a different city as argument)\n");
    
    // Initialize global context
    g_app_context = g_new0(AppContext, 1);
    g_app_context->config = g_new0(AppConfig, 1);
    g_app_context->config->city = g_strdup(city);
    g_app_context->config->use_celsius = TRUE;
    g_app_context->config->update_interval_minutes = 30;
    // Get API keys from environment variables (for CI testing)
    const char *openweather_key = getenv("OPENWEATHER_API_KEY");
    const char *tomorrow_key = getenv("TOMORROW_API_KEY");
    
    // Only set if environment variables exist
    g_app_context->config->openweather_api_key = openweather_key ? g_strdup(openweather_key) : NULL;
    g_app_context->config->tomorrow_api_key = tomorrow_key ? g_strdup(tomorrow_key) : NULL;
    
    // Warn if API keys are missing
    if (!openweather_key) {
        printf("Warning: OPENWEATHER_API_KEY not set - OpenWeather provider will fail\n");
    }
    if (!tomorrow_key) {
        printf("Warning: TOMORROW_API_KEY not set - Tomorrow.io provider will fail\n");
    }
    g_mutex_init(&g_app_context->data_mutex);
    
    // Initialize providers
    weather_provider_init();
    
    // Test each provider
    for (int i = 0; i < PROVIDER_COUNT; i++) {
        printf("\n%d. Testing %s...\n", i + 1, weather_provider_get_name((WeatherProvider)i));
        test_provider(city, (WeatherProvider)i);
        if (i < PROVIDER_COUNT - 1) {
            sleep(1); // Small delay between requests
        }
    }
    
    // Summary
    printf("\n==============================\n");
    printf("Test complete!\n");
    printf("\nNote:\n");
    for (int i = 0; i < PROVIDER_COUNT; i++) {
        printf("- %s: %s\n", weather_provider_get_name((WeatherProvider)i), 
               weather_provider_get_description((WeatherProvider)i));
    }
    printf("- All providers use OpenStreetMap geocoding for any city worldwide\n");
    
    // Cleanup
    g_free(g_app_context->config->city);
    g_free(g_app_context->config->openweather_api_key);
    g_free(g_app_context->config->tomorrow_api_key);
    g_free(g_app_context->config);
    g_free(g_app_context->cached_city);
    g_mutex_clear(&g_app_context->data_mutex);
    g_free(g_app_context);
    
    log_info("Weather provider test program finished");
    logger_cleanup();
    
    return 0;
}