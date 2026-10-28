"""Speaks the game's lines with Qwen3-TTS (local, G:\\Tools\\VoiceLab): every line of [lines] and [talk] in
assets/levels/*.txt and of the films (tools/films.json), per language, in the voice of its speaker (tools/voices.json).

    design   a voice per figure and language from its description (VoiceDesign 1.7B), spoken on a few of its own
             lines -> tools/voices/<figure>_<language>.flac and .txt (kept, so the figures keep their voices for lines
             written later); all of them one after the other -> work/voice/voices_<language>.wav, to listen to
    speak    every line cloned from that voice (Base 1.7B) -> work/voice/<language>/<file>.wav, again when its length
             does not fit its text (the model rambles or breaks off now and then)
    encode   the figure's effects (radio, machine, pa), all at one loudness, OGG Vorbis ->
             assets/voice/<language>/<file>.ogg

The file is the line's key, with ".<hero>" where the hero speaks (each of the three has their own voice) or where the
line has a text of its own for that hero; the game tries "<key>.<hero>" first, then "<key>". Stage directions in
brackets are not spoken; a line of nothing else stays silent. The heroes' answers in conversations stay unspoken.

    set PYTHONNOUSERSITE=1
    G:\\Tools\\VoiceLab\\env-qwen\\python.exe tools\\make_voices.py [--lang de] [--who ysolde ...] [--only KEY ...]
    G:\\Tools\\VoiceLab\\env-qwen\\python.exe tools\\make_voices.py --list

Runs below normal priority. Needs ffmpeg with libvorbis on the PATH (as make_audio.py).
"""
import argparse
import collections
import glob
import json
import os
import re
import sys

import numpy as np
import soundfile as sf
from scipy.signal import butter, lfilter, sosfilt

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_audio  # noqa: E402  (loudness and encoding)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MODELS = r"G:\Tools\VoiceLab\models"
REFS = os.path.join(ROOT, "tools", "voices")
WORK = os.path.join(ROOT, "work", "voice")
LANGS = {"de": "German", "en": "English"}
HEROES = ("corvin", "dorra", "ione")

VOICE_LUFS = -17.0   # a step above the effects (-19), so the lines carry over a fight
QUALITY = 3          # OGG Vorbis, mono 24 kHz: ~50 kbit/s
PEAK_DB = -2.0       # before encoding (Vorbis overshoots by up to a decibel)
BATCH = 16           # lines per generation (sorted by length, so a batch ends together)
CANDIDATES = 4       # takes of a voice with a pitch range (tools/voices.json "pitch")
# Placeholders the game fills in; the voice says something that fits any number.
PLACEHOLDERS = {"de": [("{deaths}-mal", "so oft")], "en": [("{deaths} times", "so many times")]}


def texts(lang):
    t = {}
    for path in glob.glob(os.path.join(ROOT, "assets", "text", lang, "*.txt")):
        for line in open(path, encoding="utf-8"):
            m = re.match(r"^([A-Za-z0-9_.]+)\s*=\s*(.*)$", line.rstrip("\n"))
            if m:
                t[m.group(1)] = m.group(2).strip()
    return t


def clean(text, lang):
    """What is spoken of a text: no stage directions, no quote marks, shouted words in normal case (else the model
    spells them); None if nothing is left or a placeholder stays."""
    for a, b in PLACEHOLDERS.get(lang, []):
        text = text.replace(a, b)
    if "{" in text:
        return None
    text = re.sub(r"\([^)]*\)", " ", text)
    text = re.sub(r"[»«„“”\"]", "", text)
    text = re.sub(r"\b([A-ZÄÖÜ]{4,})\b", lambda m: m.group(1).capitalize(), text)
    text = re.sub(r"\s+", " ", text).strip().lstrip("–—-").strip()
    return text if re.search(r"\w", text) else None


