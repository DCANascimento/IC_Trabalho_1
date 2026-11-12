# Junta o ponto de entrada aos módulos WAV, CLSS, ZigZag, Rice e fluxo de bits.

cmake_minimum_required(VERSION 3.20)

project(MyLosslessCodec VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(MyLosslessCodec
    src/main.cpp
    src/path_utils.hpp
    src/wav.cpp
    src/zigzag.cpp
    src/rice.cpp
    src/block_codec.cpp
    src/bitstream.cpp
    src/format.cpp
)

# Mantém o executável na raiz de build em geradores de uma e várias configurações.
set_target_properties(MyLosslessCodec PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(MyLosslessCodec PROPERTIES
        "RUNTIME_OUTPUT_DIRECTORY_${config}" "${CMAKE_BINARY_DIR}"
    )
endforeach()

# Torna o executável MinGW independente das DLLs de runtime do GCC.
if(MINGW)
    target_link_options(MyLosslessCodec PRIVATE
        -static
        -static-libgcc
        -static-libstdc++
        -municode
    )
endif()

# Cria os dois nomes de comando pedidos no exercício. O programa escolhe a
# operação pelo nome com que foi chamado; MyLosslessCodec continua disponível
# para a linha de comandos antiga e para os menus de contexto.
add_custom_command(TARGET MyLosslessCodec POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:MyLosslessCodec>"
        "$<TARGET_FILE_DIR:MyLosslessCodec>/wav_lossless_enc${CMAKE_EXECUTABLE_SUFFIX}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:MyLosslessCodec>"
        "$<TARGET_FILE_DIR:MyLosslessCodec>/wav_lossless_dec${CMAKE_EXECUTABLE_SUFFIX}"
    VERBATIM
)
