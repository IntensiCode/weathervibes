#include "weather_resume.h"
#include "weather_update.h"
#include "logger.h"
#include <gio/gio.h>
#include <time.h>

#define POLL_INTERVAL_SECONDS 10
#define RESUME_RETRY_COUNT 3
#define RESUME_RETRY_WINDOW_SECONDS 180

// NetworkManager states (org.freedesktop.NetworkManager)
#define NM_STATE_CONNECTED_LOCAL 50

typedef struct _ResumeDetector {
    WeatherApplet *applet;
    guint update_interval_seconds;
    time_t last_actual_update;
    time_t last_suspend_time;
    time_t last_resume_time;
    gboolean pending_resume_refresh;
    guint poll_timer_id;
    guint dbus_signal_id;
    guint nm_signal_id;
    GDBusConnection *dbus_connection;
    gboolean has_network_manager;
    guint resume_retry_ids[RESUME_RETRY_COUNT];
} ResumeDetector;

// Global list to track all active detectors (for cleanup)
static GList *active_detectors = NULL;

// Forward declarations
static gboolean poll_timer_callback(gpointer user_data);
static gboolean resume_retry_callback(gpointer user_data);
static void schedule_resume_retries(ResumeDetector *detector);
static void cancel_resume_retries(ResumeDetector *detector);
static void trigger_update(ResumeDetector *detector, const char *reason);
static ResumeDetector *find_detector_by_applet(WeatherApplet *applet);
static gboolean network_manager_is_ready(ResumeDetector *detector);
static gboolean setup_network_manager_monitoring(ResumeDetector *detector);
static void on_nm_properties_changed(GDBusConnection *connection,
                                     const gchar *sender_name,
                                     const gchar *object_path,
                                     const gchar *interface_name,
                                     const gchar *signal_name,
                                     GVariant *parameters,
                                     gpointer user_data);
static void on_prepare_for_sleep(GDBusConnection *connection,
                                 const gchar *sender_name,
                                 const gchar *object_path,
                                 const gchar *interface_name,
                                 const gchar *signal_name,
                                 GVariant *parameters,
                                 gpointer user_data);

static void cancel_resume_retries(ResumeDetector *detector) {
    if (!detector) return;

    for (int i = 0; i < RESUME_RETRY_COUNT; i++) {
        if (detector->resume_retry_ids[i] > 0) {
            g_source_remove(detector->resume_retry_ids[i]);
            detector->resume_retry_ids[i] = 0;
        }
    }
}

static gboolean is_within_resume_retry_window(ResumeDetector *detector) {
    if (!detector || !detector->pending_resume_refresh || detector->last_resume_time <= 0) {
        return FALSE;
    }

    time_t now = time(NULL);
    return (now - detector->last_resume_time) <= RESUME_RETRY_WINDOW_SECONDS;
}

static gboolean network_manager_is_ready(ResumeDetector *detector) {
    if (!detector || !detector->dbus_connection || !detector->has_network_manager) {
        return FALSE;
    }

    GError *error = NULL;
    GVariant *result = g_dbus_connection_call_sync(
        detector->dbus_connection,
        "org.freedesktop.NetworkManager",
        "/org/freedesktop/NetworkManager",
        "org.freedesktop.DBus.Properties",
        "Get",
        g_variant_new("(ss)", "org.freedesktop.NetworkManager", "State"),
        G_VARIANT_TYPE("(v)"),
        G_DBUS_CALL_FLAGS_NONE,
        1000,
        NULL,
        &error);

    if (!result) {
        if (error) {
            log_debug("NetworkManager state query failed: %s", error->message);
            g_error_free(error);
        }
        return FALSE;
    }

    GVariant *value = NULL;
    guint state = 0;
    g_variant_get(result, "(v)", &value);
    if (value) {
        g_variant_get(value, "u", &state);
        g_variant_unref(value);
    }
    g_variant_unref(result);

    return state >= NM_STATE_CONNECTED_LOCAL;
}

static void trigger_update(ResumeDetector *detector, const char *reason) {
    if (!detector || !detector->applet) return;

    log_info("Resume detector: Triggering weather update (%s)", reason);
    detector->last_actual_update = time(NULL);
    update_weather(detector->applet);
}

