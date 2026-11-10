#include <iostream> 
#ifdef _WIN32 // Para evitar conflito com std::min e std::max do C++17, que são usados no código
#ifndef NOMINMAX // Evita que windows.h defina macros min/max que conflitam com std::min/std::max
#define NOMINMAX 
#endif
#include <windows.h> // Inclui a API do Windows para manipulação de ficheiros e caminhos
#endif
#include <vector>
#include <cstdint> // Inclui tipos de dados inteiros com tamanho fixo, como uint32_t e int16_t
#include <algorithm> // Inclui funções de algoritmos padrão, como std::min e std::max
#include <cstdlib> 
#include <string>
#include <fstream>
#include <chrono> // Inclui funções para medir o tempo de execução
#include <array>
#include <iomanip>
#include <filesystem> // Inclui funcionalidades para manipulação de caminhos e ficheiros
#include <system_error> 

#include "wav.hpp"
#include "block_codec.hpp"
#include "format.hpp"
#include "path_utils.hpp"

using namespace std;

// Compara os bytes completos de dois ficheiros sem os carregar integralmente para memoria
bool filesEqualByteForByte(const string& firstFilename, const string& secondFilename){
    ifstream first(filePathFromUtf8(firstFilename), ios::binary);
    ifstream second(filePathFromUtf8(secondFilename), ios::binary);
    if (!first || !second)
        return false;

    first.seekg(0, ios::end);
    second.seekg(0, ios::end);
    const streamoff firstSize = first.tellg();
    const streamoff secondSize = second.tellg();
    if (firstSize < 0 || secondSize < 0 || firstSize != secondSize)
        return false;

    first.seekg(0, ios::beg);
    second.seekg(0, ios::beg);
    array<char, 8192> firstBytes{};
    array<char, 8192> secondBytes{};
    streamoff remaining = firstSize;
    while (remaining > 0){
        const auto count = static_cast<streamsize>(min<streamoff>(remaining, static_cast<streamoff>(firstBytes.size())));
        if (!first.read(firstBytes.data(), count) || !second.read(secondBytes.data(), count)|| !equal(firstBytes.begin(), firstBytes.begin() + count,secondBytes.begin()))
            return false;
        remaining -= count;
    }
    return true;
}

// Compressão
// Executa o pipeline WAV → resíduos → ZigZag → Rice e grava o cabeçalho e payload no ficheiro CLSS.
bool compressFile(const string& inputFilename, const string& outputFilename){
    const auto operationStart = chrono::steady_clock::now();

    WavInfo info;
    vector<int16_t> samples;
    WavLayout wavLayout;

    if (!loadWavInfo(inputFilename, info, samples, &wavLayout)){
        cerr << "Error: could not open WAV file.\n";
        return false;
    }

    // Escolhe canais, preditor, parâmetro Rice e PCM bruto por bloco.
    vector<uint8_t> payload;
    uint64_t payloadBitCount = 0;
    if (!encodeBlockPayload(samples, info.channels, payload, payloadBitCount)){
        cerr << "Error: failed to encode CLSS blocks.\n";
        return false;
    }

    // Header
    CodecHeader header{};
    header.channels = info.channels;
    header.sampleRate = info.sampleRate;
    header.bitsPerSample = info.bitsPerSample;
    header.sampleCount = samples.size();
    header.compressedBitCount = payloadBitCount;
    header.originalFileSize = wavLayout.originalFileSize;
    header.wavDataOffset = wavLayout.dataOffset;

    // Guardar CLSS
    if (!writeCompressedFile(outputFilename, header, wavLayout, payload)){
        cerr << "Error: failed to write the compressed file.\n";
        return false;
    }

    const double elapsedSeconds = chrono::duration<double>(chrono::steady_clock::now() - operationStart).count();
    const double compressionSpeedMBps = elapsedSeconds > 0.0 ? (static_cast<double>(info.dataSize) / 1'000'000.0) / elapsedSeconds: 0.0;

    cout << "\nCompressed file created: " << outputFilename << '\n';
    cout << "Processing speed     : " << compressionSpeedMBps << " MB/s of PCM audio\n";
    cout << "Compression time     : " << elapsedSeconds << " s\n";

    ifstream compressedFile(filePathFromUtf8(outputFilename), ios::binary);

    if (compressedFile){
        compressedFile.seekg(0, ios::end);
        const streamoff compressedSize = static_cast<streamoff>(compressedFile.tellg());

        if (compressedSize >= 0){
            const uint64_t compressedBytes = static_cast<uint64_t>(compressedSize);
            cout << "Original WAV size    : " << wavLayout.originalFileSize << " bytes\n";
            cout << "Compressed file size : " << compressedBytes << " bytes\n";
            if (wavLayout.originalFileSize != 0){
                cout << "Compressed / original: "
                     << (100.0 * static_cast<double>(compressedBytes) / wavLayout.originalFileSize) << "%\n";
                if (compressedBytes != 0){
                    const double compressionRatio =
                        static_cast<double>(wavLayout.originalFileSize) / compressedBytes;
                    cout << "Compression ratio: " << setprecision(3) << compressionRatio << ":1\n";
                }
            }
        }
    }
    return true;
}

