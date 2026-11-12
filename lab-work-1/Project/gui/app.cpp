#include "app.h"
#include "histogram_view.h"

#include <gtk/gtk.h>

#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <string>

extern "C" {
#include "comparison.h"
#include "effects.h"
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

    std::string comparison_original_path;
    std::string comparison_processed_path;
    GtkWidget *comparison_original_label;
    GtkWidget *comparison_processed_label;
    GtkWidget *comparison_results;

    Audio effects_audio;
    bool has_effects_audio;
    std::string effects_input_path;
    GtkWidget *effects_file_label;
    GtkWidget *effects_frequency_entry;
    GtkWidget *effects_depth_entry;
    GtkWidget *effects_status;
    GSubprocess *effects_process;
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

    Audio audio = {};
    if (wav_read(filename, &audio) != 0) {
        g_print("Could not read WAV file\n");
        g_free(filename);
        g_object_unref(file);
        return;
    }

    if (audio.num_channels != 2) {
        g_print("Histogram mode requires a stereo WAV file\n");
        audio_free(&audio);
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

static void display_comparison_results(AppData *app_data)
{
    AudioComparison comparison;
    char error_message[160];

    if (compare_wav_files(
            app_data->comparison_original_path.c_str(),
            app_data->comparison_processed_path.c_str(),
            &comparison,
            error_message,
            sizeof(error_message)) != 0) {
        gtk_label_set_text(
            GTK_LABEL(app_data->comparison_results), error_message);
        return;
    }

    std::ostringstream text;
    text << std::fixed;

    for (int channel = 0; channel < comparison.num_channels; channel++) {
        text << std::setprecision(0) << "Channel " << channel + 1 << '\n'
             << std::setprecision(10)
             << "  MSE: " << comparison.channels[channel].mse << '\n'
             << "  Maximum absolute error: "
             << comparison.channels[channel].max_error << '\n'
             << std::setprecision(2)
             << "  SNR: " << comparison.channels[channel].snr_db
             << " dB\n\n";
    }

    text << "Channel average\n"
         << std::setprecision(10)
         << "  Average MSE: " << comparison.average.mse << '\n'
         << "  Average maximum absolute error: "
         << comparison.average.max_error << '\n'
         << std::setprecision(2)
         << "  Average SNR: " << comparison.average.snr_db << " dB";

    gtk_label_set_text(
        GTK_LABEL(app_data->comparison_results), text.str().c_str());
}

static void comparison_file_selected(
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

    gboolean original = GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(dialog), "comparison-original"));
    std::string *path = original
        ? &app_data->comparison_original_path
        : &app_data->comparison_processed_path;
    GtkWidget *label = original
        ? app_data->comparison_original_label
        : app_data->comparison_processed_label;
    *path = filename;

    char *basename = g_path_get_basename(filename);
    std::string label_text = original ? "Original: " : "Quantized: ";
    label_text += basename;
    gtk_label_set_text(GTK_LABEL(label), label_text.c_str());
    g_free(basename);

    if (!app_data->comparison_original_path.empty() &&
        !app_data->comparison_processed_path.empty()) {
        display_comparison_results(app_data);
    }

    g_free(filename);
    g_object_unref(file);
}

static void open_comparison_file(GtkWidget *button, gpointer user_data)
{
    AppData *app_data = static_cast<AppData *>(user_data);
    gboolean original = GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(button), "comparison-original"));
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(
        dialog,
        original ? "Choose original WAV" : "Choose quantized WAV");
    g_object_set_data(
        G_OBJECT(dialog),
        "comparison-original",
        GINT_TO_POINTER(original));
    gtk_file_dialog_open(
        dialog,
        app_data->window,
        NULL,
        comparison_file_selected,
        app_data);
}

static GtkWidget *make_comparison_button(
    const char *title,
    gboolean original,
    AppData *app_data)
{
    GtkWidget *button = gtk_button_new_with_label(title);
    g_object_set_data(
        G_OBJECT(button),
        "comparison-original",
        GINT_TO_POINTER(original));
    g_signal_connect(
        button,
        "clicked",
        G_CALLBACK(open_comparison_file),
        app_data);
    return button;
}