def inventory(voices):
    """Every spoken line: {(language, file): (whose voice, who speaks, text)}."""
    words = {lang: texts(lang) for lang in LANGS}
    out, owner = {}, {}

    def add(key, who, place):
        if owner.setdefault(key, who) != who:
            print("warning: %s is said by %s and %s" % (key, owner[key], who))
        heroic = who == "hero" or voices.get(who, {}).get("voice") == "hero"
        voice = voices.get(who, {}).get("voice", who)
        # A figure may sound different in one place ("in": {level: voice}): Ilka grown up in the net.
        voice = voices.get(who, {}).get("in", {}).get(place, voice)
        for lang in LANGS:
            own, de = words[lang], words["de"]
            said = []
            for h in HEROES:
                variant = key + "." + h
                has = variant in own or variant in de
                if heroic:
                    said.append((variant, h, own.get(variant if has else key)))
                elif has:
                    said.append((variant, voice, own.get(variant)))
            if not heroic:
                said.append((key, voice, own.get(key)))
            for file, v, text in said:
                # A text only in German: the game plays the German voice under it.
                spoken = clean(text, lang) if text and text != "-" else None
                if spoken:
                    out[(lang, file)] = (v, who, spoken)

    for path in sorted(glob.glob(os.path.join(ROOT, "assets", "levels", "*.txt"))):
        section = None
        for line in open(path, encoding="utf-8"):
            t = line.strip()
            if t.startswith("["):
                section = t
                continue
            if not t or t[0] == "#" or section not in ("[lines]", "[talk]"):
                continue
            f = [x.strip() for x in t.split("|")]
            if len(f) >= (3 if section == "[lines]" else 2):
                add(f[0], f[2] if section == "[lines]" else f[1], os.path.basename(path)[:-4])
    # The films' lines (tools/films.json), spoken as a level's are.
    for name, film in json.load(open(os.path.join(ROOT, "tools", "films.json"), encoding="utf-8")).items():
        for shot in film.get("shots", []) if isinstance(film, dict) else []:
            for who, key in shot.get("say", []):
                add(key, who, "film_" + name)
    return out


def design_text(lines):
    """A few of a figure's lines, enough for a voice to show itself (~100-200 characters, 7-15 s)."""
    chosen, n = [], 0
    for t in sorted(set(lines), key=lambda s: -len(s)):
        if n >= 100:
            break
        if n + len(t) <= 200 or not chosen:
            chosen.append(t)
            n += len(t) + 1
    return " ".join(chosen)


def below_normal():
    if os.name == "nt":
        import ctypes
        ctypes.windll.kernel32.SetPriorityClass(ctypes.windll.kernel32.GetCurrentProcess(), 0x00004000)


def plausible(wav, sr, text):
    """Whether a take's length fits its text (German and English run at about 12-16 characters a second)."""
    seconds = len(wav) / sr
    return len(text) / 30.0 + 0.3 <= seconds <= len(text) / 6.0 + 2.0


def design(jobs, voices, langs, whos):
    """A voice per figure and language that has none yet."""
    need = sorted({(v, lang) for (lang, _), (v, _, _) in jobs.items() if lang in langs and (not whos or v in whos)})
    need = [(v, lang) for v, lang in need if not os.path.exists(os.path.join(REFS, "%s_%s.flac" % (v, lang)))]
    if not need:
        return
    import torch
    from qwen_tts import Qwen3TTSModel
    model = Qwen3TTSModel.from_pretrained(os.path.join(MODELS, "Qwen3-TTS-12Hz-1.7B-VoiceDesign"), device_map="cuda:0",
                                          dtype=torch.bfloat16, attn_implementation="sdpa")
    os.makedirs(REFS, exist_ok=True)
    for v, _ in need:
        if "describe" not in voices.get(v, {}):
            print("no description for", v)
    todo = [(v, lang, design_text([t for (l, _), (w, _, t) in jobs.items() if l == lang and w == v]))
            for v, lang in need if "describe" in voices.get(v, {})]
    # A figure with a pitch range gets a few takes, and the one nearest the middle of its range is kept (a description
    # alone sometimes lands an octave off, a loud man as a woman).
    todo = [d for d in todo for _ in range(CANDIDATES if "pitch" in voices[d[0]] else 1)]
    todo.sort(key=lambda d: len(d[2]))
    takes = collections.defaultdict(list)
    for attempt in range(3):
        again = []
        for i in range(0, len(todo), BATCH):
            batch = todo[i:i + BATCH]
            wavs, sr = model.generate_voice_design(text=[d[2] for d in batch],
                                                   instruct=[voices[d[0]]["describe"] for d in batch],
                                                   language=[LANGS[d[1]] for d in batch])
            for (v, lang, text), wav in zip(batch, wavs):
                if plausible(wav, sr, text) or attempt == 2:
                    takes[(v, lang, text)].append((wav, sr))
                else:
                    again.append((v, lang, text))
        todo = again
    for (v, lang, text), got in sorted(takes.items()):
        wav, sr = got[0]
        if "pitch" in voices[v]:
            lo, hi = voices[v]["pitch"]
            heard = [(pitch(w, r), w, r) for w, r in got]
            f0, wav, sr = min(heard, key=lambda h: abs(np.log(h[0] / np.sqrt(lo * hi))))
            print("  %s %s: %s Hz, kept %.0f" % (v, lang, " ".join("%.0f" % h[0] for h in heard), f0))
        sf.write(os.path.join(REFS, "%s_%s.flac" % (v, lang)), wav, sr)
        with open(os.path.join(REFS, "%s_%s.txt" % (v, lang)), "w", encoding="utf-8") as f:
            f.write(text + "\n")
        print("designed", v, lang, flush=True)
    del model
    torch.cuda.empty_cache()


