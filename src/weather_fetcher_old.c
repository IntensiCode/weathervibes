#include "weather_fetcher.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <json-glib/json-glib.h>

static char* strip_ansi_codes(const char *input) {
    regex_t regex;
    regmatch_t match;
    char *output = g_strdup(input);
    char *temp = g_malloc(strlen(input) + 1);
    
    if (regcomp(&regex, "\x1b\\[[0-9;]*m", REG_EXTENDED) != 0) {
        g_free(temp);
        return output;
    }
    
    while (regexec(&regex, output, 1, &match, 0) == 0) {
        strcpy(temp, output);
        memmove(temp + match.rm_so, 
                temp + match.rm_eo, 
                strlen(temp + match.rm_eo) + 1);
        strcpy(output, temp);
    }
    
    regfree(&regex);
    g_free(temp);
    return output;
}

static char* extract_value(const char *text, const char *prefix) {
    char *start = strstr(text, prefix);
    if (!start) return NULL;
    
    start += strlen(prefix);
    while (*start == ' ' || *start == ':') start++;
    
    char *end = strstr(start, " -");
    if (!end) end = start + strlen(start);
    
    int len = end - start;
    char *result = g_malloc(len + 1);
    strncpy(result, start, len);
    result[len] = '\0';
    
    g_strstrip(result);
    return result;
}

static void parse_weather_output(const char *output, WeatherData *data) {
    if (!output || !data) return;
    
    char *clean = strip_ansi_codes(output);
    
    // Extract city and temperature from "Weather in Berlin: 22 °C"
    char *weather_start = strstr(clean, "Weather in ");
    if (weather_start) {
        char *city_start = weather_start + strlen("Weather in ");
        char *colon = strchr(city_start, ':');
        if (colon) {
            // Extract city
            int city_len = colon - city_start;
            data->city = g_malloc(city_len + 1);
            strncpy(data->city, city_start, city_len);
            data->city[city_len] = '\0';
            g_strstrip(data->city);
            
            // Extract temperature (after colon)
            char *temp_start = colon + 1;
            while (*temp_start == ' ') temp_start++;
            char *temp_end = strstr(temp_start, " ");
            if (temp_end) {
                int temp_len = temp_end - temp_start;
                char *temp_str = g_malloc(temp_len + 1);
                strncpy(temp_str, temp_start, temp_len);
                temp_str[temp_len] = '\0';
                data->temperature = atof(temp_str);
                g_free(temp_str);
            }
        }
    }
    
    // No feels_like in ansiweather output
    data->feels_like = data->temperature;
    
    // Extract humidity (format: "Humidity: 72%")
    char *humidity_str = extract_value(clean, "Humidity");
    if (humidity_str) {
        data->humidity = atoi(humidity_str);
        g_free(humidity_str);
    }
    
    // Extract wind speed (format: "Wind: 1.34 m/s S")
    char *wind_str = extract_value(clean, "Wind");
    if (wind_str) {
        char *speed_end = strchr(wind_str, ' ');
        if (speed_end) {
            *speed_end = '\0';
            data->wind_speed = atof(wind_str);
            
            // Skip "m/s" and get direction
            char *dir_start = strstr(speed_end + 1, "m/s");
            if (dir_start) {
                dir_start += 3;
                while (*dir_start == ' ') dir_start++;
                if (*dir_start) {
                    data->wind_direction = g_strdup(dir_start);
                    g_strstrip(data->wind_direction);
                }
            }
        }
        g_free(wind_str);
    }
    
    // Extract pressure (format: "Pressure: 1013 hPa")
    char *pressure_str = extract_value(clean, "Pressure");
    if (pressure_str) {
        data->pressure = atoi(pressure_str);
        g_free(pressure_str);
    }
    
    // Extract UV index (format: "UVI: 4.52")
    char *uvi_str = extract_value(clean, "UVI");
    if (uvi_str) {
        data->uvi = atof(uvi_str);
        g_free(uvi_str);
    }
    
    // AnsiWeather doesn't provide sunrise/sunset
    data->sunrise = NULL;
    data->sunset = NULL;
    
    // AnsiWeather doesn't provide condition text in the basic output
    // We'd need to use the extended format (-a) to get it
    // For now, set a default
    data->condition_icon = g_strdup("🌤️");
    data->condition_text = g_strdup("Current Weather");
    
    g_free(clean);
}

