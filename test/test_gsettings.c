#include <stdio.h>
#include <glib.h>
#include <gio/gio.h>
#include <stdlib.h>
#include "../src/app.h"
#include "../src/config.h"
#include "../src/logger.h"

// Global app context
AppContext *g_app_context = NULL;

void test_gsettings_basic() {
    printf("Testing GSettings basic operations...\n");
    
    // Initialize config
    config_init();
    config_load();
    
    // Check defaults loaded
    printf("  Default city: %s\n", g_app_context->config->city);
    printf("  Default use_celsius: %s\n", g_app_context->config->use_celsius ? "true" : "false");
    printf("  Default interval: %d minutes\n", g_app_context->config->update_interval_minutes);
    printf("  Default provider: %d\n", g_app_context->config->provider);
    
    // Test saving new values
    g_free(g_app_context->config->city);
    g_app_context->config->city = g_strdup("Paris");
    g_app_context->config->use_celsius = FALSE;
    g_app_context->config->update_interval_minutes = 15;
    g_app_context->config->provider = 2;
    
    printf("\nSaving new configuration...\n");
    config_save();
    
    // Clear and reload to verify persistence
    g_free(g_app_context->config->city);
    g_app_context->config->city = NULL;
    
    printf("Reloading configuration...\n");
    config_load();
    
    // Check values were saved
    printf("  Loaded city: %s\n", g_app_context->config->city);
    printf("  Loaded use_celsius: %s\n", g_app_context->config->use_celsius ? "true" : "false");
    printf("  Loaded interval: %d minutes\n", g_app_context->config->update_interval_minutes);
    printf("  Loaded provider: %d\n", g_app_context->config->provider);
    
    // Verify
    if (g_strcmp0(g_app_context->config->city, "Paris") == 0 &&
        g_app_context->config->use_celsius == FALSE &&
        g_app_context->config->update_interval_minutes == 15 &&
        g_app_context->config->provider == 2) {
        printf("✅ GSettings save/load working correctly\n");
    } else {
        printf("❌ GSettings save/load FAILED\n");
    }
}

void test_api_keys() {
    printf("\nTesting API key storage...\n");
    
    // Set API keys
    g_free(g_app_context->config->openweather_api_key);
    g_app_context->config->openweather_api_key = g_strdup("test-openweather-key-123");
    
    g_free(g_app_context->config->tomorrow_api_key);
    g_app_context->config->tomorrow_api_key = g_strdup("test-tomorrow-key-456");
    
    config_save();
    
    // Clear and reload
    g_free(g_app_context->config->openweather_api_key);
    g_app_context->config->openweather_api_key = NULL;
    g_free(g_app_context->config->tomorrow_api_key);
    g_app_context->config->tomorrow_api_key = NULL;
    
    config_load();
    
    // Check
    if (g_strcmp0(g_app_context->config->openweather_api_key, "test-openweather-key-123") == 0 &&
        g_strcmp0(g_app_context->config->tomorrow_api_key, "test-tomorrow-key-456") == 0) {
        printf("✅ API key storage working correctly\n");
    } else {
        printf("❌ API key storage FAILED\n");
    }
    
    // Clear sensitive data
    g_free(g_app_context->config->openweather_api_key);
    g_app_context->config->openweather_api_key = NULL;
    g_free(g_app_context->config->tomorrow_api_key);
    g_app_context->config->tomorrow_api_key = NULL;
    config_save();
}

void test_validation() {
    printf("\nTesting value validation...\n");
    
    // Test interval clamping (should be 5-120)
    g_app_context->config->update_interval_minutes = 3;
    config_save();
    config_load();
    
    // GSettings should enforce the range
    if (g_app_context->config->update_interval_minutes >= 5) {
        printf("✅ Interval minimum enforced\n");
    } else {
        printf("⚠️  Interval minimum not enforced (got %d)\n", 
               g_app_context->config->update_interval_minutes);
    }
    
    g_app_context->config->update_interval_minutes = 200;
    config_save();
    config_load();
    
    if (g_app_context->config->update_interval_minutes <= 120) {
        printf("✅ Interval maximum enforced\n");
    } else {
        printf("⚠️  Interval maximum not enforced (got %d)\n",
               g_app_context->config->update_interval_minutes);
    }
}

void cleanup_test_settings() {
    printf("\nCleaning up test settings...\n");
    
    // Reset to sensible defaults
    g_free(g_app_context->config->city);
    g_app_context->config->city = g_strdup("Berlin");
    g_app_context->config->use_celsius = TRUE;
    g_app_context->config->update_interval_minutes = 10;
    g_app_context->config->provider = 0;
    
    config_save();
    printf("Reset to defaults\n");
}

int main() {
    // Set environment to use local schema
    setenv("GSETTINGS_SCHEMA_DIR", "/home/dl/Projects/mate-weather/schemas", 1);
    setenv("GSETTINGS_BACKEND", "memory", 1); // Use memory backend for testing
    
    logger_init();
    
    // Initialize app context
    g_app_context = g_new0(AppContext, 1);
    g_mutex_init(&g_app_context->data_mutex);
    
    printf("GSettings Configuration Test Suite\n");
    printf("==================================\n\n");
    
    test_gsettings_basic();
    test_api_keys();
    test_validation();
    cleanup_test_settings();
    
    printf("\n==================================\n");
    printf("All tests completed!\n");
    
    // Cleanup
    config_free();
    g_mutex_clear(&g_app_context->data_mutex);
    g_free(g_app_context);
    logger_cleanup();
    
    return 0;
}