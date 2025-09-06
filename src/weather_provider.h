#ifndef WEATHER_PROVIDER_H
#define WEATHER_PROVIDER_H

#include <glib.h>
#include <time.h>
#include "weather_conditions.h"

typedef enum {
    PROVIDER_ANSIWEATHER = 0,
    PROVIDER_BRIGHTSKY,
    PROVIDER_OPENWEATHER,
    PROVIDER_TOMORROW,
    PROVIDER_COUNT  // Useful for array sizing and validation
} WeatherProvider;

// Hourly forecast data
typedef struct {
    time_t timestamp;              // Unix timestamp for this hour
    double temperature;            // Temperature in Celsius
    double precipitation_probability;  // 0-100%, -1 if not available
    double rain_amount;            // Rain in mm, 0 if none, -1 if not available
    double snow_amount;            // Snow in mm, 0 if none, -1 if not available
    gboolean has_thunderstorm;     // TRUE if thunderstorm expected
} HourlyData;

// Forecast data for a single day
typedef struct {
    time_t date;           // Date of forecast
    double temp_min;       // Minimum temperature
    double temp_max;       // Maximum temperature
    WeatherCondition condition;  // Weather condition
    char *condition_text;  // Condition description
    double precipitation_probability;  // Rain/precipitation probability (0-100%, -1 if not available)
    double precipitation_amount;  // Expected precipitation in mm (-1 if not available)
} ForecastDay;

typedef struct {
    double temperature;
    char *condition_text;
    WeatherCondition condition;  // Standardized weather condition enum
    
    double feels_like;
    double uvi;
    double wind_speed;
    char *wind_direction;
    int humidity;
    int pressure;
    char *sunrise;
    char *sunset;
    
    // Rain/Precipitation data (all optional - can be NULL or -1 for not available)
    double precipitation_probability;  // Probability of precipitation (0-100%)
    double rain_intensity;             // Rain intensity in mm/hr
    double snow_intensity;             // Snow intensity in mm/hr
    double sleet_intensity;            // Sleet intensity in mm/hr
    double freezing_rain_intensity;    // Freezing rain intensity in mm/hr
    double precipitation_accumulation;  // Total precipitation in mm (for period)
    
    char *city;
    time_t last_update;
    char *raw_output;
    char *error_message;
    
    // 5-day forecast data
    ForecastDay *forecast;  // Array of forecast days
    int forecast_days;      // Number of forecast days (0-5)
    
    // Hourly forecast data (next 24 hours)
    HourlyData *hourly_forecast;  // Array of hourly data
    int hourly_count;             // Number of hours available (0-24)
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