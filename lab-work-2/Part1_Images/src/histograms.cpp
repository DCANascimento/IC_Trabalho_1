#include "histograms.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <filesystem>

int plot_histogram(std::vector<int> hist_counts, std::string file_name, std::string file_type, int bin_width, int max_pixel_value) {

    std::string dir_name = "Histograms";
    std::filesystem::create_directories(dir_name);

    std::string full_path = dir_name + "/" + file_name + "_" + file_type + ".svg";

    // Writes to the new histogram file
    std::ofstream file(full_path);
    if (!file.is_open()) {
        std::cerr << "Error opening file for writing: " << full_path << std::endl;
        return 1;
    }

    // Graph dimensions and layout configurations
    int width = 1000;
    int height = 400;
    int margin_left = 70;
    int margin_right = 30;
    int margin_top = 40;
    int margin_bottom = 60;

    int plot_width = width - margin_left - margin_right;
    int plot_height = height - margin_top - margin_bottom;

    // Find the maximum bar height to scale the Y-axis
    long max_count = 1;
    for (int count : hist_counts) {
        if (count > max_count) {
            max_count = count;
        }
    }

    int num_bins = hist_counts.size();

    // SVG Writer
    file << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width << "\" height=\"" << height << "\">\n";
    file << "  <rect width=\"100%\" height=\"100%\" fill=\"#ffffff\"/>\n";
    
    // Title
    file << "  <text x=\"" << width / 2 << "\" y=\"25\" text-anchor=\"middle\" font-family=\"Arial\" font-size=\"16\" font-weight=\"bold\" fill=\"#333\">Image Histogram</text>\n";

    // X-Axis line
    file << "  <line x1=\"" << margin_left << "\" y1=\"" << (height - margin_bottom) 
         << "\" x2=\"" << (width - margin_right) << "\" y2=\"" << (height - margin_bottom) 
         << "\" stroke=\"#333\" stroke-width=\"1.5\"/>\n";
    // Y-Axis line
    file << "  <line x1=\"" << margin_left << "\" y1=\"" << margin_top 
         << "\" x2=\"" << margin_left << "\" y2=\"" << (height - margin_bottom) 
         << "\" stroke=\"#333\" stroke-width=\"1.5\"/>\n";

    // Y Lines and Labels
    int num_y_ticks = 5;
    for (int i = 0; i <= num_y_ticks; ++i) {
        double val = static_cast<double>(max_count * i) / num_y_ticks;
        int y_pos = (height - margin_bottom) - static_cast<int>((static_cast<double>(i) / num_y_ticks) * plot_height);
        
        if (i > 0) {
            file << "  <line x1=\"" << margin_left << "\" y1=\"" << y_pos 
                 << "\" x2=\"" << (width - margin_right) << "\" y2=\"" << y_pos 
                 << "\" stroke=\"#f0f0f0\" stroke-dasharray=\"4,4\"/>\n";
        }
        file << "  <text x=\"" << (margin_left - 10) << "\" y=\"" << (y_pos + 4) 
             << "\" text-anchor=\"end\" font-family=\"Arial\" font-size=\"11\" fill=\"#666\">" << static_cast<long>(val) << "</text>\n";
    }

    // X-Axis Ticks and Labels (dynamic based on max_pixel_value)
    int num_x_ticks = 5; 
    for (int i = 0; i <= num_x_ticks; ++i) {
        double pixel_val = (static_cast<double>(max_pixel_value) / num_x_ticks) * i;
        int x_pos = margin_left + static_cast<int>((pixel_val / max_pixel_value) * plot_width);

        // Tick mark
        file << "  <line x1=\"" << x_pos << "\" y1=\"" << (height - margin_bottom) 
             << "\" x2=\"" << x_pos << "\" y2=\"" << (height - margin_bottom + 6) 
             << "\" stroke=\"#333\" stroke-width=\"1\"/>\n";

        // Text label
        file << "  <text x=\"" << x_pos << "\" y=\"" << (height - margin_bottom + 20) 
             << "\" text-anchor=\"middle\" font-family=\"Arial\" font-size=\"10\" fill=\"#666\">" << static_cast<int>(pixel_val) << "</text>\n";
    }

    // Draw histogram bars
    double bin_width_px = static_cast<double>(plot_width) / num_bins;
    for (int i = 0; i < num_bins; ++i) {
        double bar_h = (static_cast<double>(hist_counts[i]) / max_count) * plot_height;
        double x_pos = margin_left + (i * bin_width_px);
        double y_pos = (height - margin_bottom) - bar_h;

        file << "  <rect x=\"" << x_pos << "\" y=\"" << y_pos 
             << "\" width=\"" << std::max(1.0, bin_width_px - 0.5) 
             << "\" height=\"" << bar_h << "\" fill=\"#0064ff\" opacity=\"0.85\"/>\n";
    }

    // Bottom Axis Title (Dynamic Range)
    file << "  <text x=\"" << (margin_left + plot_width / 2) << "\" y=\"" << (height - 15) 
         << "\" text-anchor=\"middle\" font-family=\"Arial\" font-size=\"12\" font-weight=\"bold\" fill=\"#333\">Pixel Intensity (0 - " << max_pixel_value << ")</text>\n";
    
    file << "</svg>\n";
    file.close();

    std::cout << "Successfully generated SVG graph: " << full_path << std::endl;

    return 0;
}


int raw16_histogram(std::vector<uint16_t> pixels, int bin_width, std::string file_name) {
    int max_val = 65535;
    int vector_length = (max_val / bin_width) + 1;

    std::vector<int> hist_counts(vector_length, 0);

    for (uint16_t pixel : pixels) {
        if (pixel <= max_val) {
            hist_counts[pixel / bin_width]++;
        }
    }

    if (plot_histogram(hist_counts, file_name, "raw16", bin_width, max_val)) {
        std::cerr << "Error creating the svg file\n";
        return 1;
    }

    return 0;
}

int pgm8_histogram(std::vector<uint8_t> pixels, int bin_width, std::string file_name) {
    int max_val = 255;
    int vector_length = (max_val / bin_width) + 1;

    std::vector<int> hist_counts(vector_length, 0);

    for (uint8_t pixel : pixels) {
        hist_counts[pixel / bin_width]++;
    }

    if (plot_histogram(hist_counts, file_name, "pgm8", bin_width, max_val)) {
        std::cerr << "Error creating the svg file\n";
        return 1;
    }

    return 0;
}