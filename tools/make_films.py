"""Makes the films for the big moments (tools/films.json), all locally: photographic pictures, motion, the cut. One film
serves every language; its voices and subtitles come from the game at playback (assets/voice, assets/text/*/films.txt).

    pictures  a picture per shot, FLUX.2-dev in WSL2 (tools/flux_wsl.py), the speakers' portraits as references for
              faces -> work/film/<film>/s<n>.png and a contact sheet work/film/<film>/sheet.jpg to look at
    motion    each picture set moving, LTX-2.3 distilled through Text2Video's production batch in WSL2
              (~/Projekte/Text2Video/LTX-V, silence as its sound), as long as its lines need (the longer language, the
              longest hero) -> work/film/<film>/s<n>.mp4
    cut       the shots cross-faded, faded in and out, AV1 (SVT-AV1) in IVF, which the game decodes with dav1d ->
              assets/films/<film>.ivf, and the cue list assets/films/<film>.txt (the music cue, when each line is said)

    python tools\\make_films.py pictures|motion|cut|all [--film prolog ...] [--redo prolog:3 ...]

The voices must be spoken first (tools/make_voices.py reads the films' lines too). Pictures and motion run below normal
priority, one model at a time on the GPU; a shot already there is kept (--redo makes it again, picture and motion).
"""
import argparse
import json
import math
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORK = os.path.join(ROOT, "work", "film")
OUT = os.path.join(ROOT, "assets", "films")
HEROES = ("corvin", "dorra", "ione")
LANGS = ("de", "en")
FPS = 24
WIDTH, HEIGHT = 1280, 704       # LTX's two stages need multiples of 64; FLUX paints 1280x720, cut to this
FADE = 0.5                      # cross-fade between shots (s)
LEAD, GAP, TAIL = 0.6, 0.4, 0.8  # before the first line of a shot, between lines, after the last
MAX_SHOT = 10.0                 # LTX holds a picture together for about this long
NEGATIVE = "low quality, worst quality, deformed, distorted, watermark, jump cut, scene change, talking, text"
CRF = 23                        # SVT-AV1's quality (lower is better; the game's decoder is the same either way)
BELOW_NORMAL = 0x00004000 if os.name == "nt" else 0


def films():
    data = json.load(open(os.path.join(ROOT, "tools", "films.json"), encoding="utf-8"))
    return data, {k: v for k, v in data.items() if isinstance(v, dict) and "shots" in v}


def shot_id(shot, n):
    """A shot's name for its files: its "id" (shots added later keep the older ones' files), else s<place>."""
    return shot.get("id", "s%d" % n)


def seed_of(sid):
    """The first shots kept their place as seed; the ones added later count from 101."""
    digits = int("".join(ch for ch in sid if ch.isdigit()) or 0)
    return 1 + digits if sid.startswith("s") else 101 + digits


def shot_path(film, sid, ext):
    return os.path.join(WORK, film, "%s.%s" % (sid, ext))


def linux(path):
    path = os.path.abspath(path).replace("\\", "/")
    return "/mnt/%s%s" % (path[0].lower(), path[2:])


