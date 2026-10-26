# LosslessCodec1

Compressor lossless de WAV PCM 16-bit para o formato CLSS. Preserva os bytes do contentor WAV que não são áudio, permitindo reconstruir o ficheiro WAV original byte a byte.

## Requisitos de compilação

- CMake 3.20 ou superior
- Compilador C++17
- Windows: MinGW-w64 ou MSVC
- Linux: GCC ou Clang

## Compilar no Windows

A partir da pasta do projeto, num terminal com CMake e um compilador C++ disponíveis:

    cmake -S . -B build-user
    cmake --build build-user --config Release

Se o compilador MSYS2 falhar por causa de caracteres Unicode no caminho do projeto, usa Strawberry GCC ou MSVC, ou coloca a pasta do projeto num caminho sem caracteres Unicode.

O executável principal fica em build-user\MyLosslessCodec.exe. O CMake também cria `wav_lossless_enc.exe` e `wav_lossless_dec.exe` na mesma pasta. Em Windows, o programa recebe caminhos em UTF-16 e abre ficheiros por caminhos UTF-8 convertidos para o formato nativo. Com MinGW, o CMake liga estaticamente os runtimes do GCC e winpthread para o executável não depender dessas DLLs no computador de quem o usa.

Instala as opções do menu para a tua conta:

    powershell -NoProfile -File .\install_context_menu.ps1 -ExecutablePath .\build-user\MyLosslessCodec.exe

Depois podes clicar com o botão direito num WAV e escolher Comprimir para CLSS, ou num CLSS e escolher Descomprimir para WAV. Em Windows 11, estas opções podem estar em Mostrar mais opções. Para as remover, executa uninstall_context_menu.ps1.

## Compilar no Linux

    cmake -S . -B build-user -DCMAKE_BUILD_TYPE=Release
    cmake --build build-user

O executável principal fica em build-user/MyLosslessCodec. O CMake também cria `wav_lossless_enc` e `wav_lossless_dec` na mesma pasta. Instala as ações de contexto para o utilizador atual com:

    bash ./install_context_menu.sh build-user/MyLosslessCodec

A instalação cria ações para:

- Nautilus e Nemo, no submenu Scripts
- Dolphin, como ações para ficheiros WAV e CLSS
- Thunar, se Python 3 estiver instalado (as ações são integradas na configuração existente)

Para remover as ações, executa:

    bash ./uninstall_context_menu.sh

O suporte ao menu de contexto no Linux depende do gestor de ficheiros; o instalador cobre Nautilus, Nemo, Dolphin e Thunar. Para Dolphin, update-mime-database atualiza a associação local de .clss. Outros gestores podem exigir uma integração própria. Se moveres a pasta depois da instalação, volta a executar o instalador com o novo caminho do executável.

## Uso pela linha de comandos

Os nomes de comando pedidos no exercício são:

    wav_lossless_enc input.wav compressed_file
    wav_lossless_dec compressed_file output.wav

Executa-os a partir da pasta do projeto, por exemplo:

    Windows: .\build-user\wav_lossless_enc.exe input.wav compressed.clss
    Windows: .\build-user\wav_lossless_dec.exe compressed.clss restored.wav
    Linux:   ./build-user/wav_lossless_enc input.wav compressed.clss
    Linux:   ./build-user/wav_lossless_dec compressed.clss restored.wav

Os dois ficheiros devem ser indicados explicitamente nesses comandos. A interface anterior continua disponível; nela o argumento de saída é opcional e, se omitido, o resultado é criado junto do ficheiro de entrada. Se o nome já existir, acrescenta-se um número para evitar substituí-lo.

Se o caminho de entrada indicado não existir, o programa procura um ficheiro com o mesmo nome nas subpastas da pasta onde o comando está a ser executado. Se encontrar mais do que um, pede que indiques o caminho completo. Um caminho de saída relativo, como `compressed.clss`, continua a ser criado na pasta atual.

MyLosslessCodec -c entrada.wav [saida.clss]
MyLosslessCodec -d entrada.clss [saida.wav]
MyLosslessCodec -t original.wav reconstruido.wav

O formato WAV suportado é PCM de 16 bits. Em Windows, escolhe um compilador MinGW-w64/Strawberry ou MSVC que consiga compilar o projeto no caminho onde o descarregaste; algumas distribuições MSYS2 podem falhar quando o caminho da pasta contém caracteres Unicode.

O modo `-t` compara primeiro as amostras e depois todos os bytes dos dois WAVs.

## Estrutura do formato CLSS

O encoder divide o áudio em blocos de 4096 frames. Um frame é um instante de amostragem: em estéreo contém uma amostra esquerda e uma direita. Assim, a 44,1 kHz, cada bloco representa cerca de 93 ms. Escolhemos 4096 por ser uma potência de dois e um tamanho habitual em áudio; o FLAC também o usa como predefinição quando ativa predição LPC. Para este codec ainda não medimos sistematicamente outros tamanhos, por isso 4096 é um ponto de partida razoável, não um ótimo comprovado. Blocos menores aumentam a proporção de cabeçalhos; blocos maiores reduzem esse custo, mas adaptam-se menos depressa a mudanças no sinal.

Cada bloco escolhe independentemente entre PCM cru e Rice, o modo de canais esquerdo/direito ou mid/side (em estéreo), a ordem 1 ou 2 do preditor e o parâmetro Rice `k`. No modo mid/side, `M=L+R` e `S=L-R`; quando esquerda e direita são parecidas, o canal `S` tende a ficar pequeno e precisa de menos bits. O decoder recupera os canais com `L=(M+S)/2` e `R=(M-S)/2`, sem perda de informação. A escolha local permite adaptar a codificação a mudanças no conteúdo do áudio. O parâmetro `k` começa numa estimativa baseada na média dos resíduos ZigZag e testa os três valores próximos; a implementação calcula o custo Rice exato desses candidatos, sem percorrer todos os parâmetros possíveis.

O formato guarda por bloco o número de frames, o modo de canais, a ordem do preditor, `k` e o número de bits úteis. Se o Rice calculado não ocupar menos bytes do que PCM cru, esse bloco é guardado como PCM cru. Os blocos reiniciam a predição, o que limita a dependência entre partes do ficheiro e permite parâmetros diferentes ao longo do áudio.

O payload usa codificação Rice em blocos adaptativos. O cabeçalho guarda apenas os metadados necessários para reconstruir o WAV e ler o payload.

Ficheiros CLSS criados por implementações anteriores usam outro cabeçalho e não são compatíveis. Para os abrir com esta implementação, volta a comprimi-los a partir do WAV original.

O ficheiro WAV, o vetor PCM, o payload CLSS e os vetores temporários do bloco atual são mantidos em memória durante a operação. Assim, o custo temporário da procura de preditor fica limitado a um bloco, embora a leitura e escrita do ficheiro continuem a ser feitas sobre buffers em memória.
