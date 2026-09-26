#include "app.h"
#include "histogram_view.h"

#include <gtk/gtk.h>

#include <cstdlib>
#include <string>

extern "C" {
#include "histogram.h"
#include "quantization.h"
#include "wav.h"
}

typedef struct {
    GtkWindow *window;
    GtkStack *stack;

    GtkWidget *histogram_bits_entry;
    GtkWidget *left_area;
    GtkWidget *right_area;
    GtkWidget *mid_area;
    GtkWidget *side_area;
    Histogram left;
    Histogram right;
    Histogram mid;
    Histogram side;
    bool has_histograms;

    GtkWidget *quantization_bits_entry;
    GtkWidget *quantization_status;
} AppData;

static GtkWidget *make_page_box()
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_top(box, 15);
    gtk_widget_set_margin_bottom(box, 15);
    gtk_widget_set_margin_start(box, 15);
    gtk_widget_set_margin_end(box, 15);
    return box;
}

static void show_page(GtkWidget *button, gpointer user_data)
{
    (void)button;
    const char *page_name = static_cast<const char *>(user_data);
    GtkStack *stack = GTK_STACK(g_object_get_data(G_OBJECT(button), "stack"));
    gtk_stack_set_visible_child_name(stack, page_name);
}

static GtkWidget *make_back_button(GtkStack *stack)
{
    GtkWidget *button = gtk_button_new_with_label("Back");
    g_object_set_data(G_OBJECT(button), "stack", stack);
    g_signal_connect(button, "clicked", G_CALLBACK(show_page), (gpointer)"home");
    return button;
}

static void file_selected(
    GObject *source_object,
    GAsyncResult *result,
    gpointer user_data)
{
    AppData *app_data = static_cast<AppData *>(user_data);
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);
    GError *error = NULL;
    GFile *file = gtk_file_dialog_open_finish(dialog, result, &error);

    if (file == NULL) {
        if (error != NULL) {
            g_error_free(error);
        }
        return;
    }

    char *filename = g_file_get_path(file);
    if (filename == NULL) {
        g_object_unref(file);
        return;
    }

    int bits = atoi(gtk_editable_get_text(
        GTK_EDITABLE(app_data->histogram_bits_entry)));

    if (bits <= 0) {
        g_print("Bits must be greater than 0\n");
        g_free(filename);
        g_object_unref(file);
        return;
    }

    Audio audio = {0};
    if (wav_read(filename, &audio) != 0) {
        g_print("Could not read WAV file\n");
        g_free(filename);
        g_object_unref(file);
        return;
    }

    if (app_data->has_histograms) {
        histogram_free(&app_data->left);
        histogram_free(&app_data->right);
        histogram_free(&app_data->mid);
        histogram_free(&app_data->side);
    }

    create_histograms(
        &audio,
        bits,
        &app_data->left,
        &app_data->right,
        &app_data->mid,
        &app_data->side);
    app_data->has_histograms = true;
    audio_free(&audio);

    gtk_widget_queue_draw(app_data->left_area);
    gtk_widget_queue_draw(app_data->right_area);
    gtk_widget_queue_draw(app_data->mid_area);
    gtk_widget_queue_draw(app_data->side_area);

    g_print("Opened: %s\n", filename);
    g_free(filename);
    g_object_unref(file);
}

static void open_histogram_wav(GtkWidget *button, gpointer user_data)
{
    (void)button;
    AppData *app_data = static_cast<AppData *>(user_data);
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Open WAV file");
    gtk_file_dialog_open(dialog, app_data->window, NULL, file_selected, app_data);
}

static bool get_quantization_bits(AppData *app_data, int *bits)
{
    *bits = atoi(gtk_editable_get_text(
        GTK_EDITABLE(app_data->quantization_bits_entry)));

    if (*bits < 1 || *bits > 16) {
        gtk_label_set_text(
            GTK_LABEL(app_data->quantization_status),
            "Bits must be an integer from 1 to 16.");
        return false;
    }

    return true;
}

static std::string quantized_name(const char *filename, int bits)
{
    std::string path(filename);
    std::string::size_type slash = path.find_last_of("\\/");
    std::string name = path.substr(slash + 1);
    std::string::size_type extension = name.find_last_of('.');

    if (extension != std::string::npos) {
        name.erase(extension);
    }

    return "QuantizedFiles/" + name + "_" +
        std::to_string(bits) + "b.wav";
}

