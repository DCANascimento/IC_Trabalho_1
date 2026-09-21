#include "app.h"
#include "histogram_view.h"

#include <gtk/gtk.h>

#include <cstdlib>


extern "C" {
#include "wav.h"
#include "histogram.h"
}


/*
 * Data used by the GUI.
 */
typedef struct {
    GtkWindow *window;
    GtkWidget *bits_entry;

    GtkWidget *left_area;
    GtkWidget *right_area;
    GtkWidget *mid_area;
    GtkWidget *side_area;

    Histogram left;
    Histogram right;
    Histogram mid;
    Histogram side;

    bool has_histograms;
} AppData;


/*
 * Called when the user selects a WAV file.
 */
static void file_selected(
    GObject *source_object,
    GAsyncResult *result,
    gpointer user_data)
{
    AppData *app_data =
        static_cast<AppData *>(user_data);

    GtkFileDialog *dialog =
        GTK_FILE_DIALOG(source_object);

    GError *error = NULL;


    /*
     * Get the selected file.
     */
    GFile *file =
        gtk_file_dialog_open_finish(
            dialog,
            result,
            &error
        );

    if (file == NULL) {

        if (error != NULL) {
            g_error_free(error);
        }

        return;
    }


    /*
     * Get the filename.
     */
    char *filename =
        g_file_get_path(file);

    if (filename == NULL) {
        g_object_unref(file);
        return;
    }


    /*
     * Get the bits value.
     */
    const char *bits_text =
        gtk_editable_get_text(
            GTK_EDITABLE(app_data->bits_entry)
        );

    int bits =
        atoi(bits_text);


    /*
     * Validate bits.
     */
    if (bits <= 0) {

        g_print(
            "Bits must be greater than 0\n"
        );

        g_free(filename);
        g_object_unref(file);

        return;
    }


    g_print(
        "Bits: %d\n",
        bits
    );


    /*
     * Read the WAV.
     */
    Audio audio;

    if (wav_read(filename, &audio) != 0) {

        g_print(
            "Could not read WAV file\n"
        );

        g_free(filename);
        g_object_unref(file);

        return;
    }


    /*
     * Print WAV information.
     */
    g_print(
        "Opened: %s\n",
        filename
    );

    g_print(
        "Sample rate: %d Hz\n",
        audio.sample_rate
    );

    g_print(
        "Channels: %d\n",
        audio.num_channels
    );

    g_print(
        "Samples: %zu\n",
        audio.num_samples
    );


    /*
     * Free previous histograms.
     */
    if (app_data->has_histograms) {

        histogram_free(
            &app_data->left
        );

        histogram_free(
            &app_data->right
        );

        histogram_free(
            &app_data->mid
        );

        histogram_free(
            &app_data->side
        );

        app_data->has_histograms = false;
    }


    /*
     * Create all four histograms.
     */
    create_histograms(
        &audio,
        bits,
        &app_data->left,
        &app_data->right,
        &app_data->mid,
        &app_data->side
    );

    app_data->has_histograms = true;


    /*
     * Print histogram information.
     */
    g_print(
        "Left bins: %d\n",
        app_data->left.num_bins
    );

    g_print(
        "Right bins: %d\n",
        app_data->right.num_bins
    );

    g_print(
        "Mid bins: %d\n",
        app_data->mid.num_bins
    );

    g_print(
        "Side bins: %d\n",
        app_data->side.num_bins
    );


    /*
     * Free audio data.
     */
    audio_free(&audio);


    /*
     * Tell GTK to redraw all four areas.
     */
    gtk_widget_queue_draw(
        app_data->left_area
    );

    gtk_widget_queue_draw(
        app_data->right_area
    );

    gtk_widget_queue_draw(
        app_data->mid_area
    );

    gtk_widget_queue_draw(
        app_data->side_area
    );


    /*
     * Clean up.
     */
    g_free(filename);
    g_object_unref(file);
}


/*
 * Called when Open WAV is clicked.
 */
static void open_wav(
    GtkWidget *button,
    gpointer user_data)
{
    (void)button;

    AppData *app_data =
        static_cast<AppData *>(user_data);


    /*
     * Create file dialog.
     */
    GtkFileDialog *dialog =
        gtk_file_dialog_new();

    gtk_file_dialog_set_title(
        dialog,
        "Open WAV file"
    );


    /*
     * Open file chooser.
     */
    gtk_file_dialog_open(
        dialog,
        app_data->window,
        NULL,
        file_selected,
        app_data
    );
}


/*
 * Called when the application starts.
 */
