#include <stdio.h>
#include <string.h>
#include "../src/json_helpers.h"

void test_basic_parsing() {
    printf("Testing basic JSON parsing...\n");
    
    const char *json = "{"
        "\"string\": \"hello\","
        "\"number\": 42.5,"
        "\"integer\": 100,"
        "\"boolean\": true,"
        "\"nested\": {\"value\": 123}"
        "}";
    
    GError *error = NULL;
    JsonObject *root = json_parse_string(json, &error);
    
    if (!root) {
        printf("❌ Failed to parse JSON: %s\n", error ? error->message : "Unknown");
        if (error) g_error_free(error);
        return;
    }
    
    // Test string extraction
    char *str = NULL;
    if (json_get_string(root, "string", &str)) {
        if (strcmp(str, "hello") == 0) {
            printf("✅ String extraction works\n");
        } else {
            printf("❌ String value wrong: %s\n", str);
        }
        g_free(str);
    } else {
        printf("❌ String extraction failed\n");
    }
    
    // Test double extraction
    double num = 0;
    if (json_get_double(root, "number", &num)) {
        if (num == 42.5) {
            printf("✅ Double extraction works\n");
        } else {
            printf("❌ Double value wrong: %f\n", num);
        }
    } else {
        printf("❌ Double extraction failed\n");
    }
    
    // Test integer extraction
    int integer = 0;
    if (json_get_int(root, "integer", &integer)) {
        if (integer == 100) {
            printf("✅ Integer extraction works\n");
        } else {
            printf("❌ Integer value wrong: %d\n", integer);
        }
    } else {
        printf("❌ Integer extraction failed\n");
    }
    
    // Test boolean extraction
    gboolean bool_val = FALSE;
    if (json_get_boolean(root, "boolean", &bool_val)) {
        if (bool_val == TRUE) {
            printf("✅ Boolean extraction works\n");
        } else {
            printf("❌ Boolean value wrong\n");
        }
    } else {
        printf("❌ Boolean extraction failed\n");
    }
    
    // Test nested object
    JsonObject *nested = json_get_object(root, "nested");
    if (nested) {
        int nested_val = 0;
        if (json_get_int(nested, "value", &nested_val) && nested_val == 123) {
            printf("✅ Nested object extraction works\n");
        } else {
            printf("❌ Nested object value wrong\n");
        }
    } else {
        printf("❌ Nested object extraction failed\n");
    }
    
    json_object_cleanup(root);
}

void test_nested_paths() {
    printf("\nTesting nested path helpers...\n");
    
    const char *json = "{"
        "\"main\": {"
            "\"temp\": 25.5,"
            "\"humidity\": 65"
        "},"
        "\"sys\": {"
            "\"country\": \"DE\""
        "}"
        "}";
    
    GError *error = NULL;
    JsonObject *root = json_parse_string(json, &error);
    
    if (!root) {
        printf("❌ Failed to parse JSON\n");
        if (error) g_error_free(error);
        return;
    }
    
    // Test nested double
    double temp = 0;
    if (json_get_nested_double(root, "main.temp", &temp)) {
        if (temp == 25.5) {
            printf("✅ Nested double extraction works\n");
        } else {
            printf("❌ Nested double wrong: %f\n", temp);
        }
    } else {
        printf("❌ Nested double extraction failed\n");
    }
    
    // Test nested int
    int humidity = 0;
    if (json_get_nested_int(root, "main.humidity", &humidity)) {
        if (humidity == 65) {
            printf("✅ Nested integer extraction works\n");
        } else {
            printf("❌ Nested integer wrong: %d\n", humidity);
        }
    } else {
        printf("❌ Nested integer extraction failed\n");
    }
    
    // Test nested string
    char *country = NULL;
    if (json_get_nested_string(root, "sys.country", &country)) {
        if (strcmp(country, "DE") == 0) {
            printf("✅ Nested string extraction works\n");
        } else {
            printf("❌ Nested string wrong: %s\n", country);
        }
        g_free(country);
    } else {
        printf("❌ Nested string extraction failed\n");
    }
    
    json_object_cleanup(root);
}

void test_error_handling() {
    printf("\nTesting error handling...\n");
    
    // Test invalid JSON
    GError *error = NULL;
    JsonObject *root = json_parse_string("not valid json", &error);
    if (!root && error) {
        printf("✅ Invalid JSON rejected correctly\n");
        g_error_free(error);
    } else {
        printf("❌ Invalid JSON not handled properly\n");
        if (root) json_object_cleanup(root);
    }
    
    // Test empty JSON
    error = NULL;
    root = json_parse_string("", &error);
    if (!root && error) {
        printf("✅ Empty JSON rejected correctly\n");
        g_error_free(error);
    } else {
        printf("❌ Empty JSON not handled properly\n");
        if (root) json_object_cleanup(root);
    }
    
    // Test missing fields
    root = json_parse_string("{\"exists\": true}", NULL);
    if (root) {
        char *missing = NULL;
        if (!json_get_string(root, "nonexistent", &missing)) {
            printf("✅ Missing field handled correctly\n");
        } else {
            printf("❌ Missing field returned value\n");
            g_free(missing);
        }
        json_object_cleanup(root);
    }
}

void test_array_handling() {
    printf("\nTesting array handling...\n");
    
    const char *json = "{"
        "\"weather\": ["
            "{\"id\": 800, \"main\": \"Clear\"},"
            "{\"id\": 801, \"main\": \"Clouds\"}"
        "]"
        "}";
    
    JsonObject *root = json_parse_string(json, NULL);
    if (!root) {
        printf("❌ Failed to parse JSON with array\n");
        return;
    }
    
    JsonArray *weather = json_get_array(root, "weather");
    if (weather) {
        guint length = json_array_get_length(weather);
        if (length == 2) {
            printf("✅ Array extraction works (length=%u)\n", length);
            
            // Test first element
            JsonObject *first = json_array_get_object_element(weather, 0);
            if (first) {
                int id = 0;
                if (json_get_int(first, "id", &id) && id == 800) {
                    printf("✅ Array element extraction works\n");
                } else {
                    printf("❌ Array element value wrong\n");
                }
            }
        } else {
            printf("❌ Array length wrong: %u\n", length);
        }
    } else {
        printf("❌ Array extraction failed\n");
    }
    
    json_object_cleanup(root);
}

int main() {
    printf("JSON Helpers Test Suite\n");
    printf("=======================\n\n");
    
    test_basic_parsing();
    test_nested_paths();
    test_error_handling();
    test_array_handling();
    
    printf("\n=======================\n");
    printf("All tests completed!\n");
    
    return 0;
}