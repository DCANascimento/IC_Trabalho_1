#include "histogram_view.h"

#include <algorithm>


/*
 * Draw one histogram.
 */
static void draw_histogram(
    GtkDrawingArea *area,
    cairo_t *cr,
    int width,
    int height,
    gpointer user_data)
{
    Histogram *hist =
        static_cast<Histogram *>(user_data);


    /*
     * Clear the drawing area.
     */
    cairo_set_source_rgb(
        cr,
        1.0,
        1.0,
        1.0
    );

    cairo_paint(cr);


    /*
     * If there is no histogram,
     * there is nothing to draw.
     */
    if (hist->counts == NULL ||
        hist->num_bins <= 0) {
        return;
    }


    /*
     * Get the title stored on the drawing area.
     */
    const char *title =
        static_cast<const char *>(
            g_object_get_data(
                G_OBJECT(area),
                "histogram-title"
            )
        );


    /*
     * Histogram margins.
     */
    const double margin_left = 45.0;
    const double margin_right = 10.0;
    const double margin_top = 35.0;
    const double margin_bottom = 30.0;

    const double plot_width =
        width -
        margin_left -
        margin_right;

    const double plot_height =
        height -
        margin_top -
        margin_bottom;


    if (plot_width <= 0 ||
        plot_height <= 0) {
        return;
    }


    /*
     * Find the largest bin.
     */
    int max_count = 0;

    for (int i = 0;
         i < hist->num_bins;
         i++) {

        max_count =
            std::max(
                max_count,
                hist->counts[i]
            );
    }


    /*
     * Avoid division by zero.
     */
    if (max_count == 0) {
        return;
    }


    /*
     * Draw title.
     */
    cairo_set_source_rgb(
        cr,
        0.0,
        0.0,
        0.0
    );

    cairo_select_font_face(
        cr,
        "Sans",
        CAIRO_FONT_SLANT_NORMAL,
        CAIRO_FONT_WEIGHT_BOLD
    );

    cairo_set_font_size(
        cr,
        16
    );

    cairo_move_to(
        cr,
        margin_left,
        22
    );

    cairo_show_text(
        cr,
        title
    );


    /*
     * Draw axes.
     */
    cairo_set_line_width(
        cr,
        1.0
    );

    cairo_move_to(
        cr,
        margin_left,
        margin_top
    );

    cairo_line_to(
        cr,
        margin_left,
        height - margin_bottom
    );

    cairo_line_to(
        cr,
        width - margin_right,
        height - margin_bottom
    );

    cairo_stroke(cr);


    /*
     * Width of each histogram bar.
     */
    const double bar_width =
        plot_width /
        hist->num_bins;


    /*
     * Draw histogram bars.
     */
    for (int i = 0;
         i < hist->num_bins;
         i++) {

        double normalized_height =
            (double)hist->counts[i] /
            (double)max_count;

        double bar_height =
            normalized_height *
            plot_height;

        double x =
            margin_left +
            i * bar_width;

        double y =
            height -
            margin_bottom -
            bar_height;


        cairo_rectangle(
            cr,
            x,
            y,
            bar_width,
            bar_height
        );

        cairo_fill(cr);
    }
}


/*
 * Create a drawing area for one histogram.
 */
GtkWidget *create_histogram_area(
    Histogram *hist,
    const char *title)
{
    GtkWidget *area =
        gtk_drawing_area_new();


    /*
     * Allow the drawing area to expand.
     */
    gtk_widget_set_hexpand(
        area,
        TRUE
    );

    gtk_widget_set_vexpand(
        area,
        TRUE
    );


    /*
     * Store the title on the drawing area.
     */
    g_object_set_data(
        G_OBJECT(area),
        "histogram-title",
        (gpointer)title
    );


    /*
     * Configure drawing.
     */
    gtk_drawing_area_set_draw_func(
        GTK_DRAWING_AREA(area),
        draw_histogram,
        hist,
        NULL
    );


    return area;
}