#include "block_codec.hpp"

#include "bitstream.hpp"
#include "rice.hpp"
#include "zigzag.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <utility>

namespace
{
// 4096 é uma potência de dois e um tamanho habitual em codificação de áudio (também é a predefinição do FLAC)
constexpr uint32_t kFramesPerBlock = 4096;
// Limite usado pelo encoder e pelo decoder para validar o parâmetro Rice.
constexpr uint8_t kMaximumRiceK = 19;

// Os valores são gravados no cabeçalho de cada bloco para o decoder saber como o ler.
enum class BlockMode : uint8_t{
    rawPcm = 0,             // Amostras PCM sem compressão.
    independentChannels = 1, // Cada canal é codificado separadamente.
    midSide = 2              // Estéreo é transformado nos canais M e S.
};

// Acrescenta um inteiro de 32 bits ao vetor no formato little-endian do CLSS.
void appendU32LE(std::vector<uint8_t>& output, uint32_t value){
    output.push_back(static_cast<uint8_t>(value & 0xFFu));
    output.push_back(static_cast<uint8_t>((value >> 8) & 0xFFu));
    output.push_back(static_cast<uint8_t>((value >> 16) & 0xFFu));
    output.push_back(static_cast<uint8_t>((value >> 24) & 0xFFu));
}

// Lê um inteiro little-endian de 32 bits e avança o offset; false indica dados insuficientes.
bool readU32LE(const std::vector<uint8_t>& input, size_t& offset, uint32_t& value){
    if (offset > input.size() || input.size() - offset < 4)
        return false;

    value = static_cast<uint32_t>(input[offset])
          | (static_cast<uint32_t>(input[offset + 1]) << 8)
          | (static_cast<uint32_t>(input[offset + 2]) << 16)
          | (static_cast<uint32_t>(input[offset + 3]) << 24);
    offset += 4;
    return true;
}

// Acrescenta uma amostra PCM assinada como os seus dois bytes little-endian.
void appendPcmSample(std::vector<uint8_t>& output, int16_t sample){
    const uint16_t raw = static_cast<uint16_t>(sample);
    output.push_back(static_cast<uint8_t>(raw & 0xFFu));
    output.push_back(static_cast<uint8_t>((raw >> 8) & 0xFFu));
}

// Lê dois bytes little-endian e interpreta-os como uma amostra PCM assinada de 16 bits.
int16_t readPcmSample(const std::vector<uint8_t>& input, size_t offset){
    const uint16_t raw = static_cast<uint16_t>(input[offset]) | static_cast<uint16_t>(static_cast<uint16_t>(input[offset + 1]) << 8);
    const int32_t value = raw < 0x8000u ? raw : static_cast<int32_t>(raw) - 0x10000;
    return static_cast<int16_t>(value);
}

// Calcula o logaritmo de base 2 arredondado para baixo para estimar o parâmetro Rice.
uint8_t floorLog2(uint64_t value){
    uint8_t result = 0;
    while (value > 1){
        value >>= 1;
        ++result;
    }
    return result;
}

// Escolhe o parâmetro k do código Rice para os resíduos deste bloco. 
// A média dá uma estimativa inicial; depois comparamos k e os dois valores vizinhos, calculando o número exato de bits que riceEncode irá produzir.
uint8_t chooseRiceK(const std::vector<uint32_t>& values, uint64_t valueSum, uint64_t& bestBits){
    if (values.empty()){
        bestBits = 0;
        return 0;
    }

    const uint64_t mean = valueSum / values.size();
    const int base = std::min<int>(kMaximumRiceK, floorLog2(mean + 1));
    const int firstK = std::max(0, base - 1);
    const int lastK = std::min<int>(kMaximumRiceK, base + 1);
    uint8_t bestK = static_cast<uint8_t>(firstK);
    bestBits = std::numeric_limits<uint64_t>::max();

    for (int candidate = firstK; candidate <= lastK; ++candidate){
        uint64_t bits = 0;
        const uint8_t k = static_cast<uint8_t>(candidate);
        for (const uint32_t value : values)
            bits += static_cast<uint64_t>(value >> k) + 1u + k;

        if (bits < bestBits){
            bestBits = bits;
            bestK = k;
        }
    }
    return bestK;
}

// Prepara os valores Rice de um bloco: aplica o modo recebido (canais independentes ou mid/side), 
// calcula resíduos com predição de 1.ª ou 2.ª ordem e converte-os para inteiros não negativos com ZigZag. 
// Em mid/side, a predição continua a ser feita separadamente em M e S.
void makeResidualValues(
    const std::vector<int16_t>& samples,
    size_t blockStartFrame,
    uint32_t frameCount,
    uint16_t channels,
    BlockMode mode,
    uint8_t order,
    std::vector<uint32_t>& values,
    uint64_t& valueSum)
{
    const size_t blockSampleCount = static_cast<size_t>(frameCount) * channels;
    std::vector<int32_t> transformed(blockSampleCount);
    values.resize(blockSampleCount);
    valueSum = 0;

    for (uint32_t frame = 0; frame < frameCount; ++frame){
        const size_t sourceFrame = blockStartFrame + frame;
        const size_t frameOffset = static_cast<size_t>(frame) * channels;

        if (mode == BlockMode::midSide){
            const int32_t left = samples[sourceFrame * 2];
            const int32_t right = samples[sourceFrame * 2 + 1];
            // M=L+R e S=L-R 
            transformed[frameOffset] = left + right;
            transformed[frameOffset + 1] = left - right;
        }
        else{
            // No modo independente, mantém cada canal sem misturar as amostras.
            for (uint16_t channel = 0; channel < channels; ++channel)
                transformed[frameOffset + channel] = samples[sourceFrame * channels + channel];
        }

        for (uint16_t channel = 0; channel < channels; ++channel){
            // Na primeira frame do bloco não há histórico: guarda-se a amostra transformada. 
            // A ordem 1 subtrai a amostra anterior; a ordem 2 subtrai também a diferença anterior, produzindo uma segunda diferença.
            int32_t residual = transformed[frameOffset + channel];
            if (frame >= 1){
                const int32_t previous = transformed[frameOffset - channels + channel];
                residual -= previous;
                if (order == 2 && frame >= 2){
                    const int32_t previousPrevious = transformed[frameOffset - 2 * channels + channel];
                    residual -= previous - previousPrevious;
                }
            }
            const size_t index = frameOffset + channel;

            // ZigZag 
            // Guarda o resíduo mapeado e acumula a soma, usada para estimar k.
            values[index] = zigzagEncode(residual);
            valueSum += values[index];
        }
    }
}

bool reconstructRiceBlock(
    const std::vector<uint8_t>& blockBytes,
    uint32_t bitCount,
    uint32_t frameCount,
    uint16_t channels,
    BlockMode mode,
    uint8_t order,
    uint8_t k,
    std::vector<int16_t>& output,
    size_t outputFrameOffset)
{
    // Inverte a sequência usada na codificação: descodifica Rice, aplica a inversa de ZigZag e soma as diferenças previstas para recuperar os canais.
    // Primeiro valida os parâmetros do bloco para rejeitar cabeçalhos incompatíveis.
    if (k > kMaximumRiceK || (order != 1 && order != 2) || (mode == BlockMode::midSide && channels != 2))
        return false;

    BitReader reader(blockBytes);
    const size_t blockSampleCount = static_cast<size_t>(frameCount) * channels;
    std::vector<int32_t> transformed(blockSampleCount);

    for (uint32_t frame = 0; frame < frameCount; ++frame) {
        const size_t frameOffset = static_cast<size_t>(frame) * channels;
        for (uint16_t channel = 0; channel < channels; ++channel){
            uint32_t mapped = 0;
            // Cada amostra codificada começa por Rice e depois é convertida de volta de ZigZag.
            if (!riceDecode(reader, k, mapped))
                return false;

            const int32_t residual = zigzagDecode(mapped);
            int64_t value = residual;
            if (frame >= 1){
                const int32_t previous = transformed[frameOffset - channels + channel];
                value += previous;
                if (order == 2 && frame >= 2){
                    const int32_t previousPrevious = transformed[frameOffset - 2 * channels + channel];
                    value += static_cast<int64_t>(previous) - previousPrevious;
                }
            }
            if (value < std::numeric_limits<int32_t>::min()|| value > std::numeric_limits<int32_t>::max())
                return false;
            transformed[frameOffset + channel] = static_cast<int32_t>(value);
        }
    }

    // Confirma que Rice consumiu exatamente os bits úteis declarados no registo do bloco.
    if (reader.bitsRead() != bitCount)
        return false;

    for (uint32_t frame = 0; frame < frameCount; ++frame) {
        const size_t blockOffset = static_cast<size_t>(frame) * channels;
        const size_t outputOffset = (outputFrameOffset + frame) * channels;

        if (mode == BlockMode::midSide) {
            // Converte M e S novamente em L e R, verificando paridade e limites PCM.
            const int64_t mid = transformed[blockOffset];
            const int64_t side = transformed[blockOffset + 1];
            // Inverte M/S exatamente: L=(M+S)/2 e R=(M-S)/2.
            const int64_t leftTwice = mid + side;
            const int64_t rightTwice = mid - side;
            if ((leftTwice % 2) != 0 || (rightTwice % 2) != 0)
                return false;
            const int64_t left = leftTwice / 2;
            const int64_t right = rightTwice / 2;
            if (left < std::numeric_limits<int16_t>::min()|| left > std::numeric_limits<int16_t>::max()|| right < std::numeric_limits<int16_t>::min()|| right > std::numeric_limits<int16_t>::max())
                return false;
            output[outputOffset] = static_cast<int16_t>(left);
            output[outputOffset + 1] = static_cast<int16_t>(right);
        }
        else{
            // No modo independente, cada valor reconstruído já corresponde ao canal de saída.
            for (uint16_t channel = 0; channel < channels; ++channel){
                const int32_t value = transformed[blockOffset + channel];
                if (value < std::numeric_limits<int16_t>::min()|| value > std::numeric_limits<int16_t>::max())
                    return false;
                output[outputOffset + channel] = static_cast<int16_t>(value);
            }
        }
    }
    return true;
}
}

