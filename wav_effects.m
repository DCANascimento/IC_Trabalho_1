%  AUDIO EFFECTS
%  1 - Single Echo
%  2 - Multiple Echoes
%  3 - Amplitude Modulation
%  4 - Time-Varying Delays
%      1 - Vibrato
%      2 - Chorus
%      3 - Flanger
%      4 - Doppler
%  5 - Sair

clear;
clc;
close all;

filename = 'sample02.wav';

if ~isfile(filename)
    error('File "%s" not found.', filename);
end

[y, fs] = audioread(filename);

% Passar dados para double (caso não estejam) para o processamento funcionar melhor (sem perdas de precisão)
y = double(y);

fprintf('Sampling rate: %d Hz\n', fs);
fprintf('Number of samples: %d\n', size(y,1));
fprintf('Number of channels: %d\n', size(y,2));

% Limitar o sinal à amplitude máxima permitida (+ ou -1)
y = normalizeAudio(y); %função no final do código 

opcao = 0;

while opcao ~= 5

    fprintf('\n========================================\n');
    fprintf('          AUDIO EFFECTS\n');
    fprintf('========================================\n');
    fprintf('1. Single echo\n');
    fprintf('2. Multiple echoes\n');
    fprintf('3. Amplitude modulation\n');
    fprintf('4. Time-varying delays\n');
    fprintf('5. Exit program\n');
    fprintf('========================================\n');

    opcao = input('Option? ');

    switch opcao

        case 1

            fprintf('\n--- SINGLE ECHO ---\n');
            fprintf('Three examples of echo will be reproduced.\n\n');

            % Eco 1
            delay1 = 0.45;      % segundos
            gain1  = 0.75;      % amplitude do eco

            x1 = addEcho(y, fs, delay1, gain1); % função no final do ficheiro

            fprintf('Echo 1: delay = %.2f s | gain = %.2f\n', ...
                    delay1, gain1);

            playAudio(x1, fs); % função no final do ficheiro

            pause(0.5);

            % Eco 2
            delay2 = 0.75;
            gain2  = 0.60;

            x2 = addEcho(y, fs, delay2, gain2);

            fprintf('Echo 2: delay = %.2f s | gain = %.2f\n', delay2, gain2);

            playAudio(x2, fs);

            pause(0.5);

            % Eco 3 - mais evidente
            delay3 = 0.30;
            gain3  = 0.90;

            x3 = addEcho(y, fs, delay3, gain3);

            fprintf('Echo 3: delay = %.2f s | gain = %.2f\n', delay3, gain3);

            playAudio(x3, fs);

        case 2

            fprintf('\n--- MULTIPLE ECHOES ---\n');

            % Intervalo entre ecos
            delay = 0.35;

            % Ganhos dos vários ecos
            gains = [0.75 0.55 0.40 0.28 0.18];

            x = addMultipleEchoes(y, fs, delay, gains); % função no final do ficheiro

            fprintf('Number of echoes: %d\n', length(gains));
            fprintf('Inter-echo interval: %.2f s\n', delay);

            playAudio(x, fs);

        case 3

            ouvir = 0;

            while ouvir ~= 3

                fprintf('\n----------------------------------------\n');
                fprintf('       AMPLITUDE MODULATION\n');
                fprintf('----------------------------------------\n');
                fprintf('1. Classic AM modulation\n');
                fprintf('2. Tremolo effect\n');
                fprintf('3. Return to the main menu\n');

                ouvir = input('Option? ');

                switch ouvir

                    case 1

                        fprintf('\n--- AM MODULATION ---\n');

                        % Frequência da portadora - Tem de ser inferior a fs/2
                        maxFc = fs/2 - 1;

                        fprintf('The Nyquist frequency is %.0f Hz.\n', fs/2);

                        fc = input(sprintf( 'Choose fc between 20 and %.0f Hz: ', maxFc));

                        while isempty(fc) || fc < 20 || fc >= fs/2
                            fprintf('Invalid frequency.\n');
                            fc = input(sprintf( 'Choose fc between 20 and %.0f Hz: ', maxFc));
                        end

                        % Índice de modulação
                        m = 1.0;

                        t = (0:size(y,1)-1)' / fs;

                        carrier = cos(2*pi*fc*t);

                        % Funciona tanto para mono como para estéreo
                        if size(y,2) == 1
                            y_am = y .* (1 + m*carrier);
                        else
                            y_am = y .* repmat(1 + m*carrier, 1, size(y,2));
                        end

                        y_am = normalizeAudio(y_am);

                        fprintf('Carrier: %.0f Hz\n', fc); %portadora
                        fprintf('Modulation index: %.2f\n', m);

                        playAudio(y_am, fs);

                    case 2

                        fprintf('\n--- TREMOLO ---\n');

                        fm = input( 'Tremolo frequency (1-15 Hz): ');

                        while isempty(fm) || fm < 1 || fm > 15
                            fprintf('Choose a frequency between 1 and 15 Hz.\n');
                            fm = input('Tremolo frequency: ');
                        end

                        % Profundidade bastante evidente
                        depth = 0.85;

                        t = (0:size(y,1)-1)' / fs;

                        envelope = 1 + depth*cos(2*pi*fm*t);

                        if size(y,2) == 1
                            y_tremolo = y .* envelope;
                        else
                            y_tremolo = y .* repmat( envelope, 1, size(y,2));
                        end

                        y_tremolo = normalizeAudio(y_tremolo);

                        fprintf('Frequency: %.2f Hz\n', fm);
                        fprintf('Depth: %.2f\n', depth);

                        playAudio(y_tremolo, fs);


                    case 3
                        fprintf('Returning to the main menu...\n');


                    otherwise
                        fprintf('Invalid option.\n');

                end
            end

        case 4

            efeito = 0;

            while efeito ~= 5

                fprintf('\n========================================\n');
                fprintf('       TIME-VARYING DELAYS\n');
                fprintf('========================================\n');
                fprintf('1. Vibrato\n');
                fprintf('2. Chorus\n');
                fprintf('3. Flanger\n');
                fprintf('4. Doppler\n');
                fprintf('5. Back to the main menu\n');
                fprintf('========================================\n');

                efeito = input('Opção? ');

                switch efeito

                    case 1

                        fprintf('\n--- VIBRATO ---\n');

                        fv = input( 'Vibrato frequency (1-10 Hz): ');

                        while isempty(fv) || fv < 1 || fv > 10
                            fprintf('Choose a value between 1 and 10 Hz.\n');
                            fv = input('Vibrato frequency: ');
                        end

                        D_ms = input('Vibrato depth (1-10 ms): ');

                        while isempty(D_ms) || D_ms < 1 || D_ms > 10
                            fprintf('Choose a depth between 1 and 10 ms.\n');
                            D_ms = input('Depth (ms): ');
                        end

                        D = D_ms / 1000;

                        t = (0:size(y,1)-1)' / fs;

                        % O atraso varia entre -D e +D.
                        delay = D * sin(2*pi*fv*t);

                        % Para um atraso positivo, procuramos uma amostra anterior.
                        td = t - delay;

                        y_vibrato = variableDelay(y, t, td); % função no final do ficheiro (com explicação)

                        y_vibrato = normalizeAudio(y_vibrato);

                        fprintf('Frequency: %.2f Hz\n', fv);
                        fprintf('Depth: %.2f ms\n', D_ms);

                        playAudio(y_vibrato, fs);

                    case 2

                        fprintf('\n--- CHORUS ---\n');

                        fc = input( ...
                            'Modulation frequency (0.2-3 Hz): ');

                        while isempty(fc) || fc < 0.2 || fc > 3
                            fprintf('Choose a value between 0.2 and 3 Hz.\n');
                            fc = input('Modulation frequency        : ');
                        end

                        % Valores propositadamente altos para tornar o chorus claramente audível
                        D0_ms = 18;
                        D1_ms = 8;

                        D0 = D0_ms / 1000;
                        D1 = D1_ms / 1000;

                        t = (0:size(y,1)-1)' / fs;

                        delay = D0 + D1*sin(2*pi*fc*t);

                        td = t - delay;

                        y_delay = variableDelay(y, t, td);

                        % Mistura mais forte
                        wet = 0.80;

                        y_chorus = (1-wet)*y + wet*y_delay;

                        y_chorus = normalizeAudio(y_chorus);

                        fprintf('Average delay: %.1f ms\n', D0_ms);
                        fprintf('Depth: %.1f ms\n', D1_ms);
                        fprintf('Blend of effects: %.0f%%\n', wet*100);

                        playAudio(y_chorus, fs);

                    case 3

                        fprintf('\n--- FLANGER ---\n');

                        ff = input('Flanger frequency (0.1-2 Hz): ');

                        while isempty(ff) || ff < 0.1 || ff > 2
                            fprintf('Choose a value between 0.1 and 2 Hz.\n');
                            ff = input('Flanger frequency: ');
                        end

                        % Flanger usa atrasos muito pequenos
                        D0_ms = 5;
                        D1_ms = 4;

                        D0 = D0_ms / 1000;
                        D1 = D1_ms / 1000;

                        t = (0:size(y,1)-1)' / fs;

                        delay = D0 + D1*sin(2*pi*ff*t);

                        td = t - delay;

                        y_delay = variableDelay(y, t, td);

                        % Mistura forte para tornar as interferências construtivas/destrutivas evidentes
                        wet = 0.90;

                        % Pequena inversão de fase ajuda a destacar
                        % o caráter "jet plane" do flanger.
                        y_flanger = y + wet*y_delay;

                        y_flanger = normalizeAudio(y_flanger);

                        fprintf('Average delay: %.1f ms\n', D0_ms);
                        fprintf('Depth: %.1f ms\n', D1_ms);
                        fprintf('Intensity of the effect: %.0f%%\n', wet*100);

                        playAudio(y_flanger, fs);

                    case 4

                        fprintf('\n--- DOPPLER ---\n');

                        t = (0:size(y,1)-1)' / fs;

                        T = t(end);

                        if T <= 0
                            fprintf('Audio too short to apply Doppler.\n');
                            continue;
                        end

                        % Atraso médio
                        D0 = 0.040;      % 40 ms

                        % Variação do atraso
                        D1 = 0.025;      % 25 ms

                        % Um ciclo completo de aproximação + afastamento
                        delay = D0 + D1*sin(2*pi*t/T);

                        td = t - delay;

                        y_doppler = variableDelay(y, t, td);

                        y_doppler = normalizeAudio(y_doppler);

                        fprintf('Average delay: %.0f ms\n', D0*1000);
                        fprintf('Variação máxima: %.0f ms\n', D1*1000);

                        playAudio(y_doppler, fs);

                    case 5

                        fprintf('Returning to the main menu...\n');


                    otherwise

                        fprintf('Invalid option.\n');

                end
            end

        case 5

            fprintf('\nProgram finished.\n');

        otherwise

            fprintf('\nInvalid option. Choose a number between 1 and 5.\n');

    end
