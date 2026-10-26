#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// Para escrever bit terminador do quociente unário
class BitWriter{
private:
    std::vector<uint8_t> data;

    uint8_t currentByte = 0;
    uint8_t bitCount = 0;

public:
    // Escreve um run de bits 1 seguido do terminador 0 usado pelo quociente Rice.
    void writeUnary(uint32_t quotient);
    // Escreve os count bits menos significativos de value, do mais significativo para o menos.
    void writeBits(uint32_t value, uint8_t count);
    // Fecha o byte parcial com zeros e deixa os bytes prontos para leitura ou gravação.
    void flush();
    // Dá acesso somente de leitura aos bytes produzidos pelo BitWriter.
    const std::vector<uint8_t>& getData() const;
};

// BIT READER
// Lê a sequência empacotada por BitWriter na mesma ordem de bits e informa falhas ao atingir o fim.
class BitReader{
private:
    const std::vector<uint8_t>& data;
    std::size_t byteIndex = 0;
    uint8_t bitIndex = 0;

public:
    // Associa o leitor ao vetor de bytes que será percorrido bit a bit.
    explicit BitReader(const std::vector<uint8_t>& data);
    // Lê o próximo bit MSB-first; false indica que o vetor terminou.
    bool readBit(bool& bit);
    // Lê o quociente unário Rice, saltando bytes completos de 1 de uma só vez.
    bool readUnary(uint32_t& quotient);
    // Lê vários bits consecutivos para um inteiro; false indica dados insuficientes.
    bool readBits(uint32_t& value, uint8_t count);
    // Devolve quantos bits úteis foram consumidos, sem contar o padding final do byte.
    uint64_t bitsRead() const;
};
