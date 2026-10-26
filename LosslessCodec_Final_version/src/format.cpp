// Serializa o cabeçalho CLSS com inteiros little-endian e preserva os bytes WAV fora do áudio.
#include "format.hpp"
#include "path_utils.hpp" 

#include <fstream>
#include <iostream>
#include <limits>

using namespace std;
namespace
{
constexpr uint64_t kHeaderSize = 44;
constexpr uint64_t kMaxWavDataSize = numeric_limits<uint32_t>::max();

bool writeU16LE(ostream& file, uint16_t value){
    const unsigned char bytes[2] = {
        static_cast<unsigned char>(value & 0xFFu),
        static_cast<unsigned char>((value >> 8) & 0xFFu)
    };
    return static_cast<bool>(file.write( reinterpret_cast<const char*>(bytes), sizeof(bytes)));
}

bool writeU32LE(ostream& file, uint32_t value){
    const unsigned char bytes[4] = {
        static_cast<unsigned char>(value & 0xFFu),
        static_cast<unsigned char>((value >> 8) & 0xFFu),
        static_cast<unsigned char>((value >> 16) & 0xFFu),
        static_cast<unsigned char>((value >> 24) & 0xFFu)
    };
    return static_cast<bool>(file.write( reinterpret_cast<const char*>(bytes), sizeof(bytes)));
}

bool writeU64LE(ostream& file, uint64_t value){
    for (unsigned int i = 0; i < 8; ++i){
        const unsigned char byte = static_cast<unsigned char>((value >> (8 * i)) & 0xFFu);
        if (!file.write(reinterpret_cast<const char*>(&byte), 1))
            return false;
    }
    return true;
}

bool readU16LE(istream& file, uint16_t& value){
    unsigned char bytes[2];
    if (!file.read(reinterpret_cast<char*>(bytes), sizeof(bytes)))
        return false;
    value = static_cast<uint16_t>(bytes[0])
          | static_cast<uint16_t>(static_cast<uint16_t>(bytes[1]) << 8);
    return true;
}

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

bool readU64LE(istream& file, uint64_t& value){
    value = 0;
    for (unsigned int i = 0; i < 8; ++i){
        unsigned char byte = 0;
        if (!file.read(reinterpret_cast<char*>(&byte), 1))
            return false;
        value |= static_cast<uint64_t>(byte) << (8 * i);
    }
    return true;
}

uint64_t encodedByteCount(uint64_t bitCount){
    return bitCount / 8 + (bitCount % 8 != 0 ? 1 : 0);
}

bool writeBytes(ostream& file, const vector<uint8_t>& bytes){
    if (bytes.empty())
        return true;
    if (bytes.size() > static_cast<size_t>(numeric_limits<streamsize>::max()))
        return false;
    return static_cast<bool>(file.write( reinterpret_cast<const char*>(bytes.data()), static_cast<streamsize>(bytes.size())));
}

bool readBytes(istream& file, vector<uint8_t>& bytes, uint64_t count){
    if (count > bytes.max_size()
        || count > static_cast<uint64_t>(numeric_limits<streamsize>::max()))
        return false;
    bytes.resize(static_cast<size_t>(count));
    if (bytes.empty())
        return true;
    return static_cast<bool>(file.read( reinterpret_cast<char*>(bytes.data()), static_cast<streamsize>(bytes.size())));
}
}