// descompressão
// Lê o formato CLSS atual, valida os metadados e reconstrói o WAV byte a byte.
bool decompressFile(const string& inputFilename, const string& outputFilename){
    const auto operationStart = chrono::steady_clock::now();

    cout << "\n================================\n" << "         DECOMPRESSION\n" << "================================\n";

    // Ler ficheiro CLSS
    CodecHeader header;
    WavLayout wavLayout;
    vector<uint8_t> compressedData; // Payload CLSS com blocos adaptativos.

    if (!readCompressedFile(inputFilename, header, wavLayout, compressedData)){
        cerr << "Error: could not read the CLSS file.\n";
        return false;
    }

    // Validar header
    if (header.channels == 0){
        cerr << "Error: invalid channel count.\n";
        return false;
    }

    if (header.sampleRate == 0){
        cerr << "Error: invalid sample rate.\n";
        return false;
    }

    if (header.bitsPerSample != 16){
        cerr << "Error: only 16-bit PCM is supported.\n";
        return false;
    }

    cout << "\n=== CLSS FILE ===\n";
    cout << "Container format : CLSS\n";
    cout << "Channels         : " << header.channels << '\n';
    cout << "Sample Rate      : " << header.sampleRate << '\n';
    cout << "Bits/Sample      : " << header.bitsPerSample << '\n';
    cout << "Sample Count     : " << header.sampleCount << '\n';
    cout << "Payload method   : Block Rice\n";
    cout << "Compressed Bits  : " << header.compressedBitCount << '\n';
    cout << "Compressed Bytes : " << compressedData.size() << '\n';

    // Verificar se temos bytes suficientes
    const uint64_t availableBits =
        static_cast<uint64_t>(compressedData.size()) * 8;

    if (header.compressedBitCount >
        availableBits)
    {
        cerr << "Error: incomplete bitstream.\n";

        return false;
    }

    // Rice Decode
    if (header.sampleCount > vector<int16_t>().max_size()|| header.sampleCount % header.channels != 0){
        cerr << "Error: sample count is invalid for the channel count.\n";
        return false;
    }

    vector<int16_t> decodedSamples(static_cast<size_t>(header.sampleCount));

    if (header.compressedBitCount != static_cast<uint64_t>(compressedData.size()) * 8u
        || !decodeBlockPayload(compressedData, header.sampleCount, header.channels, decodedSamples)){
        cerr << "Error: failed to decode CLSS blocks.\n";
        return false;
    }

    // Criar WavInfo
    WavInfo info{};
    info.channels = header.channels;
    info.sampleRate = header.sampleRate;
    info.bitsPerSample = header.bitsPerSample;
    info.dataSize = static_cast<uint32_t>(decodedSamples.size() * sizeof(int16_t));

    // Guardar WAV
    if (!saveWavPreservingLayout(outputFilename, info, decodedSamples, wavLayout)){
        cerr << "Error: failed to create the reconstructed WAV file.\n";
        return false;
    }

    const double elapsedSeconds = chrono::duration<double>(chrono::steady_clock::now() - operationStart).count();
    const uint64_t reconstructedPcmBytes = static_cast<uint64_t>(decodedSamples.size()) * sizeof(int16_t);
    const double decompressionSpeedMBps = elapsedSeconds > 0.0 ? (static_cast<double>(reconstructedPcmBytes) / 1'000'000.0)/ elapsedSeconds: 0.0;

    cout << "\nReconstructed WAV created: " << outputFilename << '\n';
    cout << "Processing speed         : " << decompressionSpeedMBps << " MB/s of PCM audio\n";
    cout << "Decompression time       : " << elapsedSeconds << " s\n";
    cout << "\n=== DECOMPRESSION SUCCESS ===\n";
    cout << "Samples reconstructed    : " << decodedSamples.size() << '\n';

    return true;
}