static void activate(
    GtkApplication *app,
    gpointer user_data)
{
    (void)user_data;


    /*
     * Create main window.
     */
    GtkWidget *window =
        gtk_application_window_new(app);

    gtk_window_set_title(
        GTK_WINDOW(window),
        "WAV Histogram"
    );

    gtk_window_set_default_size(
        GTK_WINDOW(window),
        1000,
        700
    );


    /*
     * Create application data.
     */
    AppData *app_data =
        new AppData{};

    app_data->window =
        GTK_WINDOW(window);

    app_data->has_histograms =
        false;


    /*
     * Main vertical layout.
     */
    GtkWidget *main_box =
        gtk_box_new(
            GTK_ORIENTATION_VERTICAL,
            10
        );

    gtk_widget_set_margin_top(
        main_box,
        15
    );

    gtk_widget_set_margin_bottom(
        main_box,
        15
    );

    gtk_widget_set_margin_start(
        main_box,
        15
    );

    gtk_widget_set_margin_end(
        main_box,
        15
    );


    /*
     * Controls row.
     */
    GtkWidget *controls =
        gtk_box_new(
            GTK_ORIENTATION_HORIZONTAL,
            10
        );


    /*
     * Bits label.
     */
    GtkWidget *bits_label =
        gtk_label_new("Bits:");

    gtk_box_append(
        GTK_BOX(controls),
        bits_label
    );


    /*
     * Bits entry.
     */
    GtkWidget *bits_entry =
        gtk_entry_new();

    gtk_editable_set_text(
        GTK_EDITABLE(bits_entry),
        "10"
    );

    gtk_widget_set_size_request(
        bits_entry,
        60,
        -1
    );

    gtk_box_append(
        GTK_BOX(controls),
        bits_entry
    );

    app_data->bits_entry =
        bits_entry;


    /*
     * Open button.
     */
    GtkWidget *button =
        gtk_button_new_with_label(
            "Open WAV"
        );

    g_signal_connect(
        button,
        "clicked",
        G_CALLBACK(open_wav),
        app_data
    );

    gtk_box_append(
        GTK_BOX(controls),
        button
    );


    /*
     * Add controls to main layout.
     */
    gtk_box_append(
        GTK_BOX(main_box),
        controls
    );


    /*
     * Create 2x2 grid.
     */
    GtkWidget *grid =
        gtk_grid_new();

    gtk_grid_set_row_spacing(
        GTK_GRID(grid),
        10
    );

    gtk_grid_set_column_spacing(
        GTK_GRID(grid),
        10
    );

    gtk_widget_set_hexpand(
        grid,
        TRUE
    );

    gtk_widget_set_vexpand(
        grid,
        TRUE
    );


    /*
     * Create histogram drawing areas.
     */
    app_data->left_area =
        create_histogram_area(
            &app_data->left,
            "L Channel"
        );

    app_data->mid_area =
        create_histogram_area(
            &app_data->mid,
            "MID Channel"
        );

    app_data->right_area =
        create_histogram_area(
            &app_data->right,
            "R Channel"
        );

    app_data->side_area =
        create_histogram_area(
            &app_data->side,
            "SIDE Channel"
        );


    /*
     * Add the four histograms.
     *
     * Top-left:     L
     * Top-right:    MID
     * Bottom-left:  R
     * Bottom-right: SIDE
     */
    gtk_grid_attach(
        GTK_GRID(grid),
        app_data->left_area,
        0,
        0,
        1,
        1
    );

    gtk_grid_attach(
        GTK_GRID(grid),
        app_data->mid_area,
        1,
        0,
        1,
        1
    );

    gtk_grid_attach(
        GTK_GRID(grid),
        app_data->right_area,
        0,
        1,
        1,
        1
    );

    gtk_grid_attach(
        GTK_GRID(grid),
        app_data->side_area,
        1,
        1,
        1,
        1
    );


    /*
     * Add grid to main layout.
     */
    gtk_box_append(
        GTK_BOX(main_box),
        grid
    );


    /*
     * Put everything in the window.
     */
    gtk_window_set_child(
        GTK_WINDOW(window),
        main_box
    );


    /*
     * Show window.
     */
    gtk_window_present(
        GTK_WINDOW(window)
    );
}


/*
 * Run the GTK application.
 */
int run_app(int argc, char **argv)
{
    /*
     * Create GTK application.
     */
    GtkApplication *app =
        gtk_application_new(
            "com.example.wavhist",
            G_APPLICATION_DEFAULT_FLAGS
        );


    /*
     * Connect activate signal.
     */
    g_signal_connect(
        app,
        "activate",
        G_CALLBACK(activate),
        NULL
    );


    /*
     * Run application.
     */
    int status =
        g_application_run(
            G_APPLICATION(app),
            argc,
            argv
        );


    /*
     * Clean up.
     */
    g_object_unref(app);

    return status;
}