end


% FUNÇÕES
function x = addEcho(y, fs, delay, gain)

    % Número de amostras correspondente ao atraso
    delaySamples = round(delay * fs);

    % Criar espaço para o áudio original + eco
    x = zeros(size(y,1) + delaySamples, size(y,2));

    % Áudio original
    x(1:size(y,1), :) = y;

    % Eco atrasado
    x(delaySamples+1:end, :) = x(delaySamples+1:end, :) + gain*y;

    % Normalizar
    x = normalizeAudio(x);

end


function x = addMultipleEchoes(y, fs, delay, gains)

    delaySamples = round(delay * fs);

    % Comprimento necessário para incluir todos os ecos
    totalDelay = delaySamples * length(gains);

    x = zeros(size(y,1) + totalDelay, size(y,2));

    % Áudio original
    x(1:size(y,1), :) = y;

    % Adicionar cada eco
    for k = 1:length(gains)

        startSample = k*delaySamples + 1;
        endSample = startSample + size(y,1) - 1;

        x(startSample:endSample, :) = x(startSample:endSample, :) + gains(k)*y;

    end

    x = normalizeAudio(x);

end

%interpolação linear, que é a base de muitos efeitos de áudio, como os usados no ficheiro na opção 4 do menu
function y_out = variableDelay(y, t, td) % serve para calcular o valor nos instantes td,
% mesmo que esse instante não exista na amostragem original (e desde que esteja entre os valores de t)

    y_out = zeros(size(y));

    for channel = 1:size(y,2)

        y_out(:,channel) = interp1(t, y(:,channel),td,'linear',0);

    end

end

% garantir que nenhuma amostra ultrapassa a amplitude máxima de +-1
function x = normalizeAudio(x)

    maxValue = max(abs(x), [], 'all');

    if maxValue > 1

        x = x / maxValue;

    end

end


function playAudio(x, fs)

    % Garantir que o áudio não ultrapassa [-1,1]
    x = normalizeAudio(x);

    player = audioplayer(x, fs);

    playblocking(player);

end

