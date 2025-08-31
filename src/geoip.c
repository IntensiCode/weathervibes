#include "geoip.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-glib/json-glib.h>

// Fetch JSON from URL using curl command
static char* fetch_json_from_url(const char *url) {
    char *command = g_strdup_printf("curl -s --max-time 5 '%s' 2>/dev/null", url);
    
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

char* geoip_get_city(void) {
    // Try multiple GeoIP services for redundancy
    const char *services[] = {
        "http://ip-api.com/json/?fields=status,city,country",
        "https://ipapi.co/json/",
        "https://ipinfo.io/json",
        NULL
    };
    
    for (int i = 0; services[i] != NULL; i++) {
        g_message("Trying GeoIP service: %s", services[i]);
        
        char *json_response = fetch_json_from_url(services[i]);
        if (!json_response) {
            g_debug("GeoIP service %d failed to respond", i);
            continue;
        }
        
        // Parse JSON response
        JsonParser *parser = json_parser_new();
        GError *error = NULL;
        
        if (!json_parser_load_from_data(parser, json_response, -1, &error)) {
            if (error) {
                g_debug("Failed to parse GeoIP JSON: %s", error->message);
                g_error_free(error);
            }
            g_object_unref(parser);
            g_free(json_response);
            continue;
        }
        
        JsonNode *root = json_parser_get_root(parser);
        if (!JSON_NODE_HOLDS_OBJECT(root)) {
            g_object_unref(parser);
            g_free(json_response);
            continue;
        }
        
        JsonObject *root_obj = json_node_get_object(root);
        const char *city = NULL;
        const char *country = NULL;
        
        // Different services use different field names
        if (i == 0) {  // ip-api.com
            const char *status = json_object_get_string_member(root_obj, "status");
            if (g_strcmp0(status, "success") == 0) {
                city = json_object_get_string_member(root_obj, "city");
                country = json_object_get_string_member(root_obj, "country");
            }
        } else if (i == 1) {  // ipapi.co
            city = json_object_get_string_member(root_obj, "city");
            country = json_object_get_string_member(root_obj, "country_name");
        } else if (i == 2) {  // ipinfo.io
            city = json_object_get_string_member(root_obj, "city");
            country = json_object_get_string_member(root_obj, "country");
        }
        
        char *location = NULL;
        if (city && strlen(city) > 0) {
            // Format as "City,CountryCode" if we have country
            if (country && strlen(country) > 0) {
                // For ipapi.co we get full country name, for others we get code
                if (i == 1) {
                    // Just use city name for full country names
                    location = g_strdup(city);
                } else {
                    location = g_strdup_printf("%s,%s", city, country);
                }
            } else {
                location = g_strdup(city);
            }
            
            g_message("GeoIP detected location: %s", location);
            g_object_unref(parser);
            g_free(json_response);
            return location;
        }
        
        g_object_unref(parser);
        g_free(json_response);
    }
    
    g_warning("All GeoIP services failed, falling back to default");
    return NULL;
}