// COMPARAR DOIS WAVS
// Compara metadados, amostras e todos os bytes dos WAVs para confirmar igualdade exata.
bool compareWavFiles(const string& firstFilename, const string& secondFilename){
    WavInfo firstInfo;
    WavInfo secondInfo;

    vector<int16_t> firstSamples;
    vector<int16_t> secondSamples;

    if (!loadWavInfo(firstFilename, firstInfo, firstSamples)){
        return false;
    }

    if (!loadWavInfo(secondFilename, secondInfo, secondSamples)){
        return false;
    }

    cout << "\n=== LOSSLESS COMPARISON ===\n";

    // Comparar canais
    if (firstInfo.channels != secondInfo.channels){
        cout << "FAIL: channel counts differ.\n";
        return false;
    }

    // Comparar sample rate
    if (firstInfo.sampleRate != secondInfo.sampleRate){
        cout << "FAIL: sample rates differ.\n";
        return false;
    }

    // Comparar bits por sample
    if (firstInfo.bitsPerSample != secondInfo.bitsPerSample){
        cout << "FAIL: bits per sample differ.\n";
        return false;
    }

    // Comparar número de samples
    if (firstSamples.size() != secondSamples.size()){
        cout << "FAIL: sample counts differ.\n";
        cout << "Original sample count: " << firstSamples.size() << '\n';
        cout << "Restored sample count: " << secondSamples.size() << '\n';
        return false;
    }

    // Comparar cada sample
    for (size_t i = 0; i < firstSamples.size(); ++i){
        if (firstSamples[i] != secondSamples[i]){
            cout << "FAIL: samples differ.\n";
            cout << "First difference at index: " << i << '\n';
            cout << "Original sample      : " << firstSamples[i] << '\n';
            cout << "Reconstructed sample : " << secondSamples[i] << '\n';
            return false;
        }
    }

    if (!filesEqualByteForByte(firstFilename, secondFilename)){
        cout << "FAIL: sample values match, but the WAV files differ byte for byte.\n";
        return false;
    }

    // Tudo igual
    cout << "SUCCESS: the WAV files are identical byte for byte.\n";
    return true;
}


// Gera o destino preservando os bytes UTF-8 do caminho de entrada.
string chooseDefaultOutputFilename(const string& inputFilename, const string& extension){
    const size_t separator = inputFilename.find_last_of("/\\");
    const string parent = separator == string::npos ? string{} : inputFilename.substr(0, separator + 1);
    const string filename = separator == string::npos ? inputFilename : inputFilename.substr(separator + 1);
    const size_t dot = filename.find_last_of('.');
    const string stem = dot == string::npos || dot == 0 ? filename : filename.substr(0, dot);

    for (uint32_t suffix = 1; suffix <= 100000; ++suffix){
        const string candidate = parent + stem + (suffix == 1 ? string{} : " (" + to_string(suffix) + ")") + extension;
        ifstream existing(filePathFromUtf8(candidate), ios::binary);
        if (!existing)
            return candidate;
    }
    return {};
}

