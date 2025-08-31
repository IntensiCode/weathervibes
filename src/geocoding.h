#ifndef GEOCODING_H
#define GEOCODING_H

#include <glib.h>

// Structure to hold geocoding results
typedef struct {
    double latitude;
    double longitude;
    char *display_name;  // Full location name from geocoding service
    char *city;          // Extracted city name
    char *country;       // Extracted country
} GeoLocation;

// Geocode a city name to coordinates using OpenStreetMap Nominatim
// Returns NULL on failure, caller must free result with geo_location_free()
GeoLocation* geocode_location(const char *location);

// Free a GeoLocation structure
void geo_location_free(GeoLocation *location);

#endif // GEOCODING_H