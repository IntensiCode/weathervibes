#ifndef APP_H
#define APP_H

#include <gtk/gtk.h>
#include <time.h>
#include "weather_provider.h"

typedef struct {
    char *city;
    int update_interval_minutes;
    gboolean use_celsius;
    WeatherProvider provider;
    char *openweather_api_key;  // API key for OpenWeather provider
    char *tomorrow_api_key;     // API key for Tomorrow.io provider
} AppConfig;

typedef struct {
    GtkApplication *app;
    GtkWidget *main_window;
    
    WeatherData *weather_data;  // Currently displayed weather data
    AppConfig *config;
    
    guint update_timer_id;
    GMutex data_mutex;
    
    // Per-provider caching
    WeatherData *provider_cache[PROVIDER_COUNT];  // Cached data for each provider
    time_t last_fetch_time[PROVIDER_COUNT];  // Last fetch time for each provider
    WeatherProvider cached_data_provider;  // Which provider the current weather_data is from
    char *cached_city;  // The city for which we have cached data
} AppContext;

extern AppContext *g_app_context;

#endif