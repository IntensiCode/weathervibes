#include "about_dialog.h"
#include "version.h"
#include "build_info.h"
#include <gtk/gtk.h>
#include <string.h>

void show_about_dialog(GtkAction *action, WeatherApplet *weather_applet) {
    (void)action; // Unused
    (void)weather_applet; // Unused
    
    GtkWidget *dialog;
    gchar *version_text;
    
    dialog = gtk_about_dialog_new();
    
    gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(dialog), "Weather Vibes");
    if (strstr(WEATHER_VIBES_VERSION, "-dev") != NULL) {
        version_text = g_strdup_printf("%s (%s%s)",
                                       WEATHER_VIBES_VERSION,
                                       WEATHER_VIBES_GIT_COMMIT,
                                       WEATHER_VIBES_GIT_DIRTY ? "-dirty" : "");
    } else {
        version_text = g_strdup(WEATHER_VIBES_VERSION);
    }
    gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), version_text);
    gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dialog), 
                                  "☀️ 🌤️ ⛅ 🌧️ ⛈️ 🌨️\n\n"
                                  "A vibrant weather applet for MATE Panel\n\n"
                                  "Multiple weather providers with geocoding support\n\n"
                                  "Vibe-Coded with ♥\n"
                                  "Feel the weather vibes!");
    gtk_about_dialog_set_logo_icon_name(GTK_ABOUT_DIALOG(dialog), NULL);
    gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(dialog), "https://github.com/fcambus/ansiweather");
    gtk_about_dialog_set_website_label(GTK_ABOUT_DIALOG(dialog), "AnsiWeather on GitHub");
    
    // Don't set authors to avoid Credits button
    
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    g_free(version_text);
}
