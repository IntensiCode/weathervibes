/* Direct test of weather applet functionality */
#include <gtk/gtk.h>
#include "src/app.h"
#include "src/config.h"
#include "src/weather_fetcher.h"
#include "src/logger.h"

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    
    // Initialize logging
    logger_init();
    log_info("Direct test starting");
    
    // Initialize global app context
    g_app_context = g_new0(AppContext, 1);
    g_app_context->config = g_new0(AppConfig, 1);
    
    // Set up config for OpenWeather
    g_app_context->config->city = g_strdup("Geisenheim,DE");
    g_app_context->config->provider = PROVIDER_OPENWEATHER;
    const char *openweather_key = getenv("OPENWEATHER_API_KEY");
    g_app_context->config->openweather_api_key = openweather_key ? g_strdup(openweather_key) : NULL;
    g_app_context->config->use_celsius = TRUE;
    g_app_context->config->update_interval_minutes = 30;
    
    log_info("Config: City=%s, Provider=%d, API Key=%s", 
             g_app_context->config->city,
             g_app_context->config->provider,
             g_app_context->config->openweather_api_key);
    
    // Create a simple window to simulate applet
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Weather Test");
    gtk_window_set_default_size(GTK_WINDOW(window), 200, 100);
    
    GtkWidget *label = gtk_label_new("Fetching weather...");
    gtk_container_add(GTK_CONTAINER(window), label);
    
    // Force an immediate update
    log_info("Triggering weather update...");
    
    // Call the provider directly
    WeatherData *data = NULL;
    const WeatherProviderInterface *provider = weather_provider_get_interface(PROVIDER_OPENWEATHER);
    if (provider && provider->fetch_weather) {
        gboolean success = provider->fetch_weather(g_app_context->config->city, &data);
        log_info("Direct fetch result: %s", success ? "SUCCESS" : "FAILED");
    }
    
    if (data) {
        char *text = g_strdup_printf("%s %s\n%.1f°C",
            data->condition_icon ? data->condition_icon : "?",
            data->city ? data->city : "Unknown",
            data->temperature);
        gtk_label_set_text(GTK_LABEL(label), text);
        g_free(text);
        
        log_info("Weather fetched successfully: %s %.1f°C", 
                 data->city, data->temperature);
    } else {
        gtk_label_set_text(GTK_LABEL(label), "Failed to fetch weather");
        log_error("Failed to fetch weather data");
    }
    
    gtk_widget_show_all(window);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    
    // Run for 5 seconds then quit
    g_timeout_add_seconds(5, (GSourceFunc)gtk_main_quit, NULL);
    
    gtk_main();
    
    log_info("Direct test completed");
    logger_cleanup();
    
    return 0;
}