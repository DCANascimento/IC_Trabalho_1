cmake_minimum_required(VERSION 3.20)

project(lossyCodec VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimized build by default (running time is part of the evaluation).
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra)

# Code shared by all programs.
add_library(lossycore STATIC
    src/wav.cpp
    src/fft.cpp
    src/dct.cpp
    src/mdct.cpp
    src/transform.cpp
    src/codec.cpp
)
target_include_directories(lossycore PUBLIC src)

add_executable(wav_lossy_enc src/wav_lossy_enc.cpp)
target_link_libraries(wav_lossy_enc PRIVATE lossycore)

add_executable(wav_lossy_dec src/wav_lossy_dec.cpp)
target_link_libraries(wav_lossy_dec PRIVATE lossycore)

# Measures the error between the original and the decoded file.
add_executable(wav_snr src/wav_snr.cpp)
target_link_libraries(wav_snr PRIVATE lossycore)
