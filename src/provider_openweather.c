#include "provider_openweather.h"
#include "app.h"
#include "weather_provider.h"
#include "weather_conditions.h"
#include "network.h"
#include "logger.h"
#include "json_helpers.h"
#include <json-glib/json-glib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

// Map OpenWeather condition codes to our enum
static WeatherCondition map_openweather_condition(int weather_id, const char *icon_code) {
    // Thunderstorm (2xx)
    if (weather_id >= 200 && weather_id < 300) {
        if (weather_id == 200 || weather_id == 201 || weather_id == 202) {
            return WEATHER_CONDITION_THUNDERSTORM_RAIN;
        }
        return WEATHER_CONDITION_THUNDERSTORM;
    }
    
    // Drizzle (3xx)
    if (weather_id >= 300 && weather_id < 400) {
        return WEATHER_CONDITION_DRIZZLE;
    }
    
    // Rain (5xx)
    if (weather_id >= 500 && weather_id < 600) {
        if (weather_id == 500) return WEATHER_CONDITION_LIGHT_RAIN;
        if (weather_id == 501) return WEATHER_CONDITION_RAIN;
        if (weather_id >= 502 && weather_id <= 504) return WEATHER_CONDITION_HEAVY_RAIN;
        if (weather_id == 511) return WEATHER_CONDITION_SLEET;
        if (weather_id >= 520 && weather_id <= 531) return WEATHER_CONDITION_SHOWERS;
        return WEATHER_CONDITION_RAIN;
    }
    
    // Snow (6xx)
    if (weather_id >= 600 && weather_id < 700) {
        if (weather_id == 600) return WEATHER_CONDITION_LIGHT_SNOW;
        if (weather_id == 601) return WEATHER_CONDITION_SNOW;
        if (weather_id >= 602 && weather_id <= 622) return WEATHER_CONDITION_HEAVY_SNOW;
        if (weather_id >= 611 && weather_id <= 616) return WEATHER_CONDITION_SLEET;
        return WEATHER_CONDITION_SNOW;
    }
    
    // Atmosphere (7xx)
    if (weather_id >= 700 && weather_id < 800) {
        if (weather_id == 701 || weather_id == 721 || weather_id == 741) return WEATHER_CONDITION_MIST;
        if (weather_id == 711) return WEATHER_CONDITION_SMOKE;
        if (weather_id == 731 || weather_id == 761) return WEATHER_CONDITION_DUST;
        if (weather_id == 751) return WEATHER_CONDITION_SAND;
        if (weather_id == 762) return WEATHER_CONDITION_DUST;  // Volcanic ash
        if (weather_id == 771) return WEATHER_CONDITION_HEAVY_RAIN;  // Squalls
        if (weather_id == 781) return WEATHER_CONDITION_TORNADO;
        return WEATHER_CONDITION_FOG;
    }
    
    // Clear/Clouds (800, 80x)
    if (weather_id == 800) {
        // Check icon for day/night
        if (icon_code && strstr(icon_code, "n")) {
            return WEATHER_CONDITION_CLEAR_NIGHT;
        }
        return WEATHER_CONDITION_CLEAR_DAY;
    }
    
    if (weather_id == 801 || weather_id == 802) {
        // Few/scattered clouds
        if (icon_code && strstr(icon_code, "n")) {
            return WEATHER_CONDITION_PARTLY_CLOUDY_NIGHT;
        }
        return WEATHER_CONDITION_PARTLY_CLOUDY_DAY;
    }
    
    if (weather_id == 803) {
        return WEATHER_CONDITION_CLOUDY;
    }
    
    if (weather_id == 804) {
        return WEATHER_CONDITION_OVERCAST;
    }
    
    return WEATHER_CONDITION_UNKNOWN;
}

