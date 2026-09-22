
script_dir = fileparts(mfilename('fullpath'));
root_dir   = fileparts(script_dir);            % raiz do repositório
in_dir     = fullfile(root_dir, 'AudioFiles');

[audio, freq_amostragem] = audioread(fullfile(in_dir, 'sample01.wav'));

% [audio, freq_amostragem] = audioread("Audio Files/sample01.wav");

% L/R Channels
L = audio(:, 1);
R = audio(:, 2);

t = (0:length(audio)-1) / freq_amostragem;
bin_length = (t(2) - t(1));

% Mudar este valor permite mudar o tamanho de um bin
bits = 10;

figure;

subplot(2, 2, 1);
histogram(L, 'BinWidth', 2^bits * bin_length);
ylabel("Amplitude");
xlabel("Tempo (segundos)");
title("L Channel");

subplot(2, 2, 3);
histogram(R, 'BinWidth', 2^bits * bin_length);
ylabel("Amplitude");
xlabel("Tempo (segundos)");
title("R Channel");

% --------------------
% MID/SIDE channels

mid = (L + R) / 2;
side = (L - R) / 2;

subplot(2, 2, 2);
histogram(mid, 'BinWidth', 2^bits * bin_length);
ylabel("Amplitude");
xlabel("Tempo (segundos)");
title("MID Channel");

subplot(2, 2, 4);
histogram(side, 'BinWidth', 2^bits * bin_length);
ylabel("Amplitude");
xlabel("Tempo (segundos)");
title("SIDE Channel");



