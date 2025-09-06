#include <stdio.h>
#include <unistd.h>
#include <glib.h>
#include "src/app.h"
#include "src/weather_fetcher.h"
#include "src/config.h"

// Global app context
AppContext *g_app_context = NULL;

int main() {
    printf("Weather Provider Caching Test\n");
    printf("==============================\n\n");
    
    // Initialize global context
    g_app_context = g_new0(AppContext, 1);
    g_app_context->config = g_new0(AppConfig, 1);
    g_app_context->config->city = g_strdup("Berlin");
    g_app_context->config->use_celsius = TRUE;
    g_app_context->config->update_interval_minutes = 30;
    g_mutex_init(&g_app_context->data_mutex);
    
    // Test 1: Fetch with AnsiWeather
    printf("1. First fetch with AnsiWeather:\n");
    gboolean success = weather_fetcher_update_with_provider("Berlin", PROVIDER_ANSIWEATHER);
    printf("   Result: %s\n", success ? "SUCCESS" : "FAILED");
    if (g_app_context->weather_data) {
        printf("   Temperature: %.1f°C\n", g_app_context->weather_data->temperature);
    }
    
    // Test 2: Try to fetch again immediately (should be cached)
    printf("\n2. Second fetch with AnsiWeather (should be cached):\n");
    success = weather_fetcher_update_with_provider("Berlin", PROVIDER_ANSIWEATHER);
    printf("   Result: %s\n", success ? "SUCCESS" : "FAILED");
    
    // Test 3: Fetch with different provider (should work - different cache)
    printf("\n3. First fetch with Bright Sky (different cache):\n");
    success = weather_fetcher_update_with_provider("Berlin", PROVIDER_BRIGHTSKY);
    printf("   Result: %s\n", success ? "SUCCESS" : "FAILED");
    if (g_app_context->weather_data) {
        printf("   Temperature: %.1f°C\n", g_app_context->weather_data->temperature);
    }
    
    // Test 4: Try Bright Sky again (should be cached)
    printf("\n4. Second fetch with Bright Sky (should be cached):\n");
    success = weather_fetcher_update_with_provider("Berlin", PROVIDER_BRIGHTSKY);
    printf("   Result: %s\n", success ? "SUCCESS" : "FAILED");
    
    // Test 5: Wait 61 seconds and try again
    printf("\n5. Waiting 61 seconds for cache to expire...\n");
    sleep(61);
    
    printf("6. Fetch with AnsiWeather after cache expiry:\n");
    success = weather_fetcher_update_with_provider("Berlin", PROVIDER_ANSIWEATHER);
    printf("   Result: %s\n", success ? "SUCCESS" : "FAILED");
    if (g_app_context->weather_data) {
        printf("   Temperature: %.1f°C\n", g_app_context->weather_data->temperature);
    }
    
    // Cleanup
    g_free(g_app_context->config->city);
    g_free(g_app_context->config);
    g_mutex_clear(&g_app_context->data_mutex);
    g_free(g_app_context);
    
    return 0;
}