static bool quantize_file(const char *filename, int bits)
{
    g_mkdir_with_parents("QuantizedFiles", 0755);
    std::string output = quantized_name(filename, bits);
    return quantize_wav_file(filename, output.c_str(), bits) == 0;
}

static void quantization_files_selected(
    GObject *source_object,
    GAsyncResult *result,
    gpointer user_data)
{
    AppData *app_data = static_cast<AppData *>(user_data);
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);
    GError *error = NULL;
    GListModel *files = gtk_file_dialog_open_multiple_finish(
        dialog, result, &error);

    if (files == NULL) {
        if (error != NULL) {
            g_error_free(error);
        }
        return;
    }

    int bits;
    if (!get_quantization_bits(app_data, &bits)) {
        g_object_unref(files);
        return;
    }

    guint total = g_list_model_get_n_items(files);
    guint succeeded = 0;

    for (guint i = 0; i < total; i++) {
        GFile *file = G_FILE(g_list_model_get_item(files, i));
        char *filename = g_file_get_path(file);

        if (filename != NULL && quantize_file(filename, bits)) {
            succeeded++;
        }

        g_free(filename);
        g_object_unref(file);
    }

    std::string message = "Quantized " + std::to_string(succeeded) +
        " of " + std::to_string(total) + " file(s) in QuantizedFiles.";
    gtk_label_set_text(GTK_LABEL(app_data->quantization_status), message.c_str());
    g_object_unref(files);
}

static void open_quantization_wavs(GtkWidget *button, gpointer user_data)
{
    (void)button;
    AppData *app_data = static_cast<AppData *>(user_data);
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Choose WAV file(s)");
    gtk_file_dialog_open_multiple(
        dialog,
        app_data->window,
        NULL,
        quantization_files_selected,
        app_data);
}

static gint compare_strings(gconstpointer first, gconstpointer second)
{
    return g_ascii_strcasecmp(
        static_cast<const char *>(first),
        static_cast<const char *>(second));
}

static void quantize_all_wavs(GtkWidget *button, gpointer user_data)
{
    (void)button;
    AppData *app_data = static_cast<AppData *>(user_data);
    int bits;

    if (!get_quantization_bits(app_data, &bits)) {
        return;
    }

    GDir *directory = g_dir_open("../AudioFiles", 0, NULL);
    if (directory == NULL) {
        gtk_label_set_text(
            GTK_LABEL(app_data->quantization_status),
            "Could not open ../AudioFiles. Run the app from Project.");
        return;
    }

    GList *filenames = NULL;
    const char *name;
    while ((name = g_dir_read_name(directory)) != NULL) {
        if (g_str_has_suffix(name, ".wav") ||
            g_str_has_suffix(name, ".WAV")) {
            filenames = g_list_prepend(filenames, g_strdup(name));
        }
    }
    g_dir_close(directory);
    filenames = g_list_sort(filenames, compare_strings);

    guint total = g_list_length(filenames);
    guint succeeded = 0;
    for (GList *item = filenames; item != NULL; item = item->next) {
        std::string path = "../AudioFiles/";
        path += static_cast<const char *>(item->data);
        if (quantize_file(path.c_str(), bits)) {
            succeeded++;
        }
    }

    std::string message = "Quantized " + std::to_string(succeeded) +
        " of " + std::to_string(total) + " file(s) in QuantizedFiles.";
    gtk_label_set_text(GTK_LABEL(app_data->quantization_status), message.c_str());
    g_list_free_full(filenames, g_free);
}

static GtkWidget *create_home_page(AppData *app_data)
{
    GtkWidget *box = make_page_box();
    gtk_box_append(GTK_BOX(box), gtk_label_new("WAV tools"));

    GtkWidget *histogram = gtk_button_new_with_label("WAV Histogram");
    GtkWidget *quantization = gtk_button_new_with_label("WAV Quantization");
    gtk_widget_set_size_request(histogram, 260, 50);
    gtk_widget_set_size_request(quantization, 260, 50);
    g_object_set_data(G_OBJECT(histogram), "stack", app_data->stack);
    g_object_set_data(G_OBJECT(quantization), "stack", app_data->stack);
    g_signal_connect(histogram, "clicked", G_CALLBACK(show_page), (gpointer)"histogram");
    g_signal_connect(quantization, "clicked", G_CALLBACK(show_page), (gpointer)"quantization");
    gtk_box_append(GTK_BOX(box), histogram);
    gtk_box_append(GTK_BOX(box), quantization);
    return box;
}

