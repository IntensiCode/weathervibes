#ifndef NETWORK_H
#define NETWORK_H

#include <glib.h>

// Fetch JSON content from a URL using libsoup (secure HTTPS)
// Returns newly allocated string that must be freed with g_free()
char* network_fetch_json(const char *url);

// Fetch raw content from a URL using libsoup (secure HTTPS)
// Returns newly allocated string that must be freed with g_free()
char* network_fetch_url(const char *url);

// Cleanup network resources (call on app shutdown)
void network_cleanup(void);

#endif // NETWORK_H