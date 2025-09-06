#include "config.h"
#include "config_gsettings.h"
#include "geoip.h"
#include "logger.h"
#include <gio/gio.h>

#define GSETTINGS_SCHEMA "org.mate.panel.applet.weather-vibes"

static GSettings *settings = NULL;

// Migration helper: load old INI config one time
static void migrate_from_ini_if_needed(void) {
    const char *config_dir = g_get_user_config_dir();
    char *app_config_dir = g_build_filename(config_dir, "weather-vibes", NULL);
    char *config_file = g_build_filename(app_config_dir, "config.ini", NULL);
    
    // Check if old config exists
    if (!g_file_test(config_file, G_FILE_TEST_EXISTS)) {
        g_free(config_file);
        g_free(app_config_dir);
        return;
    }
    
    // Check if we've already migrated
    char *migrated_flag = g_build_filename(app_config_dir, ".migrated-to-gsettings", NULL);
    if (g_file_test(migrated_flag, G_FILE_TEST_EXISTS)) {
        g_free(migrated_flag);
        g_free(config_file);
        g_free(app_config_dir);
        return;
    }
    
    log_info("Migrating configuration from INI to GSettings");
    
    // Load old config
    GKeyFile *key_file = g_key_file_new();
    GError *error = NULL;
    
    if (g_key_file_load_from_file(key_file, config_file, G_KEY_FILE_NONE, &error)) {
        // Migrate city
        char *city = g_key_file_get_string(key_file, "Weather", "city", NULL);
        if (city) {
            g_settings_set_string(settings, "city", city);
            g_free(city);
        }
        
        // Migrate temperature unit
        gboolean use_celsius = g_key_file_get_boolean(key_file, "Weather", "use_celsius", NULL);
        g_settings_set_boolean(settings, "use-celsius", use_celsius);
        
        // Migrate update interval
        int interval = g_key_file_get_integer(key_file, "Weather", "update_interval_minutes", NULL);
        if (interval >= 5 && interval <= 120) {
            g_settings_set_int(settings, "update-interval-minutes", interval);
        }
        
        // Migrate provider
        int provider = g_key_file_get_integer(key_file, "Weather", "provider", NULL);
        if (provider >= 0 && provider <= 3) {
            g_settings_set_int(settings, "provider", provider);
        }
        
        // Migrate API keys
        char *openweather_key = g_key_file_get_string(key_file, "Weather", "openweather_api_key", NULL);
        if (openweather_key) {
            g_settings_set_string(settings, "openweather-api-key", openweather_key);
            g_free(openweather_key);
        }
        
        char *tomorrow_key = g_key_file_get_string(key_file, "Weather", "tomorrow_api_key", NULL);
        if (tomorrow_key) {
            g_settings_set_string(settings, "tomorrow-api-key", tomorrow_key);
            g_free(tomorrow_key);
        }
        
        // Create migration flag
        FILE *flag = fopen(migrated_flag, "w");
        if (flag) {
            fprintf(flag, "Migrated on %ld\n", time(NULL));
            fclose(flag);
        }
        
        log_info("Configuration migration completed successfully");
    } else {
        if (error) {
            log_warn("Failed to load old config for migration: %s", error->message);
            g_error_free(error);
        }
    }
    
    g_key_file_free(key_file);
    g_free(migrated_flag);
    g_free(config_file);
    g_free(app_config_dir);
}

void config_gsettings_init(void) {
    if (settings) {
        return; // Already initialized
    }
    
    // Check if schema is installed
    GSettingsSchemaSource *source = g_settings_schema_source_get_default();
    GSettingsSchema *schema = g_settings_schema_source_lookup(source, GSETTINGS_SCHEMA, FALSE);
    
    if (!schema) {
        log_error("GSettings schema %s not installed! Falling back to defaults.", GSETTINGS_SCHEMA);
        // You might want to fall back to the old config system here
        return;
    }
    
    g_settings_schema_unref(schema);
    
    // Create GSettings instance
    settings = g_settings_new(GSETTINGS_SCHEMA);
    
    // Migrate from old config if needed
    migrate_from_ini_if_needed();
}