static void effects_file_selected(
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

    Audio audio = {};
    if (wav_read(filename, &audio) != 0) {
        gtk_label_set_text(
            GTK_LABEL(app_data->effects_status),
            "Could not read file. Select a 16-bit stereo PCM WAV.");
        g_free(filename);
        g_object_unref(file);
        return;
    }

    if (app_data->has_effects_audio) {
        audio_free(&app_data->effects_audio);
    }
    if (app_data->effects_process != NULL) {
        g_subprocess_force_exit(app_data->effects_process);
        g_clear_object(&app_data->effects_process);
    }
    app_data->effects_audio = audio;
    app_data->has_effects_audio = true;
    app_data->effects_input_path = filename;

    char *basename = g_path_get_basename(filename);
    std::string label = "Source: ";
    label += basename;
    gtk_label_set_text(GTK_LABEL(app_data->effects_file_label), label.c_str());
    gtk_label_set_text(
        GTK_LABEL(app_data->effects_status),
        "Choose an effect to create, save, and play its preview.");
    g_free(basename);
    g_free(filename);
    g_object_unref(file);
}

static void open_effects_wav(GtkWidget *button, gpointer user_data)
{
    (void)button;
    AppData *app_data = static_cast<AppData *>(user_data);
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Choose source WAV");
    gtk_file_dialog_open(
        dialog,
        app_data->window,
        NULL,
        effects_file_selected,
        app_data);
}

static std::string effects_output_path(const AppData *app_data, AudioEffect effect)
{
    std::string name = app_data->effects_input_path;
    std::string::size_type slash = name.find_last_of("\\/");
    name = name.substr(slash == std::string::npos ? 0 : slash + 1);
    std::string::size_type extension = name.find_last_of('.');
    if (extension != std::string::npos) {
        name.erase(extension);
    }
    return "EffectsFiles/" + name + "_" + audio_effect_name(effect) + ".wav";
}

static double effects_entry_value(GtkWidget *entry, double fallback)
{
    const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
    char *end = NULL;
    double value = g_ascii_strtod(text, &end);
    return end == text || *end != '\0' ? fallback : value;
}

static GSubprocess *start_effect_playback(const char *filename)
{
    GError *error = NULL;
    GSubprocess *process = g_subprocess_new(
        G_SUBPROCESS_FLAGS_NONE,
        &error,
        "paplay",
        filename,
        NULL);
    g_clear_error(&error);

    if (process == NULL) {
        process = g_subprocess_new(
            G_SUBPROCESS_FLAGS_NONE,
            &error,
            "aplay",
            "-q",
            filename,
            NULL);
        g_clear_error(&error);
    }

    if (process == NULL) {
        process = g_subprocess_new(
            G_SUBPROCESS_FLAGS_NONE,
            &error,
            "ffplay",
            "-nodisp",
            "-autoexit",
            "-loglevel",
            "error",
            filename,
            NULL);
        g_clear_error(&error);
    }

    return process;
}

static gboolean stop_effects_on_close(GtkWindow *window, gpointer user_data)
{
    (void)window;
    AppData *app_data = static_cast<AppData *>(user_data);
    if (app_data->effects_process != NULL) {
        g_subprocess_force_exit(app_data->effects_process);
        g_clear_object(&app_data->effects_process);
    }
    return FALSE;
}

