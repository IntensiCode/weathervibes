#include "provider_tomorrow.h"
#include "app.h"
#include "logger.h"
#include "geocoding.h"
#include "weather_conditions.h"
#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <json-glib/json-glib.h>


// Map Tomorrow.io weather code to our standardized enum
static WeatherCondition map_tomorrow_condition(int weather_code, gboolean is_day) {
    switch (weather_code) {
        case 0:     // Unknown
            return WEATHER_CONDITION_UNKNOWN;
        case 1000:  // Clear
            return is_day ? WEATHER_CONDITION_CLEAR_DAY : WEATHER_CONDITION_CLEAR_NIGHT;
        case 1100:  // Mostly Clear
            return is_day ? WEATHER_CONDITION_PARTLY_CLOUDY_DAY : WEATHER_CONDITION_PARTLY_CLOUDY_NIGHT;
        case 1101:  // Partly Cloudy
            return is_day ? WEATHER_CONDITION_PARTLY_CLOUDY_DAY : WEATHER_CONDITION_PARTLY_CLOUDY_NIGHT;
        case 1102:  // Mostly Cloudy
            return WEATHER_CONDITION_CLOUDY;
        case 1001:  // Cloudy
            return WEATHER_CONDITION_OVERCAST;
        case 2000:  // Fog
        case 2100:  // Light Fog
            return WEATHER_CONDITION_FOG;
        case 4000:  // Drizzle
            return WEATHER_CONDITION_DRIZZLE;
        case 4001:  // Rain
            return WEATHER_CONDITION_RAIN;
        case 4200:  // Light Rain
            return WEATHER_CONDITION_LIGHT_RAIN;
        case 4201:  // Heavy Rain
            return WEATHER_CONDITION_HEAVY_RAIN;
        case 5000:  // Snow
            return WEATHER_CONDITION_SNOW;
        case 5001:  // Flurries
        case 5100:  // Light Snow
            return WEATHER_CONDITION_LIGHT_SNOW;
        case 5101:  // Heavy Snow
            return WEATHER_CONDITION_HEAVY_SNOW;
        case 6000:  // Freezing Drizzle
        case 6001:  // Freezing Rain
        case 6200:  // Light Freezing Rain
        case 6201:  // Heavy Freezing Rain
            return WEATHER_CONDITION_SLEET;  // Using sleet for freezing rain
        case 7000:  // Ice Pellets
        case 7101:  // Heavy Ice Pellets
        case 7102:  // Light Ice Pellets
            return WEATHER_CONDITION_HAIL;
        case 8000:  // Thunderstorm
            return WEATHER_CONDITION_THUNDERSTORM;
        default:
            return WEATHER_CONDITION_UNKNOWN;
    }
}


static gboolean fetch_tomorrow_forecast(const char* city, const char* api_key, double lat, double lon, WeatherData* data);

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
    
    char *json_response = network_fetch_json(url);
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
                
                // Precipitation data - Tomorrow.io has the most comprehensive data
                if (json_object_has_member(values, "precipitationProbability")) {
                    (*data)->precipitation_probability = json_object_get_double_member(values, "precipitationProbability");
                }
                
                if (json_object_has_member(values, "rainIntensity")) {
                    (*data)->rain_intensity = json_object_get_double_member(values, "rainIntensity");
                }
                
                if (json_object_has_member(values, "snowIntensity")) {
                    (*data)->snow_intensity = json_object_get_double_member(values, "snowIntensity");
                }
                
                if (json_object_has_member(values, "sleetIntensity")) {
                    (*data)->sleet_intensity = json_object_get_double_member(values, "sleetIntensity");
                }
                
                if (json_object_has_member(values, "freezingRainIntensity")) {
                    (*data)->freezing_rain_intensity = json_object_get_double_member(values, "freezingRainIntensity");
                }
                
                // Weather condition from weather code
                if (json_object_has_member(values, "weatherCode")) {
                    int weather_code = (int)json_object_get_int_member(values, "weatherCode");
                    
                    // Determine if it's day or night (we can use a simple heuristic for now)
                    time_t now = time(NULL);
                    struct tm *tm_info = localtime(&now);
                    gboolean is_day = (tm_info->tm_hour >= 6 && tm_info->tm_hour < 18);
                    
                    // Set the enum value
                    (*data)->condition = map_tomorrow_condition(weather_code, is_day);
                    (*data)->condition_text = g_strdup(weather_condition_get_display_text((*data)->condition));
                }
                
                // Additional fields available but not currently used:
                // - visibility (km)
                // - cloudCover (percentage)
            }
        }
    }
    
    (*data)->last_update = time(NULL);
    (*data)->raw_output = g_strdup(json_response);
    
    g_object_unref(parser);
    g_free(json_response);
    
    // Fetch forecast data
    fetch_tomorrow_forecast(city, g_app_context->config->tomorrow_api_key, lat, lon, *data);
    
    geo_location_free(location);
    return TRUE;
}

