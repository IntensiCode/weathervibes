#include "network.h"
#include "logger.h"
#include <libsoup/soup.h>
#include <string.h>

// Maximum response size: 1MB
#define MAX_RESPONSE_SIZE (1024 * 1024)

// Timeout in seconds
#define REQUEST_TIMEOUT 30

static SoupSession *session = NULL;

static void ensure_session_initialized(void) {
    if (!session) {
        session = soup_session_new_with_options(
            SOUP_SESSION_TIMEOUT, REQUEST_TIMEOUT,
            SOUP_SESSION_IDLE_TIMEOUT, 60,
            SOUP_SESSION_USER_AGENT, "WeatherVibes/1.0",
            SOUP_SESSION_ACCEPT_LANGUAGE_AUTO, TRUE,
            SOUP_SESSION_SSL_STRICT, TRUE,  // Enforce HTTPS certificate validation
            NULL
        );
    }
}

char* network_fetch_json(const char *url) {
    return network_fetch_url(url);
}

char* network_fetch_url(const char *url) {
    if (!url) {
        log_error("network_fetch_url: NULL URL provided");
        return NULL;
    }
    
    // Enforce HTTPS for security (except for localhost/testing)
    if (strncmp(url, "http://", 7) == 0) {
        // Check if it's localhost or local IP for testing
        const char *host_start = url + 7;
        if (strncmp(host_start, "localhost", 9) != 0 && 
            strncmp(host_start, "127.0.0.1", 9) != 0 &&
            strncmp(host_start, "ip-api.com", 10) != 0) {  // ip-api.com doesn't support HTTPS on free tier
            // Convert HTTP to HTTPS
            char *https_url = g_strdup_printf("https://%s", url + 7);
            log_info("Upgrading HTTP to HTTPS: %s", https_url);
            char *result = network_fetch_url(https_url);
            g_free(https_url);
            return result;
        }
    }
    
    ensure_session_initialized();
    
    // Create the request
    SoupMessage *msg = soup_message_new("GET", url);
    if (!msg) {
        log_error("Failed to create request for URL: %s", url);
        return NULL;
    }
    
    // Send the request synchronously
    guint status = soup_session_send_message(session, msg);
    
    if (status != SOUP_STATUS_OK) {
        log_warn("HTTP request failed with status %u for URL: %s", status, url);
        log_debug("HTTP status reason: %s", soup_status_get_phrase(status));
        g_object_unref(msg);
        return NULL;
    }
    
    // Get the response body
    SoupMessageBody *body = msg->response_body;
    if (!body || body->length == 0) {
        log_warn("Empty response from URL: %s", url);
        g_object_unref(msg);
        return NULL;
    }
    
    // Check response size
    if (body->length > MAX_RESPONSE_SIZE) {
        log_error("Response too large (%ld bytes) from URL: %s", body->length, url);
        g_object_unref(msg);
        return NULL;
    }
    
    // Copy the response data
    char *result = g_strndup(body->data, body->length);
    
    log_debug("Successfully fetched %ld bytes from %s", body->length, url);
    
    g_object_unref(msg);
    return result;
}

void network_cleanup(void) {
    if (session) {
        g_object_unref(session);
        session = NULL;
    }
}