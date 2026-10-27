"""Makes the game's music and background sounds and keeps them small: OGG Vorbis (the format raylib plays), the music
at quality 4 (~128 kbit/s), the backgrounds at quality 3.

    music      tools/music.json -> the AudioGenerator's soundtrack mode (ACE-Step 1.5) -> work/music, then each cue
               levelled to the same loudness (the same gain for all its layers and takes, so the layers keep their
               balance) -> assets/music/<cue>_<layer>_t<take>.ogg
    ambience   tools/ambience.txt -> TextureGen (Stable Audio Open) -> work/ambience, then the steadier of two takes cut
               into a seamless loop -> assets/sound/amb_<name>.ogg

    G:\\Tools\\PythonSkripts\\AudioGenerator\\venv\\Scripts\\python.exe tools\\make_audio.py music [--encode-only]
    G:\\Tools\\PythonSkripts\\AudioGenerator\\venv\\Scripts\\python.exe tools\\make_audio.py ambience [--encode-only]

Both generators run below normal priority. Needs numpy, scipy and soundfile (the AudioGenerator's venv has them) and
ffmpeg with libvorbis on the PATH.
"""
import argparse
import json
import os
import subprocess
import sys
import tempfile

import numpy as np
import soundfile as sf
from scipy.signal import lfilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
AUDIOGEN = r"G:\Tools\PythonSkripts\AudioGenerator"
TEXTUREGEN = r"G:\Tools\VRAudio\AmbientSynth\Tools\TextureGen"

# Measured against the game's own sounds (--dumpsound): the effects sit at about -19 LUFS, the synthesised beds at
# -27 (Velm's hum) to -35 (the suit). The music stays a step below the effects, the recorded beds where the hum was.
# Each layer its own loudness, so the music grows with the fight even where the covers came out alike.
MUSIC_LUFS = {"low": -27.0, "mid": -23.5, "high": -20.0, "fight": -21.0, "rage": -19.5, "main": -20.0}
AMBIENCE_LUFS = -29.0    # beds under everything
PEAK_DB = -1.0           # no sample above this after the gain
AMBIENCE_XFADE = 4.0     # seconds of the loop's seam


def kweight(x, fs):
    """BS.1770 K-weighting: a high shelf (+4 dB above ~1.7 kHz) and a high pass at ~38 Hz."""
    g, q, fc = 3.999843853973347, 0.7071752369554196, 1681.974450955533
    a = 10.0 ** (g / 40.0)
    w0 = 2.0 * np.pi * fc / fs
    alpha = np.sin(w0) / (2.0 * q)
    c, s = np.cos(w0), 2.0 * np.sqrt(a) * alpha
    b1 = [a * ((a + 1) + (a - 1) * c + s), -2 * a * ((a - 1) + (a + 1) * c), a * ((a + 1) + (a - 1) * c - s)]
    a1 = [(a + 1) - (a - 1) * c + s, 2 * ((a - 1) - (a + 1) * c), (a + 1) - (a - 1) * c - s]
    q, fc = 0.5003270373238773, 38.13547087602444
    w0 = 2.0 * np.pi * fc / fs
    alpha = np.sin(w0) / (2.0 * q)
    b2 = [1.0, -2.0, 1.0]
    a2 = [1.0 + alpha, -2.0 * np.cos(w0), 1.0 - alpha]
    return lfilter(b2, a2, lfilter(b1, a1, x, axis=0), axis=0)


def lufs(x, fs):
    """Integrated loudness (BS.1770-4: 400 ms blocks, 75 % overlap, absolute and relative gate)."""
    y = kweight(x, fs)
    block, hop = int(0.4 * fs), int(0.1 * fs)
    if len(y) < block:
        block = hop = len(y)
    power = np.array([np.mean(y[i:i + block] ** 2, axis=0).sum() for i in range(0, len(y) - block + 1, hop)])
    loud = -0.691 + 10.0 * np.log10(np.maximum(power, 1e-12))
    power = power[loud > -70.0]
    if not len(power):
        return -70.0
    rel = -0.691 + 10.0 * np.log10(power.mean()) - 10.0
    gated = power[-0.691 + 10.0 * np.log10(power) > rel]
    return -0.691 + 10.0 * np.log10(gated.mean())


def peak_db(x):
    return 20.0 * np.log10(max(1e-9, float(np.max(np.abs(x)))))


