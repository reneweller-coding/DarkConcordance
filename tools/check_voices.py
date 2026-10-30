"""Listens to the spoken lines (tools/make_voices.py's takes in work/voice) with Whisper and compares what it hears with
the text: a take that rambles, swallows half a sentence or speaks another language shows up, and its file goes to
work/voice/redo_<language>.txt, ready for `make_voices.py --only $(cat ...) --force`.

    G:\\Tools\\Text2Video\\venv\\Scripts\\python.exe tools\\check_voices.py [--lang de] [--below 0.8]

Needs faster-whisper (Text2Video's venv has it, the model large-v3-turbo in the Hugging Face cache) and, for the GPU,
CUDA 12's cuBLAS and cuDNN 9 (miniforge's nvidia packages have them; on the CPU it takes hours).
"""
import argparse
import difflib
import glob
import json
import os
import re
import sys
import unicodedata

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_voices  # noqa: E402  (the lines and their texts)

LANGS = {"de": "de", "en": "en"}
NVIDIA = r"G:\Tools\miniforge3\Lib\site-packages\nvidia"


# Whisper writes numbers as digits, the texts spell them out: both are left out of the comparison.
NUMBER = re.compile(r"^(?:\d+|(?:und|ein|eine|eins|einen|zwei|drei|vier|fünf|sechs|sieben|acht|neun|zehn|elf|zwölf|"
                    r"zwanzig|dreissig|vierzig|fünfzig|sechzig|siebzig|achtzig|neunzig|hundert|tausend|million|"
                    r"millionen|one|two|three|four|five|six|seven|eight|nine|ten|eleven|twelve|thirteen|fifteen|"
                    r"twenty|thirty|forty|fifty|sixty|seventy|eighty|ninety|teen|hundred|thousand|millions?)+)$")

# The game's names: a voice may say them well and Whisper still write a word it knows (Tamar "Tama", Siel "Seal",
# Concordance "concordant"), so they and what is heard in their place are left out of the comparison.
NAMES = {"corvin", "corvins", "dorra", "dorras", "ione", "iones", "velm", "ysolde", "kiri", "ottla", "ilka", "okafor",
         "lamm", "brask", "josse", "mira", "pell", "ruul", "unzu", "siel", "hale", "lance", "seren", "hesper",
         "halden", "arche", "tesh", "kael", "brannt", "langmut", "concordance", "konkordanz", "sevenfold"}


def words(text):
    """Lower case words, numbers left out, one space between them (umlauts kept)."""
    text = unicodedata.normalize("NFKC", text).lower().replace("ß", "ss")
    text = re.sub(r"(?<=\d)[.,](?=\d)", "", text)
    # "94%" is heard for "ninety-four per cent", "1,112" for "one thousand one hundred and twelve".
    text = text.replace("%", " percent ").replace("per cent", "percent").replace("prozent", "percent")
    text = re.sub(r"\b(hundred|thousand)\s+and\b", r"\1", text)
    return " ".join(w for w in re.sub(r"[^\wäöü ]+", " ", text).split() if not NUMBER.match(w))


def hear(model, path, lang):
    segments, _ = model.transcribe(path, language=LANGS[lang], beam_size=1, vad_filter=False,
                                   condition_on_previous_text=False)
    return " ".join(s.text.strip() for s in segments)


def likeness(text, heard):
    """0..1, word by word; a short line letter by letter where that is better, since there a compound written apart
    ("Gefangenen Kahn"), a contraction ("I'm") or a spelling ("authorized", "Mom") weighs too much (in a long line the
    letters would hide a word that went wrong)."""
    want, got = words(text).split(), words(heard).split()
    names = [w for w in want if w in NAMES]
    if names:
        want = [w for w in want if w not in NAMES]
        got = [w for w in got if len(w) < 3 or w in want or
               all(difflib.SequenceMatcher(None, w, n).ratio() <= 0.6 for n in names)]
    by_words = difflib.SequenceMatcher(None, want, got, autojunk=False).ratio()
    if len(want) > 6:
        return by_words
    return max(by_words, difflib.SequenceMatcher(None, "".join(want), "".join(got), autojunk=False).ratio())


def ends_alike(text, heard):
    """Whether what is heard ends as the text does (its last three words, letters only: compounds may be joined)."""
    want = "".join(words(text).split()[-3:])
    got = "".join(words(heard).split()[-5:])
    return not want or difflib.SequenceMatcher(None, want, got[-len(want) - 4:], autojunk=False).ratio() >= 0.7


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lang", nargs="*", default=list(LANGS))
    ap.add_argument("--below", type=float, default=0.8, help="a take whose likeness to its text is lower is redone")
    ap.add_argument("--refs", action="store_true", help="only the voices in tools/voices against their texts")
    ap.add_argument("--only", nargs="*", default=[], help="only these takes (key or key.hero; the redo lists stay)")
    a = ap.parse_args()
    os.environ.setdefault("HF_HUB_OFFLINE", "1")   # the model is in the cache; asking the hub hangs without a network
    from faster_whisper import WhisperModel
    make_voices.below_normal()
    # Its CTranslate2 is built for CUDA 12; the system has CUDA 13 only.
    for sub in ("cublas", "cudnn"):
        lib = os.path.join(NVIDIA, sub, "bin")
        if os.path.isdir(lib):
            os.add_dll_directory(lib)
            os.environ["PATH"] = lib + os.pathsep + os.environ["PATH"]
    model = WhisperModel("large-v3-turbo", device="cuda", compute_type="int8_float16")
    voices = {k: v for k, v in json.load(open(os.path.join(make_voices.ROOT, "tools", "voices.json"),
                                              encoding="utf-8")).items() if k != "_"}
    # A voice whose clip says less than its text makes every clone say the rest first: such a text is cut.
    for lang in a.lang if not a.only else []:
        for ref in sorted(glob.glob(os.path.join(make_voices.REFS, "*_%s.flac" % lang))):
            text = open(ref[:-5] + ".txt", encoding="utf-8").read().strip()
            heard = hear(model, ref, lang)
            if likeness(text, heard) < a.below or not ends_alike(text, heard):
                print("voice %s: %s\n      heard: %s" % (os.path.basename(ref)[:-5], text, heard), flush=True)
    if a.refs:
        return
    jobs = make_voices.inventory(voices)
    for lang in a.lang:
        bad, n = [], 0
        for (l, file), (v, who, text) in sorted(jobs.items()):
            raw = os.path.join(make_voices.WORK, l, file + ".wav")
            if l != lang or not os.path.exists(raw) or (a.only and file not in a.only):
                continue
            heard = hear(model, raw, lang)
            like = likeness(text, heard)
            n += 1
            if like < a.below:
                bad.append(file)
                print("%.2f  %-24s %s\n      heard: %s" % (like, file, text, heard), flush=True)
        if not a.only:
            with open(os.path.join(make_voices.WORK, "redo_%s.txt" % lang), "w", encoding="utf-8") as f:
                f.write("\n".join(bad) + ("\n" if bad else ""))
        print("%s: %d of %d below %.2f" % (lang, len(bad), n, a.below), flush=True)


if __name__ == "__main__":
    main()
