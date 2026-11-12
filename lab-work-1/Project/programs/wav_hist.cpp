#include <iostream>
#include <string>
#include <cstdlib>
#include <fstream>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <sys/types.h>

#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#else
#include <direct.h>
#endif

extern "C" {
#include "wav.h"
#include "histogram.h"
}

// Function to save a histogram as a professional SVG graph
void save_histogram_svg(const std::string &filename, const Histogram *hist, const std::string &title, double min_val = -1.0, double max_val = 1.0, int width = 1000, int height = 400) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening file for writing: " << filename << std::endl;
        return;
    }

    int num_bins = hist->num_bins; 
    long max_count = 1;
    for (int i = 0; i < num_bins; ++i) {
        if (hist->counts[i] > max_count) {
            max_count = hist->counts[i];
        }
    }

    // Graph layout margins
    int margin_left = 70;
    int margin_right = 30;
    int margin_top = 40;
    int margin_bottom = 60;

    int plot_width = width - margin_left - margin_right;
    int plot_height = height - margin_top - margin_bottom;

    // SVG Header
    file << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width << "\" height=\"" << height << "\">\n";
    
    // Background
    file << "  <rect width=\"100%\" height=\"100%\" fill=\"#ffffff\"/>\n";

    // Title
    file << "  <text x=\"" << width / 2 << "\" y=\"25\" text-anchor=\"middle\" font-family=\"Arial\" font-size=\"16\" font-weight=\"bold\" fill=\"#333\">" << title << "</text>\n";

    // Axes lines
    file << "  <line x1=\"" << margin_left << "\" y1=\"" << (height - margin_bottom) << "\" x2=\"" << (width - margin_right) << "\" y2=\"" << (height - margin_bottom) << "\" stroke=\"#333\" stroke-width=\"1.5\"/>\n";
    file << "  <line x1=\"" << margin_left << "\" y1=\"" << margin_top << "\" x2=\"" << margin_left << "\" y2=\"" << (height - margin_bottom) << "\" stroke=\"#333\" stroke-width=\"1.5\"/>\n";

    // Y-Axis Grid Lines & Labels (5 intervals)
    int num_y_ticks = 5;
    for (int i = 0; i <= num_y_ticks; ++i) {
        double val = static_cast<double>(max_count * i) / num_y_ticks;
        int y_pos = (height - margin_bottom) - static_cast<int>((static_cast<double>(i) / num_y_ticks) * plot_height);
        
        if (i > 0) {
            file << "  <line x1=\"" << margin_left << "\" y1=\"" << y_pos << "\" x2=\"" << (width - margin_right) << "\" y2=\"" << y_pos << "\" stroke=\"#f0f0f0\" stroke-dasharray=\"4,4\"/>\n";
        }
        file << "  <text x=\"" << (margin_left - 10) << "\" y=\"" << (y_pos + 4) << "\" text-anchor=\"end\" font-family=\"Arial\" font-size=\"11\" fill=\"#666\">" << static_cast<long>(val) << "</text>\n";
    }

    // X-Axis Ticks & Labels (every 0.1)
    double tick_step = 0.1;
    for (int step_idx = 0; ; ++step_idx) {
        double val = min_val + step_idx * tick_step;
        if (val > max_val + 1e-9) break;

        int x_pos = margin_left + static_cast<int>((val - min_val) / (max_val - min_val) * plot_width);

        // Tick mark
        file << "  <line x1=\"" << x_pos << "\" y1=\"" << (height - margin_bottom) 
             << "\" x2=\"" << x_pos << "\" y2=\"" << (height - margin_bottom + 6) 
             << "\" stroke=\"#333\" stroke-width=\"1\"/>\n";

        // Vertical grid line for major intervals (e.g., every 0.2 or 0.5 to keep it clean, or keep all)
        if (step_idx > 0 && step_idx < static_cast<int>((max_val - min_val) / tick_step)) {
            // Light grid line every 0.2 units (even steps) to prevent grid clutter
            if (step_idx % 2 == 0) {
                file << "  <line x1=\"" << x_pos << "\" y1=\"" << margin_top 
                     << "\" x2=\"" << x_pos << "\" y2=\"" << (height - margin_bottom) 
                     << "\" stroke=\"#f5f5f5\" stroke-dasharray=\"2,2\"/>\n";
            }
        }

        // Text label for every 0.1 value
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f", val);
        file << "  <text x=\"" << x_pos << "\" y=\"" << (height - margin_bottom + 20) 
             << "\" text-anchor=\"middle\" font-family=\"Arial\" font-size=\"10\" fill=\"#666\">" << buf << "</text>\n";
    }

    // Draw Histogram Bars
    double bin_width_px = static_cast<double>(plot_width) / num_bins;
    for (int i = 0; i < num_bins; ++i) {
        double bar_h = (static_cast<double>(hist->counts[i]) / max_count) * plot_height;
        double x_pos = margin_left + i * bin_width_px;
        double y_pos = (height - margin_bottom) - bar_h;

        file << "  <rect x=\"" << x_pos << "\" y=\"" << y_pos << "\" width=\"" << std::max(1.0, bin_width_px - 0.5) << "\" height=\"" << bar_h << "\" fill=\"#0064ff\" opacity=\"0.85\"/>\n";
    }

    // Axis Labels
    file << "  <text x=\"" << (margin_left + plot_width / 2) << "\" y=\"" << (height - 10) << "\" text-anchor=\"middle\" font-family=\"Arial\" font-size=\"12\" font-weight=\"bold\" fill=\"#333\">Amplitude Value</text>\n";
    
    file << "</svg>\n";

    std::cout << "Generated SVG graph: " << filename << std::endl;
}

