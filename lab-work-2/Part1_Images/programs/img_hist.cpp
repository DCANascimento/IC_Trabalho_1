#include "read_images.h"
#include "histograms.h"

int main(int argc, char **argv) {

    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " file_name bin_width\n";
        return 1;
    }

    std::string file_name = argv[1];
    int bin_width = std::stoi(argv[2]);

    std::vector<uint16_t> pixels = read_raw16(file_name);

    if (raw16_histogram(pixels, bin_width, file_name)) {
        return 1;
    }

}