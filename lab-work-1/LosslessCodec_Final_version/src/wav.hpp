#pragma once

#include <cstdint>
#include <string>
#include <vector>
// Metadados que o codec conserva do WAV PCM: canais, frequência, resolução e tamanho dos dados.
struct WavInfo
{
    uint16_t channels;
    uint32_t sampleRate;
    uint16_t bitsPerSample;
    uint32_t dataSize;
};

// Guarda os bytes do WAV que ficam antes e depois do payload PCM, para o poder
// reconstruir byte a byte, incluindo chunks e metadados que o codec não interpreta.
struct WavLayout
{
    uint64_t originalFileSize = 0;
    uint64_t dataOffset = 0;
    std::vector<uint8_t> prefix;
    std::vector<uint8_t> suffix;
};

// Lê um WAV PCM de 16 bits e devolve amostras intercaladas por frame/canal; false indica ficheiro inválido ou não suportado.
bool loadWavInfo(const std::string& filename, WavInfo& info, std::vector<int16_t>& samples, WavLayout* layout = nullptr);
// Restaura as amostras entre o prefixo e sufixo originais, preservando todos os bytes do contentor WAV.
bool saveWavPreservingLayout(const std::string& filename, const WavInfo& info, const std::vector<int16_t>& samples, const WavLayout& layout);