// junta o cabeçalho CLSS, os bytes WAV preservados e o payload no ficheiro comprimido, validando a coerência dos dados antes de escrever o CLSS
bool writeCompressedFile(const string& filename, const CodecHeader& header, const WavLayout& layout, const vector<uint8_t>& data){
    if (header.sampleCount > kMaxWavDataSize / 2){
        cerr << "Error: WAV payload is too large for RIFF.\n";
        return false;
    }
    const uint64_t pcmSize = header.sampleCount * 2;
    if (layout.originalFileSize != header.originalFileSize
        || layout.dataOffset != header.wavDataOffset
        || layout.prefix.size() != layout.dataOffset
        || layout.dataOffset > layout.originalFileSize
        || pcmSize > layout.originalFileSize - layout.dataOffset
        || layout.suffix.size() !=
            layout.originalFileSize - layout.dataOffset - pcmSize
        || header.compressedBitCount % 8 != 0
        || data.size() != encodedByteCount(header.compressedBitCount))
    {
        cerr << "Error: inconsistent WAV layout or bitstream.\n";
        return false;
    }

    ofstream file(filePathFromUtf8(filename), ios::binary);
    if (!file){
        cerr << "Error: could not create " << filename << '\n';
        return false;
    }

    file.write("CLSS", 4);
    writeU16LE(file, header.channels);
    writeU32LE(file, header.sampleRate);
    writeU16LE(file, header.bitsPerSample);
    writeU64LE(file, header.sampleCount);
    writeU64LE(file, header.compressedBitCount);
    writeU64LE(file, header.originalFileSize);
    writeU64LE(file, header.wavDataOffset);

    const bool bodyWritten = writeBytes(file, layout.prefix)
        && writeBytes(file, layout.suffix)
        && writeBytes(file, data);
    if (!file || !bodyWritten){
        cerr << "Error: failed to write " << filename << '\n';
        return false;
    }
    return true;
}

// valida a assinatura, os tamanhos declarados e a correspondência com o tamanho real do ficheiro
bool readCompressedFile(const string& filename, CodecHeader& header, WavLayout& layout, vector<uint8_t>& data){
    layout = {};
    data.clear();
    ifstream file(filePathFromUtf8(filename), ios::binary);
    if (!file){
        cerr << "Error: could not open " << filename << '\n';
        return false;
    }

    char magic[4];
    if (!file.read(magic, sizeof(magic))
        || string(magic, sizeof(magic)) != "CLSS"){
        cerr << "Error: invalid CLSS file.\n";
        return false;
    }

    if (!readU16LE(file, header.channels)
        || !readU32LE(file, header.sampleRate)
        || !readU16LE(file, header.bitsPerSample)
        || !readU64LE(file, header.sampleCount)
        || !readU64LE(file, header.compressedBitCount)
        || !readU64LE(file, header.originalFileSize)
        || !readU64LE(file, header.wavDataOffset))
    {
        cerr << "Error: truncated CLSS header.\n";
        return false;
    }
    if (header.sampleCount > kMaxWavDataSize / 2){
        cerr << "Error: invalid declared PCM size.\n";
        return false;
    }
    const uint64_t pcmSize = header.sampleCount * 2;
    if (header.compressedBitCount % 8 != 0){
        cerr << "Error: invalid CLSS payload size.\n";
        return false;
    }
    if (header.wavDataOffset > header.originalFileSize || pcmSize > header.originalFileSize - header.wavDataOffset){
        cerr << "Error: invalid declared WAV bounds.\n";
        return false;
    }

    const uint64_t suffixSize = header.originalFileSize - header.wavDataOffset - pcmSize;
    const uint64_t payloadSize = encodedByteCount(header.compressedBitCount);
    const uint64_t maxStreamSize = static_cast<uint64_t>(numeric_limits<streamoff>::max());
    if (kHeaderSize > maxStreamSize
        || header.wavDataOffset > maxStreamSize - kHeaderSize
        || suffixSize > maxStreamSize - kHeaderSize - header.wavDataOffset
        || payloadSize > maxStreamSize - kHeaderSize
            - header.wavDataOffset - suffixSize)
    {
        cerr << "Error: CLSS file size exceeds supported limits.\n";
        return false;
    }

    file.seekg(0, ios::end);
    const streamoff physicalSize = file.tellg();
    const uint64_t expectedSize = kHeaderSize + header.wavDataOffset + suffixSize + payloadSize;
    if (physicalSize < 0 || static_cast<uint64_t>(physicalSize) != expectedSize){
        cerr << "Error: CLSS file size does not match the header.\n";
        return false;
    }

    file.seekg(static_cast<streamoff>(kHeaderSize), ios::beg);
    layout.originalFileSize = header.originalFileSize;
    layout.dataOffset = header.wavDataOffset;
    if (!readBytes(file, layout.prefix, header.wavDataOffset)
        || !readBytes(file, layout.suffix, suffixSize)
        || !readBytes(file, data, payloadSize))
    {
        cerr << "Error: truncated CLSS data or data exceeds supported limits.\n";
        layout = {};
        data.clear();
        return false;
    }
    return true;
}
