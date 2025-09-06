/* Simple test of OpenWeather provider */
#include <gtk/gtk.h>
#include <stdio.h>
#include "src/app.h"
#include "src/weather_provider.h"
#include "src/logger.h"

// Define g_app_context here
AppContext *g_app_context = NULL;

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    
    // Initialize logging
    logger_init();
    log_info("Simple OpenWeather test starting");
    
    // Initialize global app context
    g_app_context = g_new0(AppContext, 1);
    g_app_context->config = g_new0(AppConfig, 1);
    const char *openweather_key = getenv("OPENWEATHER_API_KEY");
    g_app_context->config->openweather_api_key = openweather_key ? g_strdup(openweather_key) : NULL;
    
    log_info("API Key set: %s", g_app_context->config->openweather_api_key);
    
    // Get OpenWeather provider
    const WeatherProviderInterface *providers[] = {
        &ansiweather_provider,
        &brightsky_provider,
        &openweather_provider
    };
    
    const WeatherProviderInterface *openweather = providers[PROVIDER_OPENWEATHER];
    
    log_info("Using provider: %s", openweather->name);
    
    // Test fetching weather
    WeatherData *data = NULL;
    const char *city = "Geisenheim,DE";
    
    log_info("Fetching weather for %s...", city);
    gboolean success = openweather->fetch_weather(city, &data);
    
    if (success && data) {
        printf("\n=== SUCCESS ===\n");
        printf("City: %s\n", data->city);
        printf("Temperature: %.1f°C\n", data->temperature);
        printf("Condition: %s %s\n", data->condition_icon, data->condition_text);
        printf("Humidity: %d%%\n", data->humidity);
        
        log_info("Fetch successful: %.1f°C", data->temperature);
    } else {
        printf("\n=== FAILED ===\n");
        if (data && data->error_message) {
            printf("Error: %s\n", data->error_message);
        }
        log_error("Fetch failed");
    }
    
    // Check the log
    printf("\nCheck /tmp/weather.log for details\n");
    
    logger_cleanup();
    return success ? 0 : 1;
}