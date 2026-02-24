#ifndef WEATHER_RESUME_H
#define WEATHER_RESUME_H

#include <glib.h>
#include "weather_applet.h"

typedef struct _ResumeDetector ResumeDetector;

// Create a resume-aware timer that triggers on interval OR system resume
// Returns a source ID that can be used with g_source_remove()
guint weather_resume_timer_start(WeatherApplet *applet, guint interval_minutes);

// Stop the resume-aware timer
void weather_resume_timer_stop(guint timer_id);

// Notify resume detector about async fetch result.
void weather_resume_notify_fetch_failed(WeatherApplet *applet);
void weather_resume_notify_fetch_success(WeatherApplet *applet);

#endif // WEATHER_RESUME_H