void config_gsettings_cleanup(void) {
    if (settings) {
        g_object_unref(settings);
        settings = NULL;
    }
}

void config_init(void) {
    g_app_context->config = g_new0(AppConfig, 1);
    
    // Initialize GSettings
    config_gsettings_init();
    
    // Set defaults (will be overridden by config_load)
    g_app_context->config->city = g_strdup("Berlin");
    g_app_context->config->update_interval_minutes = 10;
    g_app_context->config->use_celsius = TRUE;
    g_app_context->config->provider = PROVIDER_ANSIWEATHER;
    g_app_context->config->openweather_api_key = NULL;
    g_app_context->config->tomorrow_api_key = NULL;
}

void config_load(void) {
    if (!settings) {
        log_warn("GSettings not initialized, using defaults");
        
        // First run - try GeoIP detection
        char *detected_city = geoip_get_city();
        if (detected_city) {
            g_free(g_app_context->config->city);
            g_app_context->config->city = detected_city;
            log_info("First run: Using GeoIP detected city: %s", detected_city);
        }
        return;
    }
    
    // Load from GSettings
    g_free(g_app_context->config->city);
    g_app_context->config->city = g_settings_get_string(settings, "city");
    
    // If city is still default and this looks like first run, try GeoIP
    if (g_strcmp0(g_app_context->config->city, "Berlin") == 0) {
        char *detected_city = geoip_get_city();
        if (detected_city) {
            g_free(g_app_context->config->city);
            g_app_context->config->city = detected_city;
            g_settings_set_string(settings, "city", detected_city);
            log_info("First run: Using GeoIP detected city: %s", detected_city);
        }
    }
    
    g_app_context->config->use_celsius = g_settings_get_boolean(settings, "use-celsius");
    g_app_context->config->update_interval_minutes = g_settings_get_int(settings, "update-interval-minutes");
    g_app_context->config->provider = g_settings_get_int(settings, "provider");
    
    g_free(g_app_context->config->openweather_api_key);
    g_app_context->config->openweather_api_key = g_settings_get_string(settings, "openweather-api-key");
    if (strlen(g_app_context->config->openweather_api_key) == 0) {
        g_free(g_app_context->config->openweather_api_key);
        g_app_context->config->openweather_api_key = NULL;
    }
    
    g_free(g_app_context->config->tomorrow_api_key);
    g_app_context->config->tomorrow_api_key = g_settings_get_string(settings, "tomorrow-api-key");
    if (strlen(g_app_context->config->tomorrow_api_key) == 0) {
        g_free(g_app_context->config->tomorrow_api_key);
        g_app_context->config->tomorrow_api_key = NULL;
    }
    
    log_info("Configuration loaded from GSettings: city=%s, provider=%d, interval=%d min",
             g_app_context->config->city,
             g_app_context->config->provider,
             g_app_context->config->update_interval_minutes);
}

void config_save(void) {
    if (!settings) {
        log_error("Cannot save config: GSettings not initialized");
        return;
    }
    
    g_settings_set_string(settings, "city", 
                         g_app_context->config->city ? g_app_context->config->city : "Berlin");
    g_settings_set_boolean(settings, "use-celsius", g_app_context->config->use_celsius);
    g_settings_set_int(settings, "update-interval-minutes", g_app_context->config->update_interval_minutes);
    g_settings_set_int(settings, "provider", g_app_context->config->provider);
    
    g_settings_set_string(settings, "openweather-api-key",
                         g_app_context->config->openweather_api_key ? g_app_context->config->openweather_api_key : "");
    g_settings_set_string(settings, "tomorrow-api-key",
                         g_app_context->config->tomorrow_api_key ? g_app_context->config->tomorrow_api_key : "");
    
    // GSettings automatically syncs to disk
    g_settings_sync();
    
    log_info("Configuration saved to GSettings");
}

void config_free(void) {
    if (g_app_context && g_app_context->config) {
        g_free(g_app_context->config->city);
        g_free(g_app_context->config->openweather_api_key);
        g_free(g_app_context->config->tomorrow_api_key);
        g_free(g_app_context->config);
        g_app_context->config = NULL;
    }
    
    config_gsettings_cleanup();
}