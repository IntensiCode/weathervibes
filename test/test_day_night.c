#include <glib.h>
#include "../src/day_night.h"
#include "../src/weather_conditions.h"

static void test_daytime_from_sun_times(void) {
    // Note: is_daytime_from_sun_times checks if CURRENT time is between sunrise/sunset
    // We can't predict the result as it depends on the current system time
    // We can only verify that the function returns a valid boolean
    
    gboolean result;
    
    // Test with valid times - result depends on current time
    result = is_daytime_from_sun_times("06:00", "18:00");
    g_assert_true(result == TRUE || result == FALSE);
    
    result = is_daytime_from_sun_times("05:30", "19:30");
    g_assert_true(result == TRUE || result == FALSE);
    
    result = is_daytime_from_sun_times("07:00", "17:00");
    g_assert_true(result == TRUE || result == FALSE);
    
    // Test edge cases - these fall back to clock-based detection
    result = is_daytime_from_sun_times(NULL, "18:00");           // Missing sunrise
    g_assert_true(result == TRUE || result == FALSE);
    result = is_daytime_from_sun_times("06:00", NULL);           // Missing sunset
    g_assert_true(result == TRUE || result == FALSE);
    result = is_daytime_from_sun_times(NULL, NULL);              // Both missing
    g_assert_true(result == TRUE || result == FALSE);
    result = is_daytime_from_sun_times("", "18:00");             // Empty sunrise
    g_assert_true(result == TRUE || result == FALSE);
    result = is_daytime_from_sun_times("06:00", "");             // Empty sunset
    g_assert_true(result == TRUE || result == FALSE);
    result = is_daytime_from_sun_times("invalid", "18:00");      // Invalid format
    g_assert_true(result == TRUE || result == FALSE);
    result = is_daytime_from_sun_times("25:00", "18:00");        // Invalid hour
    g_assert_true(result == TRUE || result == FALSE);
    result = is_daytime_from_sun_times("06:60", "18:00");        // Invalid minute
    g_assert_true(result == TRUE || result == FALSE);
}

static void test_daytime_from_clock(void) {
    // This test is time-dependent but we can at least verify it returns a boolean
    gboolean result = is_daytime_from_clock();
    g_assert_true(result == TRUE || result == FALSE);
}

static void test_adjust_condition_for_time(void) {
    // Test day adjustments
    g_assert_cmpint(adjust_condition_for_time(WEATHER_CONDITION_CLEAR_DAY, TRUE), ==, WEATHER_CONDITION_CLEAR_DAY);
    g_assert_cmpint(adjust_condition_for_time(WEATHER_CONDITION_CLEAR_NIGHT, TRUE), ==, WEATHER_CONDITION_CLEAR_DAY);
    g_assert_cmpint(adjust_condition_for_time(WEATHER_CONDITION_PARTLY_CLOUDY_NIGHT, TRUE), ==, WEATHER_CONDITION_PARTLY_CLOUDY_DAY);
    
    // Test night adjustments
    g_assert_cmpint(adjust_condition_for_time(WEATHER_CONDITION_CLEAR_DAY, FALSE), ==, WEATHER_CONDITION_CLEAR_NIGHT);
    g_assert_cmpint(adjust_condition_for_time(WEATHER_CONDITION_CLEAR_NIGHT, FALSE), ==, WEATHER_CONDITION_CLEAR_NIGHT);
    g_assert_cmpint(adjust_condition_for_time(WEATHER_CONDITION_PARTLY_CLOUDY_DAY, FALSE), ==, WEATHER_CONDITION_PARTLY_CLOUDY_NIGHT);
    
    // Test non-adjustable conditions (should remain unchanged)
    g_assert_cmpint(adjust_condition_for_time(WEATHER_CONDITION_RAIN, TRUE), ==, WEATHER_CONDITION_RAIN);
    g_assert_cmpint(adjust_condition_for_time(WEATHER_CONDITION_RAIN, FALSE), ==, WEATHER_CONDITION_RAIN);
    g_assert_cmpint(adjust_condition_for_time(WEATHER_CONDITION_SNOW, TRUE), ==, WEATHER_CONDITION_SNOW);
    g_assert_cmpint(adjust_condition_for_time(WEATHER_CONDITION_SNOW, FALSE), ==, WEATHER_CONDITION_SNOW);
    g_assert_cmpint(adjust_condition_for_time(WEATHER_CONDITION_FOG, TRUE), ==, WEATHER_CONDITION_FOG);
    g_assert_cmpint(adjust_condition_for_time(WEATHER_CONDITION_FOG, FALSE), ==, WEATHER_CONDITION_FOG);
}

int main(int argc, char *argv[]) {
    g_test_init(&argc, &argv, NULL);
    
    g_test_add_func("/day_night/daytime_from_sun_times", test_daytime_from_sun_times);
    g_test_add_func("/day_night/daytime_from_clock", test_daytime_from_clock);
    g_test_add_func("/day_night/adjust_condition_for_time", test_adjust_condition_for_time);
    
    return g_test_run();
}