static void play_effect(GtkWidget *button, gpointer user_data)
{
    AppData *app_data = static_cast<AppData *>(user_data);
    AudioEffect effect = (AudioEffect)GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(button), "audio-effect"));

    if (!app_data->has_effects_audio) {
        gtk_label_set_text(
            GTK_LABEL(app_data->effects_status),
            "Choose a source WAV before selecting an effect.");
        return;
    }

    double frequency = effects_entry_value(app_data->effects_frequency_entry, 5.0);
    double depth = effects_entry_value(app_data->effects_depth_entry, 5.0);
    Audio effected = {};
    char error_message[160];

    if (audio_apply_effect(
            &app_data->effects_audio,
            &effected,
            effect,
            frequency,
            depth,
            error_message,
            sizeof(error_message)) != 0) {
        gtk_label_set_text(
            GTK_LABEL(app_data->effects_status), error_message);
        return;
    }

    g_mkdir_with_parents("EffectsFiles", 0755);
    std::string output = effects_output_path(app_data, effect);
    if (audio_effect_write_wav(output.c_str(), &effected) != 0) {
        gtk_label_set_text(
            GTK_LABEL(app_data->effects_status),
            "Could not save the effected WAV file.");
        audio_free(&effected);
        return;
    }
    audio_free(&effected);

    if (app_data->effects_process != NULL) {
        g_subprocess_force_exit(app_data->effects_process);
        g_clear_object(&app_data->effects_process);
    }
    char *absolute_path = g_canonicalize_filename(output.c_str(), NULL);
    app_data->effects_process = start_effect_playback(absolute_path);

    std::string status = "Playing ";
    status += audio_effect_name(effect);
    if (app_data->effects_process == NULL) {
        status = "Effect saved, but no player was found. Install paplay, aplay, or ffplay. Saved to ";
    } else {
        status += "; saved to ";
    }
    status += output;
    gtk_label_set_text(GTK_LABEL(app_data->effects_status), status.c_str());
    g_free(absolute_path);
}

static GtkWidget *make_effect_button(
    const char *label,
    AudioEffect effect,
    AppData *app_data)
{
    GtkWidget *button = gtk_button_new();
    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *icon = gtk_image_new_from_icon_name("audio-volume-high-symbolic");
    gtk_box_append(GTK_BOX(content), icon);
    gtk_box_append(GTK_BOX(content), gtk_label_new(label));
    gtk_button_set_child(GTK_BUTTON(button), content);
    gtk_widget_set_tooltip_text(button, "Generate and play this effect");
    g_object_set_data(
        G_OBJECT(button), "audio-effect", GINT_TO_POINTER(effect));
    g_signal_connect(button, "clicked", G_CALLBACK(play_effect), app_data);
    return button;
}

