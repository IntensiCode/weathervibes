#include "async_fetch.h"
#include "network.h"
#include "weather_fetcher.h"
#include "app.h"
#include "logger.h"
#include <libsoup/soup.h>

extern AppContext *g_app_context;

typedef struct {
    char *url;
    GCancellable *cancellable;
} FetchData;

static void fetch_data_free(FetchData *data) {
    g_free(data->url);
    if (data->cancellable) {
        g_object_unref(data->cancellable);
    }
    g_free(data);
}

// Thread function that performs the actual fetch
static void fetch_in_thread(GTask *task,
                           gpointer source_object,
                           gpointer task_data,
                           GCancellable *cancellable) {
    FetchData *data = task_data;
    
    // Check if cancelled before starting
    if (g_task_return_error_if_cancelled(task)) {
        return;
    }
    
    log_debug("Async fetch starting for URL: %s", data->url);
    
    // Perform the synchronous fetch in this thread
    char *result = network_fetch_url(data->url);
    
    // Check if cancelled after fetch
    if (g_task_return_error_if_cancelled(task)) {
        g_free(result);
        return;
    }
    
    if (result) {
        // Success - return the result
        g_task_return_pointer(task, result, g_free);
        log_debug("Async fetch completed successfully");
    } else {
        // Failure - return an error
        g_task_return_new_error(task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Failed to fetch data from URL");
        log_warn("Async fetch failed");
    }
}

void async_fetch_url(const char *url,
                    GCancellable *cancellable,
                    GAsyncReadyCallback callback,
                    gpointer user_data) {
    GTask *task;
    FetchData *data;
    
    // Create the task
    task = g_task_new(NULL, cancellable, callback, user_data);
    g_task_set_source_tag(task, async_fetch_url);
    
    // Prepare the data
    data = g_new0(FetchData, 1);
    data->url = g_strdup(url);
    data->cancellable = cancellable ? g_object_ref(cancellable) : NULL;
    
    // Set the task data with destructor
    g_task_set_task_data(task, data, (GDestroyNotify)fetch_data_free);
    
    // Run in thread pool
    g_task_run_in_thread(task, fetch_in_thread);
    
    // Unref the task (it will stay alive until completion)
    g_object_unref(task);
}

char* async_fetch_finish(GAsyncResult *result, GError **error) {
    g_return_val_if_fail(g_task_is_valid(result, NULL), NULL);
    g_return_val_if_fail(g_task_get_source_tag(G_TASK(result)) == async_fetch_url, NULL);
    
    return g_task_propagate_pointer(G_TASK(result), error);
}

// Async wrapper for weather fetching
typedef struct {
    WeatherProvider provider;
    char *location;
    char *api_key;
    GCancellable *cancellable;
} WeatherFetchData;

static void weather_fetch_data_free(WeatherFetchData *data) {
    g_free(data->location);
    g_free(data->api_key);
    if (data->cancellable) {
        g_object_unref(data->cancellable);
    }
    g_free(data);
}

static void weather_fetch_in_thread(GTask *task,
                                   gpointer source_object,
                                   gpointer task_data,
                                   GCancellable *cancellable) {
    WeatherFetchData *data = task_data;
    
    // Check if cancelled before starting
    if (g_task_return_error_if_cancelled(task)) {
        return;
    }
    
    log_info("Async weather fetch starting for location: %s", data->location);
    
    // Perform the synchronous fetch in this thread
    // We need to use weather_fetcher_update which handles everything
    gboolean success = weather_fetcher_update_with_provider(data->location, data->provider);
    
    WeatherData *result = NULL;
    if (success && g_app_context && g_app_context->weather_data) {
        // Copy the data from global context
        g_mutex_lock(&g_app_context->data_mutex);
        result = weather_fetcher_copy_data(g_app_context->weather_data);
        g_mutex_unlock(&g_app_context->data_mutex);
    }
    
    // Check if cancelled after fetch
    if (g_task_return_error_if_cancelled(task)) {
        if (result) {
            weather_fetcher_free_data(result);
        }
        return;
    }
    
    if (result) {
        // Success - return the result
        g_task_return_pointer(task, result, (GDestroyNotify)weather_fetcher_free_data);
        log_info("Async weather fetch completed successfully");
    } else {
        // Failure - return an error
        g_task_return_new_error(task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Failed to fetch weather data");
        log_warn("Async weather fetch failed");
    }
}

void async_fetch_weather(WeatherProvider provider,
                        const char *location,
                        const char *api_key,
                        GCancellable *cancellable,
                        GAsyncReadyCallback callback,
                        gpointer user_data) {
    GTask *task;
    WeatherFetchData *data;
    
    // Create the task
    task = g_task_new(NULL, cancellable, callback, user_data);
    g_task_set_source_tag(task, async_fetch_weather);
    
    // Prepare the data
    data = g_new0(WeatherFetchData, 1);
    data->provider = provider;
    data->location = g_strdup(location);
    data->api_key = g_strdup(api_key);
    data->cancellable = cancellable ? g_object_ref(cancellable) : NULL;
    
    // Set the task data with destructor
    g_task_set_task_data(task, data, (GDestroyNotify)weather_fetch_data_free);
    
    // Run in thread pool
    g_task_run_in_thread(task, weather_fetch_in_thread);
    
    // Unref the task (it will stay alive until completion)
    g_object_unref(task);
}

WeatherData* async_fetch_weather_finish(GAsyncResult *result, GError **error) {
    g_return_val_if_fail(g_task_is_valid(result, NULL), NULL);
    g_return_val_if_fail(g_task_get_source_tag(G_TASK(result)) == async_fetch_weather, NULL);
    
    return g_task_propagate_pointer(G_TASK(result), error);
}