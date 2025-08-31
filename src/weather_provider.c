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
    return g_new0(WeatherData, 1);
}

void weather_data_free(WeatherData* data) {
    if (!data) return;
    
    g_free(data->condition_icon);
    g_free(data->condition_text);
    g_free(data->city);
    g_free(data->wind_direction);
    g_free(data->sunrise);
    g_free(data->sunset);
    g_free(data->raw_output);
    g_free(data->error_message);
    g_free(data);
}