int main(int argc, char **argv)
{
    /*
     * Expected:
     * ./wav_hist input.wav --bits 10
     */

    if (argc != 4) {
        std::cout << "Usage: " << argv[0] << " input.wav --bits N\n";
        return 1;
    }

    std::string filename = argv[1];

    if (std::string(argv[2]) != "--bits") {
        std::cerr << "Expected --bits\n";
        return 1;
    }

    int bits = std::stoi(argv[3]);

    if (bits <= 0) {
        std::cerr << "Bits must be greater than 0\n";
        return 1;
    }


    // Audio reading
    Audio audio;

    if (wav_read(filename.c_str(), &audio) != 0) {
        return 1;
    }

    if (audio.num_channels != 2) {
        std::cerr << "Histogram mode requires a stereo WAV file\n";
        audio_free(&audio);
        return 1;
    }


    // Extract audio stem name (e.g., "input" from "path/to/input.wav")
    size_t last_slash = filename.find_last_of("/\\");
    std::string base = (last_slash == std::string::npos) ? filename : filename.substr(last_slash + 1);
    size_t last_dot = base.find_last_of('.');
    std::string stem = (last_dot == std::string::npos) ? base : base.substr(0, last_dot);

    // Create directories: HistogramPlots/ and HistogramPlots/<stem>/
#ifdef _WIN32
    _mkdir("HistogramPlots");
    std::string subfolder = "HistogramPlots/" + stem;
    _mkdir(subfolder.c_str());
#else
    mkdir("HistogramPlots", 0755);
    std::string subfolder = "HistogramPlots/" + stem;
    mkdir(subfolder.c_str(), 0755);
#endif


    Histogram left;
    Histogram right;
    Histogram mid;
    Histogram side;

    create_histograms(&audio, bits, &left, &right, &mid, &side);

    save_histogram_svg(subfolder + "/histogram_left.svg", &left, "Left Channel Histogram", -1.0, 1.0);
    save_histogram_svg(subfolder + "/histogram_right.svg", &right, "Right Channel Histogram", -1.0, 1.0);
    save_histogram_svg(subfolder + "/histogram_mid.svg", &mid, "Mid Channel Histogram", -1.0, 1.0);
    save_histogram_svg(subfolder + "/histogram_side.svg", &side, "Side Channel Histogram", -1.0, 1.0);

    // Free memory of the histograms
    histogram_free(&left);
    histogram_free(&right);
    histogram_free(&mid);
    histogram_free(&side);

    audio_free(&audio);

    return 0;
}