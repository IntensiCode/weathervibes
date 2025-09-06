#include "weather_conditions.h"
#include <glib.h>

const char* weather_condition_get_display_text(WeatherCondition condition) {
    switch (condition) {
        case WEATHER_CONDITION_CLEAR_DAY:
            return "Clear";
            
        case WEATHER_CONDITION_CLEAR_NIGHT:
            return "Clear Night";
            
        case WEATHER_CONDITION_PARTLY_CLOUDY_DAY:
            return "Partly Cloudy";
            
        case WEATHER_CONDITION_PARTLY_CLOUDY_NIGHT:
            return "Partly Cloudy";
            
        case WEATHER_CONDITION_CLOUDY:
            return "Cloudy";
            
        case WEATHER_CONDITION_OVERCAST:
            return "Overcast";
            
        case WEATHER_CONDITION_DRIZZLE:
            return "Drizzle";
            
        case WEATHER_CONDITION_LIGHT_RAIN:
            return "Light Rain";
            
        case WEATHER_CONDITION_RAIN:
            return "Rain";
            
        case WEATHER_CONDITION_HEAVY_RAIN:
            return "Heavy Rain";
            
        case WEATHER_CONDITION_SHOWERS:
            return "Showers";
            
        case WEATHER_CONDITION_LIGHT_SNOW:
            return "Light Snow";
            
        case WEATHER_CONDITION_SNOW:
            return "Snow";
            
        case WEATHER_CONDITION_HEAVY_SNOW:
            return "Heavy Snow";
            
        case WEATHER_CONDITION_SLEET:
            return "Sleet";
            
        case WEATHER_CONDITION_THUNDERSTORM:
            return "Thunderstorm";
            
        case WEATHER_CONDITION_THUNDERSTORM_RAIN:
            return "Thunderstorm with Rain";
            
        case WEATHER_CONDITION_HAIL:
            return "Hail";
            
        case WEATHER_CONDITION_FOG:
            return "Fog";
            
        case WEATHER_CONDITION_MIST:
            return "Mist";
            
        case WEATHER_CONDITION_HAZE:
            return "Haze";
            
        case WEATHER_CONDITION_SMOKE:
            return "Smoke";
            
        case WEATHER_CONDITION_DUST:
            return "Dust";
            
        case WEATHER_CONDITION_SAND:
            return "Sand";
            
        case WEATHER_CONDITION_TORNADO:
            return "Tornado";
            
        case WEATHER_CONDITION_HURRICANE:
            return "Hurricane";
            
        case WEATHER_CONDITION_UNKNOWN:
        default:
            return "Unknown";
    }
}

const char* weather_condition_get_emoji(WeatherCondition condition) {
    switch (condition) {
        case WEATHER_CONDITION_CLEAR_DAY:
            return "☀️";
        case WEATHER_CONDITION_CLEAR_NIGHT:
            return "🌙";
        case WEATHER_CONDITION_PARTLY_CLOUDY_DAY:
            return "⛅";
        case WEATHER_CONDITION_PARTLY_CLOUDY_NIGHT:
            return "☁️";
        case WEATHER_CONDITION_CLOUDY:
        case WEATHER_CONDITION_OVERCAST:
            return "☁️";
        case WEATHER_CONDITION_DRIZZLE:
        case WEATHER_CONDITION_LIGHT_RAIN:
            return "🌦️";
        case WEATHER_CONDITION_RAIN:
        case WEATHER_CONDITION_HEAVY_RAIN:
        case WEATHER_CONDITION_SHOWERS:
            return "🌧️";
        case WEATHER_CONDITION_LIGHT_SNOW:
        case WEATHER_CONDITION_SNOW:
        case WEATHER_CONDITION_HEAVY_SNOW:
        case WEATHER_CONDITION_SLEET:
            return "🌨️";
        case WEATHER_CONDITION_THUNDERSTORM:
        case WEATHER_CONDITION_THUNDERSTORM_RAIN:
            return "⛈️";
        case WEATHER_CONDITION_HAIL:
            return "🌨️";
        case WEATHER_CONDITION_FOG:
        case WEATHER_CONDITION_MIST:
        case WEATHER_CONDITION_HAZE:
            return "🌁";
        case WEATHER_CONDITION_SMOKE:
        case WEATHER_CONDITION_DUST:
        case WEATHER_CONDITION_SAND:
            return "💨";
        case WEATHER_CONDITION_TORNADO:
        case WEATHER_CONDITION_HURRICANE:
            return "🌪️";
        case WEATHER_CONDITION_UNKNOWN:
        default:
            return "🌤️";
    }
}