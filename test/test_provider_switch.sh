#!/bin/bash
# Stress test for provider switching crashes

echo "Provider Switching Stress Test"
echo "=============================="
echo "This test rapidly switches between providers to detect crashes"
echo ""

# Source environment for API keys
source .envrc

# Build test program that switches providers rapidly
cat > test_provider_switch.c << 'EOFC'
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
EOFC

# Build the test
echo "Building test program..."
PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig gcc -Wall -g \
    test_provider_switch.c \
    src/weather_fetcher.c \
    src/config.c \
    src/weather_provider.c \
    src/weather_conditions.c \
    src/provider_ansiweather.c \
    src/provider_brightsky.c \
    src/provider_openweather.c \
    src/provider_tomorrow.c \
    src/network.c \
    src/logger.c \
    src/geocoding.c \
    src/geoip.c \
    src/json_helpers.c \
    -I/usr/include/glib-2.0 \
    -I/usr/lib/x86_64-linux-gnu/glib-2.0/include \
    -I/usr/include/json-glib-1.0 \
    -I/usr/include/libsoup-2.4 \
    -I/usr/include/libxml2 \
    -lgio-2.0 -lgobject-2.0 -lglib-2.0 -ljson-glib-1.0 -lsoup-2.4 -lm \
    -o test_provider_switch 2>/dev/null

if [ $? -ne 0 ]; then
    echo "Build failed. Trying simpler test..."
    exit 1
fi

echo "Running stress test..."
echo ""

# Run the test
./test_provider_switch "$@"

# Check exit code
if [ $? -eq 0 ]; then
    echo ""
    echo "✓ Test passed! No crashes detected during rapid provider switching."
else
    echo ""
    echo "✗ Test failed or crashed. Check dmesg for segfault messages."
    echo "Recent kernel messages:"
    dmesg | tail -5
fi

# Clean up
rm -f test_provider_switch.c test_provider_switch
