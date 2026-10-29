"""Makes chosen pieces of tools/music.json again with more takes and keeps the darkest: ACE-Step drifts to major keys
and bright moods, so a piece gets several candidates and a listening model (CLAP) picks among them and the takes it
had before.

    make   the pieces with --candidates takes each -> work/music_redo (the AudioGenerator's soundtrack mode, below
           normal priority, on the GPU)
    pick   every take of a piece (the old ones in work/music and the new ones) heard by CLAP on the CPU: how dark it
           sounds against how bright, how far it strays to other genres, whether it stays in the asked mode; the best
           go into work/music as t1, t2 and the manifest takes them with the piece's present hash

    G:\\Tools\\PythonSkripts\\AudioGenerator\\venv\\Scripts\\python.exe tools\\redo_music.py make title velm ...
    G:\\Tools\\PythonSkripts\\AudioGenerator\\venv\\Scripts\\python.exe tools\\redo_music.py pick title velm ...

then tools/make_audio.py music --encode-only levels and encodes what changed. A layer of a take belongs to its take
(the covers share its cut), so a take is chosen with all its layers.
"""
import argparse
import hashlib
import json
import os
import shutil
import sys

import numpy as np
import soundfile as sf
from scipy.signal import resample_poly, stft

from make_audio import AUDIOGEN, ROOT, lufs, nice_run

SPEC = os.path.join(ROOT, "tools", "music.json")
MAIN = os.path.join(ROOT, "work", "music", "dark_concordance")
REDO = os.path.join(ROOT, "work", "music_redo")
SR = 48000
WIN = 10.0
HEAR_LUFS = -20.0   # CLAP hears a quiet file as brighter, so every file is heard at one loudness

DARK = ["dark, threatening, gloomy music", "bright, happy, carefree music"]
FIT = ["dark science fiction film score", "dark ambient space drone music", "epic orchestral battle music with choir",
       "tense suspenseful thriller score", "dark industrial electronic music", "sad melancholic piano and strings",
       "horror movie soundtrack, dread", "eerie alien soundscape", "noir jazz at night, melancholic"]
OFF = ["cheerful upbeat pop song", "happy bright children's music", "country music", "smooth lounge jazz, relaxed",
       "reggae", "EDM dance club track", "lo-fi hip hop beat", "rock band with electric guitar", "christmas music",
       "corporate background music, positive", "video game chiptune"]

# Krumhansl-Kessler key profiles
MAJOR = np.array([6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88])
MINOR = np.array([6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17])


def mode_of(x):
    """'major' or 'minor', whichever key profile fits the whole file's chroma best."""
    f, _, z = stft(x, SR, nperseg=8192, noverlap=4096)
    mag = np.abs(z).mean(axis=1)
    chroma = np.zeros(12)
    for fi, m in zip(f, mag):
        if 55.0 <= fi <= 2000.0:
            chroma[int(round(12 * np.log2(fi / 261.6256))) % 12] += m
    fits = [(np.corrcoef(np.roll(p, r), chroma)[0, 1], mode) for r in range(12)
            for p, mode in ((MAJOR, "major"), (MINOR, "minor"))]
    return max(fits)[1]


class Ear:
    """CLAP on the CPU: per file how dark it sounds and how much of it other genres claim."""

    def __init__(self):
        os.environ.setdefault("HF_HUB_OFFLINE", "1")
        import torch
        from transformers import ClapModel, ClapProcessor
        torch.set_num_threads(6)
        self.torch = torch
        self.model = ClapModel.from_pretrained("laion/clap-htsat-unfused").eval()
        self.proc = ClapProcessor.from_pretrained("laion/clap-htsat-unfused")
        with torch.no_grad():
            t = self.model.get_text_features(**self.proc(text=DARK + FIT + OFF, return_tensors="pt", padding=True))
        self.text = t / t.norm(dim=-1, keepdim=True)
        self.scale = float(self.model.logit_scale_a.exp())

    def hear(self, path):
        x, fs = sf.read(path, dtype="float32", always_2d=True)
        x = x.mean(axis=1)
        if fs != SR:
            x = resample_poly(x, SR, fs).astype(np.float32)
        x = x * np.float32(10.0 ** ((HEAR_LUFS - lufs(x, SR)) / 20.0))
        n = int(WIN * SR)
        starts = [int(len(x) * p) for p in (0.1, 0.45, 0.8)] if len(x) > 2 * n else [0]
        waves = [x[s:s + n] for s in starts if len(x[s:s + n]) > SR]
        torch = self.torch
        with torch.no_grad():
            try:
                inp = self.proc(audio=waves, sampling_rate=SR, return_tensors="pt", padding=True)
            except TypeError:
                inp = self.proc(audios=waves, sampling_rate=SR, return_tensors="pt", padding=True)
            a = self.model.get_audio_features(**inp)
            a = a / a.norm(dim=-1, keepdim=True)
            sims = (a @ self.text.T).mean(dim=0)
        dark = float(torch.softmax(sims[:2] * self.scale, dim=0)[0])
        genres = torch.softmax(sims[2:] * self.scale, dim=0).numpy()
        return {"dark": round(dark, 3), "off": round(float(genres[len(FIT):].sum()), 3), "mode": mode_of(x)}