static const char* get_wind_direction(int degrees) {
    if (degrees < 0 || degrees > 360) return "";
    
    const char* directions[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
                                "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
    int index = (int)((degrees + 11.25) / 22.5) % 16;
    return directions[index];
}

// Forward declaration
static gboolean fetch_openweather_forecast(const char* city, const char* api_key, WeatherData* data);

static gboolean openweather_fetch_weather(const char* city, WeatherData** data) {
    if (!city || !data) {
        return FALSE;
    }
    
    // Get API key
    const char *api_key = NULL;
    if (g_app_context && g_app_context->config) {
        api_key = g_app_context->config->openweather_api_key;
    }
    
    if (!api_key || strlen(api_key) == 0) {
        log_error("OpenWeather requires an API key. Please set it in preferences.");
        *data = weather_data_new();
        (*data)->temperature = -999.0;
        (*data)->condition_text = g_strdup("API Key Required");
        (*data)->error_message = g_strdup(
            "OpenWeather requires an API key.\n\n"
            "To get a free API key:\n"
            "1. Visit openweathermap.org\n"
            "2. Sign up for a free account\n"
            "3. Copy your API key\n"
            "4. Open Weather Vibes preferences\n"
            "5. Paste the key and save"
        );
        (*data)->city = g_strdup(city);
        (*data)->last_update = time(NULL);
        return FALSE;
    }
    
    // Construct API URL with city name
    char *encoded_city = g_uri_escape_string(city, NULL, TRUE);
    char *url = g_strdup_printf(
        "https://api.openweathermap.org/data/2.5/weather?q=%s&appid=%s&units=metric",
        encoded_city, api_key
    );
    g_free(encoded_city);
    
    // Fetch weather data
    char *json_response = network_fetch_json(url);
    g_free(url);
    
    if (!json_response) {
        log_error("Failed to fetch data from OpenWeather");
        return FALSE;
    }
    
    // Parse JSON response using new helpers
    GError *error = NULL;
    JsonObject *root = json_parse_string(json_response, &error);
    
    if (!root) {
        log_error("Failed to parse OpenWeather JSON: %s", 
                 error ? error->message : "Unknown error");
        if (error) g_error_free(error);
        g_free(json_response);
        return FALSE;
    }
    
    // Create weather data structure
    *data = weather_data_new();
    
    // Check for API error
    int cod = 0;
    if (json_get_int(root, "cod", &cod) && cod != 200) {
        char *message = NULL;
        json_get_string(root, "message", &message);
        
        log_warn("OpenWeather API error %d: %s", cod, message ? message : "Unknown error");
        
        // Create error data
        (*data)->temperature = -999.0;
        (*data)->condition_text = g_strdup("API Error");
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
        
        g_free(message);
        json_object_cleanup(root);
        g_free(json_response);
        return FALSE;
    }
    
    // Extract city name
    if (!json_get_string(root, "name", &(*data)->city)) {
        (*data)->city = g_strdup(city);
    }
    
    // Extract main weather data using nested helpers
    json_get_nested_double(root, "main.temp", &(*data)->temperature);
    json_get_nested_double(root, "main.feels_like", &(*data)->feels_like);
    
    int humidity = 0, pressure = 0;
    if (json_get_nested_int(root, "main.humidity", &humidity)) {
        (*data)->humidity = humidity;
    }
    if (json_get_nested_int(root, "main.pressure", &pressure)) {
        (*data)->pressure = pressure;
    }
    
    // Extract wind data
    double wind_speed = 0;
    if (json_get_nested_double(root, "wind.speed", &wind_speed)) {
        (*data)->wind_speed = wind_speed;
    }
    
    int wind_deg = 0;
    if (json_get_nested_int(root, "wind.deg", &wind_deg)) {
        (*data)->wind_direction = g_strdup(get_wind_direction(wind_deg));
    }
    
    // Extract weather condition
    JsonArray *weather_array = json_get_array(root, "weather");
    if (weather_array && json_array_get_length(weather_array) > 0) {
        JsonObject *weather = json_array_get_object_element(weather_array, 0);
        if (weather) {
            char *description = NULL;
            char *icon_code = NULL;
            int weather_id = 0;
            
            json_get_string(weather, "description", &description);
            json_get_string(weather, "icon", &icon_code);
            json_get_int(weather, "id", &weather_id);
            
            if (description) {
                // Capitalize first letter
                if (strlen(description) > 0) {
                    description[0] = g_ascii_toupper(description[0]);
                }
                (*data)->condition_text = description;
            }
            
            (*data)->condition = map_openweather_condition(weather_id, icon_code);
            g_free(icon_code);
        }
    }
    
    // Extract rain data if available
    JsonObject *rain = json_get_object(root, "rain");
    if (rain) {
        double rain_1h = 0, rain_3h = 0;
        if (json_get_double(rain, "1h", &rain_1h)) {
            (*data)->rain_intensity = rain_1h;  // mm/hr
        }
        if (json_get_double(rain, "3h", &rain_3h)) {
            (*data)->precipitation_accumulation = rain_3h;  // mm for 3 hours
        }
    }
    
    // Extract snow data if available
    JsonObject *snow = json_get_object(root, "snow");
    if (snow) {
        double snow_1h = 0;
        if (json_get_double(snow, "1h", &snow_1h)) {
            (*data)->snow_intensity = snow_1h;  // mm/hr
        }
    }
    
    // Note: Current weather API doesn't provide precipitation probability
    // That's only available in forecast API
    
    // Extract sunrise/sunset
    JsonObject *sys = json_get_object(root, "sys");
    if (sys) {
        int sunrise_ts = 0, sunset_ts = 0;
        
        if (json_get_int(sys, "sunrise", &sunrise_ts)) {
            time_t sunrise = (time_t)sunrise_ts;
            struct tm *tm = localtime(&sunrise);
            (*data)->sunrise = g_strdup_printf("%02d:%02d", tm->tm_hour, tm->tm_min);
        }
        
        if (json_get_int(sys, "sunset", &sunset_ts)) {
            time_t sunset = (time_t)sunset_ts;
            struct tm *tm = localtime(&sunset);
            (*data)->sunset = g_strdup_printf("%02d:%02d", tm->tm_hour, tm->tm_min);
        }
    }
    
    (*data)->last_update = time(NULL);
    (*data)->raw_output = g_strdup(json_response);
    
    json_object_cleanup(root);
    g_free(json_response);
    
    // Fetch forecast data (don't fail if forecast fails)
    fetch_openweather_forecast(city, api_key, *data);
    
    log_info("OpenWeather data fetched successfully for %s", city);
    return TRUE;
}

// Fetch 5-day forecast data
static gboolean fetch_openweather_forecast(const char* city, const char* api_key, WeatherData* data) {
    if (!city || !api_key || !data) {
        return FALSE;
    }
    
    // Construct forecast API URL
    char *encoded_city = g_uri_escape_string(city, NULL, TRUE);
    char *url = g_strdup_printf(
        "https://api.openweathermap.org/data/2.5/forecast?q=%s&appid=%s&units=metric&cnt=40",
        encoded_city, api_key
    );
    g_free(encoded_city);
    
    // Fetch forecast data
    char *json_response = network_fetch_json(url);
    g_free(url);
    
    if (!json_response) {
        log_warn("Failed to fetch forecast from OpenWeather");
        return FALSE;
    }
    
    // Parse JSON response
    GError *error = NULL;
    JsonObject *root = json_parse_string(json_response, &error);
    
    if (!root) {
        log_warn("Failed to parse forecast JSON: %s", 
                 error ? error->message : "Unknown error");
        if (error) g_error_free(error);
        g_free(json_response);
        return FALSE;
    }
    
    // Check for API error
    char *cod = NULL;
    if (json_get_string(root, "cod", &cod) && strcmp(cod, "200") != 0) {
        log_warn("OpenWeather forecast API error: %s", cod);
        g_free(cod);
        json_object_cleanup(root);
        g_free(json_response);
        return FALSE;
    }
    g_free(cod);
    
    // Get list array
    JsonArray *list = json_get_array(root, "list");
    if (!list) {
        json_object_cleanup(root);
        g_free(json_response);
        return FALSE;
    }
    
    // Get precipitation probability from first forecast item (closest to now)
    if (json_array_get_length(list) > 0) {
        JsonObject *first_item = json_array_get_object_element(list, 0);
        if (first_item) {
            double pop = 0;
            if (json_get_double(first_item, "pop", &pop)) {
                data->precipitation_probability = pop * 100;  // Convert from 0-1 to 0-100%
            }
        }
    }
    
    // Process forecast data - group by day and find min/max temps
    GHashTable *daily_data = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    
    guint list_length = json_array_get_length(list);
    for (guint i = 0; i < list_length; i++) {
        JsonObject *item = json_array_get_object_element(list, i);
        if (!item) continue;
        
        // Get timestamp
        int dt = 0;
        if (!json_get_int(item, "dt", &dt)) continue;
        
        // Convert to date string for grouping
        time_t timestamp = (time_t)dt;
        struct tm *tm_info = localtime(&timestamp);
        char date_key[11];
        strftime(date_key, sizeof(date_key), "%Y-%m-%d", tm_info);
        
        // Get temperature data
        JsonObject *main_obj = json_get_object(item, "main");
        if (!main_obj) continue;
        
        double temp_min = 0, temp_max = 0;
        json_get_double(main_obj, "temp_min", &temp_min);
        json_get_double(main_obj, "temp_max", &temp_max);
        
        // Get weather condition (use first weather item)
        JsonArray *weather_array = json_get_array(item, "weather");
        int weather_id = 800; // Default to clear
        if (weather_array && json_array_get_length(weather_array) > 0) {
            JsonObject *weather = json_array_get_object_element(weather_array, 0);
            json_get_int(weather, "id", &weather_id);
        }
        
        // Get rain probability (pop = probability of precipitation)
        double pop = 0;
        json_get_double(item, "pop", &pop);
        
        // Get rain/snow amounts
        double rain_amount = 0;
        JsonObject *rain_obj = json_get_object(item, "rain");
        if (rain_obj) {
            json_get_double(rain_obj, "3h", &rain_amount);
        }
        JsonObject *snow_obj = json_get_object(item, "snow");
        if (snow_obj) {
            double snow_amount = 0;
            json_get_double(snow_obj, "3h", &snow_amount);
            rain_amount += snow_amount;  // Combine as total precipitation
        }
        
        // Update or create daily data
        typedef struct {
            double min_temp;
            double max_temp;
            int weather_id;
            time_t date;
            double max_pop;  // Maximum probability for the day
            double total_precip;  // Total precipitation for the day
        } DailyInfo;
        
        DailyInfo *info = g_hash_table_lookup(daily_data, date_key);
        if (!info) {
            info = g_new0(DailyInfo, 1);
            info->min_temp = temp_min;
            info->max_temp = temp_max;
            info->weather_id = weather_id;
            info->date = timestamp;
            info->max_pop = pop * 100;  // Convert to percentage
            info->total_precip = rain_amount;
            g_hash_table_insert(daily_data, g_strdup(date_key), info);
        } else {
            if (temp_min < info->min_temp) info->min_temp = temp_min;
            if (temp_max > info->max_temp) info->max_temp = temp_max;
            if (pop * 100 > info->max_pop) info->max_pop = pop * 100;
            info->total_precip += rain_amount;
        }
    }
    
    // Convert hash table to forecast array (skip today, get next 5 days)
    GList *keys = g_hash_table_get_keys(daily_data);
    keys = g_list_sort(keys, (GCompareFunc)strcmp);
    
    int forecast_count = 0;
    data->forecast = g_new0(ForecastDay, 5);
    // Initialize precipitation fields to -1 (not available)
    for (int i = 0; i < 5; i++) {
        data->forecast[i].precipitation_probability = -1;
        data->forecast[i].precipitation_amount = -1;
    }
    
    // Skip first day (today) and get next 5
    GList *iter = keys ? keys->next : NULL;
    while (iter && forecast_count < 5) {
        typedef struct {
            double min_temp;
            double max_temp;
            int weather_id;
            time_t date;
            double max_pop;
            double total_precip;
        } DailyInfo;
        
        DailyInfo *info = g_hash_table_lookup(daily_data, iter->data);
        if (info) {
            data->forecast[forecast_count].date = info->date;
            data->forecast[forecast_count].temp_min = info->min_temp;
            data->forecast[forecast_count].temp_max = info->max_temp;
            data->forecast[forecast_count].condition = map_openweather_condition(info->weather_id, NULL);
            data->forecast[forecast_count].condition_text = NULL;
            data->forecast[forecast_count].precipitation_probability = info->max_pop;
            data->forecast[forecast_count].precipitation_amount = info->total_precip;
            forecast_count++;
        }
        iter = iter->next;
    }
    
    data->forecast_days = forecast_count;
    
    g_list_free(keys);
    g_hash_table_destroy(daily_data);
    json_object_cleanup(root);
    g_free(json_response);
    
    return TRUE;
}

const WeatherProviderInterface openweather_provider = {
    .name = "OpenWeather",
    .description = "Global weather data via OpenWeather API (requires API key)",
    .fetch_weather = openweather_fetch_weather
};