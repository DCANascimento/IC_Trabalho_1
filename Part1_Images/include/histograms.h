#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>

int plot_histogram(std::vector<int> hist_counts, std::string file_name, std::string file_type, int bin_width);

int raw16_histogram(std::vector<uint16_t> pixels, int bin_width, std::string file_name);
int pgm8_histogram(std::vector<uint8_t> pixels, int bin_width, std::string file_name);
int ppm_histogram(std::vector<uint8_t> pixels, int bin_width, std::string file_name);