// Fetch JSON from URL using curl command
static char* fetch_json_from_url(const char *url) {
    char *command = g_strdup_printf("curl -s '%s' 2>/dev/null", url);
    
    FILE *pipe = popen(command, "r");
    g_free(command);
    
    if (!pipe) return NULL;
    
    char buffer[1024];
    GString *output = g_string_new("");
    
    while (fgets(buffer, sizeof(buffer), pipe)) {
        g_string_append(output, buffer);
    }
    
    int ret = pclose(pipe);
    if (ret != 0) {
        g_string_free(output, TRUE);
        return NULL;
    }
    
    char *result = g_strdup(output->str);
    g_string_free(output, TRUE);
    return result;
}

// Structure to hold geocoding results
typedef struct {
    double latitude;
    double longitude;
    char *display_name;
} GeoLocation;

// Free GeoLocation structure
static void free_geolocation(GeoLocation *loc) {
    if (!loc) return;
    g_free(loc->display_name);
    g_free(loc);
}

// Geocode city name to coordinates using OpenStreetMap Nominatim
static GeoLocation* geocode_city(const char *city) {
    if (!city || strlen(city) == 0) return NULL;
    
    // URL encode the city name
    char *encoded_city = g_uri_escape_string(city, NULL, TRUE);
    
    // Build Nominatim URL
    char url[512];
    snprintf(url, sizeof(url), 
        "https://nominatim.openstreetmap.org/search?q=%s&format=json&limit=1",
        encoded_city);
    
    g_free(encoded_city);
    
    // Fetch JSON response
    char *json_response = fetch_json_from_url(url);
    if (!json_response) {
        g_warning("Failed to fetch geocoding data for %s", city);
        return NULL;
    }
    
    // Parse JSON response
    JsonParser *parser = json_parser_new();
    GError *error = NULL;
    
    if (!json_parser_load_from_data(parser, json_response, -1, &error)) {
        if (error) {
            g_warning("Failed to parse geocoding JSON: %s", error->message);
            g_error_free(error);
        }
        g_object_unref(parser);
        g_free(json_response);
        return NULL;
    }
    
    JsonNode *root = json_parser_get_root(parser);
    if (!JSON_NODE_HOLDS_ARRAY(root)) {
        g_object_unref(parser);
        g_free(json_response);
        return NULL;
    }
    
    JsonArray *results = json_node_get_array(root);
    if (json_array_get_length(results) == 0) {
        g_warning("No geocoding results found for %s", city);
        g_object_unref(parser);
        g_free(json_response);
        return NULL;
    }
    
    // Get first result
    JsonObject *first_result = json_array_get_object_element(results, 0);
    
    GeoLocation *location = g_new0(GeoLocation, 1);
    
    // Extract coordinates
    const char *lat_str = json_object_get_string_member(first_result, "lat");
    const char *lon_str = json_object_get_string_member(first_result, "lon");
    
    if (lat_str && lon_str) {
        location->latitude = atof(lat_str);
        location->longitude = atof(lon_str);
    }
    
    // Extract display name
    const char *display_name = json_object_get_string_member(first_result, "display_name");
    if (display_name) {
        location->display_name = g_strdup(display_name);
    }
    
    g_object_unref(parser);
    g_free(json_response);
    
    g_message("Geocoded '%s' to lat=%.4f, lon=%.4f (%s)", 
              city, location->latitude, location->longitude,
              location->display_name ? location->display_name : "Unknown");
    
    return location;
}

