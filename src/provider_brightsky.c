#include "provider_brightsky.h"
#include "geocoding.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <json-glib/json-glib.h>

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


static gboolean brightsky_fetch_weather(const char* city, WeatherData** data) {
    if (!city || !data) {
        return FALSE;
    }
    
    // Geocode the city to get coordinates
    GeoLocation *location = geocode_location(city);
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
    
    geo_location_free(location);
    
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
    
    *data = weather_data_new();
    if (!*data) {
        g_object_unref(parser);
        g_free(json_response);
        return FALSE;
    }
    
    (*data)->city = g_strdup(city);
    
    // Check for critical weather data - temperature is absolutely required
    if (!json_object_has_member(weather, "temperature")) {
        g_warning("Bright Sky API did not return temperature for %s - this location has incomplete data", city);
        
        // Create an error data structure to show the alert
        (*data)->temperature = -999.0; // Signal missing data
        (*data)->condition_text = g_strdup("Data Unavailable");
        (*data)->condition_icon = g_strdup("⚠️");
        (*data)->error_message = g_strdup_printf(
            "Critical weather data missing for %s\n\n"
            "The weather station at this location does not provide temperature data.\n"
            "Please try:\n"
            "• A nearby larger city\n"
            "• A different weather provider\n"
            "• Check the city name spelling",
            city
        );
        (*data)->last_update = time(NULL);
        (*data)->raw_output = g_strdup(json_response);
        
        g_object_unref(parser);
        g_free(json_response);
        return FALSE; // Return failure but with error data stored
    }
    
    (*data)->temperature = json_object_get_double_member(weather, "temperature");
    (*data)->feels_like = (*data)->temperature; // Bright Sky doesn't provide feels_like
    
    // Handle optional fields gracefully
    if (json_object_has_member(weather, "relative_humidity")) {
        (*data)->humidity = (int)json_object_get_int_member(weather, "relative_humidity");
    }
    
    // Check for wind_speed (also critical)
    if (json_object_has_member(weather, "wind_speed")) {
        (*data)->wind_speed = json_object_get_double_member(weather, "wind_speed") / 3.6; // Convert km/h to m/s
    } else if (json_object_has_member(weather, "wind_speed_10")) {
        (*data)->wind_speed = json_object_get_double_member(weather, "wind_speed_10") / 3.6;
    } else {
        // No wind data - also critical
        g_warning("Bright Sky API did not return wind speed for %s", city);
        (*data)->wind_speed = 0.0;
    }
    
    if (json_object_has_member(weather, "pressure_msl")) {
        (*data)->pressure = (int)json_object_get_double_member(weather, "pressure_msl");
    }
    
    // Map icon to condition text
    const char *icon = json_object_get_string_member(weather, "icon");
    if (icon && strstr(icon, "clear")) {
        (*data)->condition_text = g_strdup("Clear");
        (*data)->condition_icon = g_strdup("☀️");
    } else if (icon && strstr(icon, "partly-cloudy")) {
        (*data)->condition_text = g_strdup("Partly Cloudy");
        (*data)->condition_icon = g_strdup("⛅");
    } else if (icon && strstr(icon, "cloudy")) {
        (*data)->condition_text = g_strdup("Cloudy");
        (*data)->condition_icon = g_strdup("☁️");
    } else if (icon && strstr(icon, "rain")) {
        (*data)->condition_text = g_strdup("Rain");
        (*data)->condition_icon = g_strdup("🌧️");
    } else if (icon && strstr(icon, "snow")) {
        (*data)->condition_text = g_strdup("Snow");
        (*data)->condition_icon = g_strdup("❄️");
    } else {
        (*data)->condition_text = g_strdup("Unknown");
        (*data)->condition_icon = g_strdup("❓");
    }
    
    (*data)->last_update = time(NULL);
    (*data)->raw_output = g_strdup(json_response);
    
    g_object_unref(parser);
    g_free(json_response);
    return TRUE;
}

const WeatherProviderInterface brightsky_provider = {
    .name = "Bright Sky (DWD - Germany)",
    .description = "German weather data via DWD",
    .fetch_weather = brightsky_fetch_weather
};