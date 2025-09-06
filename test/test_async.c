#include <stdio.h>
#include <glib.h>
#include <gio/gio.h>
#include "../src/async_fetch.h"
#include "../src/app.h"
#include "../src/config.h"
#include "../src/logger.h"
#include "../src/weather_provider.h"

// Global app context
AppContext *g_app_context = NULL;
static GMainLoop *loop = NULL;
static gboolean test_passed = FALSE;

// Callback for async URL fetch
static void on_url_fetched(GObject *source, GAsyncResult *result, gpointer user_data) {
    GError *error = NULL;
    char *response = async_fetch_finish(result, &error);
    
    if (error) {
        printf("❌ URL fetch failed: %s\n", error->message);
        g_error_free(error);
    } else if (response) {
        printf("✅ URL fetch succeeded, got %zu bytes\n", strlen(response));
        // Check if we got JSON-like content
        if (strstr(response, "{") && strstr(response, "}")) {
            printf("✅ Response looks like valid JSON\n");
            test_passed = TRUE;
        } else {
            printf("❌ Response doesn't look like JSON\n");
        }
        g_free(response);
    } else {
        printf("❌ Got NULL response\n");
    }
    
    g_main_loop_quit(loop);
}

// Callback for async weather fetch
static void on_weather_fetched(GObject *source, GAsyncResult *result, gpointer user_data) {
    GError *error = NULL;
    WeatherData *data = async_fetch_weather_finish(result, &error);
    
    if (error) {
        printf("❌ Weather fetch failed: %s\n", error->message);
        g_error_free(error);
    } else if (data) {
        printf("✅ Weather fetch succeeded!\n");
        printf("   City: %s\n", data->city ? data->city : "Unknown");
        printf("   Temperature: %.1f°C\n", data->temperature);
        printf("   Condition: %s\n", data->condition_text ? data->condition_text : "Unknown");
        test_passed = TRUE;
        weather_fetcher_free_data(data);
    } else {
        printf("❌ Got NULL weather data\n");
    }
    
    g_main_loop_quit(loop);
}

void test_async_url_fetch() {
    printf("\n=== Testing Async URL Fetch ===\n");
    
    // Test fetching a simple JSON endpoint
    const char *test_url = "https://api.github.com/zen";  // GitHub's zen API - simple and reliable
    
    printf("Fetching: %s\n", test_url);
    
    test_passed = FALSE;
    loop = g_main_loop_new(NULL, FALSE);
    
    // Start async fetch
    async_fetch_url(test_url, NULL, on_url_fetched, NULL);
    
    // Run event loop with timeout
    guint timeout_id = g_timeout_add_seconds(10, (GSourceFunc)g_main_loop_quit, loop);
    g_main_loop_run(loop);
    g_source_remove(timeout_id);
    
    g_main_loop_unref(loop);
    
    if (!test_passed) {
        printf("❌ Async URL fetch test FAILED\n");
    }
}

void test_async_weather_fetch() {
    printf("\n=== Testing Async Weather Fetch ===\n");
    
    // Initialize what we need
    config_init();
    config_load();
    weather_provider_init();
    
    printf("Fetching weather for: Berlin\n");
    printf("Using provider: AnsiWeather\n");
    
    test_passed = FALSE;
    loop = g_main_loop_new(NULL, FALSE);
    
    // Start async weather fetch
    async_fetch_weather(PROVIDER_ANSIWEATHER, "Berlin", NULL, NULL, on_weather_fetched, NULL);
    
    // Run event loop with timeout
    guint timeout_id = g_timeout_add_seconds(15, (GSourceFunc)g_main_loop_quit, loop);
    g_main_loop_run(loop);
    g_source_remove(timeout_id);
    
    g_main_loop_unref(loop);
    
    if (!test_passed) {
        printf("❌ Async weather fetch test FAILED\n");
    }
}

void test_cancellation() {
    printf("\n=== Testing Cancellation ===\n");
    
    GCancellable *cancellable = g_cancellable_new();
    loop = g_main_loop_new(NULL, FALSE);
    
    printf("Starting async fetch with cancellable...\n");
    async_fetch_url("https://httpbin.org/delay/5", cancellable, on_url_fetched, NULL);
    
    // Cancel after 1 second
    printf("Cancelling after 1 second...\n");
    g_timeout_add_seconds(1, (GSourceFunc)g_cancellable_cancel, cancellable);
    
    // Quit loop after 3 seconds
    g_timeout_add_seconds(3, (GSourceFunc)g_main_loop_quit, loop);
    
    g_main_loop_run(loop);
    
    if (g_cancellable_is_cancelled(cancellable)) {
        printf("✅ Cancellation worked correctly\n");
    } else {
        printf("❌ Cancellation didn't work\n");
    }
    
    g_object_unref(cancellable);
    g_main_loop_unref(loop);
}

int main(int argc, char *argv[]) {
    // Initialize logging
    logger_init();
    
    // Initialize app context
    g_app_context = g_new0(AppContext, 1);
    g_app_context->config = g_new0(AppConfig, 1);
    g_mutex_init(&g_app_context->data_mutex);
    
    printf("Async Fetch Test Suite\n");
    printf("======================\n");
    
    // Run tests
    test_async_url_fetch();
    test_async_weather_fetch();
    test_cancellation();
    
    printf("\n======================\n");
    printf("Tests completed!\n");
    
    // Cleanup
    g_mutex_clear(&g_app_context->data_mutex);
    g_free(g_app_context->config);
    g_free(g_app_context);
    logger_cleanup();
    
    return 0;
}