def pitch(wav, sr):
    """The median pitch of a voice (Hz)."""
    import librosa
    x = librosa.resample(np.asarray(wav, dtype=np.float32), orig_sr=sr, target_sr=16000)
    f0, _, _ = librosa.pyin(x, fmin=60, fmax=500, sr=16000, frame_length=1024)
    return float(np.nanmedian(f0)) if np.any(np.isfinite(f0)) else 1.0


def listening_sheet(lang):
    """All voices of a language one after the other (a second of silence between them)."""
    parts, rate, names = [], None, []
    for path in sorted(glob.glob(os.path.join(REFS, "*_%s.flac" % lang))):
        x, sr = sf.read(path, dtype="float32")
        rate = rate or sr
        if sr != rate:
            continue
        x = x * 10.0 ** ((VOICE_LUFS - make_audio.lufs(x, sr)) / 20.0)
        parts += [x, np.zeros(sr, dtype=np.float32)]
        names.append(os.path.basename(path)[:-len("_%s.flac" % lang)])
    if parts:
        os.makedirs(WORK, exist_ok=True)
        sf.write(os.path.join(WORK, "voices_%s.wav" % lang), np.concatenate(parts), rate)
        print("voices_%s.wav: %s" % (lang, ", ".join(names)))


def speak(jobs, langs, whos, only, force):
    """Every line that has no take yet, cloned from its figure's voice."""
    groups = collections.defaultdict(list)
    for (lang, file), (v, who, text) in jobs.items():
        if lang not in langs or (whos and v not in whos and who not in whos) or (only and file not in only):
            continue
        raw = os.path.join(WORK, lang, file + ".wav")
        if force and os.path.exists(raw):
            os.remove(raw)
        if not os.path.exists(raw):
            groups[(v, lang)].append((file, text))
    if not groups:
        return
    import torch
    from qwen_tts import Qwen3TTSModel
    model = Qwen3TTSModel.from_pretrained(os.path.join(MODELS, "Qwen3-TTS-12Hz-1.7B-Base"), device_map="cuda:0",
                                          dtype=torch.bfloat16, attn_implementation="sdpa")
    total = sum(len(g) for g in groups.values())
    done = 0
    for (v, lang), lines in sorted(groups.items()):
        ref = os.path.join(REFS, "%s_%s.flac" % (v, lang))
        if not os.path.exists(ref):
            print("no voice for", v, lang)
            continue
        prompt = model.create_voice_clone_prompt(ref_audio=ref,
                                                 ref_text=open(ref[:-5] + ".txt", encoding="utf-8").read().strip())
        os.makedirs(os.path.join(WORK, lang), exist_ok=True)
        lines.sort(key=lambda l: len(l[1]))
        for i in range(0, len(lines), BATCH):
            todo = lines[i:i + BATCH]
            for attempt in range(4):
                if not todo:
                    break
                said = [t for _, t in todo]
                wavs, sr = model.generate_voice_clone(text=said, language=[LANGS[lang]] * len(said),
                                                      voice_clone_prompt=prompt * len(said),
                                                      max_new_tokens=int(12 * (len(max(said, key=len)) / 6.0 + 4)))
                again = []
                for (file, text), wav in zip(todo, wavs):
                    if plausible(wav, sr, text) or attempt == 3:
                        sf.write(os.path.join(WORK, lang, file + ".wav"), wav, sr)
                        done += 1
                    else:
                        again.append((file, text))
                todo = again
            print("%d/%d  %s %s" % (done, total, v, lang), flush=True)
    del model
    torch.cuda.empty_cache()


def trim(x, sr):
    """Off with the silence before and after (50 ms kept before, 150 ms after)."""
    hop = int(0.01 * sr)
    frames = np.array([np.sqrt(np.mean(x[i:i + hop] ** 2) + 1e-12) for i in range(0, max(1, len(x) - hop), hop)])
    loud = np.where(20.0 * np.log10(frames / frames.max()) > -40.0)[0]
    if not len(loud):
        return x
    return x[max(0, loud[0] * hop - int(0.05 * sr)):min(len(x), (loud[-1] + 1) * hop + int(0.15 * sr))]


