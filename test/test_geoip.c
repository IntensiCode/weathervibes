#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include "src/geoip.h"

int main(void) {
    printf("GeoIP Location Detection Test\n");
    printf("==============================\n\n");
    
    printf("Detecting your location based on IP address...\n");
    
    char *city = geoip_get_city();
    
    if (city) {
        printf("\n✅ Success! Detected location: %s\n", city);
        printf("\nThis will be used as the default city on first run.\n");
        g_free(city);
    } else {
        printf("\n❌ Failed to detect location.\n");
        printf("Will fall back to default city: Berlin\n");
    }
    
    return 0;
}