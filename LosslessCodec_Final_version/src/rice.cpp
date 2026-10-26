#include "rice.hpp"

// Codifica um valor como quociente unario seguido do resto em k bits.
// BitWriter concatena os codigos em bytes, sem alinhar cada valor separadamente.
bool riceEncode(uint32_t value, uint8_t k, BitWriter& writer){
    if (k > 31)
        return false;

    const uint32_t quotient = value >> k;
    const uint32_t remainder = value & ((1u << k) - 1u);

    // O writer empacota runs unários e restos em grupos, reduzindo o custo por bit.
    writer.writeUnary(quotient);
    writer.writeBits(remainder, k);
    return true;
}

// Le o quociente unario e os k bits do resto pelo BitReader, validando limites.
bool riceDecode(BitReader& reader, uint8_t k, uint32_t& value){
    if (k > 31)
        return false;

    uint32_t quotient = 0;
    if (!reader.readUnary(quotient))
        return false;

    uint32_t remainder = 0;
    if (!reader.readBits(remainder, k))
        return false;

    const uint32_t divisor = 1u << k;
    const uint64_t decoded = static_cast<uint64_t>(quotient) * divisor + remainder;
    if (decoded > 0xFFFFFFFFu)
        return false;

    value = static_cast<uint32_t>(decoded);
    return true;
}