// Resolve primeiro o caminho indicado; se não existir, procura pelo nome do ficheiro nas subpastas da pasta de trabalho atual
bool resolveInputFilename(const string& inputFilename, string& resolvedFilename){
    namespace fs = std::filesystem;

    const fs::path requestedPath = filePathFromUtf8(inputFilename);
    std::error_code error;
    if (fs::is_regular_file(requestedPath, error)){
        resolvedFilename = inputFilename;
        return true;
    }

    const string requestedName = requestedPath.filename().u8string();
    if (requestedName.empty()){
        cerr << "Error: invalid input path: " << inputFilename << '\n';
        return false;
    }

    const fs::path searchRoot = fs::current_path(error);
    if (error){
        cerr << "Error: could not get the current working directory.\n";
        return false;
    }

    vector<fs::path> matches;
    const auto options = fs::directory_options::skip_permission_denied;
    fs::recursive_directory_iterator it(searchRoot, options, error);
    const fs::recursive_directory_iterator end;
    while (!error && it != end){
        std::error_code fileError;
        if (it->is_regular_file(fileError) && it->path().filename().u8string() == requestedName){
            matches.push_back(it->path());
            if (matches.size() > 1)
                break;
        }

        it.increment(error);
    }

    if (error){
        cerr << "Error: a problem occurred while searching for '" << requestedName
             << "' in subdirectories of " << searchRoot.u8string() << ".\n";
        return false;
    }

    if (matches.empty()){
        cerr << "Error: could not find '" << requestedName
             << "' at the given path or in subdirectories of "
             << searchRoot.u8string() << ".\n";
        return false;
    }

    if (matches.size() > 1){
        cerr << "Error: more than one file is named '" << requestedName
             << "'. Please provide the full path.\n";
        for (const fs::path& match : matches)
            cerr << "  " << match.u8string() << '\n';
        return false;
    }

    resolvedFilename = matches.front().u8string();
    cout << "File found: " << resolvedFilename << '\n';
    return true;
}

// Extrai o nome do executável para reconhecer as entradas encoder/decoder pedidas no exercício.
string executableStem(const string& executablePath){
    const size_t separator = executablePath.find_last_of("/\\");
    const string filename = separator == string::npos ? executablePath : executablePath.substr(separator + 1);
    const size_t dot = filename.find_last_of('.');
    return dot == string::npos ? filename : filename.substr(0, dot);
}

// Apresenta os modos de chamada e a forma opcional de indicar o ficheiro de saida.
void printUsage(const char* programName){
    cout << "\nUsage:\n\n"
         << "  Compression:\n"
         << "    " << programName << " -c input.wav [output.clss]\n\n"
         << "  Decompression:\n"
         << "    " << programName << " -d input.clss [output.wav]\n\n"
         << "  Lossless comparison:\n"
         << "    " << programName << " -t original.wav restored.wav\n\n"
         << "  Exercise-compatible executable names:\n"
         << "    wav_lossless_enc input.wav compressed_file\n"
         << "    wav_lossless_dec compressed_file output.wav\n\n";
}

// MAIN
// Interpreta os argumentos; sem argumentos pede interativamente o WAV e o destino da compressão.
#ifdef _WIN32
// Converte um argumento UTF-16 do Windows para UTF-8, que e o formato interno usado pelo codec para todos os caminhos.
string wideToUtf8(const wstring& value){
    if (value.empty())
        return {};

    const int sourceLength = static_cast<int>(value.size());
    const int outputLength = WideCharToMultiByte(CP_UTF8, 0, value.data(), sourceLength, nullptr, 0, nullptr, nullptr);

    if (outputLength <= 0)
        return {};

    string output(static_cast<size_t>(outputLength), '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, value.data(), sourceLength, output.data(), outputLength, nullptr, nullptr) <= 0){
        return {};
    }
    return output;
}
#endif

