#include "read_images.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>

std::vector<uint16_t> read_raw16(std::string file_name) {

    std::string full_path_file = "../datasets/imgs-raw16/" + file_name;

    // opens the file in the end, to count the total size
    std::ifstream file(full_path_file, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file");
    }

    int file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    int total_pixels = file_size / sizeof(uint16_t);

    std::cout << "File size: " << file_size << " bytes\n";

    std::vector<uint16_t> pixels(total_pixels);

    if (!file.read(reinterpret_cast<char*>(pixels.data()), file_size)) {
        throw std::runtime_error("Failed to read from file");
    }

    file.close();

    return pixels;
}

std::vector<uint8_t> read_pgm8(std::string file_name) {

    std::string full_path_file = "../datasets/imgs-pgm8/" + file_name;

    // opens the file in the end, to count the total size
    std::ifstream file(full_path_file, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file");
    }

    // Skips P5
    file.seekg(3, std::ios::beg);

    int width;
    int height;

    int width_done = 0;

    char full_buffer[50];
    char buffer;
    int count = 0;

    while (file.read(&buffer, sizeof(char))) {

        if (static_cast<int>(buffer) == 32 || static_cast<int>(buffer) == 10) {

            if (width_done) {
                height = atoi(full_buffer);
                break;
            } else {
                width = atoi(full_buffer);
                
                for (int i = 0; i < count; i++) {
                    // ascii value of '0', holy genius
                    full_buffer[i] = 48;
                }

                width_done = 1;
            }
            count = 0;

        } else {
            full_buffer[count++] = buffer;
        }

    }

    int total_pixels = width * height;
    std::vector<uint8_t> pixels(total_pixels);

    std::cout << "total pixels: " << total_pixels << "\n";

    // Skip 255 gray scale thing
    file.read(&full_buffer[0], sizeof(char) * 4);

    if (!file.read(reinterpret_cast<char*>(pixels.data()), (total_pixels * sizeof(uint8_t)))) {
        throw std::runtime_error("Failed to read from file");
    }

    file.close();

    // Teste
    //uint8_t old_buffer;
    //std::FILE* f = std::fopen("../datasets/imgs-pgm8/barbara.pgm", "r");
    //int old_count = 0;
    //while (std::fread(&old_buffer, sizeof(uint8_t), 1, f) && old_count++ < 100) {
    //    std::cout << static_cast<int>(old_buffer) << "\n";
    //    std::cout << old_buffer << "\n";
    //}
    //std::fclose(f);

    return pixels;

}




