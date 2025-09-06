#include <stdio.h>
#include "src/app.h"
#include "src/weather_fetcher.h"
#include "src/config.h"
#include "src/weather_provider.h"
#include "src/logger.h"

AppContext *g_app_context = NULL;

int main(int argc, char *argv[]) {
    const char *city = (argc > 1) ? argv[1] : "Berlin";
    
    // Initialize
    logger_init();
    g_app_context = g_new0(AppContext, 1);
    g_app_context->config = g_new0(AppConfig, 1);
    g_mutex_init(&g_app_context->data_mutex);
    
    // Set up config
    g_app_context->config->city = g_strdup(city);
    g_app_context->config->use_celsius = TRUE;
    g_app_context->config->openweather_api_key = g_strdup(getenv("OPENWEATHER_API_KEY"));
    g_app_context->config->provider = PROVIDER_OPENWEATHER;
    
    printf("Fetching weather and forecast for %s...\n", city);
    
    // Fetch weather with forecast
    gboolean success = weather_fetcher_update(city, PROVIDER_OPENWEATHER);
    
    if (success && g_app_context->weather_data) {
        WeatherData *data = g_app_context->weather_data;
        printf("\nCurrent weather: %.1f°C, %s\n", 
               data->temperature, 
               data->condition_text ? data->condition_text : "Unknown");
        
        if (data->forecast && data->forecast_days > 0) {
            printf("\n5-Day Forecast:\n");
            for (int i = 0; i < data->forecast_days && i < 5; i++) {
                struct tm *tm = localtime(&data->forecast[i].date);
                char date_str[20];
                strftime(date_str, sizeof(date_str), "%a %b %d", tm);
                printf("  %s: %.0f°/%.0f°C\n", 
                       date_str,
                       data->forecast[i].temp_max,
                       data->forecast[i].temp_min);
            }
        } else {
            printf("\nNo forecast data available.\n");
        }
    } else {
        printf("Failed to fetch weather data.\n");
    }
    
    // Cleanup
    if (g_app_context->weather_data) {
        weather_fetcher_free_data(g_app_context->weather_data);
    }
    g_mutex_clear(&g_app_context->data_mutex);
    g_free(g_app_context->config->city);
    g_free(g_app_context->config->openweather_api_key);
    g_free(g_app_context->config);
    g_free(g_app_context);
    logger_cleanup();
    
    return 0;
}
