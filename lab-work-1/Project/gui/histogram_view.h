#ifndef HISTOGRAM_VIEW_H
#define HISTOGRAM_VIEW_H

#include <gtk/gtk.h>

extern "C" {
#include "histogram.h"
}

GtkWidget *create_histogram_area(
    Histogram *hist,
    const char *title
);

#endif