#ifndef WEATHER_PROVIDER_H
#define WEATHER_PROVIDER_H

#include <glib.h>
#include <time.h>

typedef enum {
    PROVIDER_ANSIWEATHER = 0,
    PROVIDER_BRIGHTSKY,
    PROVIDER_OPENWEATHER,
    PROVIDER_TOMORROW,
    PROVIDER_COUNT  // Useful for array sizing and validation
} WeatherProvider;

typedef struct {
    double temperature;
    char *condition_icon;
    char *condition_text;
    
    double feels_like;
    double uvi;
    double wind_speed;
    char *wind_direction;
    int humidity;
    int pressure;
    char *sunrise;
    char *sunset;
    
    char *city;
    time_t last_update;
    char *raw_output;
    char *error_message;
} WeatherData;

// Provider interface - each provider implements these functions
typedef struct {
    const char* name;
    const char* description;
    gboolean (*fetch_weather)(const char* city, WeatherData** data);
} WeatherProviderInterface;

// Provider registration functions
void weather_provider_init(void);
const WeatherProviderInterface* weather_provider_get(WeatherProvider provider);
const char* weather_provider_get_name(WeatherProvider provider);
const char* weather_provider_get_description(WeatherProvider provider);

// Weather data management
WeatherData* weather_data_new(void);
void weather_data_free(WeatherData* data);

#endif