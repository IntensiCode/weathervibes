#include <glib.h>
#include "../src/weather_conditions.h"

static void test_emoji_mapping(void) {
    // Test clear conditions
    g_assert_cmpstr(weather_condition_get_emoji(WEATHER_CONDITION_CLEAR_DAY), ==, "☀️");
    g_assert_cmpstr(weather_condition_get_emoji(WEATHER_CONDITION_CLEAR_NIGHT), ==, "🌙");
    
    // Test cloudy conditions
    g_assert_cmpstr(weather_condition_get_emoji(WEATHER_CONDITION_PARTLY_CLOUDY_DAY), ==, "⛅");
    g_assert_cmpstr(weather_condition_get_emoji(WEATHER_CONDITION_PARTLY_CLOUDY_NIGHT), ==, "☁️");
    g_assert_cmpstr(weather_condition_get_emoji(WEATHER_CONDITION_CLOUDY), ==, "☁️");
    g_assert_cmpstr(weather_condition_get_emoji(WEATHER_CONDITION_OVERCAST), ==, "☁️");
    
    // Test rain conditions
    g_assert_cmpstr(weather_condition_get_emoji(WEATHER_CONDITION_RAIN), ==, "🌧️");
    g_assert_cmpstr(weather_condition_get_emoji(WEATHER_CONDITION_LIGHT_RAIN), ==, "🌦️");
    g_assert_cmpstr(weather_condition_get_emoji(WEATHER_CONDITION_HEAVY_RAIN), ==, "🌧️");
    g_assert_cmpstr(weather_condition_get_emoji(WEATHER_CONDITION_SHOWERS), ==, "🌧️");
    
    // Test snow conditions
    g_assert_cmpstr(weather_condition_get_emoji(WEATHER_CONDITION_SNOW), ==, "🌨️");
    g_assert_cmpstr(weather_condition_get_emoji(WEATHER_CONDITION_LIGHT_SNOW), ==, "🌨️");
    g_assert_cmpstr(weather_condition_get_emoji(WEATHER_CONDITION_HEAVY_SNOW), ==, "🌨️");
    
    // Test storm conditions
    g_assert_cmpstr(weather_condition_get_emoji(WEATHER_CONDITION_THUNDERSTORM), ==, "⛈️");
    g_assert_cmpstr(weather_condition_get_emoji(WEATHER_CONDITION_THUNDERSTORM_RAIN), ==, "⛈️");
    
    // Test special conditions
    g_assert_cmpstr(weather_condition_get_emoji(WEATHER_CONDITION_FOG), ==, "🌁");
    g_assert_cmpstr(weather_condition_get_emoji(WEATHER_CONDITION_TORNADO), ==, "🌪️");
    g_assert_cmpstr(weather_condition_get_emoji(WEATHER_CONDITION_UNKNOWN), ==, "🌤️");
}

static void test_display_text(void) {
    // Test a few representative cases
    g_assert_cmpstr(weather_condition_get_display_text(WEATHER_CONDITION_CLEAR_DAY), ==, "Clear");
    g_assert_cmpstr(weather_condition_get_display_text(WEATHER_CONDITION_RAIN), ==, "Rain");
    g_assert_cmpstr(weather_condition_get_display_text(WEATHER_CONDITION_THUNDERSTORM), ==, "Thunderstorm");
    g_assert_cmpstr(weather_condition_get_display_text(WEATHER_CONDITION_FOG), ==, "Fog");
    g_assert_cmpstr(weather_condition_get_display_text(WEATHER_CONDITION_UNKNOWN), ==, "Unknown");
}

int main(int argc, char *argv[]) {
    g_test_init(&argc, &argv, NULL);
    
    g_test_add_func("/weather_conditions/emoji_mapping", test_emoji_mapping);
    g_test_add_func("/weather_conditions/display_text", test_display_text);
    
    return g_test_run();
}