"""Reads the play log (playlog.txt in the player's folder) and writes an HTML page: per level the time, the deaths,
the kills, what killed the hero and who stood round, the skills used; per visit a map with the way walked and where
the hero died (the maps the game put beside the log, "maps/<level>_<time>.txt").

    python tools/playlog_report.py [playlog.txt] [report.html]

Without arguments: the log in the player's folder (%APPDATA%/SpaceOpera on Windows, ~/.local/share/SpaceOpera
elsewhere), the page beside it.
"""
import base64
import collections
import html
import io
import os
import re
import sys

try:
    from PIL import Image, ImageDraw
except ImportError:  # the pages still come, without maps
    Image = None

CELL = 2.0     # metres per map cell
PIXELS = 6     # pixels per cell on the maps


def default_log():
    if os.name == "nt":
        return os.path.join(os.environ.get("APPDATA", "."), "SpaceOpera", "playlog.txt")
    base = os.environ.get("XDG_DATA_HOME") or os.path.join(os.path.expanduser("~"), ".local", "share")
    return os.path.join(base, "SpaceOpera", "playlog.txt")


class Visit:
    """One time a level was played: from its "level" line to the next one."""

    def __init__(self, clock, words):
        self.level = words[1]
        self.hero = words[3]
        self.hero_level = int(words[5])
        self.difficulty = int(words[7])
        self.cycle = int(words[9])
        self.map = words[11] if len(words) > 11 and words[10] == "map" else ""
        self.start = self.end = clock
        self.time = 0.0
        self.kills = 0
        self.done = False
        self.path = []
        self.deaths = []        # (x, z, cause, killers)
        self.anomalies = ""
        self.uses = ""
        self.events = collections.Counter()


def parse(path):
    visits = []
    for raw in open(path, encoding="utf-8", errors="replace"):
        m = re.match(r"(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d)  (.*)", raw.rstrip("\n"))
        if not m:
            continue
        clock, text = m.group(1), m.group(2)
        words = text.split()
        if not words:
            continue
        kind = words[0]
        if kind == "level" and len(words) >= 10:
            visits.append(Visit(clock, words))
            continue
        if not visits:
            continue
        v = visits[-1]
        v.end = clock
        if kind == "at":
            a = re.match(r"at \S+ (-?\d+) (-?\d+) t (\d+) hp \S+ kills (\d+)", text)
            if a:
                v.path.append((float(a.group(1)), float(a.group(2))))
                v.time = float(a.group(3))
                v.kills = int(a.group(4))
        elif kind == "death":
            d = re.match(r"death \S+ at (-?\d+) (-?\d+) t (\d+) cause (\S+) hpmax \S+ by(.*)", text)
            if d:
                v.deaths.append((float(d.group(1)), float(d.group(2)), d.group(4), d.group(5).strip()))
                v.time = max(v.time, float(d.group(3)))
        elif kind == "done":
            d = re.match(r"done \S+ t (\d+) deaths \d+ kills (\d+) scrap \d+ level \d+(.*)", text)
            if d:
                v.time, v.kills, v.uses, v.done = float(d.group(1)), int(d.group(2)), d.group(3).strip(), True
        elif kind == "anomalies":
            v.anomalies = " ".join(words[1:])
        else:
            v.events[kind] += 1
    return visits


def read_map(folder, name):
    """The [map] section of a level file beside the log."""
    path = os.path.join(folder, name)
    if not name or not os.path.exists(path):
        return []
    rows, inside = [], False
    for line in open(path, encoding="utf-8", errors="replace"):
        line = line.rstrip("\r\n")
        if line.strip() == "[map]":
            inside = True
            continue
        if inside and (not line.strip() or line.startswith("[")):
            break
        if inside:
            rows.append(line)
    return rows


def picture(rows, visit):
    """The map: rock dark, shafts blue, floor light; the way walked in orange, deaths as red crosses."""
    if Image is None or not rows:
        return ""
    w, h = max(len(r) for r in rows), len(rows)
    im = Image.new("RGB", (w * PIXELS, h * PIXELS), (14, 14, 18))
    dr = ImageDraw.Draw(im)
    for r, row in enumerate(rows):
        for c, ch in enumerate(row):
            if ch == "#":
                continue
            colour = (40, 60, 110) if ch in "~_" else (120, 116, 108)
            dr.rectangle([c * PIXELS, r * PIXELS, (c + 1) * PIXELS - 1, (r + 1) * PIXELS - 1], fill=colour)

    def at(x, z):
        return x / CELL * PIXELS, z / CELL * PIXELS

    if len(visit.path) > 1:
        dr.line([at(x, z) for x, z in visit.path], fill=(255, 170, 60), width=2)
    for x, z, _, _ in visit.deaths:
        px, pz = at(x, z)
        dr.line([px - 5, pz - 5, px + 5, pz + 5], fill=(255, 40, 40), width=3)
        dr.line([px - 5, pz + 5, px + 5, pz - 5], fill=(255, 40, 40), width=3)
    out = io.BytesIO()
    im.save(out, "PNG")
    return "data:image/png;base64," + base64.b64encode(out.getvalue()).decode()