// Escolhe a operação pedida: compressao, descompressao e comparacao.
// Os argumentos de ficheiro chegam aqui sempre em UTF-8.
int runApplication(const vector<string>& args){
    const int argc = static_cast<int>(args.size());
    const string programStem = executableStem(args[0]);

    // Os nomes de executável do enunciado selecionam diretamente a operação.
    if (programStem == "wav_lossless_enc"){
        if (argc != 3){ // argc é o número de argumentos
            printUsage(args[0].c_str());
            return EXIT_FAILURE;
        }
        string inputFilename;
        if (!resolveInputFilename(args[1], inputFilename))
            return EXIT_FAILURE;
        return compressFile(inputFilename, args[2]) ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (programStem == "wav_lossless_dec"){
        if (argc != 3){
            printUsage(args[0].c_str());
            return EXIT_FAILURE;
        }
        string inputFilename;
        if (!resolveInputFilename(args[1], inputFilename))
            return EXIT_FAILURE;
        return decompressFile(inputFilename, args[2]) ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    // Sem argumentos, pedir interativamente o WAV a comprimir.
    if (argc == 1){
        string inputFilename;
        string outputFilename;
        cout << "WAV file to compress: ";
#ifdef _WIN32
        wstring wideInput;
        getline(wcin, wideInput);
        inputFilename = wideToUtf8(wideInput);
#else
        getline(cin, inputFilename);
#endif
        if (inputFilename.empty()){
            cerr << "Error: please enter a WAV file path.\n";
            return EXIT_FAILURE;
        }

        string resolvedInputFilename;
        if (!resolveInputFilename(inputFilename, resolvedInputFilename))
            return EXIT_FAILURE;
        inputFilename = resolvedInputFilename;

        outputFilename = chooseDefaultOutputFilename(inputFilename, ".clss");
        if (outputFilename.empty()){
            cerr << "Error: could not choose an output path.\n";
            return EXIT_FAILURE;
        }

        cout << "Output file [" << outputFilename << "]: ";
        string chosenOutput;
#ifdef _WIN32
        wstring wideOutput;
        getline(wcin, wideOutput);
        chosenOutput = wideToUtf8(wideOutput);
#else
        getline(cin, chosenOutput);
#endif
        if (!chosenOutput.empty())
            outputFilename = chosenOutput;

        return compressFile(inputFilename, outputFilename)? EXIT_SUCCESS: EXIT_FAILURE;
    }

    // O menu de contexto envia apenas modo e caminho; o codec inventa a saida:
    // A função gera automaticamente um nome: .clss para compressão ou .wav para descompressão
    if (argc == 3 && (args[1] == "-c" || args[1] == "-d")){
        const string& mode = args[1];
        string inputFilename;
        if (!resolveInputFilename(args[2], inputFilename))
            return EXIT_FAILURE;
        const string extension = mode == "-c" ? ".clss" : ".wav";
        const string outputFilename =
            chooseDefaultOutputFilename(inputFilename, extension);

        if (outputFilename.empty()){
            cerr << "Error: could not choose an output path.\n";
            return EXIT_FAILURE;
        }

        cout << "Output file: " << outputFilename << '\n';
        const bool success = mode == "-c" ? compressFile(inputFilename, outputFilename): decompressFile(inputFilename, outputFilename);
        return success ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (argc != 4){
        printUsage(args[0].c_str());
        return EXIT_FAILURE;
    }

    const string& mode = args[1];
    string inputFilename;
    const string& outputFilename = args[3];

    if (mode == "-c"){
        if (!resolveInputFilename(args[2], inputFilename))
            return EXIT_FAILURE;
        if (compressFile(inputFilename, outputFilename)){
            cout << "\nCompression completed successfully.\n";
            return EXIT_SUCCESS;
        }
        return EXIT_FAILURE;
    }

    if (mode == "-d"){
        if (!resolveInputFilename(args[2], inputFilename))
            return EXIT_FAILURE;
        if (decompressFile(inputFilename, outputFilename)){
            cout << "\nDecompression completed successfully.\n";
            return EXIT_SUCCESS;
        }
        return EXIT_FAILURE;
    }

    if (mode == "-t"){
        string secondInputFilename;
        if (!resolveInputFilename(args[2], inputFilename)
            || !resolveInputFilename(outputFilename, secondInputFilename))
            return EXIT_FAILURE;
        return compareWavFiles(inputFilename, secondInputFilename)
            ? EXIT_SUCCESS
            : EXIT_FAILURE;
    }

    cerr << "Error: unknown mode: " << mode << '\n';
    printUsage(args[0].c_str());
    return EXIT_FAILURE;
}

#ifdef _WIN32
// O Windows fornece argv em UTF-16, incluindo caminhos com qualquer idioma.
int wmain(int argc, wchar_t* argv[]){
    vector<string> args;
    args.reserve(static_cast<size_t>(argc));
    for (int i = 0; i < argc; ++i)
        args.push_back(wideToUtf8(argv[i]));
    return runApplication(args);
}
#else
// Em Linux, argv ja usa os bytes nativos do caminho; estes sao UTF-8 na maioria dos sistemas modernos
int main(int argc, char* argv[]){
    vector<string> args;
    args.reserve(static_cast<size_t>(argc));
    for (int i = 0; i < argc; ++i)
        args.emplace_back(argv[i]);
    return runApplication(args);
}
#endif
