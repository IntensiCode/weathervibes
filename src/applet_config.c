#include "applet_config.h"
#include "app.h"
#include "config.h"
#include "logger.h"
#include <string.h>

// External global context
extern AppContext *g_app_context;

AppConfig* applet_config_new_from_global(void) {
    if (!g_app_context || !g_app_context->config) {
        log_error("Cannot create applet config - no global config available");
        return NULL;
    }
    
    AppConfig *config = g_new0(AppConfig, 1);
    
    // Copy from global config
    config->city = g_strdup(g_app_context->config->city);
    config->provider = g_app_context->config->provider;
    config->openweather_api_key = g_strdup(g_app_context->config->openweather_api_key);
    config->tomorrow_api_key = g_strdup(g_app_context->config->tomorrow_api_key);
    config->use_celsius = g_app_context->config->use_celsius;
    config->update_interval_minutes = g_app_context->config->update_interval_minutes;
    
    return config;
}

void applet_config_update(AppConfig *config,
                          const char *location,
                          int provider,
                          const char *api_key,
                          gboolean use_celsius,
                          int interval_minutes) {
    if (!config) {
        log_error("Cannot update NULL config");
        return;
    }
    
    // Update location
    g_free(config->city);
    config->city = g_strdup(location);
    
    // Update provider
    config->provider = provider;
    
    // Update API key for current provider
    if (provider == PROVIDER_OPENWEATHER) {
        g_free(config->openweather_api_key);
        config->openweather_api_key = g_strdup(api_key);
        if (config->openweather_api_key) {
            g_strstrip(config->openweather_api_key);
        }
        log_debug("OpenWeather API key updated: %zu chars", 
                  config->openweather_api_key ? strlen(config->openweather_api_key) : 0);
    } else if (provider == PROVIDER_TOMORROW) {
        g_free(config->tomorrow_api_key);
        config->tomorrow_api_key = g_strdup(api_key);
        if (config->tomorrow_api_key) {
            g_strstrip(config->tomorrow_api_key);
        }
        log_debug("Tomorrow.io API key updated: %zu chars", 
                  config->tomorrow_api_key ? strlen(config->tomorrow_api_key) : 0);
    }
    
    // Update other settings
    config->use_celsius = use_celsius;
    config->update_interval_minutes = interval_minutes;
}

void applet_config_sync_to_global(AppConfig *config) {
    if (!config || !g_app_context || !g_app_context->config) {
        log_error("Cannot sync config - NULL config or context");
        return;
    }
    
    // Update global config from applet config
    g_free(g_app_context->config->city);
    g_app_context->config->city = g_strdup(config->city);
    
    g_app_context->config->provider = config->provider;
    
    g_free(g_app_context->config->openweather_api_key);
    g_app_context->config->openweather_api_key = g_strdup(config->openweather_api_key);
    
    g_free(g_app_context->config->tomorrow_api_key);
    g_app_context->config->tomorrow_api_key = g_strdup(config->tomorrow_api_key);
    
    g_app_context->config->use_celsius = config->use_celsius;
    g_app_context->config->update_interval_minutes = config->update_interval_minutes;
    
    // Save to disk
    config_save();
    
    log_debug("Config synced to global and saved");
}

void applet_config_free(AppConfig *config) {
    if (config) {
        g_free(config->city);
        g_free(config->openweather_api_key);
        g_free(config->tomorrow_api_key);
        g_free(config);
    }
}