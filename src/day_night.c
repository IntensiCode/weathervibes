#include "day_night.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

// Parse time string in format "HH:MM" to minutes since midnight
static int parse_time_to_minutes(const char *time_str) {
    if (!time_str || strlen(time_str) < 5) {
        return -1;
    }
    
    int hours, minutes;
    if (sscanf(time_str, "%d:%d", &hours, &minutes) == 2) {
        return hours * 60 + minutes;
    }
    
    return -1;
}

// Get current time as minutes since midnight
static int get_current_minutes(void) {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    return tm->tm_hour * 60 + tm->tm_min;
}

gboolean is_daytime_from_sun_times(const char *sunrise, const char *sunset) {
    // Parse sunrise and sunset times
    int sunrise_minutes = parse_time_to_minutes(sunrise);
    int sunset_minutes = parse_time_to_minutes(sunset);
    
    if (sunrise_minutes < 0 || sunset_minutes < 0) {
        // Invalid times, fall back to time-based detection
        return is_daytime_from_clock();
    }
    
    int current_minutes = get_current_minutes();
    
    // Handle normal case (sunrise before sunset)
    if (sunrise_minutes < sunset_minutes) {
        return current_minutes >= sunrise_minutes && current_minutes < sunset_minutes;
    }
    
    // Handle edge case (sunset after midnight, shouldn't normally happen)
    // In this case, it's day if we're after sunrise OR before sunset
    return current_minutes >= sunrise_minutes || current_minutes < sunset_minutes;
}

gboolean is_daytime_from_clock(void) {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    
    // Consider daytime to be between 6 AM and 8 PM
    return tm->tm_hour >= 6 && tm->tm_hour < 20;
}

gboolean is_daytime(const WeatherData *data) {
    if (data && data->sunrise && data->sunset) {
        // We have sunrise/sunset data, use it
        return is_daytime_from_sun_times(data->sunrise, data->sunset);
    }
    
    // Fall back to clock-based detection
    return is_daytime_from_clock();
}

WeatherCondition adjust_condition_for_time(WeatherCondition condition, gboolean is_day) {
    // Adjust weather condition based on day/night
    switch (condition) {
        case WEATHER_CONDITION_CLEAR_DAY:
            return is_day ? WEATHER_CONDITION_CLEAR_DAY : WEATHER_CONDITION_CLEAR_NIGHT;
            
        case WEATHER_CONDITION_CLEAR_NIGHT:
            return is_day ? WEATHER_CONDITION_CLEAR_DAY : WEATHER_CONDITION_CLEAR_NIGHT;
            
        case WEATHER_CONDITION_PARTLY_CLOUDY_DAY:
            return is_day ? WEATHER_CONDITION_PARTLY_CLOUDY_DAY : WEATHER_CONDITION_PARTLY_CLOUDY_NIGHT;
            
        case WEATHER_CONDITION_PARTLY_CLOUDY_NIGHT:
            return is_day ? WEATHER_CONDITION_PARTLY_CLOUDY_DAY : WEATHER_CONDITION_PARTLY_CLOUDY_NIGHT;
            
        default:
            // Other conditions don't change based on time
            return condition;
    }
}

const char* get_time_appropriate_emoji(const WeatherData *data) {
    if (!data) {
        return "🌤️";
    }
    
    gboolean is_day = is_daytime(data);
    WeatherCondition adjusted = adjust_condition_for_time(data->condition, is_day);
    
    return weather_condition_get_emoji(adjusted);
}