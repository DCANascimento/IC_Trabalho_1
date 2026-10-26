// Le e escreve WAV RIFF PCM de 16 bits, verificando os limites dos chunks.
#include "wav.hpp"
#include "path_utils.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <limits>

using namespace std;
namespace
{
// Le um inteiro de 16 bits no formato little-endian definido pelo RIFF.
bool readU16LE(istream& file, uint16_t& value){
    unsigned char bytes[2];
    if (!file.read(reinterpret_cast<char*>(bytes), sizeof(bytes)))
        return false;
    value = static_cast<uint16_t>(bytes[0])
          | static_cast<uint16_t>(bytes[1] << 8);
    return true;
}

// Le um inteiro de 32 bits no formato little-endian definido pelo RIFF.
bool readU32LE(istream& file, uint32_t& value){
    unsigned char bytes[4];
    if (!file.read(reinterpret_cast<char*>(bytes), sizeof(bytes)))
        return false;
    value = static_cast<uint32_t>(bytes[0])
          | (static_cast<uint32_t>(bytes[1]) << 8)
          | (static_cast<uint32_t>(bytes[2]) << 16)
          | (static_cast<uint32_t>(bytes[3]) << 24);
    return true;
}

}

// abre o ficheiro e verifica a assinatura RIFF/WAVE. 
// Depois percorre os chunks até encontrar fmt e data, validando que o áudio é PCM de 16 bits e que os metadados são coerentes.
bool loadWavInfo(const string& filename, WavInfo& info, vector<int16_t>& samples, WavLayout* layout){
    samples.clear();
    info = {};
    if (layout)
        *layout = {};

    ifstream file(filePathFromUtf8(filename), ios::binary);
    if (!file){
        cerr << "Error: could not open file: " << filename << '\n';
        return false;
    }

    file.seekg(0, ios::end);
    const streamoff physicalSize = file.tellg();
    if (physicalSize < 12){
        cerr << "Error: WAV file is too small.\n";
        return false;
    }
    file.seekg(0, ios::beg);

    char riff[4];
    uint32_t riffSize = 0;
    char wave[4];
    if (!file.read(riff, sizeof(riff)) || !readU32LE(file, riffSize) || !file.read(wave, sizeof(wave))){
        cerr << "Error: failed to read the RIFF/WAVE header.\n";
        return false;
    }
    if (string(riff, 4) != "RIFF" || string(wave, 4) != "WAVE"){
        cerr << "Error: file does not have a RIFF/WAVE signature.\n";
        return false;
    }

    // riffSize conta os bytes a partir do campo WAVE; bytes extra no fim sao ignorados.
    const uint64_t riffEnd = 8ull + riffSize;
    if (riffSize < 4 || riffEnd > static_cast<uint64_t>(physicalSize))
    {
        cerr << "Error: invalid RIFF size or truncated file.\n";
        return false;
    }

    bool foundFmt = false;
    bool foundData = false;
    uint16_t channels = 0;
    uint16_t bitsPerSample = 0;
    uint32_t sampleRate = 0;
    uint32_t dataSize = 0;
    uint64_t dataOffset = 0;
    uint16_t blockAlign = 0;

    // Percorre chunks sem ultrapassar o fim RIFF nem o fim fisico do ficheiro.
    while (true){
        const streamoff headerPosition = file.tellg();
        if (headerPosition < 0){
            cerr << "Error: failed to locate a WAV chunk.\n";
            return false;
        }
        const uint64_t position = static_cast<uint64_t>(headerPosition);
        if (position == riffEnd)
            break;
        if (position > riffEnd || riffEnd - position < 8){
            cerr << "Error: incomplete chunk header inside RIFF data.\n";
            return false;
        }

        char chunkId[4];
        uint32_t chunkSize = 0;
        if (!file.read(chunkId, sizeof(chunkId)) || !readU32LE(file, chunkSize))
        {
            cerr << "Error: failed to read a WAV chunk header.\n";
            return false;
        }

        const uint64_t payloadStart = position + 8;
        const uint64_t paddedChunkSize = static_cast<uint64_t>(chunkSize) + (chunkSize & 1u);
        const uint64_t physicalEnd = static_cast<uint64_t>(physicalSize);
        if (paddedChunkSize > riffEnd - payloadStart || payloadStart > physicalEnd || paddedChunkSize > physicalEnd - payloadStart){
            cerr << "Error: WAV chunk exceeds the declared bounds.\n";
            return false;
        }

        const bool isFmt = string(chunkId, 4) == "fmt ";
        const bool isData = string(chunkId, 4) == "data";

        if (isFmt && !foundFmt){
            if (chunkSize < 16){
                cerr << "Error: fmt chunk is too small.\n";
                return false;
            }

            uint16_t audioFormat = 0;
            uint32_t byteRate = 0;
            if (!readU16LE(file, audioFormat)
                || !readU16LE(file, channels)
                || !readU32LE(file, sampleRate)
                || !readU32LE(file, byteRate)
                || !readU16LE(file, blockAlign)
                || !readU16LE(file, bitsPerSample))
            {
                cerr << "Error: failed to read the fmt chunk fields.\n";
                return false;
            }

            if (audioFormat != 1){
                cerr << "Error: only uncompressed PCM WAV files are supported.\n";
                return false;
            }
            if (channels == 0 || sampleRate == 0 || bitsPerSample != 16){
                cerr << "Error: invalid WAV metadata or unsupported bit depth.\n";
                return false;
            }

            const uint64_t expectedAlign = static_cast<uint64_t>(channels) * 2;
            const uint64_t expectedByteRate =
                static_cast<uint64_t>(sampleRate) * expectedAlign;
            if (expectedAlign > UINT16_MAX || blockAlign != expectedAlign || expectedByteRate > UINT32_MAX || byteRate != expectedByteRate){
                cerr << "Error: inconsistent byte rate or block alignment.\n";
                return false;
            }
            foundFmt = true;
        }

        if (isData && !foundData){
            dataSize = chunkSize;
            dataOffset = payloadStart;
            foundData = true;
        }

        const uint64_t nextChunk = payloadStart + paddedChunkSize;
        if (foundFmt && foundData)
            break;
        file.seekg(static_cast<streamoff>(nextChunk), ios::beg);
        if (!file){
            cerr << "Error: failed to advance to the next WAV chunk.\n";
            return false;
        }
    }

    if (!foundFmt || !foundData){
        cerr << "Error: fmt or data chunk was not found.\n";
        return false;
    }
    if (dataSize % blockAlign != 0){
        cerr << "Error: audio data does not contain complete frames.\n";
        return false;
    }

    const uint64_t sampleCount64 = dataSize / sizeof(int16_t);
    if (sampleCount64 > samples.max_size()){
        cerr << "Error: WAV file is too large to fit in memory.\n";
        return false;
    }

    samples.resize(static_cast<size_t>(sampleCount64));
    file.seekg(static_cast<streamoff>(dataOffset), ios::beg);
    if (!file){
        cerr << "Error: failed to locate the PCM data.\n";
        samples.clear();
        return false;
    }

    // Converte blocos little-endian para int16_t sem depender da ordem de bytes da maquina.
    array<unsigned char, 8192> bytes{};
    size_t sampleIndex = 0;
    while (sampleIndex < samples.size()){
        const size_t count =
            min(bytes.size() / 2, samples.size() - sampleIndex);
        const streamsize byteCount =
            static_cast<streamsize>(count * 2);
        if (!file.read(reinterpret_cast<char*>(bytes.data()), byteCount))
        {
            cerr << "Error: truncated PCM data.\n";
            samples.clear();
            return false;
        }

        for (size_t i = 0; i < count; ++i)
        {
            const uint16_t raw = static_cast<uint16_t>(bytes[2 * i])
                               | static_cast<uint16_t>(bytes[2 * i + 1] << 8);
            const int32_t signedValue = raw < 0x8000u
                ? raw : static_cast<int32_t>(raw) - 0x10000;
            samples[sampleIndex + i] = static_cast<int16_t>(signedValue);
        }
        sampleIndex += count;
    }

    // Mantém os bytes fora do payload PCM para reconstruir depois o contentor
    // original, incluindo chunks desconhecidos e eventuais bytes finais.
    if (layout){
        const uint64_t suffixOffset = dataOffset + dataSize;
        const uint64_t suffixSize =
            static_cast<uint64_t>(physicalSize) - suffixOffset;
        if (dataOffset > layout->prefix.max_size()
            || suffixSize > layout->suffix.max_size()
            || dataOffset > static_cast<uint64_t>(numeric_limits<streamsize>::max())
            || suffixSize > static_cast<uint64_t>(numeric_limits<streamsize>::max()))
        {
            cerr << "Error: WAV layout is too large to fit in memory.\n";
            samples.clear();
            return false;
        }

        layout->originalFileSize = static_cast<uint64_t>(physicalSize);
        layout->dataOffset = dataOffset;
        layout->prefix.resize(static_cast<size_t>(dataOffset));
        layout->suffix.resize(static_cast<size_t>(suffixSize));

        file.clear();
        file.seekg(0, ios::beg);
        if (!layout->prefix.empty() && !file.read(reinterpret_cast<char*>(layout->prefix.data()), static_cast<streamsize>(layout->prefix.size()))){
            cerr << "Error: failed to save the WAV structure before the audio data.\n";
            samples.clear();
            *layout = {};
            return false;
        }

        file.clear();
        file.seekg(static_cast<streamoff>(suffixOffset), ios::beg);
        if (!layout->suffix.empty() && !file.read(reinterpret_cast<char*>(layout->suffix.data()), static_cast<streamsize>(layout->suffix.size()))){
            cerr << "Error: failed to save the WAV structure after the audio data.\n";
            samples.clear();
            *layout = {};
            return false;
        }
    }

    info.channels = channels;
    info.sampleRate = sampleRate;
    info.bitsPerSample = bitsPerSample;
    info.dataSize = dataSize;
    return true;
}

