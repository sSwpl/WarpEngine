import struct, wave, math, os

os.makedirs('assets/sfx', exist_ok=True)

SAMPLE_RATE = 22050

def gen_tone(filename, freq, duration, volume=0.5, fade_out=True):
    n_samples = int(SAMPLE_RATE * duration)
    data = []
    for i in range(n_samples):
        t = i / SAMPLE_RATE
        val = math.sin(2 * math.pi * freq * t) * volume
        if fade_out:
            val *= max(0, 1.0 - (i / n_samples))
        sample = int(val * 32767)
        sample = max(-32768, min(32767, sample))
        data.append(struct.pack('<h', sample))
    with wave.open(filename, 'w') as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(SAMPLE_RATE)
        wf.writeframes(b''.join(data))

def gen_chime(filename, freqs, duration_each=0.1, volume=0.4):
    """Ascending chime - multiple tones in sequence"""
    data = []
    for freq in freqs:
        n_samples = int(SAMPLE_RATE * duration_each)
        for i in range(n_samples):
            t = i / SAMPLE_RATE
            val = math.sin(2 * math.pi * freq * t) * volume
            val *= max(0, 1.0 - (i / n_samples) * 0.5)
            sample = int(val * 32767)
            data.append(struct.pack('<h', max(-32768, min(32767, sample))))
    with wave.open(filename, 'w') as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(SAMPLE_RATE)
        wf.writeframes(b''.join(data))

# Shoot: short high blip
gen_tone('assets/sfx/shoot.wav', 880, 0.08, 0.3)

# Hit: low thud
gen_tone('assets/sfx/hit.wav', 200, 0.12, 0.4)

# Collect: ascending chime
gen_chime('assets/sfx/collect.wav', [523, 659, 784], 0.08, 0.3)

# Level Up: fanfare
gen_chime('assets/sfx/levelup.wav', [523, 659, 784, 1047], 0.15, 0.5)

# Death: descending
gen_chime('assets/sfx/death.wav', [784, 523, 330, 220], 0.2, 0.5)

print("Generated SFX in assets/sfx/")
