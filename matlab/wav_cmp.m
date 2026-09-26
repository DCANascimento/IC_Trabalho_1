% Trabalho 1 parte 1 ex 3
% Nota: L vem dos espaços Lp da matemática, não do canal esquerdo

% Guardar sinal original e quantizado (ir buscar quantizado a ex 2)

% Localização das pastas
script_dir = fileparts(mfilename('fullpath'));
root_dir   = fileparts(script_dir);

in_dir     = fullfile(root_dir, 'AudioFiles');
quant_dir  = fullfile(root_dir, 'QuantizedFiles');

if ~isfolder(in_dir)
    error('Folder not found: %s', in_dir);
end

if ~isfolder(quant_dir)
    error('Folder not found: %s', quant_dir);
end

% Listar ficheiros originais

orig_files = dir(fullfile(in_dir, '*.wav'));

if isempty(orig_files)
    error('No WAV files found in %s', in_dir);
end

orig_names = sort({orig_files.name});

fprintf('=== Original Files ===\n');
for k = 1:numel(orig_names)
    fprintf('  %2d - %s\n', k, orig_names{k});
end

idx_orig = input('\nSelect the original file: ');

if idx_orig < 1 || idx_orig > numel(orig_names)
    error('Invalid selection.');
end

audio1 = fullfile(in_dir, orig_names{idx_orig});

% Listar ficheiros quantizados

quant_files = dir(fullfile(quant_dir, '*.wav'));

if isempty(quant_files)
    error('No WAV files found in %s', quant_dir);
end

quant_names = sort({quant_files.name});

fprintf('\n=== Quantized Files ===\n');
for k = 1:numel(quant_names)
    fprintf('  %2d - %s\n', k, quant_names{k});
end

idx_quant = input('\nSelect the quantized file: ');

if idx_quant < 1 || idx_quant > numel(quant_names)
    error('Invalid selection.');
end

audio2 = fullfile(quant_dir, quant_names{idx_quant});

% Ler os ficheiros escolhidos

[orig, fs1] = audioread(audio1);
[quant, fs2] = audioread(audio2);

fprintf('\nSelected files:\n');
fprintf('  Original:   %s\n', orig_names{idx_orig});
fprintf('  Quantized: %s\n\n', quant_names{idx_quant});



% Para garantir que estou a comparar 2 sinais compatíveis faço as seguintes
% verificações
%frequência de amostragem - para ver se foram amostrados da mesma forma
%(=amostras/segundo)
if fs1 ~= fs2
    error('Different sampling frequencies.');
end
%número de canais (ver se tem ambos esquerdo e direito, ser estéreo)
if size(orig,2) ~= size(quant,2) 
    error('Different number of channels.'); 
end
%duração (comprimento/número de amostras)
if size(orig,1) ~= size(quant,1) 
    error('Different lengths.'); 
end


channel_number = size(orig,2);

MSE = zeros(1,channel_number); % mean squared error
maxErr = zeros(1,channel_number); % max absolute error 
SNR = zeros(1,channel_number);

fprintf('Comparison between:\n');
    fprintf('  Original: %s\n', audio1);
    fprintf('  Quantized:    %s\n\n', audio2);

% prints for each channel (enunciado)

    for ch = 1:channel_number

        signal = orig(:,ch);
        processed = quant(:,ch);

        % Erro
        err = signal - processed;

        % MSE (norma L2 média)
        MSE(ch) = mean(err.^2);
        % ou MSE(ch) = norm(err, 2);

        % Norma L∞ / erro absoluto máximo
        maxErr(ch) = max(abs(err));
        % ou maxErr(ch) = norm(err,inf);
        % SNR
        Es = mean(signal.^2);
        %Es=sum(abs(signal).^2);
        Er = mean(err.^2);
        %Er=sum(abs(err).^2);
        SNR(ch) = 10 * log10(Es / Er);
       

        fprintf('Channel %d\n', ch);
        fprintf('  MSE (Average L2):      %.10f\n', MSE(ch));
        fprintf('  Maximum error (L∞):    %.10f\n', maxErr(ch));
        fprintf('  SNR:                 %.2f dB\n\n', SNR(ch));

    end

% prints for the average of the channels (enunciado)
    fprintf('Channel average\n');
    fprintf('  Average MSE:           %.10f\n', mean(MSE));
    fprintf('  Average maximum error:   %.10f\n', mean(maxErr));
    fprintf('  Average SNR:           %.2f dB\n', mean(SNR));


