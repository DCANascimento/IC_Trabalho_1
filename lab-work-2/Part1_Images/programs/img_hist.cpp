#include "read_images.h"
#include "histograms.h"

int main(int argc, char **argv) {

    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " file_name bin_width\n";
        return 1;
    }

    std::string file_name = argv[1];
    int bin_width = std::stoi(argv[2]);

    std::string file_type = file_name.substr(file_name.size() - 4);

    if (file_type.compare(".raw") == 0) {
    
        std::vector<uint16_t> pixels = read_raw16(file_name);

        if (raw16_histogram(pixels, bin_width, file_name)) {
            return 1;
        }

    } else if (file_type.compare(".pgm") == 0) {

        std::vector<uint8_t> pixels = read_pgm8(file_name);

        // histogram logic

    } else if (file_type.compare(".ppm") == 0) {

        std::cout << "its was a ppm\n";

    } else {
        std::cerr << "Improper input file (must be .raw .pgm or .ppm)\n";
        return 1;
    }



}