#include "weather_graph.h"
#include <cairo.h>
#include <math.h>
#include <time.h>

typedef struct {
    WeatherData *weather_data;
} WeatherGraphPrivate;

static gboolean on_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    WeatherGraphPrivate *priv = (WeatherGraphPrivate *)user_data;
    
    if (!priv || !priv->weather_data || !priv->weather_data->hourly_forecast || 
        priv->weather_data->hourly_count < 2) {
        // Get theme colors for the placeholder text
        GtkStyleContext *context = gtk_widget_get_style_context(widget);
        GdkRGBA fg_color;
        gtk_style_context_get_color(context, gtk_widget_get_state_flags(widget), &fg_color);
        
        // Draw placeholder text if no data
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 12);
        cairo_set_source_rgba(cr, fg_color.red, fg_color.green, fg_color.blue, 0.5);
        cairo_move_to(cr, 10, 25);
        cairo_show_text(cr, "No hourly data available");
        return FALSE;
    }
    
    // Get widget dimensions
    gint width = gtk_widget_get_allocated_width(widget);
    gint height = gtk_widget_get_allocated_height(widget);
    
    // Define graph area with margins
    const int margin_left = 40;
    const int margin_right = 20;
    const int margin_top = 20;
    const int margin_bottom = 40;
    const int graph_width = width - margin_left - margin_right;
    const int graph_height = height - margin_top - margin_bottom;
    
    // Get theme colors from the widget's style context
    GtkStyleContext *context = gtk_widget_get_style_context(widget);
    GdkRGBA bg_color, fg_color;
    gtk_style_context_get_background_color(context, gtk_widget_get_state_flags(widget), &bg_color);
    gtk_style_context_get_color(context, gtk_widget_get_state_flags(widget), &fg_color);
    
    // Clear background with theme color
    cairo_set_source_rgba(cr, bg_color.red, bg_color.green, bg_color.blue, bg_color.alpha);
    cairo_paint(cr);
    
    // Draw border using muted foreground color
    cairo_set_source_rgba(cr, fg_color.red, fg_color.green, fg_color.blue, 0.3);
    cairo_set_line_width(cr, 1);
    cairo_rectangle(cr, margin_left, margin_top, graph_width, graph_height);
    cairo_stroke(cr);
    
    WeatherData *data = priv->weather_data;
    int hour_count = MIN(data->hourly_count, 24);  // Limit to 24 hours
    
    // Find temperature range
    double temp_min = data->hourly_forecast[0].temperature;
    double temp_max = data->hourly_forecast[0].temperature;
    for (int i = 1; i < hour_count; i++) {
        if (data->hourly_forecast[i].temperature < temp_min)
            temp_min = data->hourly_forecast[i].temperature;
        if (data->hourly_forecast[i].temperature > temp_max)
            temp_max = data->hourly_forecast[i].temperature;
    }
    
    // Add padding to temperature range
    double temp_range = temp_max - temp_min;
    if (temp_range < 1.0) temp_range = 1.0;  // Minimum range
    temp_min -= temp_range * 0.1;
    temp_max += temp_range * 0.1;
    temp_range = temp_max - temp_min;
    
    // Draw grid lines and Y-axis labels (temperature)
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10);
    
    for (int i = 0; i <= 5; i++) {
        double y = margin_top + (graph_height * i / 5.0);
        double temp = temp_max - (temp_range * i / 5.0);
        
        // Grid line - use very transparent foreground color
        cairo_set_source_rgba(cr, fg_color.red, fg_color.green, fg_color.blue, 0.1);
        cairo_move_to(cr, margin_left, y);
        cairo_line_to(cr, margin_left + graph_width, y);
        cairo_stroke(cr);
        
        // Temperature label - use theme foreground color
        cairo_set_source_rgba(cr, fg_color.red, fg_color.green, fg_color.blue, 0.8);
        char temp_str[32];
        snprintf(temp_str, sizeof(temp_str), "%.1f°", temp);
        cairo_move_to(cr, 5, y + 3);
        cairo_show_text(cr, temp_str);
    }
    
    // Draw temperature curve
    cairo_set_source_rgb(cr, 0.8, 0.2, 0.2);
    cairo_set_line_width(cr, 2);
    
    for (int i = 0; i < hour_count; i++) {
        double x = margin_left + (graph_width * i / (double)(hour_count - 1));
        double temp = data->hourly_forecast[i].temperature;
        double y = margin_top + graph_height * (1.0 - (temp - temp_min) / temp_range);
        
        if (i == 0) {
            cairo_move_to(cr, x, y);
        } else {
            cairo_line_to(cr, x, y);
        }
    }
    cairo_stroke(cr);
    
    // Draw precipitation bars (if available)
    for (int i = 0; i < hour_count; i++) {
        double precip_prob = data->hourly_forecast[i].precipitation_probability;
        if (precip_prob >= 0) {
            double x = margin_left + (graph_width * i / (double)(hour_count - 1));
            double bar_width = graph_width / (double)hour_count * 0.8;
            double bar_height = graph_height * (precip_prob / 100.0) * 0.3;  // Max 30% of graph height
            
            // Blue for rain probability
            cairo_set_source_rgba(cr, 0.2, 0.4, 0.8, 0.4);
            cairo_rectangle(cr, x - bar_width/2, margin_top + graph_height - bar_height, 
                          bar_width, bar_height);
            cairo_fill(cr);
        }
        
        // Mark thunderstorms with special indicator
        if (data->hourly_forecast[i].has_thunderstorm) {
            double x = margin_left + (graph_width * i / (double)(hour_count - 1));
            cairo_set_source_rgb(cr, 0.8, 0.8, 0);  // Yellow
            cairo_arc(cr, x, margin_top + graph_height - 5, 3, 0, 2 * M_PI);
            cairo_fill(cr);
        }
    }
    
    // Draw X-axis labels (hours)
    cairo_set_source_rgba(cr, fg_color.red, fg_color.green, fg_color.blue, 0.8);
    cairo_set_font_size(cr, 9);
    
    for (int i = 0; i < hour_count; i += 3) {  // Show every 3 hours
        double x = margin_left + (graph_width * i / (double)(hour_count - 1));
        
        struct tm *tm = localtime(&data->hourly_forecast[i].timestamp);
        char hour_str[16];
        strftime(hour_str, sizeof(hour_str), "%H:00", tm);
        
        cairo_move_to(cr, x - 15, margin_top + graph_height + 15);
        cairo_show_text(cr, hour_str);
    }
    
    // Draw legend
    cairo_set_font_size(cr, 10);
    int legend_y = height - 15;
    
    // Temperature line
    cairo_set_source_rgb(cr, 0.8, 0.2, 0.2);
    cairo_move_to(cr, margin_left, legend_y);
    cairo_line_to(cr, margin_left + 20, legend_y);
    cairo_stroke(cr);
    cairo_set_source_rgba(cr, fg_color.red, fg_color.green, fg_color.blue, 0.8);
    cairo_move_to(cr, margin_left + 25, legend_y + 3);
    cairo_show_text(cr, "Temperature");
    
    // Precipitation bar
    cairo_set_source_rgba(cr, 0.2, 0.4, 0.8, 0.4);
    cairo_rectangle(cr, margin_left + 120, legend_y - 5, 15, 10);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, fg_color.red, fg_color.green, fg_color.blue, 0.8);
    cairo_move_to(cr, margin_left + 140, legend_y + 3);
    cairo_show_text(cr, "Precip %");
    
    // Thunderstorm indicator
    cairo_set_source_rgb(cr, 0.8, 0.8, 0);
    cairo_arc(cr, margin_left + 220, legend_y, 3, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, fg_color.red, fg_color.green, fg_color.blue, 0.8);
    cairo_move_to(cr, margin_left + 228, legend_y + 3);
    cairo_show_text(cr, "Thunder");
    
    return FALSE;
}

static void weather_graph_destroy(GtkWidget *widget, gpointer user_data) {
    WeatherGraphPrivate *priv = (WeatherGraphPrivate *)user_data;
    if (priv) {
        g_free(priv);
    }
}

GtkWidget* weather_graph_new(void) {
    GtkWidget *drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(drawing_area, 400, 200);
    
    WeatherGraphPrivate *priv = g_new0(WeatherGraphPrivate, 1);
    
    g_signal_connect(drawing_area, "draw", G_CALLBACK(on_draw), priv);
    g_signal_connect(drawing_area, "destroy", G_CALLBACK(weather_graph_destroy), priv);
    
    g_object_set_data(G_OBJECT(drawing_area), "weather-graph-private", priv);
    
    return drawing_area;
}

void weather_graph_update(GtkWidget *graph, WeatherData *data) {
    if (!graph || !GTK_IS_WIDGET(graph)) return;
    
    WeatherGraphPrivate *priv = g_object_get_data(G_OBJECT(graph), "weather-graph-private");
    if (!priv) return;
    
    priv->weather_data = data;
    gtk_widget_queue_draw(graph);
}