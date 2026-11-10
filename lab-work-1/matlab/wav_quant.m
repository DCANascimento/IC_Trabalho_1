
clear;
clc;

% dirs
script_dir = fileparts(mfilename('fullpath'));
root_dir   = fileparts(script_dir);
in_dir     = fullfile(root_dir, 'AudioFiles');
out_dir    = fullfile(root_dir, 'QuantizedFiles');

if ~isfolder(in_dir)
    error("Folder not found: %s", in_dir);
end
if ~isfolder(out_dir)
    mkdir(out_dir);
end

%list available files
files = dir(fullfile(in_dir, '*.wav'));
if isempty(files)
    error("No WAV files found in %s", in_dir);
end
names = sort({files.name});
K = numel(names);

fprintf("=== wav_quant ===\n");
fprintf("Available files:\n");
for k = 1:K
    fprintf("  %2d - %s\n", k, names{k});
end
fprintf("\n");



% menu
N = read_int(sprintf("Choose the file (1-%d, 0 = all, q = quit [or enter]): ", K), 0, K);
if isempty(N), disp("Exiting."); return; end
 
b = read_int("Number of bits to keep (1-16, q = quit [or enter]): ", 1, 16);
if isempty(b), disp("Exiting."); return; end
 
if N == 0
    file_list = 1:K;
else
    file_list = N;
end
 

% processing
step = 2^(16 - b); % size of each interval
fprintf("\nQuantization step: %d (%d levels)\n\n", step, 2^b);

for n = file_list
    in_name = sprintf("sample%02d.wav", n);
    in_path = fullfile(in_dir, in_name);

    if ~isfile(in_path)
        fprintf("  [warning] %s not found, skipping\n", in_name);
        continue;
    end

    info = audioinfo(in_path);
    if info.BitsPerSample ~= 16
        fprintf("  [warning] %s has %d bits (expected 16), skipping\n", ...
                in_name, info.BitsPerSample);
        continue;
    end

    % 'native' -> integer samples (int16), not normalized to [-1, 1]
    [x, fs] = audioread(in_path, 'native');
    x = double(x); % avoid overflow in arithmetic

    % quantization: interval index (same as clearing the 16-b lowest bits)
    q = floor(x / step) * step;

    % reconstruction at the interval midpoint (b = 16 -> step = 1 -> unchanged)
    y = q + floor(step / 2);
    y = min(max(y, -32768), 32767);              % keep within 16-bit range

    out_name = sprintf("sample%02d_%db.wav", n, b);
    audiowrite(fullfile(out_dir, out_name), int16(y), fs, 'BitsPerSample', 16);

    fprintf("  %s -> %s\n", in_name, out_name);
end

%saving
fprintf("\nDone. File(s) saved in: %s\n", out_dir);

% helper function
function v = read_int(prompt, vmin, vmax)
    % returns an integer in [vmin, vmax], or [] if the user wants to quit
    while true
        s = strtrim(input(prompt, 's')); % read as text

        if isempty(s) || strcmpi(s, 'q')
            v = []; 
            return;
        end

        v = str2double(s); % if NaN
        if ~isnan(v) && v == round(v) && v >= vmin && v <= vmax
            return;
        end
        fprintf("  Invalid value. Must be an integer between %d and %d (q or enter to quit).\n", ...
                vmin, vmax);
    end
end