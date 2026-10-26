// converte os caminhos de ficheiros, que o programa guarda em UTF-8, para o formato esperado pelo sistema operativo.
#pragma once

#include <filesystem>
#include <string>

inline std::filesystem::path filePathFromUtf8(const std::string& value)
{
#ifdef _WIN32
    return std::filesystem::u8path(value); // função usada no Windows para converter UTF-8 para o formato esperado pelo sistema operativo
#else
    return std::filesystem::path(value);
#endif
}
