#ifndef JSON_HELPERS_H
#define JSON_HELPERS_H

#include <glib.h>
#include <json-glib/json-glib.h>

// Safe JSON extraction functions that return FALSE on error

// Get string value from JSON object
gboolean json_get_string(JsonObject *object, const char *member, char **value);

// Get double value from JSON object
gboolean json_get_double(JsonObject *object, const char *member, double *value);

// Get integer value from JSON object
gboolean json_get_int(JsonObject *object, const char *member, int *value);

// Get boolean value from JSON object
gboolean json_get_boolean(JsonObject *object, const char *member, gboolean *value);

// Get nested object from JSON object (returns NULL on error)
JsonObject* json_get_object(JsonObject *object, const char *member);

// Get array from JSON object (returns NULL on error)
JsonArray* json_get_array(JsonObject *object, const char *member);

// Parse JSON string and return root object (caller must free with json_object_cleanup)
JsonObject* json_parse_string(const char *json_string, GError **error);

// Cleanup JSON object
void json_object_cleanup(JsonObject *object);

// Nested path helpers (e.g., "main.temp" to get temp from main object)
gboolean json_get_nested_double(JsonObject *root, const char *path, double *value);
gboolean json_get_nested_string(JsonObject *root, const char *path, char **value);
gboolean json_get_nested_int(JsonObject *root, const char *path, int *value);

#endif // JSON_HELPERS_H