static gboolean resume_retry_callback(gpointer user_data) {
    ResumeDetector *detector = (ResumeDetector *)user_data;

    if (!detector || !detector->pending_resume_refresh) {
        return G_SOURCE_REMOVE;
    }

    if (!is_within_resume_retry_window(detector)) {
        cancel_resume_retries(detector);
        return G_SOURCE_REMOVE;
    }

    if (detector->has_network_manager && network_manager_is_ready(detector)) {
        trigger_update(detector, "network-ready-retry");
        return G_SOURCE_REMOVE;
    }

    trigger_update(detector, "resume-retry");
    return G_SOURCE_REMOVE;
}

static void schedule_resume_retries(ResumeDetector *detector) {
    static const guint retry_delays[RESUME_RETRY_COUNT] = {15, 45, 90};

    if (!detector || !detector->pending_resume_refresh) {
        return;
    }

    cancel_resume_retries(detector);

    for (int i = 0; i < RESUME_RETRY_COUNT; i++) {
        detector->resume_retry_ids[i] = g_timeout_add_seconds(
            retry_delays[i],
            resume_retry_callback,
            detector);
    }
}

// Check if update is needed based on elapsed time or resume from suspend
static gboolean is_update_needed(ResumeDetector *detector, gboolean from_resume) {
    time_t now = time(NULL);
    time_t elapsed = now - detector->last_actual_update;
    
    if (from_resume && detector->last_suspend_time > 0) {
        time_t sleep_duration = now - detector->last_suspend_time;
        if (sleep_duration > 30) {
            log_info("Resume detector: System was asleep for %ld seconds, forcing update",
                     sleep_duration);
            return TRUE;
        }
    }
    
    return (elapsed >= detector->update_interval_seconds);
}

// Perform update if needed
static void check_and_update(ResumeDetector *detector, gboolean from_resume) {
    if (!detector) return;

    if (detector->pending_resume_refresh && detector->has_network_manager &&
        network_manager_is_ready(detector)) {
        trigger_update(detector, "network-ready-poll");
        return;
    }

    if (is_update_needed(detector, from_resume)) {
        trigger_update(detector, from_resume ? "resume" : "interval");
    }
}

// Polling timer callback - checks every 10 seconds
static gboolean poll_timer_callback(gpointer user_data) {
    ResumeDetector *detector = (ResumeDetector *)user_data;
    
    // Check if we need to update (handles both regular interval and resume)
    check_and_update(detector, FALSE);
    
    return TRUE; // Continue polling
}

static void on_nm_properties_changed(GDBusConnection *connection,
                                     const gchar *sender_name,
                                     const gchar *object_path,
                                     const gchar *interface_name,
                                     const gchar *signal_name,
                                     GVariant *parameters,
                                     gpointer user_data) {
    (void)connection; (void)sender_name; (void)object_path;
    (void)interface_name; (void)signal_name;

    ResumeDetector *detector = (ResumeDetector *)user_data;
    if (!detector || !detector->pending_resume_refresh) {
        return;
    }

    const gchar *changed_interface = NULL;
    GVariant *changed_props = NULL;
    GVariant *invalidated = NULL;
    g_variant_get(parameters, "(&s@a{sv}@as)", &changed_interface, &changed_props, &invalidated);

    if (g_strcmp0(changed_interface, "org.freedesktop.NetworkManager") != 0) {
        if (changed_props) g_variant_unref(changed_props);
        if (invalidated) g_variant_unref(invalidated);
        return;
    }

    guint state = 0;
    guint connectivity = 0;
    gboolean state_changed = g_variant_lookup(changed_props, "State", "u", &state);
    gboolean connectivity_changed = g_variant_lookup(changed_props, "Connectivity", "u", &connectivity);

    if ((state_changed || connectivity_changed) && network_manager_is_ready(detector)) {
        trigger_update(detector, "network-ready-signal");
    }

    if (changed_props) g_variant_unref(changed_props);
    if (invalidated) g_variant_unref(invalidated);
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
    
    if (going_to_sleep) {
        // System is going to sleep - record the time
        log_info("System preparing for sleep (D-Bus signal)");
        detector->last_suspend_time = time(NULL);
    } else {
        // System just resumed from suspend
        log_info("System resumed from suspend (D-Bus signal)");
        if (is_update_needed(detector, TRUE)) {
            detector->last_resume_time = time(NULL);
            detector->pending_resume_refresh = TRUE;

            if (detector->has_network_manager && network_manager_is_ready(detector)) {
                trigger_update(detector, "resume-network-ready");
            } else {
                log_info("Resume detector: Waiting for network readiness after resume");
                schedule_resume_retries(detector);
            }
        }
    }
}