def pictures(data, chosen):
    tasks = []
    for name, film in chosen.items():
        for n, shot in enumerate(film["shots"]):
            sid = shot_id(shot, n)
            refs = [os.path.join(ROOT, "assets", "textures", "portrait_%s.png" % r) for r in shot.get("refs", [])]
            face = " The person has the face of the person in image 1, photographed for real." if refs else ""
            tasks.append({"prompt": shot["picture"] + face + " " + data["style"], "out": shot_path(name, sid, "png"),
                          "width": 1280, "height": 720, "seed": seed_of(sid), "steps": 28,
                          "refs": [r.replace("\\", "/") for r in refs]})
    todo = [t for t in tasks if not os.path.exists(t["out"])]
    if todo:
        os.makedirs(WORK, exist_ok=True)
        spec = os.path.join(WORK, "pictures.json")
        json.dump(todo, open(spec, "w", encoding="utf-8"), indent=1)
        flux = linux(os.path.join(ROOT, "tools", "flux_wsl.py"))
        subprocess.run(["wsl.exe", "-d", "Ubuntu-22.04", "--", "bash", "-lc",
                        "cd /tmp && nice -n 10 ~/miniconda3/envs/flux/bin/python %s %s" % (flux, linux(spec))],
                       check=False, creationflags=BELOW_NORMAL)
    from PIL import Image
    for name, film in chosen.items():
        shots = [shot_path(name, shot_id(sh, n), "png") for n, sh in enumerate(film["shots"])]
        have = [Image.open(p).convert("RGB").resize((384, 216)) for p in shots if os.path.exists(p)]
        if have:
            sheet = Image.new("RGB", (384 * 3, 216 * ((len(have) + 2) // 3)), (0, 0, 0))
            for i, im in enumerate(have):
                sheet.paste(im, ((i % 3) * 384, (i // 3) * 216))
            sheet.save(os.path.join(WORK, name, "sheet.jpg"), quality=88)
        print("%s: %d of %d pictures" % (name, len(have), len(shots)))


def seconds_of(path):
    r = subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", path],
                       capture_output=True, text=True)
    try:
        return float(r.stdout.strip())
    except ValueError:
        return 0.0


def spoken(who, key, lang):
    """How long a line takes in a language (the longest of the heroes' takes for a hero's line); 0 without a voice."""
    files = [key + "." + h for h in HEROES] if who == "hero" else [key]
    lens = [seconds_of(os.path.join(ROOT, "assets", "voice", lang, f + ".ogg")) for f in files
            if os.path.exists(os.path.join(ROOT, "assets", "voice", lang, f + ".ogg"))]
    return max(lens) if lens else 0.0


def timing(film):
    """Each shot's length and when its lines start (from the film's start), for the longer of the languages."""
    plan, start = [], 0.0
    for shot in film["shots"]:
        lines = []
        at = LEAD
        for who, key in shot.get("say", []):
            lines.append((at, who, key))
            at += max(spoken(who, key, lang) for lang in LANGS) + GAP
        need = at - GAP + TAIL if lines else 0.0
        length = max(float(shot.get("seconds", 5.0)), need)
        if length > MAX_SHOT:
            print("  warning: a shot needs %.1f s, more than %.0f" % (length, MAX_SHOT))
        length = min(length, MAX_SHOT + 2.0)
        frames = 8 * math.ceil((length * FPS - 1) / 8.0) + 1
        plan.append({"start": start, "length": (frames - 1) / FPS, "frames": frames,
                     "lines": [(start + t, who, key) for t, who, key in lines]})
        start += (frames - 1) / FPS - FADE
    return plan


def unbar(im):
    """Cuts black bars FLUX paints in now and then (a cinema look): rows that are black all across, stars excepted."""
    g = im.convert("L")
    w, h = g.size

    def black(y):
        row = g.crop((0, y, w, y + 1))
        return max(row.getdata()) < 24 and sum(row.getdata()) < 6 * w

    top, bottom = 0, h
    while top < h // 4 and black(top):
        top += 1
    while bottom > h * 3 // 4 and black(bottom - 1):
        bottom -= 1
    return im.crop((0, top, w, bottom)) if top > h * 0.02 or h - bottom > h * 0.02 else im


def motion(data, chosen):
    from PIL import Image
    tasks = []
    for name, film in chosen.items():
        for n, (shot, p) in enumerate(zip(film["shots"], timing(film))):
            sid = shot_id(shot, n)
            png = shot_path(name, sid, "png")
            if not os.path.exists(png):
                continue
            key = shot_path(name, sid, "key.png")
            if not os.path.exists(key) or os.path.getmtime(key) < os.path.getmtime(png):
                im = unbar(Image.open(png).convert("RGB"))
                scale = max(WIDTH / im.width, HEIGHT / im.height)   # fill the frame, cut the rest evenly
                im = im.resize((round(im.width * scale), round(im.height * scale)), Image.LANCZOS)
                left, top = (im.width - WIDTH) // 2, (im.height - HEIGHT) // 2
                im.crop((left, top, left + WIDTH, top + HEIGHT)).save(key)
            out = shot_path(name, sid, "mp4")
            if os.path.exists(out):
                continue
            # The production batch is audio-to-video: silence keeps every mouth on screen shut (the game speaks).
            hush = shot_path(name, sid, "hush.wav")
            if not os.path.exists(hush):
                subprocess.run(["ffmpeg", "-y", "-v", "error", "-f", "lavfi", "-i", "anullsrc=r=48000:cl=stereo",
                                "-t", "%.3f" % ((p["frames"] - 1) / FPS + 0.2), hush], check=True)
            tasks.append({"prompt": "%s %s %s Nobody visible is talking." % (shot["motion"], shot["picture"],
                                                                               data["motion"]),
                          "negative_prompt": NEGATIVE, "audio_path": linux(hush), "image_path": linux(key),
                          "conditionings": [[linux(key), 0, 1.0]], "output_path": linux(out),
                          "width": WIDTH, "height": HEIGHT, "num_frames": p["frames"], "frame_rate": float(FPS),
                          "num_inference_steps": 30, "video_cfg_guidance_scale": 3.0, "video_stg_guidance_scale": 1.0,
                          "video_rescale_scale": 0.7, "a2v_guidance_scale": 1.0, "seed": seed_of(sid), "is_singer": False})
    print("motion: %d shots to animate" % len(tasks), flush=True)
    if tasks:
        spec = os.path.join(WORK, "motion.json")
        json.dump(tasks, open(spec, "w", encoding="utf-8"), indent=1)
        # Text2Video's production batch in WSL2 (its LTX checkout, the models on ext4): each model loaded once.
        subprocess.run(["wsl.exe", "-d", "Ubuntu-22.04", "--", "bash", "-lc",
                        "cd ~/Projekte/Text2Video/LTX-V && "
                        "PYTHONPATH=packages/ltx-core/src:packages/ltx-pipelines/src "
                        "PYTORCH_CUDA_ALLOC_CONF=expandable_segments:True nice -n 10 ~/miniconda3/envs/ltx2/bin/python "
                        "packages/ltx-pipelines/src/ltx_pipelines/batch_a2vid_two_stage.py --tasks %s "
                        "--checkpoint-path models/ltx-2.3-22b-distilled.safetensors --distilled-checkpoint "
                        "--gemma-root models/gemma --spatial-upsampler-path models/ltx-2.3-spatial-upscaler-x2-1.0.safetensors "
                        "--quantization fp8-cast --streaming-prefetch-count 2" % linux(spec)],
                       check=False, creationflags=BELOW_NORMAL)


def cut(chosen):
    os.makedirs(OUT, exist_ok=True)
    for name, film in chosen.items():
        plan = timing(film)
        clips = [shot_path(name, shot_id(sh, n), "mp4") for n, sh in enumerate(film["shots"])]
        if not all(os.path.exists(c) for c in clips):
            print("%s: shots missing, not cut" % name)
            continue
        args, chain, last, offset = [], [], "0:v", 0.0
        for i, (c, p) in enumerate(zip(clips, plan)):
            args += ["-i", c]
            chain.append("[%d:v]trim=duration=%.3f,setpts=PTS-STARTPTS,fps=%d,format=yuv420p[v%d]" %
                         (i, p["length"], FPS, i))
        last = "v0"
        offset = plan[0]["length"]
        for i in range(1, len(clips)):
            offset -= FADE
            chain.append("[%s][v%d]xfade=transition=fade:duration=%.2f:offset=%.3f[x%d]" % (last, i, FADE, offset, i))
            last = "x%d" % i
            offset += plan[i]["length"]
        total = offset
        chain.append("[%s]fade=t=in:st=0:d=0.8,fade=t=out:st=%.3f:d=1.2[out]" % (last, total - 1.2))
        ivf = os.path.join(OUT, name + ".ivf")
        subprocess.run(["ffmpeg", "-y", "-v", "error"] + args +
                       ["-filter_complex", ";".join(chain), "-map", "[out]", "-an", "-c:v", "libsvtav1",
                        "-preset", "5", "-crf", str(CRF), "-g", "48", "-pix_fmt", "yuv420p",
                        "-color_primaries", "bt709", "-color_trc", "bt709", "-colorspace", "bt709",
                        "-color_range", "tv", "-f", "ivf", ivf], check=True, creationflags=BELOW_NORMAL)
        with open(os.path.join(OUT, name + ".txt"), "w", encoding="utf-8", newline="\n") as f:
            f.write("# %s: made by tools/make_films.py from tools/films.json\n" % name)
            f.write("music  %s\n" % film["music"])
            f.write("length %.2f\n" % total)
            for p in plan:
                for at, who, key in p["lines"]:
                    f.write("line   %.2f %s %s\n" % (at, who, key))
        print("%s: %.1f s, %.1f MB" % (name, total, os.path.getsize(ivf) / 1e6), flush=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("step", choices=("pictures", "motion", "cut", "all"))
    ap.add_argument("--film", nargs="*", default=[], help="only these films")
    ap.add_argument("--redo", nargs="*", default=[], help="film:shot (its id or place) to make again (picture and motion)")
    a = ap.parse_args()
    data, all_films = films()
    chosen = {k: v for k, v in all_films.items() if not a.film or k in a.film}
    for r in a.redo:
        name, sid = r.split(":")
        sid = "s" + sid if sid.isdigit() else sid
        for ext in ("png", "key.png", "mp4"):
            if os.path.exists(shot_path(name, sid, ext)):
                os.remove(shot_path(name, sid, ext))
    if a.step in ("pictures", "all"):
        pictures(data, chosen)
    if a.step in ("motion", "all"):
        motion(data, chosen)
    if a.step in ("cut", "all"):
        cut(chosen)


if __name__ == "__main__":
    sys.exit(main())
