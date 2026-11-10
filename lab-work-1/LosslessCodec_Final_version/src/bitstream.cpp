// Empacota e desempacota bits em ordem MSB-first, isto é, do bit mais significativo para o menos significativo.
#include "bitstream.hpp"

#include <algorithm>
#include <limits>

using namespace std;

// BIT WRITER
// escreve o quociente em unário: empacota runs de bits 1 diretamente e trata os fragmentos que atravessam bytes.
void BitWriter::writeUnary(uint32_t quotient) {
    while (quotient > 0) {
        if (bitCount == 0 && quotient >= 8){
            data.push_back(0xFFu);
            quotient -= 8;
            continue;
        }

        const uint8_t available = static_cast<uint8_t>(8 - bitCount);
        const uint8_t take = static_cast<uint8_t>(std::min<uint32_t>(quotient, available));
        const uint8_t ones = static_cast<uint8_t>((1u << take) - 1u);
        currentByte |= static_cast<uint8_t>(ones << (available - take));
        bitCount = static_cast<uint8_t>(bitCount + take);
        quotient -= take;

        if (bitCount == 8){
            data.push_back(currentByte);
            currentByte = 0;
            bitCount = 0;
        }
    }
    // O bit terminador Rice é 0; basta avançar a posição, mantendo esse bit limpo.
    ++bitCount;
    if (bitCount == 8){
        data.push_back(currentByte);
        currentByte = 0;
        bitCount = 0;
    }
}

// escreve o resto em binário, usando k bits
void BitWriter::writeBits(uint32_t value, uint8_t count){
    while (count > 0){
        const uint8_t available = static_cast<uint8_t>(8 - bitCount);
        const uint8_t take = std::min(count, available);
        const uint32_t mask = (1u << take) - 1u;
        const uint32_t chunk = (value >> (count - take)) & mask;
        currentByte |= static_cast<uint8_t>(chunk << (available - take));
        bitCount = static_cast<uint8_t>(bitCount + take);
        count = static_cast<uint8_t>(count - take);

        if (bitCount == 8){
            data.push_back(currentByte);
            currentByte = 0;
            bitCount = 0;
        }
    }
}

// Guarda o byte parcial, preenchendo com zeros os bits finais que não pertencem ao payload.
void BitWriter::flush(){
    if (bitCount > 0){
        data.push_back(currentByte);
        currentByte = 0;
        bitCount = 0;
    }
}

// Devolve por referência os bytes já empacotados pelo escritor
const vector<uint8_t>& BitWriter::getData() const{
    return data;
}

// BIT READER
// Inicializa o leitor com referência aos bytes que serão consumidos bit a bit.
BitReader::BitReader(const vector<uint8_t>& data) : data(data){}

// Lê o próximo bit em ordem MSB-first; devolve false quando todos os bytes foram consumidos.
bool BitReader::readBit(bool& bit){
    // Já não existem mais bytes.
    if (byteIndex >= data.size())
        return false;

    // Ler o bit mais significativo primeiro.
    bit = ((data[byteIndex] >> (7 - bitIndex)) & 1u)!= 0;
    ++bitIndex;

    // Passamos para o byte seguinte.
    if (bitIndex == 8){
        bitIndex = 0;
        ++byteIndex;
    }
    return true;
}

//Um byte 0xFF é 11111111 em binário. Se a leitura estiver no início desse byte (bitIndex == 0), a função soma 8 ao quociente de uma vez, em vez de ler os oito bits um por um.
//Depois continua a ler bit a bit, sempre do mais significativo para o menos significativo (MSB-first), até encontrar o 0 que termina o quociente unário.
bool BitReader::readUnary(uint32_t& quotient){
    quotient = 0;
    while (byteIndex < data.size())
    {
        if (bitIndex == 0 && data[byteIndex] == 0xFFu){
            if (quotient > std::numeric_limits<uint32_t>::max() - 8u)
                return false;
            quotient += 8;
            ++byteIndex;
            continue;
        }

        bool bit = false;
        if (!readBit(bit))
            return false;
        if (!bit)
            return true;
        // Assinala que o quociente é demasiado grande ou que o fluxo está inválido, para evitar overflow    
        if (quotient == std::numeric_limits<uint32_t>::max())
            return false;
        ++quotient;
    }
    return false;
}

// Lê count bits e junta-os em value, devolvendo false se o fim do vetor chegar antes.
bool BitReader::readBits(uint32_t& value, uint8_t count){
    value = 0;
    while (count > 0)
    {
        if (byteIndex >= data.size())
            return false;

        const uint8_t available = static_cast<uint8_t>(8 - bitIndex);
        const uint8_t take = std::min(count, available);
        const uint8_t shift = static_cast<uint8_t>(available - take);
        const uint32_t mask = (1u << take) - 1u;
        const uint32_t chunk = (data[byteIndex] >> shift) & mask;
        value = (value << take) | chunk;
        bitIndex = static_cast<uint8_t>(bitIndex + take);
        count = static_cast<uint8_t>(count - take);

        if (bitIndex == 8)
        {
            bitIndex = 0;
            ++byteIndex;
        }
    }
    return true;
}

// Devolve a posição em bits para permitir validar a fronteira útil do payload Rice.
uint64_t BitReader::bitsRead() const{
    return static_cast<uint64_t>(byteIndex) * 8 + bitIndex;
}
