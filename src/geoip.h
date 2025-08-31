#ifndef GEOIP_H
#define GEOIP_H

#include <glib.h>

// Get city based on IP geolocation
// Returns city name (caller must free) or NULL on failure
char* geoip_get_city(void);

#endif