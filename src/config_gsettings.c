#include "config.h"
#include "config_gsettings.h"
#include "geoip.h"
#include "logger.h"
#include <gio/gio.h>
#include <string.h>

#define GSETTINGS_SCHEMA "org.mate.panel.applet.weather-vibes"

static GSettings *settings = NULL;
static gboolean use_ini_fallback = FALSE;

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

static gboolean ini_get_string_any(GKeyFile *key_file,
                                   const char *group_a,
                                   const char *key_a,
                                   const char *group_b,
                                   const char *key_b,
                                   char **out) {
    GError *error = NULL;
    char *value = g_key_file_get_string(key_file, group_a, key_a, &error);
    if (!error && value) {
        *out = value;
        return TRUE;
    }
    g_clear_error(&error);
    value = g_key_file_get_string(key_file, group_b, key_b, &error);
    if (!error && value) {
        *out = value;
        return TRUE;
    }
    g_clear_error(&error);
    return FALSE;
}

static gboolean ini_get_int_any(GKeyFile *key_file,
                                const char *group_a,
                                const char *key_a,
                                const char *group_b,
                                const char *key_b,
                                int *out) {
    GError *error = NULL;
    int value = g_key_file_get_integer(key_file, group_a, key_a, &error);
    if (!error) {
        *out = value;
        return TRUE;
    }
    g_clear_error(&error);
    value = g_key_file_get_integer(key_file, group_b, key_b, &error);
    if (!error) {
        *out = value;
        return TRUE;
    }
    g_clear_error(&error);
    return FALSE;
}

static gboolean ini_get_bool_any(GKeyFile *key_file,
                                 const char *group_a,
                                 const char *key_a,
                                 const char *group_b,
                                 const char *key_b,
                                 gboolean *out) {
    GError *error = NULL;
    gboolean value = g_key_file_get_boolean(key_file, group_a, key_a, &error);
    if (!error) {
        *out = value;
        return TRUE;
    }
    g_clear_error(&error);
    value = g_key_file_get_boolean(key_file, group_b, key_b, &error);
    if (!error) {
        *out = value;
        return TRUE;
    }
    g_clear_error(&error);
    return FALSE;
}

static void config_load_ini_fallback(void) {
    char *config_file = get_config_path();
    GKeyFile *key_file = g_key_file_new();
    GError *error = NULL;
    
    gboolean config_exists = g_key_file_load_from_file(key_file, config_file, G_KEY_FILE_NONE, &error);
    
    if (!config_exists) {
        if (error && error->code != G_FILE_ERROR_NOENT) {
            g_warning("Failed to load config: %s", error->message);
        } else if (error && error->code == G_FILE_ERROR_NOENT) {
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
    
    char *city = NULL;
    if (ini_get_string_any(key_file, "General", "City", "Weather", "city", &city)) {
        g_free(g_app_context->config->city);
        g_app_context->config->city = city;
    }
    
    int interval = 0;
    if (ini_get_int_any(key_file, "General", "UpdateInterval", "Weather", "update_interval_minutes", &interval)) {
        if (interval > 0 && interval <= 120) {
            g_app_context->config->update_interval_minutes = interval;
        }
    }
    
    gboolean celsius = FALSE;
    if (ini_get_bool_any(key_file, "General", "UseCelsius", "Weather", "use_celsius", &celsius)) {
        g_app_context->config->use_celsius = celsius;
    }
    
    int provider = 0;
    if (ini_get_int_any(key_file, "General", "Provider", "Weather", "provider", &provider)) {
        if (provider >= PROVIDER_ANSIWEATHER && provider < PROVIDER_COUNT) {
            g_app_context->config->provider = (WeatherProvider)provider;
        }
    }
    
    char *openweather_key = NULL;
    if (ini_get_string_any(key_file, "General", "OpenWeatherAPIKey", "Weather", "openweather_api_key", &openweather_key)) {
        g_strstrip(openweather_key);
        g_free(g_app_context->config->openweather_api_key);
        g_app_context->config->openweather_api_key = openweather_key;
    }
    
    char *tomorrow_key = NULL;
    if (ini_get_string_any(key_file, "General", "TomorrowAPIKey", "Weather", "tomorrow_api_key", &tomorrow_key)) {
        g_strstrip(tomorrow_key);
        g_free(g_app_context->config->tomorrow_api_key);
        g_app_context->config->tomorrow_api_key = tomorrow_key;
    }
    
    g_key_file_free(key_file);
    g_free(config_file);
}

static void config_save_ini_fallback(void) {
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
        gboolean migrated_any = FALSE;
        char *city = NULL;
        if (ini_get_string_any(key_file, "General", "City", "Weather", "city", &city)) {
            g_settings_set_string(settings, "city", city);
            g_free(city);
            migrated_any = TRUE;
        }
        
        gboolean use_celsius = FALSE;
        if (ini_get_bool_any(key_file, "General", "UseCelsius", "Weather", "use_celsius", &use_celsius)) {
            g_settings_set_boolean(settings, "use-celsius", use_celsius);
            migrated_any = TRUE;
        }
        
        int interval = 0;
        if (ini_get_int_any(key_file, "General", "UpdateInterval", "Weather", "update_interval_minutes", &interval)) {
            if (interval >= 5 && interval <= 120) {
                g_settings_set_int(settings, "update-interval-minutes", interval);
                migrated_any = TRUE;
            }
        }
        
        int provider = 0;
        if (ini_get_int_any(key_file, "General", "Provider", "Weather", "provider", &provider)) {
            if (provider >= 0 && provider <= 3) {
                g_settings_set_int(settings, "provider", provider);
                migrated_any = TRUE;
            }
        }
        
        char *openweather_key = NULL;
        if (ini_get_string_any(key_file, "General", "OpenWeatherAPIKey", "Weather", "openweather_api_key", &openweather_key)) {
            g_strstrip(openweather_key);
            g_settings_set_string(settings, "openweather-api-key", openweather_key);
            g_free(openweather_key);
            migrated_any = TRUE;
        }
        
        char *tomorrow_key = NULL;
        if (ini_get_string_any(key_file, "General", "TomorrowAPIKey", "Weather", "tomorrow_api_key", &tomorrow_key)) {
            g_strstrip(tomorrow_key);
            g_settings_set_string(settings, "tomorrow-api-key", tomorrow_key);
            g_free(tomorrow_key);
            migrated_any = TRUE;
        }
        
        if (migrated_any) {
            FILE *flag = fopen(migrated_flag, "w");
            if (flag) {
                fprintf(flag, "Migrated on %ld\n", time(NULL));
                fclose(flag);
            }
            log_info("Configuration migration completed successfully");
        } else {
            log_warn("Configuration migration skipped: no values found to migrate");
        }
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
        log_error("GSettings schema %s not installed! Falling back to INI config.", GSETTINGS_SCHEMA);
        use_ini_fallback = TRUE;
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
        log_warn("GSettings not initialized, using INI fallback");
        config_load_ini_fallback();
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
        log_warn("Cannot save config: GSettings not initialized, using INI fallback");
        config_save_ini_fallback();
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
