#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include "src/app.h"
#include "src/config.h"
#include "src/weather_fetcher.h"
#include "src/weather_provider.h"
#include "src/logger.h"

AppContext *g_app_context = NULL;

int main(void) {
    printf("Cache Test Program - Single Process\n");
    printf("====================================\n\n");
    
    // Initialize logging
    logger_init();
    
    // Initialize global context
    g_app_context = g_new0(AppContext, 1);
    g_app_context->config = g_new0(AppConfig, 1);
    g_app_context->config->use_celsius = TRUE;
    const char *openweather_key = getenv("OPENWEATHER_API_KEY");
    const char *tomorrow_key = getenv("TOMORROW_API_KEY");
    g_app_context->config->openweather_api_key = openweather_key ? g_strdup(openweather_key) : NULL;
    g_app_context->config->tomorrow_api_key = tomorrow_key ? g_strdup(tomorrow_key) : NULL;
    g_mutex_init(&g_app_context->data_mutex);
    
    weather_provider_init();
    
    printf("Test 1: First fetch for Berlin\n");
    printf("-------------------------------\n");
    weather_fetcher_update_with_provider("Berlin", PROVIDER_ANSIWEATHER);
    printf("✓ Fetched data for Berlin\n\n");
    
    printf("Test 2: Quick refetch for Berlin (within 60 seconds)\n");
    printf("-----------------------------------------------------\n");
    printf("Waiting 2 seconds...\n");
    sleep(2);
    weather_fetcher_update_with_provider("Berlin", PROVIDER_ANSIWEATHER);
    printf("Check logs - should show cache being used\n\n");
    
    printf("Test 3: Change city to Munich\n");
    printf("------------------------------\n");
    weather_fetcher_update_with_provider("Munich", PROVIDER_ANSIWEATHER);
    printf("✓ City changed - should have forced refresh\n\n");
    
    printf("Test 4: Quick refetch for Munich (within 60 seconds)\n");
    printf("-----------------------------------------------------\n");
    printf("Waiting 2 seconds...\n");
    sleep(2);
    weather_fetcher_update_with_provider("Munich", PROVIDER_ANSIWEATHER);
    printf("Check logs - should show cache being used for Munich\n\n");
    
    printf("Test 5: Change back to Berlin\n");
    printf("------------------------------\n");
    weather_fetcher_update_with_provider("Berlin", PROVIDER_ANSIWEATHER);
    printf("✓ City changed back - should have forced refresh\n\n");
    
    printf("\nTest complete! Check /tmp/weather.log for details.\n");
    printf("Look for:\n");
    printf("- 'City changed from...' messages when switching cities\n");
    printf("- 'Skipping fetch...' messages when using cache\n");
    
    // Cleanup
    g_free(g_app_context->config->city);
    g_free(g_app_context->config->openweather_api_key);
    g_free(g_app_context->config->tomorrow_api_key);
    g_free(g_app_context->config);
    g_free(g_app_context->cached_city);
    if (g_app_context->weather_data) {
        weather_fetcher_free_data(g_app_context->weather_data);
    }
    g_mutex_clear(&g_app_context->data_mutex);
    g_free(g_app_context);
    
    logger_cleanup();
    
    return 0;
}