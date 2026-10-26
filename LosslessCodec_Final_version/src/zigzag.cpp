// Mapeamento reversível entre resíduos com sinal e inteiros não negativos para codificação Rice.
#include "zigzag.hpp"

// Converte inteiros com sinal em inteiros sem sinal, aproximando valores pequenos de zero. 
uint32_t zigzagEncode(int32_t value){
    if (value >= 0)
        return static_cast<uint32_t>(value) * 2u; // u - unsigned
    // O alargamento antes da negação evita overflow quando value == INT32_MIN.
    return static_cast<uint32_t>(-static_cast<int64_t>(value) * 2 - 1);
}

// Reverte zigzagEncode: pares representam valores não negativos e ímpares,
// negativos. A forma usada evita overflow em value == UINT32_MAX.
int32_t zigzagDecode(uint32_t value){
    if ((value & 1u) == 0)
        return static_cast<int32_t>(value / 2u);
    return -static_cast<int32_t>(value / 2u) - 1;
}
