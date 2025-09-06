#include "provider_ansiweather.h"
#include "weather_conditions.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <regex.h>

static char* strip_ansi_codes(const char *input) {
    regex_t regex;
    regmatch_t match;
    GString *result = g_string_new("");
    const char *p = input;
    
    if (regcomp(&regex, "\x1b\\[[0-9;]*m", REG_EXTENDED) != 0) {
        return g_strdup(input);
    }
    
    // Build result string without ANSI codes
    while (regexec(&regex, p, 1, &match, 0) == 0) {
        // Append text before the match
        g_string_append_len(result, p, match.rm_so);
        // Skip the ANSI code
        p += match.rm_eo;
    }
    // Append remaining text
    g_string_append(result, p);
    
    regfree(&regex);
    return g_string_free(result, FALSE);
}

static char* extract_value(const char *text, const char *prefix) {
    char *start = strstr(text, prefix);
    if (!start) return NULL;
    
    start += strlen(prefix);
    while (*start == ' ' || *start == ':') start++;
    
    char *end = strstr(start, " -");
    if (!end) end = start + strlen(start);
    
    int len = end - start;
    char *result = g_strndup(start, len);
    g_strstrip(result);
    return result;
}

static gboolean ansiweather_fetch_weather(const char* city, WeatherData** data) {
    if (!city || !data) {
        return FALSE;
    }
    
    *data = weather_data_new();
    if (!*data) {
        return FALSE;
    }
    
    char *command = g_strdup_printf("ansiweather -l \"%s\" -s true -d true -H true 2>&1", city);
    
    FILE *pipe = popen(command, "r");
    g_free(command);
    
    if (!pipe) {
        weather_data_free(*data);
        *data = NULL;
        return FALSE;
    }
    
    char buffer[1024];
    GString *output = g_string_new("");
    
    while (fgets(buffer, sizeof(buffer), pipe)) {
        g_string_append(output, buffer);
    }
    
    int ret = pclose(pipe);
    if (ret != 0) {
        g_string_free(output, TRUE);
        weather_data_free(*data);
        *data = NULL;
        return FALSE;
    }
    
    (*data)->raw_output = g_strdup(output->str);
    (*data)->last_update = time(NULL);
    
    // Parse the output using the clean text
    char *clean = strip_ansi_codes(output->str);
    
    // Extract city and temperature from "Weather in Berlin: 22 °C"
    char *weather_start = strstr(clean, "Weather in ");
    gboolean success = FALSE;
    
    if (weather_start) {
        char *city_start = weather_start + strlen("Weather in ");
        char *colon = strchr(city_start, ':');
        if (colon) {
            // Extract city
            int city_len = colon - city_start;
            (*data)->city = g_strndup(city_start, city_len);
            g_strstrip((*data)->city);
            
            // Extract temperature (after colon)
            char *temp_start = colon + 1;
            while (*temp_start == ' ') temp_start++;
            char *temp_end = strstr(temp_start, " ");
            if (temp_end) {
                int temp_len = temp_end - temp_start;
                char *temp_str = g_strndup(temp_start, temp_len);
                (*data)->temperature = atof(temp_str);
                g_free(temp_str);
                success = TRUE;
            }
        }
    }
    
    if (!(*data)->city) {
        (*data)->city = g_strdup(city);
    }
    
    // No feels_like in ansiweather output
    (*data)->feels_like = (*data)->temperature;
    
    // Extract humidity (format: "Humidity: 72%")
    char *humidity_str = extract_value(clean, "Humidity");
    if (humidity_str) {
        (*data)->humidity = atoi(humidity_str);
        g_free(humidity_str);
    }
    
    // Extract wind speed (format: "Wind: 1.34 m/s S")
    char *wind_str = extract_value(clean, "Wind");
    if (wind_str) {
        char *speed_end = strchr(wind_str, ' ');
        if (speed_end) {
            *speed_end = '\0';
            (*data)->wind_speed = atof(wind_str);
            
            // Skip "m/s" and get direction
            char *dir_start = strstr(speed_end + 1, "m/s");
            if (dir_start) {
                dir_start += 3;
                while (*dir_start == ' ') dir_start++;
                if (*dir_start) {
                    (*data)->wind_direction = g_strdup(dir_start);
                    g_strstrip((*data)->wind_direction);
                }
            }
        }
        g_free(wind_str);
    }
    
    // Extract pressure (format: "Pressure: 1013 hPa")
    char *pressure_str = extract_value(clean, "Pressure");
    if (pressure_str) {
        (*data)->pressure = atoi(pressure_str);
        g_free(pressure_str);
    }
    
    // Extract UV index (format: "UVI: 4.52")
    char *uvi_str = extract_value(clean, "UVI");
    if (uvi_str) {
        (*data)->uvi = atof(uvi_str);
        g_free(uvi_str);
    }
    
    // AnsiWeather doesn't provide sunrise/sunset
    (*data)->sunrise = NULL;
    (*data)->sunset = NULL;
    
    // Parse weather symbol from ansiweather output to determine condition
    // Ansiweather shows symbols like ☀, ☁, 🌧, ❄, etc after temperature
    (*data)->condition = WEATHER_CONDITION_UNKNOWN;
    
    // Look for weather symbols in the output
    if (strstr(clean, "☀")) {
        // Determine day/night based on current time
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        gboolean is_day = (tm->tm_hour >= 6 && tm->tm_hour < 20);
        (*data)->condition = is_day ? WEATHER_CONDITION_CLEAR_DAY : WEATHER_CONDITION_CLEAR_NIGHT;
        (*data)->condition_text = g_strdup("Clear");
    } else if (strstr(clean, "☁")) {
        (*data)->condition = WEATHER_CONDITION_CLOUDY;
        (*data)->condition_text = g_strdup("Cloudy");
    } else if (strstr(clean, "⛅")) {
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        gboolean is_day = (tm->tm_hour >= 6 && tm->tm_hour < 20);
        (*data)->condition = is_day ? WEATHER_CONDITION_PARTLY_CLOUDY_DAY : WEATHER_CONDITION_PARTLY_CLOUDY_NIGHT;
        (*data)->condition_text = g_strdup("Partly Cloudy");
    } else if (strstr(clean, "🌧") || strstr(clean, "🌦")) {
        (*data)->condition = WEATHER_CONDITION_RAIN;
        (*data)->condition_text = g_strdup("Rain");
    } else if (strstr(clean, "⛈") || strstr(clean, "🌩")) {
        (*data)->condition = WEATHER_CONDITION_THUNDERSTORM;
        (*data)->condition_text = g_strdup("Thunderstorm");
    } else if (strstr(clean, "❄") || strstr(clean, "🌨")) {
        (*data)->condition = WEATHER_CONDITION_SNOW;
        (*data)->condition_text = g_strdup("Snow");
    } else if (strstr(clean, "🌫")) {
        (*data)->condition = WEATHER_CONDITION_FOG;
        (*data)->condition_text = g_strdup("Fog");
    } else {
        // Default fallback
        (*data)->condition_text = g_strdup("Current Weather");
    }
    
    
    g_free(clean);
    g_string_free(output, TRUE);
    
    if (!success) {
        weather_data_free(*data);
        *data = NULL;
    }
    
    return success;
}

const WeatherProviderInterface ansiweather_provider = {
    .name = "AnsiWeather (Global)",
    .description = "Global weather data via OpenWeatherMap",
    .fetch_weather = ansiweather_fetch_weather
};