// Fetch weather using Bright Sky API (DWD)
static gboolean fetch_brightsky(const char *city) {
    // Geocode the city to get coordinates
    GeoLocation *location = geocode_city(city);
    if (!location) {
        g_warning("Failed to geocode city: %s", city);
        return FALSE;
    }
    
    double lat = location->latitude;
    double lon = location->longitude;
    
    // Use C locale for URL formatting to ensure decimal points not commas
    char url[512];
    char lat_str[32], lon_str[32];
    snprintf(lat_str, sizeof(lat_str), "%.2f", lat);
    snprintf(lon_str, sizeof(lon_str), "%.2f", lon);
    // Replace any commas with dots (in case locale affected snprintf)
    for (char *p = lat_str; *p; p++) if (*p == ',') *p = '.';
    for (char *p = lon_str; *p; p++) if (*p == ',') *p = '.';
    
    snprintf(url, sizeof(url), "https://api.brightsky.dev/current_weather?lat=%s&lon=%s", lat_str, lon_str);
    
    free_geolocation(location);
    
    char *json_response = fetch_json_from_url(url);
    if (!json_response) return FALSE;
    
    // Parse JSON response
    JsonParser *parser = json_parser_new();
    GError *error = NULL;
    if (!json_parser_load_from_data(parser, json_response, -1, &error)) {
        if (error) {
            g_warning("Failed to parse JSON: %s", error->message);
            g_error_free(error);
        }
        g_object_unref(parser);
        g_free(json_response);
        return FALSE;
    }
    
    JsonNode *root = json_parser_get_root(parser);
    JsonObject *root_obj = json_node_get_object(root);
    JsonObject *weather = json_object_get_object_member(root_obj, "weather");
    
    if (!weather) {
        g_object_unref(parser);
        g_free(json_response);
        return FALSE;
    }
    
    WeatherData *new_data = g_new0(WeatherData, 1);
    new_data->city = g_strdup(city);
    
    // Check for critical weather data - temperature is absolutely required
    if (!json_object_has_member(weather, "temperature")) {
        g_warning("Bright Sky API did not return temperature for %s - this location has incomplete data", city);
        
        // Create an error data structure to show the alert
        new_data->temperature = -999.0; // Signal missing data
        new_data->condition_text = g_strdup("Data Unavailable");
        new_data->condition_icon = g_strdup("⚠️");
        new_data->error_message = g_strdup_printf(
            "Critical weather data missing for %s\n\n"
            "The weather station at this location does not provide temperature data.\n"
            "Please try:\n"
            "• A nearby larger city\n"
            "• A different weather provider\n"
            "• Check the city name spelling",
            city
        );
        new_data->last_update = time(NULL);
        new_data->raw_output = g_strdup(json_response);
        
        // Store as error data
        g_mutex_lock(&g_app_context->data_mutex);
        if (g_app_context->weather_data) {
            weather_fetcher_free_data(g_app_context->weather_data);
        }
        g_app_context->weather_data = new_data;
        g_mutex_unlock(&g_app_context->data_mutex);
        
        g_object_unref(parser);
        g_free(json_response);
        return FALSE; // Return failure but with error data stored
    }
    
    new_data->temperature = json_object_get_double_member(weather, "temperature");
    new_data->feels_like = new_data->temperature; // Bright Sky doesn't provide feels_like
    
    // Handle optional fields gracefully
    if (json_object_has_member(weather, "relative_humidity")) {
        new_data->humidity = (int)json_object_get_int_member(weather, "relative_humidity");
    }
    
    // Check for wind_speed (also critical)
    if (json_object_has_member(weather, "wind_speed")) {
        new_data->wind_speed = json_object_get_double_member(weather, "wind_speed") / 3.6; // Convert km/h to m/s
    } else if (json_object_has_member(weather, "wind_speed_10")) {
        new_data->wind_speed = json_object_get_double_member(weather, "wind_speed_10") / 3.6;
    } else {
        // No wind data - also critical
        g_warning("Bright Sky API did not return wind speed for %s", city);
        new_data->wind_speed = 0.0;
    }
    
    if (json_object_has_member(weather, "pressure_msl")) {
        new_data->pressure = (int)json_object_get_double_member(weather, "pressure_msl");
    }
    
    // Map icon to condition text
    const char *icon = json_object_get_string_member(weather, "icon");
    if (icon && strstr(icon, "clear")) {
        new_data->condition_text = g_strdup("Clear");
        new_data->condition_icon = g_strdup("☀️");
    } else if (icon && strstr(icon, "partly-cloudy")) {
        new_data->condition_text = g_strdup("Partly Cloudy");
        new_data->condition_icon = g_strdup("⛅");
    } else if (icon && strstr(icon, "cloudy")) {
        new_data->condition_text = g_strdup("Cloudy");
        new_data->condition_icon = g_strdup("☁️");
    } else if (icon && strstr(icon, "rain")) {
        new_data->condition_text = g_strdup("Rain");
        new_data->condition_icon = g_strdup("🌧️");
    } else if (icon && strstr(icon, "snow")) {
        new_data->condition_text = g_strdup("Snow");
        new_data->condition_icon = g_strdup("❄️");
    } else {
        new_data->condition_text = g_strdup("Unknown");
        new_data->condition_icon = g_strdup("❓");
    }
    
    new_data->last_update = time(NULL);
    new_data->raw_output = g_strdup(json_response);
    
    g_mutex_lock(&g_app_context->data_mutex);
    if (g_app_context->weather_data) {
        weather_fetcher_free_data(g_app_context->weather_data);
    }
    g_app_context->weather_data = new_data;
    g_mutex_unlock(&g_app_context->data_mutex);
    
    g_object_unref(parser);
    g_free(json_response);
    return TRUE;
}

