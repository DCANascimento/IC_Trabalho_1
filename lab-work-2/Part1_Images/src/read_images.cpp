#include "read_images.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>

std::vector<uint16_t> read_raw16(std::string file_name) {

    std::string filename = "../ImageFiles/imgs-raw16/ERIS.2024-01-01T06_15_12.592_u16be-1x2046x2048.raw";

    int width = 2048;
    int height = 2046;

    int total_pixels = width * height;
    int total_bytes = total_pixels * sizeof(uint16_t);

    // opens the file in the end, to count the total size
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file");
    }

    int file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::cout << "File size: " << file_size << " bytes\n";
    std::cout << "Expected:  " << total_bytes << " bytes\n";

    if (file_size != static_cast<std::streamsize>(total_bytes)) {
        throw std::runtime_error("Expected file size does not match actual size");
    }

    std::vector<uint16_t> pixels(total_pixels);

    if (!file.read(reinterpret_cast<char*>(pixels.data()), total_bytes)) {
        throw std::runtime_error("Failed to read from file");
    }

    file.close();

    return pixels;

}