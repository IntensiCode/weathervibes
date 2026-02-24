#include "network.h"
#include "logger.h"
#include <libsoup/soup.h>
#include <string.h>

// Maximum response size: 1MB
#define MAX_RESPONSE_SIZE (1024 * 1024)

// Timeout in seconds
#define REQUEST_TIMEOUT 30

// Request retry behavior
#define MAX_REQUEST_ATTEMPTS 3
#define RETRY_BASE_DELAY_MS 500
#define RETRY_MAX_DELAY_MS 2000

static SoupSession *session = NULL;

static const char* classify_status(guint status) {
    if (status >= 500 && status < 600) {
        return "http-5xx";
    }
    if (status >= 400 && status < 500) {
        return "http-4xx";
    }
    if (status == SOUP_STATUS_REQUEST_TIMEOUT) {
        return "timeout";
    }
    if (status == SOUP_STATUS_CANT_RESOLVE || status == SOUP_STATUS_CANT_RESOLVE_PROXY) {
        return "dns";
    }
    if (status == SOUP_STATUS_CANT_CONNECT || status == SOUP_STATUS_CANT_CONNECT_PROXY) {
        return "connect";
    }
    if (status == SOUP_STATUS_SSL_FAILED) {
        return "tls";
    }
    if (status == SOUP_STATUS_IO_ERROR || status == SOUP_STATUS_TRY_AGAIN) {
        return "network";
    }
    if (status == SOUP_STATUS_CANCELLED) {
        return "cancelled";
    }
    return "other";
}

static gboolean should_retry_status(guint status) {
    if (status >= 500 && status < 600) {
        return TRUE;
    }
    if (status == SOUP_STATUS_REQUEST_TIMEOUT ||
        status == SOUP_STATUS_CANT_RESOLVE ||
        status == SOUP_STATUS_CANT_RESOLVE_PROXY ||
        status == SOUP_STATUS_CANT_CONNECT ||
        status == SOUP_STATUS_CANT_CONNECT_PROXY ||
        status == SOUP_STATUS_IO_ERROR ||
        status == SOUP_STATUS_TRY_AGAIN) {
        return TRUE;
    }
    return FALSE;
}

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
    
    for (guint attempt = 1; attempt <= MAX_REQUEST_ATTEMPTS; attempt++) {
        // Create the request
        SoupMessage *msg = soup_message_new("GET", url);
        if (!msg) {
            log_error("Failed to create request for URL: %s", url);
            return NULL;
        }

        // Send the request synchronously
        guint status = soup_session_send_message(session, msg);

        if (status == SOUP_STATUS_OK) {
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

        gboolean retryable = should_retry_status(status);
        const char *error_class = classify_status(status);
        const char *reason = soup_status_get_phrase(status);

        log_warn("HTTP request failed (%s, status %u: %s) attempt %u/%u for URL: %s",
                 error_class,
                 status,
                 reason ? reason : "Unknown",
                 attempt,
                 MAX_REQUEST_ATTEMPTS,
                 url);

        g_object_unref(msg);

        if (!retryable || attempt == MAX_REQUEST_ATTEMPTS) {
            return NULL;
        }

        guint delay_ms = RETRY_BASE_DELAY_MS << (attempt - 1);
        if (delay_ms > RETRY_MAX_DELAY_MS) {
            delay_ms = RETRY_MAX_DELAY_MS;
        }

        log_info("Retrying request in %u ms (attempt %u/%u)",
                 delay_ms,
                 attempt + 1,
                 MAX_REQUEST_ATTEMPTS);
        g_usleep((gulong)delay_ms * 1000);
    }

    return NULL;
}

void network_cleanup(void) {
    if (session) {
        g_object_unref(session);
        session = NULL;
    }
}
