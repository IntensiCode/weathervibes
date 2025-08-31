#include "provider_openweather.h"
#include "app.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <json-glib/json-glib.h>

// Fetch JSON from URL using curl command
static char* fetch_json_from_url(const char *url) {
    char *command = g_strdup_printf("curl -s \"%s\" 2>&1", url); // Include stderr for debugging
    
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

// Convert OpenWeather condition ID to emoji icon
static const char* get_weather_icon(int weather_id, const char* icon_code) {
    // Use icon_code to determine day/night
    gboolean is_day = (icon_code && g_str_has_suffix(icon_code, "d"));
    
    // Weather condition mapping based on OpenWeather condition IDs
    if (weather_id >= 200 && weather_id < 300) {
        return "⛈️"; // Thunderstorm
    } else if (weather_id >= 300 && weather_id < 400) {
        return "🌦️"; // Drizzle
    } else if (weather_id >= 500 && weather_id < 600) {
        return "🌧️"; // Rain
    } else if (weather_id >= 600 && weather_id < 700) {
        return "❄️"; // Snow
    } else if (weather_id >= 700 && weather_id < 800) {
        return "🌫️"; // Atmosphere (fog, mist, etc.)
    } else if (weather_id == 800) {
        return is_day ? "☀️" : "🌙"; // Clear
    } else if (weather_id == 801) {
        return is_day ? "🌤️" : "☁️"; // Few clouds
    } else if (weather_id == 802) {
        return "⛅"; // Scattered clouds
    } else if (weather_id >= 803) {
        return "☁️"; // Broken/overcast clouds
    }
    
    return "❓"; // Unknown
}

static gboolean openweather_fetch_weather(const char* city, WeatherData** data) {
    if (!city || !data) {
        return FALSE;
    }
    
    // Check if we have an API key
    if (!g_app_context || !g_app_context->config || !g_app_context->config->openweather_api_key || 
        strlen(g_app_context->config->openweather_api_key) == 0) {
        g_warning("OpenWeather API key not configured");
        return FALSE;
    }
    
    log_debug("OpenWeather: Using API key: %.8s...", g_app_context->config->openweather_api_key);
    
    *data = weather_data_new();
    if (!*data) {
        return FALSE;
    }
    
    // Build API URL using g_strdup_printf to avoid buffer issues
    char *encoded_city = g_uri_escape_string(city, NULL, TRUE);
    char *url = g_strdup_printf("https://api.openweathermap.org/data/2.5/weather?"
                                 "q=%s&appid=%s&units=metric", 
                                 encoded_city, 
                                 g_app_context->config->openweather_api_key);
    g_free(encoded_city);
    
    log_debug("OpenWeather: Fetching URL: %s", url);
    
    char *json_response = fetch_json_from_url(url);
    g_free(url);  // Free the allocated URL string
    
    if (!json_response) {
        log_warn("OpenWeather: Failed to fetch JSON response");
        weather_data_free(*data);
        *data = NULL;
        return FALSE;
    }
    
    // Parse JSON response
    JsonParser *parser = json_parser_new();
    GError *error = NULL;
    if (!json_parser_load_from_data(parser, json_response, -1, &error)) {
        if (error) {
            g_warning("Failed to parse OpenWeather JSON: %s", error->message);
            g_error_free(error);
        }
        g_object_unref(parser);
        g_free(json_response);
        weather_data_free(*data);
        *data = NULL;
        return FALSE;
    }
    
    JsonNode *root = json_parser_get_root(parser);
    if (!JSON_NODE_HOLDS_OBJECT(root)) {
        g_object_unref(parser);
        g_free(json_response);
        weather_data_free(*data);
        *data = NULL;
        return FALSE;
    }
    
    JsonObject *root_obj = json_node_get_object(root);
    
    // Check for API error
    if (json_object_has_member(root_obj, "cod")) {
        int cod = json_object_get_int_member(root_obj, "cod");
        if (cod != 200) {
            const char *message = json_object_get_string_member(root_obj, "message");
            g_warning("OpenWeather API error %d: %s", cod, message ? message : "Unknown error");
            
            // Create error data
            (*data)->temperature = -999.0;
            (*data)->condition_text = g_strdup("API Error");
            (*data)->condition_icon = g_strdup("⚠️");
            (*data)->error_message = g_strdup_printf(
                "OpenWeather API Error %d: %s\n\n"
                "Please check:\n"
                "• API key is valid\n"
                "• City name spelling\n"
                "• Internet connection",
                cod, message ? message : "Unknown error"
            );
            (*data)->city = g_strdup(city);
            (*data)->last_update = time(NULL);
            (*data)->raw_output = g_strdup(json_response);
            
            g_object_unref(parser);
            g_free(json_response);
            return FALSE;
        }
    }
    
    // Extract city name
    if (json_object_has_member(root_obj, "name")) {
        (*data)->city = g_strdup(json_object_get_string_member(root_obj, "name"));
    } else {
        (*data)->city = g_strdup(city);
    }
    
    // Extract main weather data
    JsonObject *main = json_object_get_object_member(root_obj, "main");
    if (main) {
        (*data)->temperature = json_object_get_double_member(main, "temp");
        (*data)->feels_like = json_object_get_double_member(main, "feels_like");
        (*data)->humidity = (int)json_object_get_int_member(main, "humidity");
        (*data)->pressure = (int)json_object_get_int_member(main, "pressure");
    }
    
    // Extract wind data
    if (json_object_has_member(root_obj, "wind")) {
        JsonObject *wind = json_object_get_object_member(root_obj, "wind");
        if (wind) {
            (*data)->wind_speed = json_object_get_double_member(wind, "speed");
            
            // Convert wind direction from degrees to compass direction
            if (json_object_has_member(wind, "deg")) {
                int deg = (int)json_object_get_int_member(wind, "deg");
                const char *directions[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
                                          "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
                int index = (int)((deg + 11.25) / 22.5) % 16;
                (*data)->wind_direction = g_strdup(directions[index]);
            }
        }
    }
    
    // Extract weather condition
    if (json_object_has_member(root_obj, "weather")) {
        JsonArray *weather_array = json_object_get_array_member(root_obj, "weather");
        if (json_array_get_length(weather_array) > 0) {
            JsonObject *weather = json_array_get_object_element(weather_array, 0);
            
            const char *description = json_object_get_string_member(weather, "description");
            const char *icon_code = json_object_get_string_member(weather, "icon");
            int weather_id = (int)json_object_get_int_member(weather, "id");
            
            if (description) {
                // Capitalize first letter of description
                char *capitalized = g_strdup(description);
                if (capitalized[0]) {
                    capitalized[0] = g_ascii_toupper(capitalized[0]);
                }
                (*data)->condition_text = capitalized;
            }
            
            (*data)->condition_icon = g_strdup(get_weather_icon(weather_id, icon_code));
        }
    }
    
    // Extract sunrise/sunset
    if (json_object_has_member(root_obj, "sys")) {
        JsonObject *sys = json_object_get_object_member(root_obj, "sys");
        if (sys) {
            if (json_object_has_member(sys, "sunrise")) {
                time_t sunrise = (time_t)json_object_get_int_member(sys, "sunrise");
                struct tm *tm = localtime(&sunrise);
                (*data)->sunrise = g_strdup_printf("%02d:%02d", tm->tm_hour, tm->tm_min);
            }
            
            if (json_object_has_member(sys, "sunset")) {
                time_t sunset = (time_t)json_object_get_int_member(sys, "sunset");
                struct tm *tm = localtime(&sunset);
                (*data)->sunset = g_strdup_printf("%02d:%02d", tm->tm_hour, tm->tm_min);
            }
        }
    }
    
    // OpenWeather doesn't include UVI in current weather API
    (*data)->uvi = 0.0;
    
    (*data)->last_update = time(NULL);
    (*data)->raw_output = g_strdup(json_response);
    
    g_object_unref(parser);
    g_free(json_response);
    return TRUE;
}

const WeatherProviderInterface openweather_provider = {
    .name = "OpenWeather",
    .description = "Global weather data via OpenWeather API (requires API key)",
    .fetch_weather = openweather_fetch_weather
};