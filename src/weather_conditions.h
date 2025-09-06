#ifndef WEATHER_CONDITIONS_H
#define WEATHER_CONDITIONS_H

#include <glib.h>

// Common weather condition enum that all providers map to
typedef enum {
    WEATHER_CONDITION_UNKNOWN = 0,
    
    // Clear conditions
    WEATHER_CONDITION_CLEAR_DAY,
    WEATHER_CONDITION_CLEAR_NIGHT,
    
    // Cloud conditions
    WEATHER_CONDITION_PARTLY_CLOUDY_DAY,
    WEATHER_CONDITION_PARTLY_CLOUDY_NIGHT,
    WEATHER_CONDITION_CLOUDY,
    WEATHER_CONDITION_OVERCAST,
    
    // Precipitation
    WEATHER_CONDITION_DRIZZLE,
    WEATHER_CONDITION_LIGHT_RAIN,
    WEATHER_CONDITION_RAIN,
    WEATHER_CONDITION_HEAVY_RAIN,
    WEATHER_CONDITION_SHOWERS,
    
    // Snow
    WEATHER_CONDITION_LIGHT_SNOW,
    WEATHER_CONDITION_SNOW,
    WEATHER_CONDITION_HEAVY_SNOW,
    WEATHER_CONDITION_SLEET,
    
    // Severe weather
    WEATHER_CONDITION_THUNDERSTORM,
    WEATHER_CONDITION_THUNDERSTORM_RAIN,
    WEATHER_CONDITION_HAIL,
    
    // Atmospheric conditions
    WEATHER_CONDITION_FOG,
    WEATHER_CONDITION_MIST,
    WEATHER_CONDITION_HAZE,
    WEATHER_CONDITION_SMOKE,
    WEATHER_CONDITION_DUST,
    WEATHER_CONDITION_SAND,
    
    // Extreme
    WEATHER_CONDITION_TORNADO,
    WEATHER_CONDITION_HURRICANE
} WeatherCondition;

// Get display text for a weather condition
const char* weather_condition_get_display_text(WeatherCondition condition);

// Get emoji icon for a weather condition
const char* weather_condition_get_emoji(WeatherCondition condition);

#endif // WEATHER_CONDITIONS_H