//Codifica o payload de blocos, escolhendo PCM cru ou Rice para cada bloco
bool encodeBlockPayload(const std::vector<int16_t>& samples,uint16_t channels,std::vector<uint8_t>& payload,uint64_t& payloadBitCount){
    // Inicializa os resultados e rejeita uma entrada que não forme frames completos.
    payload.clear();
    payloadBitCount = 0;
    if (channels == 0 || samples.size() % channels != 0)
        return false;

    // Divide o áudio em blocos de, no máximo, 4096 frames. Cada bloco escolhe
    // independentemente o modo de canais, a ordem de predição e o k de Rice.
    const uint64_t totalFrames = samples.size() / channels;
    // Guarda primeiro a quantidade de blocos para o decoder saber quantos registos esperar.
    const uint64_t blockCount64 = (totalFrames + kFramesPerBlock - 1) / kFramesPerBlock;
    if (blockCount64 > std::numeric_limits<uint32_t>::max())
        return false;
    appendU32LE(payload, static_cast<uint32_t>(blockCount64));

    for (uint64_t blockStart = 0; blockStart < totalFrames; blockStart += kFramesPerBlock){
        // Calcula o tamanho deste bloco e o custo de referência se fosse PCM cru.
        const uint32_t frameCount = static_cast<uint32_t>(
            std::min<uint64_t>(kFramesPerBlock, totalFrames - blockStart));
        const size_t blockSampleCount = static_cast<size_t>(frameCount) * channels;
        const uint64_t rawBits = static_cast<uint64_t>(blockSampleCount) * 16u;
        const uint64_t rawBytes = static_cast<uint64_t>(blockSampleCount) * 2u;

        BlockMode bestMode = BlockMode::rawPcm;
        uint8_t bestOrder = 0;
        uint8_t bestK = 0;
        // PCM cru é a opção inicial. Rice só substitui esta opção se ocupar menos bytes no bloco, já contando o preenchimento do último byte.
        uint64_t bestBits = rawBits;
        std::vector<uint32_t> bestValues;

        // Para cada modo permitido, avalia as predições de 1.ª e 2.ª ordem.
        // chooseRiceK estima o custo de Rice; o menor custo válido fica guardado.
        const std::array<BlockMode, 2> modes = { BlockMode::independentChannels, BlockMode::midSide };
        // Mid/side só pode ser testado quando há exatamente dois canais.
        const size_t modeCount = channels == 2 ? 2 : 1;
        for (size_t modeIndex = 0; modeIndex < modeCount; ++modeIndex){
            const BlockMode mode = modes[modeIndex];
            for (uint8_t order = 1; order <= 2; ++order){
                std::vector<uint32_t> values;
                uint64_t valueSum = 0;
                // Transforma o bloco e calcula os resíduos para esta combinação de modo e ordem.
                makeResidualValues(samples, static_cast<size_t>(blockStart), frameCount,
                                   channels, mode, order, values, valueSum);

                uint64_t riceBits = 0;
                // Experimenta um k adequado e compara o custo Rice com o PCM cru.
                const uint8_t k = chooseRiceK(values, valueSum, riceBits);
                const uint64_t riceBytes = riceBits / 8 + (riceBits % 8 != 0 ? 1 : 0);
                // O cabeçalho de bloco é igual para PCM cru e Rice; só escolhemos Rice se
                // o payload de áudio ocupar menos bytes que PCM, evitando expandi-lo.
                if (riceBytes < rawBytes && riceBits < bestBits){
                    bestMode = mode;
                    bestOrder = order;
                    bestK = k;
                    bestBits = riceBits;
                    bestValues = std::move(values);
                }
            }
        }

        // Cada bloco tem um registo fixo: frames, modo, ordem, k, reservado e bits úteis.
        // Escreve os parâmetros escolhidos antes dos bytes de áudio do bloco.
        appendU32LE(payload, frameCount);
        payload.push_back(static_cast<uint8_t>(bestMode));
        payload.push_back(bestOrder);
        payload.push_back(bestK);
        payload.push_back(0);
        if (bestBits > std::numeric_limits<uint32_t>::max())
            return false;
        appendU32LE(payload, static_cast<uint32_t>(bestBits));

        if (bestMode == BlockMode::rawPcm){
            // Nenhuma combinação Rice reduziu o tamanho: grava as amostras PCM
            // deste bloco diretamente, em little-endian.
            const size_t firstSample = static_cast<size_t>(blockStart) * channels;
            for (size_t i = 0; i < blockSampleCount; ++i)
                appendPcmSample(payload, samples[firstSample + i]);
        }
        else{
            // Codifica cada valor ZigZag com o k escolhido. riceEncode escreve
            // os bits através de BitWriter; flush completa o último byte.
            BitWriter writer;
            for (const uint32_t value : bestValues){
                if (!riceEncode(value, bestK, writer))
                    return false;
            }
            writer.flush();
            const std::vector<uint8_t>& bytes = writer.getData();
            payload.insert(payload.end(), bytes.begin(), bytes.end());
        }
    }

    payloadBitCount = static_cast<uint64_t>(payload.size()) * 8u;
    return true;
}

