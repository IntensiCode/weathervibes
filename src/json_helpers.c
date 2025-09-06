#include "json_helpers.h"
#include <string.h>
#include <math.h>

gboolean json_get_string(JsonObject *object, const char *member, char **value) {
    if (!object || !member || !value) {
        return FALSE;
    }
    
    if (!json_object_has_member(object, member)) {
        return FALSE;
    }
    
    JsonNode *node = json_object_get_member(object, member);
    if (!JSON_NODE_HOLDS_VALUE(node)) {
        return FALSE;
    }
    
    const char *str = json_object_get_string_member(object, member);
    if (str && strlen(str) > 0) {
        *value = g_strdup(str);
        return TRUE;
    }
    
    return FALSE;
}

gboolean json_get_double(JsonObject *object, const char *member, double *value) {
    if (!object || !member || !value) {
        return FALSE;
    }
    
    if (!json_object_has_member(object, member)) {
        return FALSE;
    }
    
    JsonNode *node = json_object_get_member(object, member);
    if (!JSON_NODE_HOLDS_VALUE(node)) {
        return FALSE;
    }
    
    *value = json_object_get_double_member(object, member);
    
    // Check for NaN or infinity
    if (isnan(*value) || isinf(*value)) {
        return FALSE;
    }
    
    return TRUE;
}

gboolean json_get_int(JsonObject *object, const char *member, int *value) {
    if (!object || !member || !value) {
        return FALSE;
    }
    
    if (!json_object_has_member(object, member)) {
        return FALSE;
    }
    
    JsonNode *node = json_object_get_member(object, member);
    if (!JSON_NODE_HOLDS_VALUE(node)) {
        return FALSE;
    }
    
    *value = (int)json_object_get_int_member(object, member);
    return TRUE;
}

gboolean json_get_boolean(JsonObject *object, const char *member, gboolean *value) {
    if (!object || !member || !value) {
        return FALSE;
    }
    
    if (!json_object_has_member(object, member)) {
        return FALSE;
    }
    
    JsonNode *node = json_object_get_member(object, member);
    if (!JSON_NODE_HOLDS_VALUE(node)) {
        return FALSE;
    }
    
    *value = json_object_get_boolean_member(object, member);
    return TRUE;
}

JsonObject* json_get_object(JsonObject *object, const char *member) {
    if (!object || !member) {
        return NULL;
    }
    
    if (!json_object_has_member(object, member)) {
        return NULL;
    }
    
    JsonNode *node = json_object_get_member(object, member);
    if (!JSON_NODE_HOLDS_OBJECT(node)) {
        return NULL;
    }
    
    return json_node_get_object(node);
}

JsonArray* json_get_array(JsonObject *object, const char *member) {
    if (!object || !member) {
        return NULL;
    }
    
    if (!json_object_has_member(object, member)) {
        return NULL;
    }
    
    JsonNode *node = json_object_get_member(object, member);
    if (!JSON_NODE_HOLDS_ARRAY(node)) {
        return NULL;
    }
    
    return json_node_get_array(node);
}

JsonObject* json_parse_string(const char *json_string, GError **error) {
    if (!json_string || strlen(json_string) == 0) {
        if (error) {
            *error = g_error_new(G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                                "Empty JSON string");
        }
        return NULL;
    }
    
    JsonParser *parser = json_parser_new();
    
    if (!json_parser_load_from_data(parser, json_string, -1, error)) {
        g_object_unref(parser);
        return NULL;
    }
    
    JsonNode *root = json_parser_get_root(parser);
    if (!root || !JSON_NODE_HOLDS_OBJECT(root)) {
        if (error) {
            *error = g_error_new(G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                                "JSON root is not an object");
        }
        g_object_unref(parser);
        return NULL;
    }
    
    // Get the object and increase its reference count
    JsonObject *object = json_node_dup_object(root);
    g_object_unref(parser);
    
    return object;
}

void json_object_cleanup(JsonObject *object) {
    if (object) {
        json_object_unref(object);
    }
}

// Helper to get nested object member (e.g., "main.temp")
gboolean json_get_nested_double(JsonObject *root, const char *path, double *value) {
    if (!root || !path || !value) {
        return FALSE;
    }
    
    char **parts = g_strsplit(path, ".", 2);
    if (!parts[0]) {
        g_strfreev(parts);
        return FALSE;
    }
    
    // Single level path
    if (!parts[1]) {
        gboolean result = json_get_double(root, parts[0], value);
        g_strfreev(parts);
        return result;
    }
    
    // Nested path
    JsonObject *nested = json_get_object(root, parts[0]);
    if (!nested) {
        g_strfreev(parts);
        return FALSE;
    }
    
    gboolean result = json_get_double(nested, parts[1], value);
    g_strfreev(parts);
    return result;
}

gboolean json_get_nested_string(JsonObject *root, const char *path, char **value) {
    if (!root || !path || !value) {
        return FALSE;
    }
    
    char **parts = g_strsplit(path, ".", 2);
    if (!parts[0]) {
        g_strfreev(parts);
        return FALSE;
    }
    
    // Single level path
    if (!parts[1]) {
        gboolean result = json_get_string(root, parts[0], value);
        g_strfreev(parts);
        return result;
    }
    
    // Nested path
    JsonObject *nested = json_get_object(root, parts[0]);
    if (!nested) {
        g_strfreev(parts);
        return FALSE;
    }
    
    gboolean result = json_get_string(nested, parts[1], value);
    g_strfreev(parts);
    return result;
}

gboolean json_get_nested_int(JsonObject *root, const char *path, int *value) {
    if (!root || !path || !value) {
        return FALSE;
    }
    
    char **parts = g_strsplit(path, ".", 2);
    if (!parts[0]) {
        g_strfreev(parts);
        return FALSE;
    }
    
    // Single level path
    if (!parts[1]) {
        gboolean result = json_get_int(root, parts[0], value);
        g_strfreev(parts);
        return result;
    }
    
    // Nested path
    JsonObject *nested = json_get_object(root, parts[0]);
    if (!nested) {
        g_strfreev(parts);
        return FALSE;
    }
    
    gboolean result = json_get_int(nested, parts[1], value);
    g_strfreev(parts);
    return result;
}