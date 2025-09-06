/* Test harness to run the weather applet standalone for debugging */

#include <gtk/gtk.h>
#include <mate-panel-applet.h>
#include "app.h"
#include "config.h"
#include "weather_fetcher.h"
#include "weather_provider.h"
#include "logger.h"

// External function from weather-applet.c
extern gboolean weather_applet_fill(MatePanelApplet *applet);

// Mock applet for testing
typedef struct {
    GtkWidget window;  // First member must be compatible with GtkWidget
    GtkWidget *container;
} MockApplet;

static void on_window_destroy(GtkWidget *widget, gpointer data) {
    gtk_main_quit();
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    
    // Initialize logging
    logger_init();
    log_info("Test applet starting in standalone mode");
    
    // Create a window to host the applet
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Weather Applet Test");
    gtk_window_set_default_size(GTK_WINDOW(window), 200, 40);
    g_signal_connect(window, "destroy", G_CALLBACK(on_window_destroy), NULL);
    
    // Create a mock applet structure
    // Note: This is a simplified mock - the real applet has more fields
    MockApplet *mock_applet = g_new0(MockApplet, 1);
    
    // Initialize the applet
    if (weather_applet_fill((MatePanelApplet*)mock_applet)) {
        log_info("Applet initialized successfully");
        
        // Add the applet container to our window
        if (mock_applet->container) {
            gtk_container_add(GTK_CONTAINER(window), mock_applet->container);
        }
    } else {
        log_error("Failed to initialize applet");
        return 1;
    }
    
    // Show everything
    gtk_widget_show_all(window);
    
    log_info("Starting GTK main loop");
    gtk_main();
    
    log_info("Test applet shutting down");
    logger_cleanup();
    
    return 0;
}