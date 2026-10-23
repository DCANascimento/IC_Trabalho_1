#include <iostream>
#include <string>
#include <cstdlib>
#include <fstream>
#include <vector>
#include <algorithm>
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

// Function to save a histogram as a PPM image (P3 format)
void save_histogram_ppm(const std::string &filename, const Histogram *hist, int img_width = 800, int img_height = 400) {
    struct RGB { unsigned char r, g, b; };
    std::vector<std::vector<RGB>> image(img_height, std::vector<RGB>(img_width, {255, 255, 255}));

    int num_bins = hist->num_bins; 
    long max_val = 1;
    for (int i = 0; i < num_bins; ++i) {
        if (hist->counts[i] > max_val) {
            max_val = hist->counts[i];
        }
    }

    double col_width = static_cast<double>(img_width) / num_bins;
    for (int i = 0; i < num_bins; ++i) {
        int bar_height = static_cast<int>((static_cast<double>(hist->counts[i]) / max_val) * (img_height - 50));
        int start_x = static_cast<int>(i * col_width);
        int end_x = static_cast<int>((i + 1) * col_width);

        // Draw vertical bar (blue color: RGB{0, 100, 255})
        for (int y = img_height - 1 - bar_height; y < img_height; ++y) {
            for (int x = start_x; x < end_x && x < img_width; ++x) {
                image[y][x] = {0, 100, 255};
            }
        }
    }

    // Write to the PPM file
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening file for writing: " << filename << std::endl;
        return;
    }

    file << "P3\n" << img_width << " " << img_height << "\n255\n";
    for (int y = 0; y < img_height; ++y) {
        for (int x = 0; x < img_width; ++x) {
            file << static_cast<int>(image[y][x].r) << " "
                 << static_cast<int>(image[y][x].g) << " "
                 << static_cast<int>(image[y][x].b) << " ";
        }
        file << "\n";
    }
    
    std::cout << "Generated PPM image: " << filename << std::endl;
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

    // Saves Histograms to a ppm file inside the subfolder
    save_histogram_ppm(subfolder + "/histogram_left.ppm", &left);
    save_histogram_ppm(subfolder + "/histogram_right.ppm", &right);
    save_histogram_ppm(subfolder + "/histogram_mid.ppm", &mid);
    save_histogram_ppm(subfolder + "/histogram_side.ppm", &side);

    // Free memory of the histograms
    histogram_free(&left);
    histogram_free(&right);
    histogram_free(&mid);
    histogram_free(&side);

    audio_free(&audio);

    return 0;
}