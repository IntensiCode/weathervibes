#ifndef WEATHER_GRAPH_H
#define WEATHER_GRAPH_H

#include <gtk/gtk.h>
#include "weather_provider.h"

// Create a new weather graph widget
GtkWidget* weather_graph_new(void);

// Update the graph with new weather data
void weather_graph_update(GtkWidget *graph, WeatherData *data);

#endif