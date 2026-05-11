import math
import wave
import struct

def generate_laser(filename="laser.wav"):
    sample_rate = 44100
    duration = 0.4  # secunde
    num_samples = int(sample_rate * duration)
    
    f_start = 1500.0  # Frecvența de start (înaltă)
    f_end = 200.0     # Frecvența de final (joasă)
    
    samples = []
    phase = 0.0
    
    for i in range(num_samples):
        t = i / sample_rate
        
        # Calculăm frecvența curentă (scade exponențial pentru un sunet mai "smooth")
        # Interpolare geometrică între f_start și f_end
        freq = f_start * (f_end / f_start) ** (i / num_samples)
        
        # Generăm valoarea sinusoidei
        sample = math.sin(phase)
        
        # Actualizăm faza în funcție de frecvența curentă
        phase += 2 * math.pi * freq / sample_rate
        
        # Aplicăm un envelope de volum (fade out) ca să nu pocnească la final
        envelope = math.exp(-6.0 * t)
        
        # Convertim la 16-bit signed integer (-32768 la 32767)
        val = int(sample * envelope * 32767)
        samples.append(val)

    # Scriem fișierul WAV
    with wave.open(filename, 'w') as f:
        f.setnchannels(1)      # Mono
        f.setsampwidth(2)      # 2 bytes (16-bit)
        f.setframerate(sample_rate)
        
        # Impachetăm datele în format binar (short integers)
        binary_data = struct.pack('<' + ('h' * len(samples)), *samples)
        f.writeframes(binary_data)
    
    print(f"Gata! Fișierul '{filename}' a fost generat folosind doar Python standard.")

if __name__ == "__main__":
    generate_laser()
