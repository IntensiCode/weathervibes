#include "config.h"
#include "geoip.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>

static char* get_config_path(void) {
    const char *config_dir = g_get_user_config_dir();
    char *app_config_dir = g_build_filename(config_dir, "weather-vibes", NULL);
    
    if (!g_file_test(app_config_dir, G_FILE_TEST_EXISTS)) {
        g_mkdir_with_parents(app_config_dir, 0755);
    }
    
    char *config_file = g_build_filename(app_config_dir, "config.ini", NULL);
    g_free(app_config_dir);
    
    return config_file;
}

void config_init(void) {
    g_app_context->config = g_new0(AppConfig, 1);
    
    // Set defaults (will be overridden by config_load if config exists)
    g_app_context->config->city = g_strdup("Berlin");  // Temporary default
    g_app_context->config->update_interval_minutes = 10;
    g_app_context->config->use_celsius = TRUE;
    g_app_context->config->provider = PROVIDER_ANSIWEATHER;  // Default provider
    g_app_context->config->openweather_api_key = NULL;  // No default API key
}

void config_load(void) {
    char *config_file = get_config_path();
    GKeyFile *key_file = g_key_file_new();
    GError *error = NULL;
    
    gboolean config_exists = g_key_file_load_from_file(key_file, config_file, G_KEY_FILE_NONE, &error);
    
    if (!config_exists) {
        if (error && error->code != G_FILE_ERROR_NOENT) {
            g_warning("Failed to load config: %s", error->message);
        } else if (error && error->code == G_FILE_ERROR_NOENT) {
            // Config doesn't exist - this is first run, try GeoIP
            log_info("No config file found, attempting GeoIP detection for initial city");
            char *detected_city = geoip_get_city();
            if (detected_city) {
                g_free(g_app_context->config->city);
                g_app_context->config->city = detected_city;
                log_info("First run: Using GeoIP detected city: %s", detected_city);
            } else {
                log_info("First run: GeoIP detection failed, using default city: Berlin");
            }
        }
        g_clear_error(&error);
        g_key_file_free(key_file);
        g_free(config_file);
        return;
    }
    
    char *city = g_key_file_get_string(key_file, "General", "City", &error);
    if (city) {
        g_free(g_app_context->config->city);
        g_app_context->config->city = city;
    }
    g_clear_error(&error);
    
    int interval = g_key_file_get_integer(key_file, "General", "UpdateInterval", &error);
    if (!error && interval > 0 && interval <= 60) {
        g_app_context->config->update_interval_minutes = interval;
    }
    g_clear_error(&error);
    
    gboolean celsius = g_key_file_get_boolean(key_file, "General", "UseCelsius", &error);
    if (!error) {
        g_app_context->config->use_celsius = celsius;
    }
    g_clear_error(&error);
    
    int provider = g_key_file_get_integer(key_file, "General", "Provider", &error);
    if (!error && provider >= PROVIDER_ANSIWEATHER && provider < PROVIDER_COUNT) {
        g_app_context->config->provider = (WeatherProvider)provider;
    }
    g_clear_error(&error);
    
    char *api_key = g_key_file_get_string(key_file, "General", "OpenWeatherAPIKey", &error);
    if (api_key) {
        // Sanitize the API key - remove any whitespace including newlines
        g_strstrip(api_key);  // Removes leading and trailing whitespace
        g_free(g_app_context->config->openweather_api_key);
        g_app_context->config->openweather_api_key = api_key;
    }
    g_clear_error(&error);
    
    char *tomorrow_key = g_key_file_get_string(key_file, "General", "TomorrowAPIKey", &error);
    if (tomorrow_key) {
        // Sanitize the API key - remove any whitespace including newlines
        g_strstrip(tomorrow_key);  // Removes leading and trailing whitespace
        g_free(g_app_context->config->tomorrow_api_key);
        g_app_context->config->tomorrow_api_key = tomorrow_key;
    }
    g_clear_error(&error);
    
    g_key_file_free(key_file);
    g_free(config_file);
}

void config_save(void) {
    if (!g_app_context->config) return;
    
    char *config_file = get_config_path();
    GKeyFile *key_file = g_key_file_new();
    
    g_key_file_set_string(key_file, "General", "City", 
                          g_app_context->config->city ? g_app_context->config->city : "Berlin");
    g_key_file_set_integer(key_file, "General", "UpdateInterval",
                           g_app_context->config->update_interval_minutes);
    g_key_file_set_boolean(key_file, "General", "UseCelsius",
                          g_app_context->config->use_celsius);
    g_key_file_set_integer(key_file, "General", "Provider",
                          g_app_context->config->provider);
    g_key_file_set_string(key_file, "General", "OpenWeatherAPIKey",
                         g_app_context->config->openweather_api_key ? g_app_context->config->openweather_api_key : "");
    g_key_file_set_string(key_file, "General", "TomorrowAPIKey",
                         g_app_context->config->tomorrow_api_key ? g_app_context->config->tomorrow_api_key : "");
    log_debug("Saving Tomorrow.io API key: '%s' (length: %zu)", 
              g_app_context->config->tomorrow_api_key ? g_app_context->config->tomorrow_api_key : "NULL",
              g_app_context->config->tomorrow_api_key ? strlen(g_app_context->config->tomorrow_api_key) : 0);
    
    GError *error = NULL;
    gsize length;
    char *data = g_key_file_to_data(key_file, &length, &error);
    
    if (data) {
        g_file_set_contents(config_file, data, length, &error);
        g_free(data);
    }
    
    if (error) {
        g_warning("Failed to save config: %s", error->message);
        g_error_free(error);
    }
    
    g_key_file_free(key_file);
    g_free(config_file);
}

void config_free(void) {
    if (g_app_context->config) {
        g_free(g_app_context->config->city);
        g_free(g_app_context->config->openweather_api_key);
        g_free(g_app_context->config->tomorrow_api_key);
        g_free(g_app_context->config);
        g_app_context->config = NULL;
    }
}