#pragma once

#include <cstdint>
#include <vector>

// Codifica blocos CLSS. Os canais independentes e mid/side, preditores de
// primeira/segunda ordem e o PCM cru são escolhidos separadamente por bloco.
bool encodeBlockPayload(const std::vector<int16_t>& samples, uint16_t channels, std::vector<uint8_t>& payload, uint64_t& payloadBitCount);

// Descodifica o payload em blocos e valida contagens, limites e alinhamento Rice.
bool decodeBlockPayload(const std::vector<uint8_t>& payload, uint64_t sampleCount, uint16_t channels, std::vector<int16_t>& samples);