def page(visits, folder):
    esc = html.escape
    per_level = collections.OrderedDict()
    for v in visits:
        per_level.setdefault(v.level, []).append(v)
    out = ["<!doctype html><html lang='de'><head><meta charset='utf-8'><title>Spielprotokoll</title><style>",
           "body{font-family:Segoe UI,Arial,sans-serif;background:#121216;color:#e6e2da;margin:24px}",
           "table{border-collapse:collapse;margin:12px 0}td,th{padding:4px 10px;border-bottom:1px solid #333;"
           "text-align:left}th{color:#ffb070}h1,h2{color:#ffc890}img{border:1px solid #444;max-width:100%}"
           ".death{color:#ff7070}.small{color:#999;font-size:90%}</style></head><body>",
           "<h1>Spielprotokoll</h1>",
           "<p class='small'>%d Besuche in %d Leveln</p>" % (len(visits), len(per_level)),
           "<h2>Je Level</h2><table><tr><th>Level</th><th>Besuche</th><th>Spielzeit</th><th>Tode</th>"
           "<th>Kills</th><th>Tode je Stunde</th><th>Ursachen</th></tr>"]
    for level, vs in per_level.items():
        t = sum(v.time for v in vs)
        deaths = sum(len(v.deaths) for v in vs)
        causes = collections.Counter(d[2] for v in vs for d in v.deaths)
        out.append("<tr><td>%s</td><td>%d</td><td>%d:%02d</td><td class='death'>%d</td><td>%d</td><td>%.1f</td>"
                   "<td>%s</td></tr>" % (esc(level), len(vs), int(t) // 60, int(t) % 60, deaths,
                                         sum(v.kills for v in vs), deaths / (t / 3600) if t > 0 else 0,
                                         esc(", ".join("%s %d" % kv for kv in causes.most_common(4)))))
    out.append("</table>")
    killers = collections.Counter(k for v in visits for d in v.deaths for k in d[3].split() if k != "-")
    if killers:
        out.append("<h2>Wer dabei war, wenn der Held starb</h2><p>%s</p>" %
                   esc(", ".join("%s %d" % kv for kv in killers.most_common(15))))
    out.append("<h2>Die Besuche</h2>")
    for v in visits:
        out.append("<h3>%s — %s, Stufe %d, Schwierigkeit %d%s — %s bis %s</h3>" % (
            esc(v.level), esc(v.hero), v.hero_level, v.difficulty, ", Zyklus %d" % v.cycle if v.cycle else "",
            esc(v.start), esc(v.end[11:])))
        out.append("<p>%d:%02d Spielzeit, %d Kills, <span class='death'>%d Tode</span>%s%s</p>" % (
            int(v.time) // 60, int(v.time) % 60, v.kills, len(v.deaths),
            "" if v.done else " (nicht beendet)", " · Anomalien: " + esc(v.anomalies) if v.anomalies else ""))
        if v.uses:
            out.append("<p class='small'>Benutzt: %s</p>" % esc(v.uses))
        for x, z, cause, by in v.deaths:
            out.append("<p class='small death'>Tod bei %.0f, %.0f durch %s; dabei: %s</p>" % (x, z, esc(cause),
                                                                                                esc(by)))
        src = picture(read_map(folder, v.map), v)
        if src:
            out.append("<img src='%s' alt='Karte'>" % src)
    out.append("</body></html>")
    return "\n".join(out)


def main():
    log = sys.argv[1] if len(sys.argv) > 1 else default_log()
    if not os.path.exists(log):
        sys.exit("no play log at " + log)
    target = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(log), "playlog_report.html")
    visits = parse(log)
    with open(target, "w", encoding="utf-8") as f:
        f.write(page(visits, os.path.dirname(os.path.abspath(log))))
    print("%d visits -> %s" % (len(visits), target))


if __name__ == "__main__":
    main()
