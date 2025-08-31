#include "provider_tomorrow.h"
#include "app.h"
#include "logger.h"
#include "geocoding.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <json-glib/json-glib.h>

// Fetch JSON from URL using curl command
static char* fetch_json_from_url(const char *url) {
    char *command = g_strdup_printf("curl -s \"%s\" 2>&1", url);
    
    FILE *pipe = popen(command, "r");
    g_free(command);
    
    if (!pipe) {
        log_error("Failed to create curl process for URL: %s", url);
        return NULL;
    }
    
    char buffer[1024];
    GString *output = g_string_new("");
    
    while (fgets(buffer, sizeof(buffer), pipe)) {
        g_string_append(output, buffer);
    }
    
    int ret = pclose(pipe);
    
    // Log the full response for debugging
    log_debug("curl exit code: %d", ret);
    if (output->len > 0) {
        log_debug("curl full response: %s", output->str);
    } else {
        log_warn("curl returned empty response for URL: %s", url);
    }
    
    if (ret != 0) {
        log_error("curl failed with exit code %d for URL: %s", ret, url);
        g_string_free(output, TRUE);
        return NULL;
    }
    
    char *result = g_strdup(output->str);
    g_string_free(output, TRUE);
    return result;
}

// Convert Tomorrow.io weather code to emoji icon and description
static void get_weather_condition(int weather_code, const char **icon, const char **description) {
    switch (weather_code) {
        case 1000: // Clear
            *icon = "☀️";
            *description = "Clear";
            break;
        case 1100: // Mostly Clear
            *icon = "🌤️";
            *description = "Mostly Clear";
            break;
        case 1101: // Partly Cloudy
            *icon = "⛅";
            *description = "Partly Cloudy";
            break;
        case 1102: // Mostly Cloudy
            *icon = "🌥️";
            *description = "Mostly Cloudy";
            break;
        case 1001: // Cloudy
            *icon = "☁️";
            *description = "Cloudy";
            break;
        case 2000: // Fog
        case 2100: // Light Fog
            *icon = "🌫️";
            *description = "Fog";
            break;
        case 4000: // Drizzle
        case 4001: // Rain
        case 4200: // Light Rain
        case 4201: // Heavy Rain
            *icon = "🌧️";
            *description = "Rain";
            break;
        case 5000: // Snow
        case 5001: // Flurries
        case 5100: // Light Snow
        case 5101: // Heavy Snow
            *icon = "❄️";
            *description = "Snow";
            break;
        case 6000: // Freezing Drizzle
        case 6001: // Freezing Rain
        case 6200: // Light Freezing Rain
        case 6201: // Heavy Freezing Rain
            *icon = "🌨️";
            *description = "Freezing Rain";
            break;
        case 7000: // Ice Pellets
        case 7101: // Heavy Ice Pellets
        case 7102: // Light Ice Pellets
            *icon = "🌨️";
            *description = "Ice Pellets";
            break;
        case 8000: // Thunderstorm
            *icon = "⛈️";
            *description = "Thunderstorm";
            break;
        default:
            *icon = "❓";
            *description = "Unknown";
            break;
    }
}

