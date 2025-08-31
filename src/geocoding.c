#include "geocoding.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-glib/json-glib.h>

// Fetch JSON from URL using curl command
static char* fetch_json_from_url(const char *url) {
    char *command = g_strdup_printf("curl -s '%s' 2>/dev/null", url);
    
    FILE *pipe = popen(command, "r");
    g_free(command);
    
    if (!pipe) return NULL;
    
    char buffer[1024];
    GString *output = g_string_new("");
    
    while (fgets(buffer, sizeof(buffer), pipe)) {
        g_string_append(output, buffer);
    }
    
    int ret = pclose(pipe);
    if (ret != 0) {
        g_string_free(output, TRUE);
        return NULL;
    }
    
    char *result = g_strdup(output->str);
    g_string_free(output, TRUE);
    return result;
}

GeoLocation* geocode_location(const char *location) {
    if (!location || strlen(location) == 0) return NULL;
    
    // URL encode the location name
    char *encoded_location = g_uri_escape_string(location, NULL, TRUE);
    
    // Build Nominatim URL
    char url[512];
    snprintf(url, sizeof(url), 
        "https://nominatim.openstreetmap.org/search?q=%s&format=json&limit=1",
        encoded_location);
    
    g_free(encoded_location);
    
    // Fetch JSON response
    char *json_response = fetch_json_from_url(url);
    if (!json_response) {
        g_warning("Failed to fetch geocoding data for %s", location);
        return NULL;
    }
    
    // Parse JSON response
    JsonParser *parser = json_parser_new();
    GError *error = NULL;
    
    if (!json_parser_load_from_data(parser, json_response, -1, &error)) {
        if (error) {
            g_warning("Failed to parse geocoding JSON: %s", error->message);
            g_error_free(error);
        }
        g_object_unref(parser);
        g_free(json_response);
        return NULL;
    }
    
    JsonNode *root = json_parser_get_root(parser);
    if (!JSON_NODE_HOLDS_ARRAY(root)) {
        g_object_unref(parser);
        g_free(json_response);
        return NULL;
    }
    
    JsonArray *results = json_node_get_array(root);
    if (json_array_get_length(results) == 0) {
        g_warning("No geocoding results found for %s", location);
        g_object_unref(parser);
        g_free(json_response);
        return NULL;
    }
    
    // Get first result
    JsonObject *first_result = json_array_get_object_element(results, 0);
    
    GeoLocation *geo_location = g_new0(GeoLocation, 1);
    
    // Extract coordinates
    const char *lat_str = json_object_get_string_member(first_result, "lat");
    const char *lon_str = json_object_get_string_member(first_result, "lon");
    
    if (lat_str && lon_str) {
        geo_location->latitude = atof(lat_str);
        geo_location->longitude = atof(lon_str);
    }
    
    // Extract display name
    const char *display_name = json_object_get_string_member(first_result, "display_name");
    if (display_name) {
        geo_location->display_name = g_strdup(display_name);
        
        // Try to extract city name from display_name
        // Format is usually: "City, Region, Country" or similar
        char **parts = g_strsplit(display_name, ",", -1);
        if (parts && parts[0]) {
            geo_location->city = g_strstrip(g_strdup(parts[0]));
        }
        g_strfreev(parts);
    }
    
    g_object_unref(parser);
    g_free(json_response);
    
    g_message("Geocoded '%s' to lat=%.4f, lon=%.4f (%s)", 
              location, geo_location->latitude, geo_location->longitude,
              geo_location->display_name ? geo_location->display_name : "Unknown");
    
    return geo_location;
}

void geo_location_free(GeoLocation *location) {
    if (!location) return;
    g_free(location->display_name);
    g_free(location->city);
    g_free(location->country);
    g_free(location);
}