static gboolean setup_network_manager_monitoring(ResumeDetector *detector) {
    GError *error = NULL;
    GVariant *result = g_dbus_connection_call_sync(
        detector->dbus_connection,
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        "NameHasOwner",
        g_variant_new("(s)", "org.freedesktop.NetworkManager"),
        G_VARIANT_TYPE("(b)"),
        G_DBUS_CALL_FLAGS_NONE,
        1000,
        NULL,
        &error);

    if (!result) {
        if (error) {
            log_info("Could not detect NetworkManager on D-Bus: %s", error->message);
            g_error_free(error);
        }
        return FALSE;
    }

    gboolean has_owner = FALSE;
    g_variant_get(result, "(b)", &has_owner);
    g_variant_unref(result);

    if (!has_owner) {
        log_info("NetworkManager not present on D-Bus; using fallback retries");
        return FALSE;
    }

    detector->nm_signal_id = g_dbus_connection_signal_subscribe(
        detector->dbus_connection,
        "org.freedesktop.NetworkManager",
        "org.freedesktop.DBus.Properties",
        "PropertiesChanged",
        "/org/freedesktop/NetworkManager",
        "org.freedesktop.NetworkManager",
        G_DBUS_SIGNAL_FLAGS_NONE,
        on_nm_properties_changed,
        detector,
        NULL);

    if (detector->nm_signal_id == 0) {
        log_info("Failed to subscribe to NetworkManager state changes");
        return FALSE;
    }

    log_info("Subscribed to NetworkManager state changes");
    return TRUE;
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
        detector->has_network_manager = setup_network_manager_monitoring(detector);
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
    detector->last_resume_time = 0;
    detector->pending_resume_refresh = FALSE;
    
    // Try D-Bus first (for systemd systems)
    gboolean using_dbus = setup_dbus_monitoring(detector);
    
    // Always set up polling as well
    // - On systemd systems: provides backup and handles clock changes
    // - On non-systemd systems: primary detection method
    setup_polling(detector);
    
    if (using_dbus) {
        if (detector->has_network_manager) {
            log_info("Using hybrid resume detection (logind + NetworkManager + polling)");
        } else {
            log_info("Using hybrid resume detection (logind + polling + fallback retries)");
        }
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
    if (detector->nm_signal_id > 0 && detector->dbus_connection) {
        g_dbus_connection_signal_unsubscribe(detector->dbus_connection,
                                            detector->nm_signal_id);
    }
    if (detector->dbus_connection) {
        g_object_unref(detector->dbus_connection);
    }
    
    // Remove polling timer
    if (detector->poll_timer_id > 0) {
        g_source_remove(detector->poll_timer_id);
    }

    cancel_resume_retries(detector);
    
    // Remove from active list and free
    active_detectors = g_list_remove(active_detectors, detector);
    g_free(detector);
    
    log_info("Stopped resume-aware timer");
}

static ResumeDetector *find_detector_by_applet(WeatherApplet *applet) {
    for (GList *l = active_detectors; l != NULL; l = l->next) {
        ResumeDetector *detector = (ResumeDetector *)l->data;
        if (detector->applet == applet) {
            return detector;
        }
    }
    return NULL;
}

void weather_resume_notify_fetch_failed(WeatherApplet *applet) {
    ResumeDetector *detector = find_detector_by_applet(applet);
    if (!detector || !detector->pending_resume_refresh) {
        return;
    }

    if (is_within_resume_retry_window(detector)) {
        log_info("Resume detector: Post-resume fetch failed, keeping retry schedule active");
        schedule_resume_retries(detector);
    } else {
        detector->pending_resume_refresh = FALSE;
        cancel_resume_retries(detector);
    }
}

void weather_resume_notify_fetch_success(WeatherApplet *applet) {
    ResumeDetector *detector = find_detector_by_applet(applet);
    if (!detector) {
        return;
    }

    detector->pending_resume_refresh = FALSE;
    detector->last_resume_time = 0;
    cancel_resume_retries(detector);
}