// Fetch weather using AnsiWeather
static gboolean fetch_ansiweather(const char *city) {
    if (!city || !*city) return FALSE;
    
    char *command = g_strdup_printf("ansiweather -l \"%s\" -s true -d true -H true 2>&1", city);
    
    FILE *pipe = popen(command, "r");
    g_free(command);
    
    if (!pipe) return FALSE;
    
    char buffer[1024];
    GString *output = g_string_new("");
    
    while (fgets(buffer, sizeof(buffer), pipe)) {
        g_string_append(output, buffer);
    }
    
    int ret = pclose(pipe);
    if (ret != 0) {
        g_string_free(output, TRUE);
        return FALSE;
    }
    
    WeatherData *new_data = g_new0(WeatherData, 1);
    new_data->raw_output = g_strdup(output->str);
    new_data->last_update = time(NULL);
    
    parse_weather_output(output->str, new_data);
    
    g_mutex_lock(&g_app_context->data_mutex);
    if (g_app_context->weather_data) {
        weather_fetcher_free_data(g_app_context->weather_data);
    }
    g_app_context->weather_data = new_data;
    g_mutex_unlock(&g_app_context->data_mutex);
    
    g_string_free(output, TRUE);
    return TRUE;
}

gboolean weather_fetcher_update_with_provider(const char *city, WeatherProvider provider) {
    if (!g_app_context) return FALSE;
    
    // Check cache: don't fetch if we've fetched this provider within the last minute
    // AND we actually have weather data stored
    time_t now = time(NULL);
    time_t last_fetch = g_app_context->last_fetch_time[provider];
    
    if (last_fetch > 0 && (now - last_fetch) < 60) {
        // Only skip fetch if we actually have data AND it's from this provider
        if (g_app_context->weather_data != NULL && 
            g_app_context->cached_data_provider == provider) {
            g_message("Skipping fetch for provider %d - cached data is less than 1 minute old (%ld seconds)",
                      provider, (now - last_fetch));
            return TRUE;  // Return success but don't fetch
        } else {
            g_message("Cache time valid but no data for provider %d (current data from provider %d) - fetching anyway", 
                      provider, g_app_context->cached_data_provider);
        }
    }
    
    gboolean success = FALSE;
    
    switch (provider) {
        case PROVIDER_BRIGHTSKY:
            success = fetch_brightsky(city);
            break;
        case PROVIDER_ANSIWEATHER:
        default:
            success = fetch_ansiweather(city);
            break;
    }
    
    // Update cache timestamp and provider tracking on successful fetch
    if (success) {
        g_app_context->last_fetch_time[provider] = now;
        g_app_context->cached_data_provider = provider;
    }
    
    return success;
}

gboolean weather_fetcher_update(const char *city) {
    // Use the configured provider
    WeatherProvider provider = PROVIDER_ANSIWEATHER;
    if (g_app_context && g_app_context->config) {
        provider = g_app_context->config->provider;
    }
    g_message("weather_fetcher_update: Using provider %d for city %s", provider, city);
    return weather_fetcher_update_with_provider(city, provider);
}

void weather_fetcher_free_data(WeatherData *data) {
    if (!data) return;
    
    g_free(data->condition_icon);
    g_free(data->condition_text);
    g_free(data->city);
    g_free(data->wind_direction);
    g_free(data->sunrise);
    g_free(data->sunset);
    g_free(data->raw_output);
    g_free(data->error_message);
    g_free(data);
}

WeatherData* weather_fetcher_copy_data(const WeatherData *data) {
    if (!data) return NULL;
    
    WeatherData *copy = g_new0(WeatherData, 1);
    copy->temperature = data->temperature;
    copy->feels_like = data->feels_like;
    copy->uvi = data->uvi;
    copy->wind_speed = data->wind_speed;
    copy->humidity = data->humidity;
    copy->pressure = data->pressure;
    copy->last_update = data->last_update;
    
    if (data->condition_icon) copy->condition_icon = g_strdup(data->condition_icon);
    if (data->condition_text) copy->condition_text = g_strdup(data->condition_text);
    if (data->city) copy->city = g_strdup(data->city);
    if (data->wind_direction) copy->wind_direction = g_strdup(data->wind_direction);
    if (data->sunrise) copy->sunrise = g_strdup(data->sunrise);
    if (data->sunset) copy->sunset = g_strdup(data->sunset);
    if (data->raw_output) copy->raw_output = g_strdup(data->raw_output);
    if (data->error_message) copy->error_message = g_strdup(data->error_message);
    
    return copy;
}