static gboolean fetch_tomorrow_forecast(const char* city, const char* api_key, double lat, double lon, WeatherData* data) {
    if (!data || !api_key) {
        return FALSE;
    }
    
    log_debug("Tomorrow.io: Fetching forecast for %s", city);
    
    // Use g_ascii_formatd to ensure locale-independent formatting
    char lat_str[32], lon_str[32];
    g_ascii_formatd(lat_str, sizeof(lat_str), "%.4f", lat);
    g_ascii_formatd(lon_str, sizeof(lon_str), "%.4f", lon);
    
    // Tomorrow.io forecast API endpoint - get daily forecast
    char *url = g_strdup_printf("https://api.tomorrow.io/v4/weather/forecast?"
                                "location=%s,%s&timesteps=1d&apikey=%s",
                                lat_str, lon_str, api_key);
    
    log_debug("Tomorrow.io forecast URL: %s", url);
    
    char *json_response = network_fetch_json(url);
    g_free(url);
    
    if (!json_response) {
        log_warn("Tomorrow.io: Failed to fetch forecast");
        return FALSE;
    }
    
    // Parse JSON response
    JsonParser *parser = json_parser_new();
    GError *error = NULL;
    if (!json_parser_load_from_data(parser, json_response, -1, &error)) {
        if (error) {
            log_warn("Failed to parse Tomorrow.io forecast JSON: %s", error->message);
            g_error_free(error);
        }
        g_object_unref(parser);
        g_free(json_response);
        return FALSE;
    }
    
    JsonNode *root = json_parser_get_root(parser);
    if (!JSON_NODE_HOLDS_OBJECT(root)) {
        g_object_unref(parser);
        g_free(json_response);
        return FALSE;
    }
    
    JsonObject *root_obj = json_node_get_object(root);
    
    // Check for timelines.daily array
    if (json_object_has_member(root_obj, "timelines")) {
        JsonObject *timelines = json_object_get_object_member(root_obj, "timelines");
        if (timelines && json_object_has_member(timelines, "daily")) {
            JsonArray *daily = json_object_get_array_member(timelines, "daily");
            
            if (daily) {
                guint forecast_count = MIN(json_array_get_length(daily), 5);
                
                if (forecast_count > 0) {
                    data->forecast = g_new0(ForecastDay, forecast_count);
                    data->forecast_days = forecast_count;
                    
                    // Initialize precipitation fields to -1 (not available)
                    for (guint j = 0; j < forecast_count; j++) {
                        data->forecast[j].precipitation_probability = -1;
                        data->forecast[j].precipitation_amount = -1;
                    }
                    
                    for (guint i = 0; i < forecast_count; i++) {
                        JsonObject *day_obj = json_array_get_object_element(daily, i);
                        
                        // Get the date
                        const char *time_str = json_object_get_string_member(day_obj, "time");
                        if (time_str) {
                            // Parse the ISO date string
                            struct tm tm = {0};
                            sscanf(time_str, "%d-%d-%dT", &tm.tm_year, &tm.tm_mon, &tm.tm_mday);
                            tm.tm_year -= 1900;
                            tm.tm_mon -= 1;
                            data->forecast[i].date = mktime(&tm);
                        }
                        
                        // Get values object
                        if (json_object_has_member(day_obj, "values")) {
                            JsonObject *values = json_object_get_object_member(day_obj, "values");
                            
                            // Get min/max temperatures
                            if (json_object_has_member(values, "temperatureMin")) {
                                data->forecast[i].temp_min = json_object_get_double_member(values, "temperatureMin");
                            }
                            if (json_object_has_member(values, "temperatureMax")) {
                                data->forecast[i].temp_max = json_object_get_double_member(values, "temperatureMax");
                            }
                            
                            // Get weather code for the day
                            if (json_object_has_member(values, "weatherCodeMax")) {
                                int weather_code = json_object_get_int_member(values, "weatherCodeMax");
                                data->forecast[i].condition = map_tomorrow_condition(weather_code, TRUE);
                                data->forecast[i].condition_text = g_strdup(weather_condition_get_display_text(data->forecast[i].condition));
                            }
                            
                            // Get precipitation probability and amount
                            if (json_object_has_member(values, "precipitationProbabilityMax")) {
                                data->forecast[i].precipitation_probability = json_object_get_double_member(values, "precipitationProbabilityMax");
                            } else {
                                data->forecast[i].precipitation_probability = -1;
                            }
                            
                            if (json_object_has_member(values, "precipitationAccumulation")) {
                                data->forecast[i].precipitation_amount = json_object_get_double_member(values, "precipitationAccumulation");
                            } else {
                                data->forecast[i].precipitation_amount = -1;
                            }
                        }
                    }
                    
                    log_info("Tomorrow.io: Successfully fetched %d day forecast", forecast_count);
                }
            }
        }
    }
    
    g_object_unref(parser);
    g_free(json_response);
    return TRUE;
}

const WeatherProviderInterface tomorrow_provider = {
    .name = "Tomorrow.io",
    .description = "Global weather data via Tomorrow.io API (requires API key)",
    .fetch_weather = tomorrow_fetch_weather
};