static GtkWidget *create_histogram_page(AppData *app_data)
{
    GtkWidget *box = make_page_box();
    gtk_box_append(GTK_BOX(box), make_back_button(app_data->stack));

    GtkWidget *controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_append(GTK_BOX(controls), gtk_label_new("Bits:"));
    app_data->histogram_bits_entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(app_data->histogram_bits_entry), "10");
    gtk_widget_set_size_request(app_data->histogram_bits_entry, 60, -1);
    gtk_box_append(GTK_BOX(controls), app_data->histogram_bits_entry);
    GtkWidget *open = gtk_button_new_with_label("Open WAV");
    g_signal_connect(open, "clicked", G_CALLBACK(open_histogram_wav), app_data);
    gtk_box_append(GTK_BOX(controls), open);
    gtk_box_append(GTK_BOX(box), controls);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_widget_set_hexpand(grid, TRUE);
    gtk_widget_set_vexpand(grid, TRUE);
    app_data->left_area = create_histogram_area(&app_data->left, "L Channel");
    app_data->mid_area = create_histogram_area(&app_data->mid, "MID Channel");
    app_data->right_area = create_histogram_area(&app_data->right, "R Channel");
    app_data->side_area = create_histogram_area(&app_data->side, "SIDE Channel");
    gtk_grid_attach(GTK_GRID(grid), app_data->left_area, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), app_data->mid_area, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), app_data->right_area, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), app_data->side_area, 1, 1, 1, 1);
    gtk_box_append(GTK_BOX(box), grid);
    return box;
}

static GtkWidget *create_quantization_page(AppData *app_data)
{
    GtkWidget *box = make_page_box();
    gtk_box_append(GTK_BOX(box), make_back_button(app_data->stack));
    gtk_box_append(GTK_BOX(box), gtk_label_new("Uniform scalar quantization"));

    GtkWidget *controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_append(GTK_BOX(controls), gtk_label_new("Bits to keep (1-16):"));
    app_data->quantization_bits_entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(app_data->quantization_bits_entry), "8");
    gtk_widget_set_size_request(app_data->quantization_bits_entry, 60, -1);
    gtk_box_append(GTK_BOX(controls), app_data->quantization_bits_entry);
    gtk_box_append(GTK_BOX(box), controls);

    GtkWidget *selected = gtk_button_new_with_label("Choose WAV file(s)");
    GtkWidget *all = gtk_button_new_with_label("Quantize all files");
    g_signal_connect(selected, "clicked", G_CALLBACK(open_quantization_wavs), app_data);
    g_signal_connect(all, "clicked", G_CALLBACK(quantize_all_wavs), app_data);
    gtk_box_append(GTK_BOX(box), selected);
    gtk_box_append(GTK_BOX(box), all);
    app_data->quantization_status = gtk_label_new(
        "Choose one or more files, or quantize all files from ../AudioFiles.");
    gtk_label_set_wrap(GTK_LABEL(app_data->quantization_status), TRUE);
    gtk_box_append(GTK_BOX(box), app_data->quantization_status);
    return box;
}

static void activate(GtkApplication *app, gpointer user_data)
{
    (void)user_data;
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "WAV Tools");
    gtk_window_set_default_size(GTK_WINDOW(window), 1000, 700);

    AppData *app_data = new AppData{};
    app_data->window = GTK_WINDOW(window);
    app_data->stack = GTK_STACK(gtk_stack_new());
    app_data->has_histograms = false;

    gtk_stack_add_named(app_data->stack, create_home_page(app_data), "home");
    gtk_stack_add_named(app_data->stack, create_histogram_page(app_data), "histogram");
    gtk_stack_add_named(app_data->stack, create_quantization_page(app_data), "quantization");
    gtk_stack_set_visible_child_name(app_data->stack, "home");
    gtk_window_set_child(GTK_WINDOW(window), GTK_WIDGET(app_data->stack));
    gtk_window_present(GTK_WINDOW(window));
}

int run_app(int argc, char **argv)
{
    GtkApplication *app = gtk_application_new(
        "com.example.wavtools",
        G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
