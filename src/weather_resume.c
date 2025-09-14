#include "weather_resume.h"
#include "weather_update.h"
#include "logger.h"
#include <gio/gio.h>
#include <time.h>

#define POLL_INTERVAL_SECONDS 10

typedef struct _ResumeDetector {
    WeatherApplet *applet;
    guint update_interval_seconds;
    time_t last_actual_update;
    guint poll_timer_id;
    guint dbus_signal_id;
    GDBusConnection *dbus_connection;
} ResumeDetector;

// Global list to track all active detectors (for cleanup)
static GList *active_detectors = NULL;

// Forward declarations
static gboolean poll_timer_callback(gpointer user_data);
static void on_prepare_for_sleep(GDBusConnection *connection,
                                 const gchar *sender_name,
                                 const gchar *object_path,
                                 const gchar *interface_name,
                                 const gchar *signal_name,
                                 GVariant *parameters,
                                 gpointer user_data);

// Check if update is needed based on elapsed time
static gboolean is_update_needed(ResumeDetector *detector) {
    time_t now = time(NULL);
    time_t elapsed = now - detector->last_actual_update;
    return (elapsed >= detector->update_interval_seconds);
}

// Perform update if needed
static void check_and_update(ResumeDetector *detector) {
    if (is_update_needed(detector)) {
        log_info("Resume detector: Triggering weather update");
        detector->last_actual_update = time(NULL);
        update_weather(detector->applet);
    }
}

// Polling timer callback - checks every 10 seconds
static gboolean poll_timer_callback(gpointer user_data) {
    ResumeDetector *detector = (ResumeDetector *)user_data;
    
    // Check if we need to update (handles both regular interval and resume)
    check_and_update(detector);
    
    return TRUE; // Continue polling
}

// D-Bus signal handler for suspend/resume
static void on_prepare_for_sleep(GDBusConnection *connection,
                                 const gchar *sender_name,
                                 const gchar *object_path,
                                 const gchar *interface_name,
                                 const gchar *signal_name,
                                 GVariant *parameters,
                                 gpointer user_data) {
    (void)connection; (void)sender_name; (void)object_path;
    (void)interface_name; (void)signal_name;
    
    ResumeDetector *detector = (ResumeDetector *)user_data;
    gboolean going_to_sleep = FALSE;
    
    g_variant_get(parameters, "(b)", &going_to_sleep);
    
    if (!going_to_sleep) {
        // System just resumed from suspend
        log_info("System resumed from suspend (D-Bus signal)");
        check_and_update(detector);
    }
}

// Try to set up D-Bus monitoring for systemd suspend/resume
static gboolean setup_dbus_monitoring(ResumeDetector *detector) {
    GError *error = NULL;
    
    // Try to connect to system bus
    detector->dbus_connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (!detector->dbus_connection) {
        if (error) {
            log_info("Cannot connect to system bus: %s", error->message);
            g_error_free(error);
        }
        return FALSE;
    }
    
    // Subscribe to PrepareForSleep signal from systemd-logind
    detector->dbus_signal_id = g_dbus_connection_signal_subscribe(
        detector->dbus_connection,
        "org.freedesktop.login1",           // sender
        "org.freedesktop.login1.Manager",    // interface
        "PrepareForSleep",                   // signal name
        "/org/freedesktop/login1",           // object path
        NULL,                                // arg0
        G_DBUS_SIGNAL_FLAGS_NONE,
        on_prepare_for_sleep,
        detector,
        NULL);
    
    if (detector->dbus_signal_id > 0) {
        log_info("Successfully subscribed to systemd suspend/resume signals");
        return TRUE;
    }
    
    log_info("Failed to subscribe to systemd signals");
    return FALSE;
}

// Set up polling fallback
static void setup_polling(ResumeDetector *detector) {
    detector->poll_timer_id = g_timeout_add_seconds(
        POLL_INTERVAL_SECONDS,
        poll_timer_callback,
        detector);
    
    log_info("Set up %d-second polling for resume detection", POLL_INTERVAL_SECONDS);
}

// Main entry point - creates resume-aware timer
guint weather_resume_timer_start(WeatherApplet *applet, guint interval_minutes) {
    if (!applet) {
        log_error("weather_resume_timer_start: applet is NULL");
        return 0;
    }
    
    ResumeDetector *detector = g_new0(ResumeDetector, 1);
    detector->applet = applet;
    detector->update_interval_seconds = interval_minutes * 60;
    detector->last_actual_update = time(NULL);
    
    // Try D-Bus first (for systemd systems)
    gboolean using_dbus = setup_dbus_monitoring(detector);
    
    // Always set up polling as well
    // - On systemd systems: provides backup and handles clock changes
    // - On non-systemd systems: primary detection method
    setup_polling(detector);
    
    if (using_dbus) {
        log_info("Using hybrid resume detection (D-Bus + polling)");
    } else {
        log_info("Using polling-only resume detection (no systemd)");
    }
    
    // Add to active list for cleanup
    active_detectors = g_list_append(active_detectors, detector);
    
    // Return the poll timer ID as the main timer ID
    // (since we always have polling, this is guaranteed to exist)
    return detector->poll_timer_id;
}

// Stop the resume-aware timer
void weather_resume_timer_stop(guint timer_id) {
    if (timer_id == 0) return;
    
    // Find the detector by timer ID
    ResumeDetector *detector = NULL;
    for (GList *l = active_detectors; l != NULL; l = l->next) {
        ResumeDetector *d = (ResumeDetector *)l->data;
        if (d->poll_timer_id == timer_id) {
            detector = d;
            break;
        }
    }
    
    if (!detector) {
        // Fallback: just remove the timer
        g_source_remove(timer_id);
        return;
    }
    
    // Clean up D-Bus if active
    if (detector->dbus_signal_id > 0 && detector->dbus_connection) {
        g_dbus_connection_signal_unsubscribe(detector->dbus_connection, 
                                            detector->dbus_signal_id);
    }
    if (detector->dbus_connection) {
        g_object_unref(detector->dbus_connection);
    }
    
    // Remove polling timer
    if (detector->poll_timer_id > 0) {
        g_source_remove(detector->poll_timer_id);
    }
    
    // Remove from active list and free
    active_detectors = g_list_remove(active_detectors, detector);
    g_free(detector);
    
    log_info("Stopped resume-aware timer");
}