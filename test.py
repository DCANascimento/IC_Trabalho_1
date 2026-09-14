from scipy.io import wavfile
import matplotlib.pyplot as plt
import numpy as np

sample_rate, audio = wavfile.read("Audio Files/sample01.wav")

# Determine how many channels
print("Shape:", audio.shape)

left = audio[:, 0]
right = audio[:, 1]

time = np.arange(len(audio)) / sample_rate

figure, axes = plt.subplots(2, 2)

axes[0, 0].plot(time, left)
axes[0, 0].set_ylabel("Amplitude")
axes[0, 0].set_xlabel("Time (seconds)")
axes[0, 0].set_title("Left Channel")

axes[1, 0].plot(time, right)
axes[1, 0].set_ylabel("Amplitude")
axes[1, 0].set_xlabel("Time (seconds)")
axes[1, 0].set_title("Right Channel")

# --------------------

mid = (left + right) / 2
side = (left - right) / 2

axes[0, 1].plot(time, mid)
axes[0, 1].set_ylabel("Amplitude")
axes[0, 1].set_xlabel("Time (seconds)")
axes[0, 1].set_title("MID Channel")

axes[1, 1].plot(time, side)
axes[1, 1].set_ylabel("Amplitude")
axes[1, 1].set_xlabel("Time (seconds)")
axes[1, 1].set_title("SIDE Channel")

figure.tight_layout()
plt.show()
