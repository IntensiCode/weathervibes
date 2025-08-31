#ifndef WEATHER_FETCHER_H
#define WEATHER_FETCHER_H

#include "app.h"

gboolean weather_fetcher_update(const char *city, WeatherProvider provider);
gboolean weather_fetcher_update_with_provider(const char *city, WeatherProvider provider);
void weather_fetcher_free_data(WeatherData *data);
WeatherData* weather_fetcher_copy_data(const WeatherData *data);

#endif