// escreve o áudio entre os bytes originais do contentor WAV, a partir desta ordem: prefixo, amostras PCM 16-bit e sufixo. Mantem o tamanho original do ficheiro.
bool saveWavPreservingLayout(const string& filename, const WavInfo& info, const vector<int16_t>& samples, const WavLayout& layout)
{
    if (info.channels == 0 || info.sampleRate == 0 || info.bitsPerSample != 16 || samples.size() % info.channels != 0){
        cerr << "Error: invalid WAV metadata or samples.\n";
        return false;
    }

    const uint64_t pcmSize = static_cast<uint64_t>(samples.size()) * 2;
    if (pcmSize != info.dataSize || layout.dataOffset != layout.prefix.size()
        || layout.dataOffset > layout.originalFileSize
        || pcmSize > layout.originalFileSize - layout.dataOffset
        || layout.suffix.size() != layout.originalFileSize - layout.dataOffset - pcmSize)
    {
        cerr << "Error: inconsistent original WAV layout.\n";
        return false;
    }

    ofstream file(filePathFromUtf8(filename), ios::binary);
    if (!file){
        cerr << "Error: could not create " << filename << '\n';
        return false;
    }

    if (!layout.prefix.empty())
        file.write(reinterpret_cast<const char*>(layout.prefix.data()), static_cast<streamsize>(layout.prefix.size()));

    array<unsigned char, 8192> bytes{};
    size_t sampleIndex = 0;
    while (sampleIndex < samples.size())
    {
        const size_t count = min(bytes.size() / 2, samples.size() - sampleIndex);
        for (size_t i = 0; i < count; ++i){
            const uint16_t raw = static_cast<uint16_t>(samples[sampleIndex + i]);
            bytes[2 * i] = static_cast<unsigned char>(raw & 0xFFu);
            bytes[2 * i + 1] = static_cast<unsigned char>((raw >> 8) & 0xFFu);
        }
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<streamsize>(count * 2));
        if (!file){
            cerr << "Error: failed to write the reconstructed WAV samples.\n";
            return false;
        }
        sampleIndex += count;
    }

    if (!layout.suffix.empty())
        file.write(reinterpret_cast<const char*>(layout.suffix.data()), static_cast<streamsize>(layout.suffix.size()));

    return static_cast<bool>(file);
}