def make(cues, candidates):
    spec = json.load(open(SPEC, encoding="utf-8"))
    missing = set(cues) - {c["id"] for c in spec["cues"]}
    if missing:
        sys.exit("not in music.json: " + ", ".join(sorted(missing)))
    sub = dict(spec, name="redo", takes=candidates, cues=[c for c in spec["cues"] if c["id"] in cues])
    os.makedirs(REDO, exist_ok=True)
    path = os.path.join(REDO, "spec.json")
    with open(path, "w", encoding="utf-8") as f:
        json.dump(sub, f, indent=2, ensure_ascii=False)
    nice_run([os.path.join(AUDIOGEN, "venv", "Scripts", "python.exe"), "main.py", "--soundtrack", path,
              "--soundtrack-out", REDO, "--force"], AUDIOGEN, os.path.join(REDO, "gen.log"))


def score(heard, want):
    """Dark on every layer counts, a bright layer counts against it, other genres too; the asked mode is a bonus."""
    darks = [h["dark"] for h in heard]
    s = float(np.mean(darks)) - max(0.0, 0.5 - min(darks)) - 0.3 * float(np.mean([h["off"] for h in heard]))
    return s + (0.1 if heard[0]["mode"] == want else 0.0)


def pick(cues):
    sys.path.insert(0, AUDIOGEN)
    from core import soundtrack
    spec = soundtrack.normalize(json.load(open(SPEC, encoding="utf-8")))
    main = json.load(open(os.path.join(MAIN, "manifest.json"), encoding="utf-8"))
    redo = json.load(open(os.path.join(REDO, "redo", "manifest.json"), encoding="utf-8"))
    ear = Ear()
    report = {}
    for cid in cues:
        cue = next(c for c in spec["cues"] if c["id"] == cid)
        want = (cue.get("key") or spec["key"]).split()[1]
        layers = [lv["id"] for lv in cue["levels"]]
        found, seen = [], set()
        for where, man in ((MAIN, main), (os.path.join(REDO, "redo"), redo)):
            entry = next((c for c in man["cues"] if c["id"] == cid), None)
            if entry is None:
                continue
            files = {lv["id"]: lv["files"] for lv in entry["levels"]}
            for n, info in enumerate(entry["takes"]):
                paths = [os.path.join(where, files[lv][n]) for lv in layers]
                # a take picked before sits in both folders: hear it once
                digest = hashlib.sha1(open(paths[0], "rb").read()).hexdigest()
                if digest in seen:
                    continue
                seen.add(digest)
                heard = [ear.hear(p) for p in paths]
                found.append({"paths": paths, "info": info, "heard": heard, "score": round(score(heard, want), 3),
                              "old": where == MAIN})
        found.sort(key=lambda c: -c["score"])
        takes = int(spec["takes"])
        chosen = found[:takes]
        # Copy into a side folder first: an old t2 may become the new t1.
        stage = os.path.join(REDO, "stage")
        os.makedirs(stage, exist_ok=True)
        plan = []
        for k, c in enumerate(chosen, start=1):
            for lv, src in zip(layers, c["paths"]):
                dst = os.path.join(MAIN, "%s_%s_t%d.flac" % (cid, lv, k))
                if os.path.normcase(src) != os.path.normcase(dst):
                    tmp = os.path.join(stage, os.path.basename(dst))
                    shutil.copyfile(src, tmp)
                    plan.append((tmp, dst))
        for tmp, dst in plan:
            os.replace(tmp, dst)
        entry = {"id": cid, "type": cue["type"], "hash": soundtrack.cue_hash(spec, cue),
                 "bpm": float(cue.get("bpm") or spec["bpm"]), "timesig": str(cue.get("timesig") or spec["timesig"]),
                 "key": cue.get("key") or spec["key"],
                 "levels": [{"id": lv, "files": ["%s_%s_t%d.flac" % (cid, lv, k) for k in range(1, takes + 1)]}
                            for lv in layers],
                 "takes": [dict(c["info"], take=k) for k, c in enumerate(chosen, start=1)]}
        # a new piece joins the manifest in the order of music.json
        order = {c["id"]: i for i, c in enumerate(spec["cues"])}
        main["cues"] = sorted([c for c in main["cues"] if c["id"] != cid] + [entry],
                              key=lambda c: order.get(c["id"], len(order)))
        report[cid] = [{"take": os.path.basename(c["paths"][0]), "old": c["old"], "score": c["score"],
                        "heard": c["heard"], "chosen": c in chosen} for c in found]
        print("%-20s %s" % (cid, "  ".join("%s%s %.2f%s" % ("old " if c["old"] else "", os.path.basename(c["paths"][0]),
                                                           c["score"], " *" if c in chosen else "") for c in found)),
              flush=True)
        tmp = os.path.join(MAIN, "manifest.json.tmp")
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(main, f, indent=2, ensure_ascii=False)
        os.replace(tmp, os.path.join(MAIN, "manifest.json"))
    with open(os.path.join(REDO, "pick.json"), "w", encoding="utf-8") as f:
        json.dump(report, f, indent=1)


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("what", choices=["make", "pick"])
    ap.add_argument("cues", nargs="+", help="piece ids from tools/music.json")
    ap.add_argument("--candidates", type=int, default=4, help="new takes per piece (make)")
    a = ap.parse_args()
    make(a.cues, a.candidates) if a.what == "make" else pick(a.cues)
