#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include "src/app.h"
#include "src/weather_fetcher.h"
#include "src/config.h"
#include "src/weather_provider.h"
#include "src/logger.h"

AppContext *g_app_context = NULL;
volatile int keep_running = 1;

void handle_sigint(int sig) {
    keep_running = 0;
}

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
    g_app_context->config->tomorrow_api_key = g_strdup(getenv("TOMORROW_API_KEY"));
    
    signal(SIGINT, handle_sigint);
    
    printf("Starting rapid provider switching test for %s\n", city);
    printf("Press Ctrl+C to stop\n\n");
    
    int iteration = 0;
    WeatherProvider providers[] = {
        PROVIDER_ANSIWEATHER,
        PROVIDER_BRIGHTSKY,
        PROVIDER_OPENWEATHER,
        PROVIDER_TOMORROW
    };
    const char *provider_names[] = {
        "AnsiWeather",
        "BrightSky",
        "OpenWeather",
        "Tomorrow.io"
    };
    
    while (keep_running && iteration < 100) {
        for (int i = 0; i < 4 && keep_running; i++) {
            WeatherProvider provider = providers[i];
            printf("[%3d] Switching to %s...", iteration, provider_names[i]);
            fflush(stdout);
            
            // Switch provider
            g_app_context->config->provider = provider;
            
            // Fetch with new provider
            gboolean success = weather_fetcher_update(city, provider);
            
            if (success && g_app_context->weather_data) {
                printf(" OK (temp=%.1f°C)\n", g_app_context->weather_data->temperature);
            } else {
                printf(" FAILED\n");
            }
            
            // Small delay to simulate user interaction
            usleep(100000); // 100ms
            
            iteration++;
        }
    }
    
    printf("\nTest completed: %d provider switches without crash!\n", iteration);
    
    // Cleanup
    g_mutex_lock(&g_app_context->data_mutex);
    for (int i = 0; i < PROVIDER_COUNT; i++) {
        if (g_app_context->provider_cache[i]) {
            weather_fetcher_free_data(g_app_context->provider_cache[i]);
        }
    }
    if (g_app_context->weather_data) {
        weather_fetcher_free_data(g_app_context->weather_data);
    }
    g_free(g_app_context->cached_city);
    g_mutex_unlock(&g_app_context->data_mutex);
    
    g_mutex_clear(&g_app_context->data_mutex);
    g_free(g_app_context->config->city);
    g_free(g_app_context->config->openweather_api_key);
    g_free(g_app_context->config->tomorrow_api_key);
    g_free(g_app_context->config);
    g_free(g_app_context);
    logger_cleanup();
    
    return 0;
}
