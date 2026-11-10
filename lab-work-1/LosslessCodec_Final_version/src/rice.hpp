#pragma once

#include <cstdint>
#include "bitstream.hpp"

// Codifica um inteiro nao negativo como codigo Rice e escreve os bits no BitWriter.
// Devolve false se k nao puder ser representado pelo formato de 32 bits.
bool riceEncode(uint32_t value, uint8_t k, BitWriter& writer);

// Le um codigo Rice do BitReader e devolve o valor; false indica k ou dados invalidos/incompletos.
bool riceDecode(BitReader& reader, uint8_t k, uint32_t& value);