def radio(x, sr):
    """A suit radio: telephone band, a little overdriven, a faint hiss."""
    y = sosfilt(butter(2, [300.0, 3400.0], btype="band", fs=sr, output="sos"), x)
    y = np.tanh(3.0 * y / max(1e-6, np.max(np.abs(y)))) / np.tanh(3.0)
    rng = np.random.default_rng(len(x))
    return y + rng.normal(0.0, 0.004, len(y))


def machine(x, sr):
    """A machine's voice: a short comb (metal) and a slow hum in the amplitude."""
    d = int(0.0045 * sr)
    y = lfilter([1.0], np.r_[1.0, np.zeros(d - 1), -0.35], x)
    t = np.arange(len(x)) / sr
    return (0.6 * x + 0.4 * y / max(1e-6, np.max(np.abs(y))) * np.max(np.abs(x))) * (0.9 + 0.1 * np.sin(
        2.0 * np.pi * 50.0 * t))


def pa(x, sr):
    """A loudspeaker in a hall: a narrower band and three echoes."""
    y = sosfilt(butter(2, [180.0, 7000.0], btype="band", fs=sr, output="sos"), x)
    out = np.r_[y, np.zeros(int(0.4 * sr))]
    for delay, gain in ((0.09, 0.28), (0.19, 0.17), (0.32, 0.09)):
        k = int(delay * sr)
        out[k:k + len(y)] += gain * y
    return out


def limit(x, sr, ceiling):
    """A peak limiter: the gain dips around a peak (20 ms, smoothly) instead of the whole line getting quieter."""
    from scipy.ndimage import maximum_filter1d, minimum_filter1d, uniform_filter1d
    w = int(0.02 * sr) | 1
    gain = np.minimum(1.0, 10.0 ** (ceiling / 20.0) / np.maximum(maximum_filter1d(np.abs(x), size=w), 1e-9))
    # The mean over a window of the windowed minimum never rises above the gain a peak needs.
    return x * uniform_filter1d(minimum_filter1d(gain, size=w), size=w)


def encode(jobs, voices, langs, force):
    """The takes with their figure's effects, levelled, as OGG Vorbis."""
    n = 0
    for (lang, file), (v, who, text) in sorted(jobs.items()):
        raw = os.path.join(WORK, lang, file + ".wav")
        out = os.path.join(ROOT, "assets", "voice", lang, file + ".ogg")
        if lang not in langs or not os.path.exists(raw):
            continue
        if not force and os.path.exists(out) and os.path.getmtime(out) >= os.path.getmtime(raw):
            continue
        x, sr = sf.read(raw, dtype="float64")
        x = trim(x, sr)
        for fx in ("machine", "radio", "pa"):
            if fx in voices.get(who, {}).get("fx", []):
                x = globals()[fx](x, sr)
        for _ in range(2):   # the limiter takes a little loudness, the second pass gives it back
            x = limit(x * 10.0 ** (min(12.0, VOICE_LUFS - make_audio.lufs(x, sr)) / 20.0), sr, PEAK_DB)
        make_audio.encode(x.astype(np.float32), sr, out, QUALITY)
        n += 1
    print("encoded", n)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lang", nargs="*", default=list(LANGS), help="only these languages")
    ap.add_argument("--who", nargs="*", default=[], help="only these figures")
    ap.add_argument("--only", nargs="*", default=[], help="only these files (key or key.hero)")
    ap.add_argument("--force", action="store_true", help="speak the chosen lines again")
    ap.add_argument("--list", action="store_true", help="only count the lines per figure")
    ap.add_argument("--design-only", action="store_true", help="only design the missing voices")
    ap.add_argument("--encode-only", action="store_true", help="only encode the takes there are")
    a = ap.parse_args()
    voices = {k: v for k, v in json.load(open(os.path.join(ROOT, "tools", "voices.json"), encoding="utf-8")).items()
              if k != "_"}
    jobs = inventory(voices)
    if a.list:
        count = collections.Counter((lang, v) for (lang, _), (v, _, _) in jobs.items())
        chars = collections.Counter()
        for (lang, _), (v, _, t) in jobs.items():
            chars[(lang, v)] += len(t)
        for lang in a.lang:
            print(lang, sum(n for (l, _), n in count.items() if l == lang), "lines")
            for (l, v), n in count.most_common():
                if l == lang:
                    print("  %-12s %4d lines %6d characters" % (v, n, chars[(l, v)]))
        return
    below_normal()
    if not a.encode_only:
        design(jobs, voices, a.lang, a.who)
        for lang in a.lang:
            listening_sheet(lang)
        if a.design_only:
            return
        speak(jobs, a.lang, a.who, a.only, a.force)
    encode(jobs, voices, a.lang, a.force and a.encode_only)   # a new take is younger than its file anyway


if __name__ == "__main__":
    main()
