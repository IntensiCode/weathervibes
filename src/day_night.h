#ifndef DAY_NIGHT_H
#define DAY_NIGHT_H

#include <glib.h>
#include "weather_provider.h"
#include "weather_conditions.h"

// Check if it's daytime based on sunrise/sunset times (format: "HH:MM")
gboolean is_daytime_from_sun_times(const char *sunrise, const char *sunset);

// Check if it's daytime based on clock (fallback: 6 AM - 8 PM)
gboolean is_daytime_from_clock(void);

// Main function: checks sun times first, falls back to clock
gboolean is_daytime(const WeatherData *data);

// Adjust weather condition enum based on day/night
WeatherCondition adjust_condition_for_time(WeatherCondition condition, gboolean is_day);

// Get the appropriate emoji for current time (day/night aware)
const char* get_time_appropriate_emoji(const WeatherData *data);

#endif // DAY_NIGHT_H