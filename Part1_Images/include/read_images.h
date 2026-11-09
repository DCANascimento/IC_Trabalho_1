#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>

std::vector<uint16_t> read_raw16(std::string file_name);
std::vector<uint8_t> read_pgm8(std::string file_name);
std::vector<uint8_t> read_ppm(std::string file_name);

int write_raw16(std::vector<uint16_t> pixels);
int write_pgm8(std::vector<uint8_t> pixels);
int write_ppm(std::vector<uint8_t> pixels);