def encode(data, fs, path, quality):
    """Writes OGG Vorbis through ffmpeg and checks the decoded length (a loop must stay sample-exact)."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    fd, tmp = tempfile.mkstemp(suffix=".wav")
    os.close(fd)
    try:
        sf.write(tmp, data.astype(np.float32), fs, subtype="FLOAT")
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", tmp, "-c:a", "libvorbis", "-q:a", str(quality),
                        path], check=True)
    finally:
        os.remove(tmp)
    frames = sf.info(path).frames
    if frames != len(data):
        print("  warning: %s decodes to %d frames, not %d" % (os.path.basename(path), frames, len(data)))


def nice_run(cmd, cwd, log):
    """Runs a generator below normal priority (the machine stays usable), its output into a log file."""
    env = dict(os.environ, PYTHONUTF8="1")
    flags = 0x00004000 if os.name == "nt" else 0   # BELOW_NORMAL_PRIORITY_CLASS
    with open(log, "w", encoding="utf-8") as out:
        r = subprocess.run(cmd, cwd=cwd, env=env, stdout=out, stderr=subprocess.STDOUT, creationflags=flags)
    if r.returncode:
        sys.exit("generator failed (%d), see %s" % (r.returncode, log))


def music(encode_only):
    spec = os.path.join(ROOT, "tools", "music.json")
    work = os.path.join(ROOT, "work", "music")
    os.makedirs(work, exist_ok=True)
    if not encode_only:
        nice_run([os.path.join(AUDIOGEN, "venv", "Scripts", "python.exe"), "main.py", "--soundtrack", spec,
                  "--soundtrack-out", work], AUDIOGEN, os.path.join(work, "gen.log"))
    found = [d for d in os.listdir(work) if os.path.isfile(os.path.join(work, d, "manifest.json"))]
    if not found:
        sys.exit("no manifest.json under " + work)
    src = os.path.join(work, found[0])
    manifest = json.load(open(os.path.join(src, "manifest.json"), encoding="utf-8"))
    out = os.path.join(ROOT, "assets", "music")
    total = 0
    # The game's list: each piece, how it plays, its takes, its layers from calm to fierce.
    rank = {"low": 0, "main": 0, "fight": 0, "mid": 1, "rage": 1, "high": 2}
    listing = ["# Made by tools/make_audio.py from tools/music.json: piece, kind, takes, layers (calm to fierce)."]
    for cue in manifest["cues"]:
        layers = sorted((lv["id"] for lv in cue["levels"]), key=lambda i: rank.get(i, 9))
        takes = 1 if cue["type"] == "oneshot" else len(cue["levels"][0]["files"])
        listing.append("%s %s %d %s" % (cue["id"], cue["type"], takes, " ".join(layers)))
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "music.txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(listing) + "\n")
    for cue in manifest["cues"]:
        # Oneshots (the act's end, the loss) need one take; loops alternate theirs, stingers pick one.
        keep = 1 if cue["type"] == "oneshot" else None
        files = [(lv["id"], n + 1, f) for lv in cue["levels"] for n, f in enumerate(lv["files"])
                 if keep is None or n < keep]
        shown = []
        for layer, take, f in files:
            x, fs = sf.read(os.path.join(src, f), dtype="float32", always_2d=True)
            loud = lufs(x, fs)
            gain = min(MUSIC_LUFS.get(layer, -20.0) - loud, PEAK_DB - peak_db(x))
            path = os.path.join(out, "%s_%s_t%d.ogg" % (cue["id"], layer, take))
            encode(x * 10.0 ** (gain / 20.0), fs, path, 4)
            total += os.path.getsize(path)
            shown.append("%s%d %+.1f" % (layer, take, gain))
        print("%-14s %-8s %s" % (cue["id"], cue["type"], ", ".join(shown)))
    print("music: %.1f MB" % (total / 1e6))


def steadiness(x, fs):
    """How even a background is: the spread of its loudness in 1 s windows (lower is steadier)."""
    win = fs
    rms = [np.sqrt(np.mean(x[i:i + win] ** 2)) for i in range(0, len(x) - win + 1, win // 2)]
    rms = 20.0 * np.log10(np.maximum(rms, 1e-9))
    return float(np.std(rms))


def ambience(encode_only):
    prompts = os.path.join(ROOT, "tools", "ambience.txt")
    work = os.path.join(ROOT, "work", "ambience")
    os.makedirs(work, exist_ok=True)
    if not encode_only:
        nice_run([os.path.join(TEXTUREGEN, ".venv", "Scripts", "python.exe"), "texturegen_worker.py", "--batch",
                  prompts, "--model", "sao", "--seconds", "45", "--count", "2", "--seed", "77", "--out-dir", work,
                  "--negative", "music, melody, singing, speech, talking", "--resume"],
                 TEXTUREGEN, os.path.join(work, "gen.log"))
    names = [line.split("name=")[1].split()[0] for line in open(prompts, encoding="utf-8")
             if "name=" in line and not line.lstrip().startswith("#")]
    out = os.path.join(ROOT, "assets", "sound")
    total = 0
    for name in names:
        takes = sorted(f for f in os.listdir(work) if f.startswith(name + "_") and f.endswith(".wav"))
        if not takes:
            print("%-8s missing" % name)
            continue
        loaded = [(f,) + sf.read(os.path.join(work, f), dtype="float32", always_2d=True) for f in takes]
        f, x, fs = min(loaded, key=lambda t: steadiness(t[1], t[2]))
        # Off the first half second and the last second (the model eases in and out), then the end laid over the
        # start with an equal-power fade: the loop's seam.
        x = x[int(0.5 * fs):len(x) - fs]
        n = int(AMBIENCE_XFADE * fs)
        t = np.linspace(0.0, 1.0, n, dtype=np.float32)[:, None]
        loop = x[:len(x) - n].copy()
        loop[:n] = x[:n] * np.sin(t * np.pi / 2) + x[len(x) - n:] * np.cos(t * np.pi / 2)
        gain = min(AMBIENCE_LUFS - lufs(loop, fs), PEAK_DB - peak_db(loop))
        path = os.path.join(out, "amb_%s.ogg" % name)
        encode(loop * 10.0 ** (gain / 20.0), fs, path, 3)
        total += os.path.getsize(path)
        print("%-8s %s, %.1f s, steadiness %.2f dB, gain %+.1f dB" % (name, f, len(loop) / fs,
                                                                      steadiness(x, fs), gain))
    print("ambience: %.1f MB" % (total / 1e6))


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("what", choices=["music", "ambience"])
    ap.add_argument("--encode-only", action="store_true", help="skip the generator, only level and encode")
    a = ap.parse_args()
    (music if a.what == "music" else ambience)(a.encode_only)
