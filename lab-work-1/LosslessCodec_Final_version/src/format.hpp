#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "wav.hpp"

struct CodecHeader{
    uint16_t channels;
    uint32_t sampleRate;
    uint16_t bitsPerSample;
    uint64_t sampleCount; // número de amostras por canal
    uint64_t compressedBitCount; // número de bits no payload comprimido
    uint64_t originalFileSize; // tamanho do ficheiro WAV original, incluindo cabeçalho e dados
    uint64_t wavDataOffset; // deslocamento do payload PCM no ficheiro WAV original, incluindo cabeçalho e dados 
};

// Grava CLSS, cabeçalho e bytes comprimidos no caminho indicado.
bool writeCompressedFile(const std::string& filename, const CodecHeader& header, const WavLayout& layout, const std::vector<uint8_t>& data);

// Valida CLSS e carrega cabeçalho e bytes restantes; false indica erro na leitura  ou ficheiro inválido.
bool readCompressedFile(const std::string& filename, CodecHeader& header, WavLayout& layout, std::vector<uint8_t>& data);
