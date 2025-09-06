#include "provider_brightsky.h"
#include "geocoding.h"
#include "weather_conditions.h"
#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <json-glib/json-glib.h>


// Map BrightSky condition string to our standardized enum
static WeatherCondition map_brightsky_condition(const char* condition, const char* icon) {
    if (!condition && !icon) {
        return WEATHER_CONDITION_UNKNOWN;
    }
    
    // BrightSky uses icon strings like "clear-day", "clear-night", "partly-cloudy-day", etc.
    gboolean is_day = TRUE;
    if (icon) {
        is_day = !g_str_has_suffix(icon, "-night");
    }
    
    // Check condition field first (values: dry, fog, rain, sleet, snow, hail, thunderstorm)
    if (condition) {
        if (g_strcmp0(condition, "dry") == 0) {
            // Need to check icon for cloud coverage
            if (icon) {
                if (strstr(icon, "clear")) {
                    return is_day ? WEATHER_CONDITION_CLEAR_DAY : WEATHER_CONDITION_CLEAR_NIGHT;
                } else if (strstr(icon, "partly-cloudy")) {
                    return is_day ? WEATHER_CONDITION_PARTLY_CLOUDY_DAY : WEATHER_CONDITION_PARTLY_CLOUDY_NIGHT;
                } else if (strstr(icon, "cloudy") || strstr(icon, "overcast")) {
                    return WEATHER_CONDITION_CLOUDY;
                }
            }
            return is_day ? WEATHER_CONDITION_CLEAR_DAY : WEATHER_CONDITION_CLEAR_NIGHT;
        } else if (g_strcmp0(condition, "fog") == 0) {
            return WEATHER_CONDITION_FOG;
        } else if (g_strcmp0(condition, "rain") == 0) {
            return WEATHER_CONDITION_RAIN;
        } else if (g_strcmp0(condition, "sleet") == 0) {
            return WEATHER_CONDITION_SLEET;
        } else if (g_strcmp0(condition, "snow") == 0) {
            return WEATHER_CONDITION_SNOW;
        } else if (g_strcmp0(condition, "hail") == 0) {
            return WEATHER_CONDITION_HAIL;
        } else if (g_strcmp0(condition, "thunderstorm") == 0) {
            return WEATHER_CONDITION_THUNDERSTORM;
        }
    }
    
    // Fall back to icon-based detection
    if (icon) {
        if (strstr(icon, "clear")) {
            return is_day ? WEATHER_CONDITION_CLEAR_DAY : WEATHER_CONDITION_CLEAR_NIGHT;
        } else if (strstr(icon, "partly-cloudy")) {
            return is_day ? WEATHER_CONDITION_PARTLY_CLOUDY_DAY : WEATHER_CONDITION_PARTLY_CLOUDY_NIGHT;
        } else if (strstr(icon, "cloudy") || strstr(icon, "overcast")) {
            return WEATHER_CONDITION_CLOUDY;
        } else if (strstr(icon, "rain")) {
            return WEATHER_CONDITION_RAIN;
        } else if (strstr(icon, "snow")) {
            return WEATHER_CONDITION_SNOW;
        } else if (strstr(icon, "sleet")) {
            return WEATHER_CONDITION_SLEET;
        } else if (strstr(icon, "thunderstorm")) {
            return WEATHER_CONDITION_THUNDERSTORM;
        } else if (strstr(icon, "fog")) {
            return WEATHER_CONDITION_FOG;
        }
    }
    
    return WEATHER_CONDITION_UNKNOWN;
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
    
    char *json_response = network_fetch_json(url);
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
    
    // Map condition to our enum
    const char *icon = json_object_get_string_member(weather, "icon");
    // Extract precipitation data
    if (json_object_has_member(weather, "precipitation")) {
        (*data)->precipitation_accumulation = json_object_get_double_member(weather, "precipitation");
    }
    
    if (json_object_has_member(weather, "precipitation_probability")) {
        double prob = json_object_get_double_member(weather, "precipitation_probability");
        if (!isnan(prob)) {  // Check for null/NaN
            (*data)->precipitation_probability = prob;  // Already in percentage
        }
    }
    
    // Note: BrightSky doesn't differentiate between rain/snow intensity
    // The 'precipitation' field is total amount in mm
    
    const char *condition_str = json_object_has_member(weather, "condition") ? 
                                json_object_get_string_member(weather, "condition") : NULL;
    
    (*data)->condition = map_brightsky_condition(condition_str, icon);
    (*data)->condition_text = g_strdup(weather_condition_get_display_text((*data)->condition));
    
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