static gboolean tomorrow_fetch_weather(const char* city, WeatherData** data) {
    log_debug("Tomorrow.io: fetch_weather called for city: %s", city ? city : "NULL");
    
    if (!city || !data) {
        log_error("Tomorrow.io: Invalid parameters - city=%p, data=%p", city, data);
        return FALSE;
    }
    
    // Check if we have an API key
    if (!g_app_context) {
        log_error("Tomorrow.io: g_app_context is NULL");
        return FALSE;
    }
    if (!g_app_context->config) {
        log_error("Tomorrow.io: g_app_context->config is NULL");
        return FALSE;
    }
    if (!g_app_context->config->tomorrow_api_key) {
        log_error("Tomorrow.io: API key is NULL");
        return FALSE;
    }
    if (strlen(g_app_context->config->tomorrow_api_key) == 0) {
        log_error("Tomorrow.io: API key is empty string");
        return FALSE;
    }
    
    log_debug("Tomorrow.io: Using API key: %.8s...", g_app_context->config->tomorrow_api_key);
    
    // Geocode the city to get coordinates
    GeoLocation *location = geocode_location(city);
    if (!location) {
        g_warning("Failed to geocode city: %s", city);
        return FALSE;
    }
    
    double lat = location->latitude;
    double lon = location->longitude;
    
    *data = weather_data_new();
    if (!*data) {
        geo_location_free(location);
        return FALSE;
    }
    
    // Build API URL using coordinates
    // Use g_ascii_formatd to ensure locale-independent formatting (always uses . as decimal separator)
    char lat_str[32], lon_str[32];
    g_ascii_formatd(lat_str, sizeof(lat_str), "%.4f", lat);
    g_ascii_formatd(lon_str, sizeof(lon_str), "%.4f", lon);
    
    char *url = g_strdup_printf("https://api.tomorrow.io/v4/weather/realtime?"
                                 "location=%s,%s&apikey=%s", 
                                 lat_str, lon_str, 
                                 g_app_context->config->tomorrow_api_key);
    
    log_debug("Tomorrow.io: Fetching URL: %s", url);
    
    char *json_response = fetch_json_from_url(url);
    g_free(url);
    
    if (!json_response) {
        log_warn("Tomorrow.io: Failed to fetch JSON response");
        weather_data_free(*data);
        *data = NULL;
        geo_location_free(location);
        return FALSE;
    }
    
    // Parse JSON response
    JsonParser *parser = json_parser_new();
    GError *error = NULL;
    if (!json_parser_load_from_data(parser, json_response, -1, &error)) {
        if (error) {
            g_warning("Failed to parse Tomorrow.io JSON: %s", error->message);
            g_error_free(error);
        }
        g_object_unref(parser);
        g_free(json_response);
        weather_data_free(*data);
        *data = NULL;
        geo_location_free(location);
        return FALSE;
    }
    
    JsonNode *root = json_parser_get_root(parser);
    if (!JSON_NODE_HOLDS_OBJECT(root)) {
        g_object_unref(parser);
        g_free(json_response);
        weather_data_free(*data);
        *data = NULL;
        geo_location_free(location);
        return FALSE;
    }
    
    JsonObject *root_obj = json_node_get_object(root);
    
    // Check for API error
    if (json_object_has_member(root_obj, "code")) {
        int code = json_object_get_int_member(root_obj, "code");
        const char *type = json_object_get_string_member(root_obj, "type");
        const char *message = json_object_get_string_member(root_obj, "message");
        g_warning("Tomorrow.io API error %d (%s): %s", code, type ? type : "Unknown", message ? message : "Unknown error");
        
        // Create error data
        (*data)->temperature = -999.0;
        (*data)->condition_text = g_strdup("API Error");
        (*data)->condition_icon = g_strdup("⚠️");
        (*data)->error_message = g_strdup_printf(
            "Tomorrow.io API Error %d: %s\n\n"
            "Please check:\n"
            "• API key is valid\n"
            "• City name spelling\n"
            "• Internet connection",
            code, message ? message : "Unknown error"
        );
        (*data)->city = g_strdup(city);
        (*data)->last_update = time(NULL);
        (*data)->raw_output = g_strdup(json_response);
        
        g_object_unref(parser);
        g_free(json_response);
        geo_location_free(location);
        return FALSE;
    }
    
    // Extract city name (use what was provided or from geocoding)
    (*data)->city = g_strdup(location->city ? location->city : city);
    geo_location_free(location);
    
    // Extract weather data from "data.values" object
    if (json_object_has_member(root_obj, "data")) {
        JsonObject *data_obj = json_object_get_object_member(root_obj, "data");
        if (data_obj && json_object_has_member(data_obj, "values")) {
            JsonObject *values = json_object_get_object_member(data_obj, "values");
            
            if (values) {
                (*data)->temperature = json_object_get_double_member(values, "temperature");
                (*data)->feels_like = json_object_get_double_member(values, "temperatureApparent");
                (*data)->humidity = (int)json_object_get_int_member(values, "humidity");
                (*data)->pressure = (int)json_object_get_double_member(values, "pressureSeaLevel");
                (*data)->wind_speed = json_object_get_double_member(values, "windSpeed");
                
                // Wind direction conversion
                if (json_object_has_member(values, "windDirection")) {
                    int deg = (int)json_object_get_int_member(values, "windDirection");
                    const char *directions[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
                                              "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
                    int index = (int)((deg + 11.25) / 22.5) % 16;
                    (*data)->wind_direction = g_strdup(directions[index]);
                }
                
                // UV Index
                if (json_object_has_member(values, "uvIndex")) {
                    (*data)->uvi = json_object_get_double_member(values, "uvIndex");
                }
                
                // Weather condition from weather code
                if (json_object_has_member(values, "weatherCode")) {
                    int weather_code = (int)json_object_get_int_member(values, "weatherCode");
                    const char *icon = NULL;
                    const char *description = NULL;
                    get_weather_condition(weather_code, &icon, &description);
                    (*data)->condition_icon = g_strdup(icon);
                    (*data)->condition_text = g_strdup(description);
                }
                
                // Additional fields
                if (json_object_has_member(values, "visibility")) {
                    double visibility_km = json_object_get_double_member(values, "visibility");
                    // Store visibility in metadata or create new field if needed
                }
                
                if (json_object_has_member(values, "cloudCover")) {
                    int cloud_cover = (int)json_object_get_int_member(values, "cloudCover");
                    // Store cloud cover percentage if needed
                }
            }
        }
    }
    
    (*data)->last_update = time(NULL);
    (*data)->raw_output = g_strdup(json_response);
    
    g_object_unref(parser);
    g_free(json_response);
    return TRUE;
}

const WeatherProviderInterface tomorrow_provider = {
    .name = "Tomorrow.io",
    .description = "Global weather data via Tomorrow.io API (requires API key)",
    .fetch_weather = tomorrow_fetch_weather
};