#pragma once
#include <cstdint>

// Mapeia inteiros com sinal em inteiros sem sinal, colocando 0, -1, +1, -2... em ordem crescente.
uint32_t zigzagEncode(int32_t value);
// Reverte o mapeamento produzido por zigzagEncode.
int32_t zigzagDecode(uint32_t value);