static GtkWidget *create_home_page(AppData *app_data)
{
    GtkWidget *box = make_page_box();
    gtk_box_append(GTK_BOX(box), gtk_label_new("WAV tools"));

    GtkWidget *histogram = gtk_button_new_with_label("WAV Histogram");
    GtkWidget *quantization = gtk_button_new_with_label("WAV Quantization");
    GtkWidget *comparison = gtk_button_new_with_label("WAV Comparison");
    GtkWidget *effects = gtk_button_new_with_label("WAV Effects");
    gtk_widget_set_size_request(histogram, 260, 50);
    gtk_widget_set_size_request(quantization, 260, 50);
    gtk_widget_set_size_request(comparison, 260, 50);
    gtk_widget_set_size_request(effects, 260, 50);
    g_object_set_data(G_OBJECT(histogram), "stack", app_data->stack);
    g_object_set_data(G_OBJECT(quantization), "stack", app_data->stack);
    g_object_set_data(G_OBJECT(comparison), "stack", app_data->stack);
    g_object_set_data(G_OBJECT(effects), "stack", app_data->stack);
    g_signal_connect(histogram, "clicked", G_CALLBACK(show_page), (gpointer)"histogram");
    g_signal_connect(quantization, "clicked", G_CALLBACK(show_page), (gpointer)"quantization");
    g_signal_connect(comparison, "clicked", G_CALLBACK(show_page), (gpointer)"comparison");
    g_signal_connect(effects, "clicked", G_CALLBACK(show_page), (gpointer)"effects");
    gtk_box_append(GTK_BOX(box), histogram);
    gtk_box_append(GTK_BOX(box), quantization);
    gtk_box_append(GTK_BOX(box), comparison);
    gtk_box_append(GTK_BOX(box), effects);
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

static GtkWidget *create_comparison_page(AppData *app_data)
{
    GtkWidget *box = make_page_box();
    gtk_box_append(GTK_BOX(box), make_back_button(app_data->stack));
    gtk_box_append(GTK_BOX(box), gtk_label_new("Compare WAV files"));

    gtk_box_append(
        GTK_BOX(box),
        make_comparison_button("Choose original WAV", TRUE, app_data));
    app_data->comparison_original_label = gtk_label_new("Original: not selected");
    gtk_label_set_xalign(GTK_LABEL(app_data->comparison_original_label), 0.0f);
    gtk_box_append(GTK_BOX(box), app_data->comparison_original_label);

    gtk_box_append(
        GTK_BOX(box),
        make_comparison_button("Choose quantized WAV", FALSE, app_data));
    app_data->comparison_processed_label = gtk_label_new("Quantized: not selected");
    gtk_label_set_xalign(GTK_LABEL(app_data->comparison_processed_label), 0.0f);
    gtk_box_append(GTK_BOX(box), app_data->comparison_processed_label);

    app_data->comparison_results = gtk_label_new(
        "Select both WAV files to calculate channel statistics.");
    gtk_label_set_xalign(GTK_LABEL(app_data->comparison_results), 0.0f);
    gtk_label_set_selectable(GTK_LABEL(app_data->comparison_results), TRUE);
    gtk_label_set_wrap(GTK_LABEL(app_data->comparison_results), TRUE);
    gtk_box_append(GTK_BOX(box), app_data->comparison_results);
    return box;
}

static GtkWidget *create_effects_page(AppData *app_data)
{
    GtkWidget *box = make_page_box();
    gtk_box_append(GTK_BOX(box), make_back_button(app_data->stack));

    GtkWidget *controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_append(GTK_BOX(controls), gtk_button_new_with_label("Choose source WAV"));
    GtkWidget *open = gtk_widget_get_last_child(controls);
    g_signal_connect(open, "clicked", G_CALLBACK(open_effects_wav), app_data);
    gtk_box_append(GTK_BOX(box), controls);

    app_data->effects_file_label = gtk_label_new("Source: not selected");
    gtk_label_set_xalign(GTK_LABEL(app_data->effects_file_label), 0.0f);
    gtk_box_append(GTK_BOX(box), app_data->effects_file_label);

    GtkWidget *parameters = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_append(GTK_BOX(parameters), gtk_label_new("Frequency (Hz):"));
    app_data->effects_frequency_entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(app_data->effects_frequency_entry), "1");
    gtk_widget_set_size_request(app_data->effects_frequency_entry, 80, -1);
    gtk_box_append(GTK_BOX(parameters), app_data->effects_frequency_entry);
    gtk_box_append(GTK_BOX(parameters), gtk_label_new("Vibrato depth (ms):"));
    app_data->effects_depth_entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(app_data->effects_depth_entry), "5");
    gtk_widget_set_size_request(app_data->effects_depth_entry, 80, -1);
    gtk_box_append(GTK_BOX(parameters), app_data->effects_depth_entry);
    gtk_box_append(GTK_BOX(box), parameters);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    const char *labels[] = {
        "Echo 1", "Echo 2", "Echo 3", "Multiple echo (feedforward)",
        "Multiple echo (feedback)", "AM", "Tremolo", "Vibrato",
        "Chorus", "Flanger", "Doppler"
    };
    for (int index = 0; index < 11; index++) {
        GtkWidget *button = make_effect_button(
            labels[index], (AudioEffect)(index + 1), app_data);
        gtk_grid_attach(GTK_GRID(grid), button, index % 3, index / 3, 1, 1);
    }
    gtk_box_append(GTK_BOX(box), grid);

    app_data->effects_status = gtk_label_new(
        "Select a source WAV, then click an effect to generate and play it.");
    gtk_label_set_xalign(GTK_LABEL(app_data->effects_status), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(app_data->effects_status), TRUE);
    gtk_box_append(GTK_BOX(box), app_data->effects_status);
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
    app_data->has_effects_audio = false;
    app_data->effects_process = NULL;
    g_signal_connect(
        window,
        "close-request",
        G_CALLBACK(stop_effects_on_close),
        app_data);

    gtk_stack_add_named(app_data->stack, create_home_page(app_data), "home");
    gtk_stack_add_named(app_data->stack, create_histogram_page(app_data), "histogram");
    gtk_stack_add_named(app_data->stack, create_quantization_page(app_data), "quantization");
    gtk_stack_add_named(app_data->stack, create_comparison_page(app_data), "comparison");
    gtk_stack_add_named(app_data->stack, create_effects_page(app_data), "effects");
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
