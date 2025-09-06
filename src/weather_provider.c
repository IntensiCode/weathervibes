#include "weather_provider.h"
#include "provider_ansiweather.h"
#include "provider_brightsky.h"
#include "provider_openweather.h"
#include "provider_tomorrow.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Provider registry
static const WeatherProviderInterface* providers[PROVIDER_COUNT] = {NULL};

void weather_provider_init(void) {
    providers[PROVIDER_ANSIWEATHER] = &ansiweather_provider;
    providers[PROVIDER_BRIGHTSKY] = &brightsky_provider;
    providers[PROVIDER_OPENWEATHER] = &openweather_provider;
    providers[PROVIDER_TOMORROW] = &tomorrow_provider;
}

const WeatherProviderInterface* weather_provider_get(WeatherProvider provider) {
    if (provider < 0 || provider >= PROVIDER_COUNT) {
        return NULL;
    }
    return providers[provider];
}

const char* weather_provider_get_name(WeatherProvider provider) {
    const WeatherProviderInterface* p = weather_provider_get(provider);
    return p ? p->name : "Unknown";
}

const char* weather_provider_get_description(WeatherProvider provider) {
    const WeatherProviderInterface* p = weather_provider_get(provider);
    return p ? p->description : "Unknown provider";
}

WeatherData* weather_data_new(void) {
    WeatherData *data = g_new0(WeatherData, 1);
    // Initialize rain fields to -1 (not available)
    data->precipitation_probability = -1;
    data->rain_intensity = -1;
    data->snow_intensity = -1;
    data->sleet_intensity = -1;
    data->freezing_rain_intensity = -1;
    data->precipitation_accumulation = -1;
    return data;
}

void weather_data_free(WeatherData* data) {
    if (!data) return;
    
    g_free(data->condition_text);
    g_free(data->city);
    g_free(data->wind_direction);
    g_free(data->sunrise);
    g_free(data->sunset);
    g_free(data->raw_output);
    g_free(data->error_message);
    
    // Free forecast data
    if (data->forecast) {
        for (int i = 0; i < data->forecast_days; i++) {
            g_free(data->forecast[i].condition_text);
        }
        g_free(data->forecast);
    }
    
    g_free(data);
}