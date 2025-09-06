#ifndef ASYNC_FETCH_H
#define ASYNC_FETCH_H

#include <glib.h>
#include <gio/gio.h>
#include "weather_provider.h"
#include "weather_fetcher.h"

// Async URL fetching
void async_fetch_url(const char *url,
                    GCancellable *cancellable,
                    GAsyncReadyCallback callback,
                    gpointer user_data);

char* async_fetch_finish(GAsyncResult *result, GError **error);

// Async weather fetching
void async_fetch_weather(WeatherProvider provider,
                        const char *location,
                        const char *api_key,
                        GCancellable *cancellable,
                        GAsyncReadyCallback callback,
                        gpointer user_data);

WeatherData* async_fetch_weather_finish(GAsyncResult *result, GError **error);

#endif // ASYNC_FETCH_H