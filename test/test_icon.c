#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

static const char* get_weather_icon_name(const char* condition) {
    if (!condition || strlen(condition) == 0) {
        return "weather-severe-alert";
    }
    
    // Convert to lowercase for comparison
    char lower[256];
    size_t i;
    for (i = 0; i < sizeof(lower) - 1 && condition[i]; i++) {
        lower[i] = tolower(condition[i]);
    }
    lower[i] = '\0';
    
    // Map weather conditions to icon names
    if (strstr(lower, "clear") || strstr(lower, "sunny")) {
        return "weather-clear";
    } else if (strstr(lower, "few clouds")) {
        return "weather-few-clouds";
    } else if (strstr(lower, "scattered")) {
        return "weather-few-clouds";
    } else if (strstr(lower, "partly")) {
        return "weather-few-clouds";
    } else if (strstr(lower, "overcast") || strstr(lower, "cloud")) {
        return "weather-overcast";
    } else if (strstr(lower, "rain") || strstr(lower, "drizzle")) {
        return "weather-showers";
    } else if (strstr(lower, "storm") || strstr(lower, "thunder")) {
        return "weather-storm";
    } else if (strstr(lower, "snow")) {
        return "weather-snow";
    } else if (strstr(lower, "fog") || strstr(lower, "mist") || strstr(lower, "haze")) {
        return "weather-fog";
    }
    
    // Default to clear if we can't determine
    return "weather-severe-alert";
}

int main() {
    const char* test_conditions[] = {
        "Clear",
        "Broken clouds",
        "Cloudy",
        "Scattered clouds",
        "Few clouds", 
        "Overcast",
        "Light rain",
        "Thunderstorm",
        "Snow",
        "Fog",
        "Unknown condition",
        NULL
    };
    
    printf("Testing weather condition to icon mapping:\n");
    printf("==========================================\n");
    
    for (int i = 0; test_conditions[i]; i++) {
        const char* icon = get_weather_icon_name(test_conditions[i]);
        printf("%-20s -> %s\n", test_conditions[i], icon);
    }
    
    return 0;
}