// Descodifica o payload de blocos, reconstruindo as amostras PCM.
bool decodeBlockPayload(const std::vector<uint8_t>& payload, uint64_t sampleCount, uint16_t channels, std::vector<int16_t>& samples){
    // Reinicia a saída e verifica se a contagem de amostras e o payload são plausíveis.
    samples.clear();
    if (channels == 0 || sampleCount % channels != 0 || sampleCount > samples.max_size() || payload.size() < 4)
        return false;

    const uint64_t totalFrames = sampleCount / channels;
    // Confirma que o número de blocos anunciado corresponde ao número de frames esperado.
    const uint64_t expectedBlockCount = (totalFrames + kFramesPerBlock - 1) / kFramesPerBlock;
    size_t offset = 0;
    uint32_t blockCount = 0;
    if (!readU32LE(payload, offset, blockCount) || blockCount != expectedBlockCount)
        return false;

    samples.resize(static_cast<size_t>(sampleCount));
    uint64_t decodedFrames = 0;

    for (uint32_t blockIndex = 0; blockIndex < blockCount; ++blockIndex){
        // Lê o registo do bloco e usa o modo, a ordem e k guardados pelo encoder
        // para decidir entre copiar PCM cru e reconstruir os dados Rice.
        uint32_t frameCount = 0;
        uint32_t bitCount = 0;
        if (!readU32LE(payload, offset, frameCount) || payload.size() - offset < 4)
            return false;

        const BlockMode mode = static_cast<BlockMode>(payload[offset]);
        const uint8_t order = payload[offset + 1];
        const uint8_t k = payload[offset + 2];
        const uint8_t reserved = payload[offset + 3];
        offset += 4;
        if (reserved != 0 || !readU32LE(payload, offset, bitCount))
            return false;

        const uint32_t expectedFrames = static_cast<uint32_t>(std::min<uint64_t>(kFramesPerBlock, totalFrames - decodedFrames));
        if (frameCount == 0 || frameCount != expectedFrames)
            return false;

        // Converte os bits declarados em bytes e verifica que o bloco cabe no payload restante.
        const uint64_t encodedBytes64 = bitCount / 8 + (bitCount % 8 != 0 ? 1 : 0);
        if (encodedBytes64 > payload.size() - offset)
            return false;
        const size_t encodedBytes = static_cast<size_t>(encodedBytes64);
        const size_t blockSampleCount = static_cast<size_t>(frameCount) * channels;

        if (mode == BlockMode::rawPcm){
            // Em PCM cru, valida os parâmetros e copia as amostras diretamente para a saída.
            if (order != 0 || k != 0 || bitCount != static_cast<uint64_t>(blockSampleCount) * 16u || encodedBytes != blockSampleCount * 2u)
                return false;
            const size_t outputStart = static_cast<size_t>(decodedFrames) * channels;
            for (size_t i = 0; i < blockSampleCount; ++i)
                samples[outputStart + i] = readPcmSample(payload, offset + i * 2u);
        }
        else if (mode == BlockMode::independentChannels || mode == BlockMode::midSide){
            // Em Rice, valida o modo e os parâmetros antes de reconstruir o bloco.
            if ((mode == BlockMode::midSide && channels != 2) || (order != 1 && order != 2) || k > kMaximumRiceK || bitCount < blockSampleCount)
                return false;

            std::vector<uint8_t> blockBytes(payload.begin() + static_cast<std::ptrdiff_t>(offset), payload.begin() + static_cast<std::ptrdiff_t>(offset + encodedBytes));
            if (!reconstructRiceBlock(blockBytes, bitCount, frameCount, channels, mode, order, k, samples, static_cast<size_t>(decodedFrames)))
                return false;
        }
        else{
            return false;
        }
        offset += encodedBytes;
        decodedFrames += frameCount;
    }
    // Aceita apenas se todos os frames foram reconstruídos e não sobraram bytes no payload.
    return decodedFrames == totalFrames && offset == payload.size();
}
