import argparse
import sys
import time
from pathlib import Path
import numpy as np
import onnxruntime as ort
import soundfile as sf

SAMPLE_RATE = 44100
SEGMENT_S = 7.8
N_SAMPLES = int(SEGMENT_S * SAMPLE_RATE)
N_CHANNELS = 2
SOURCES = ["drums", "bass", "other", "vocals"]

def _make_transition_window(segment: int, overlap_frac: float = 0.25) -> np.ndarray:
    transition = int(segment * overlap_frac)
    window = np.ones(segment, dtype=np.float32)
    fade = np.linspace(0, 1, transition, dtype=np.float32)
    window[:transition] = fade
    window[-transition:] = fade[::-1]
    return window

def _load_sessions(onnx_files: dict, providers: list) -> dict:
    sessions = {}
    for stem, path in onnx_files.items():
        if not path.exists():
            print(f"Error: Missing {stem} model at {path}")
            sys.exit(1)
        sessions[stem] = ort.InferenceSession(str(path), providers=providers)
    return sessions

def separate(mix: np.ndarray, sample_rate: int, onnx_files: dict, providers: list) -> dict:
    print("Separating vocals, bass, drums, other...") # Innesca il 55% nel C++
    
    sessions = _load_sessions(onnx_files, providers)
    total_len = mix.shape[1]
    overlap = N_SAMPLES // 4
    stride = N_SAMPLES - overlap
    n_chunks = max(1, (total_len + stride - 1) // stride)
    
    window = _make_transition_window(N_SAMPLES)
    out = {stem: np.zeros((N_CHANNELS, total_len), dtype=np.float32) for stem in SOURCES}
    weight = np.zeros(total_len, dtype=np.float32)
    
    for i in range(n_chunks):
        start = i * stride
        end = min(start + N_SAMPLES, total_len)
        chunk = mix[:, start:end]
        if chunk.shape[1] < N_SAMPLES:
            chunk = np.pad(chunk, ((0, 0), (0, N_SAMPLES - chunk.shape[1])), mode="constant")
        x = chunk[np.newaxis, ...].astype(np.float32)
        chunk_len = end - start
        w = window[:chunk_len]
        
        for stem in SOURCES:
            stems = sessions[stem].run(["stems"], {"mix": x})[0][0]
            target_row = SOURCES.index(stem)
            out[stem][:, start:end] += stems[target_row, :, :chunk_len] * w
        weight[start:end] += w
        
    weight = np.maximum(weight, 1e-8)
    for stem in SOURCES:
        out[stem] /= weight
        
    return out

def main():
    # Riceve l'esatta sintassi inviata dal plugin JUCE C++
    ap = argparse.ArgumentParser()
    ap.add_argument("command", type=str) # Cattura la parola 'separate'
    ap.add_argument("input", type=Path)
    ap.add_argument("out_dir", type=Path)
    ap.add_argument("--model", type=str, default="")
    ap.add_argument("--providers", type=str, default="CPUExecutionProvider")
    ap.add_argument("--cache-dir", type=Path, required=True)
    ap.add_argument("--shifts", type=int, default=1) # Ignorato dall'algoritmo numpy ma accettato per non far crashare C++
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    args.out_dir.mkdir(parents=True, exist_ok=True)

    # Trova automaticamente i 4 file ONNX all'interno della cache-dir passata da C++
    onnx_files = {}
    for stem in SOURCES:
        # Cerca un file che contiene il nome dello strumento (es. 'drums')
        found = list(args.cache_dir.glob(f"*{stem}*.onnx"))
        if found:
            onnx_files[stem] = found[0]
        else:
            print(f"Failed to find *{stem}*.onnx in {args.cache_dir}")
            sys.exit(1)

    audio, sr = sf.read(str(args.input), dtype="float32", always_2d=True)
    audio = audio.T
    if audio.shape[0] == 1:
        audio = np.tile(audio, (2, 1))
    elif audio.shape[0] > 2:
        audio = audio[:2]

    # Mappa i provider per ONNX
    providers_list = ["CPUExecutionProvider"]
    if args.providers.lower() == "auto":
        providers_list = ort.get_available_providers() # Usa GPU se disponibile

    stems = separate(audio, sr, onnx_files, providers_list)

    print("Saving stems...") # Innesca l'88% nel C++
    for stem, audio_out in stems.items():
        out_path = args.out_dir / f"{stem}.wav"
        sf.write(str(out_path), audio_out.T, sr)

if __name__ == "__main__":
    main()
