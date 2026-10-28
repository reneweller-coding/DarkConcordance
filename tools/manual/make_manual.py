"""DARK CONCORDANCE -- the manual in German and English, built out of the game and its sources (after Sunstreak's).

    py -3 tools/manual/make_shots.py          -> docs/manual/shots/<de|en>/*.png (the game takes its own pictures)
    py -3 tools/manual/make_manual.py         -> docs/manual/DarkConcordance-Handbuch.html/.pdf and -Manual.html/.pdf

The prose is `chapters_de.txt` and `chapters_en.txt`, in a small markup: `# chapter`, `## section`, paragraphs
separated by blank lines, `- ` list items, `|` table rows (the first is the head, a `|---|` line is skipped),
`[shot NAME] caption` for a picture of docs/manual/shots/<language>, `[table NAME]` for a table made here from the
sources (the controls from src/Bindings.cpp, the difficulties from src/GameShared.h), `**bold**` and `code` in
backticks.

The PDF is printed by Edge or Chrome in headless mode; without a browser the HTML is still written and prints by hand.
"""
import base64
import html
import io
import os
import re
import subprocess
import time

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
OUT = os.path.join(ROOT, 'docs', 'manual')

CSS = """
@page { size: A4; margin: 16mm 15mm 15mm 15mm; }
body { font: 10.5pt/1.55 "Segoe UI", "Helvetica Neue", Arial, sans-serif; color: #17181c; background: #fff; margin: 0; }
h1 { font-size: 36pt; margin: 0 0 2mm 0; letter-spacing: 4pt; color: #1b1712; }
h2 { font-size: 17pt; margin: 0 0 3mm 0; padding-bottom: 2mm; border-bottom: 2px solid #c8923a; color: #6e4614; }
h3 { font-size: 11pt; margin: 5mm 0 1.5mm 0; color: #8a5a1c; letter-spacing: 0.6pt; text-transform: uppercase; }
p { margin: 0 0 3.2mm 0; text-align: justify; hyphens: auto; }
ul { margin: 0 0 3.2mm 0; padding-left: 6mm; list-style: none; }
li { margin: 0.8mm 0; }
ul > li { position: relative; }
ul > li::before { content: ''; position: absolute; left: -4mm; top: 0.62em; width: 1.5mm; height: 1.5mm; border-radius: 50%; background: #c8923a; }
code { font: 9.5pt Consolas, monospace; background: #f4efe6; padding: 0 1mm; border-radius: 2px; }
.cover { page-break-after: always; text-align: center; padding-top: 16mm; }
.cover img.logo { width: 30mm; display: block; margin: 0 auto 6mm auto; }
.cover img.hero { width: 100%; border-radius: 4px; margin-top: 10mm; }
.cover p { text-align: center; }
.sub { color: #5b5f68; font-size: 13pt; margin: 0; }
.facts { margin-top: 7mm; color: #5b5f68; font-size: 9.5pt; }
.toc { page-break-after: always; }
.toc li { margin: 1.4mm 0; }
.chapter { page-break-before: always; }
figure { margin: 2mm 0 5mm 0; page-break-inside: avoid; }
figure img { width: 100%; border-radius: 3px; }
figcaption { color: #5b5f68; font-size: 9pt; margin-top: 1.5mm; font-style: italic; }
table { border-collapse: collapse; width: 100%; font-size: 9pt; margin: 2mm 0 5mm 0; page-break-inside: avoid; }
tr { page-break-inside: avoid; }
th { text-align: left; color: #5b5f68; font-weight: 600; border-bottom: 1.5px solid #c8923a; padding: 1mm 1.5mm; }
td { border-bottom: 1px solid #efe8dd; padding: 0.9mm 1.5mm; vertical-align: top; }
"""

LANG = {
    'de': {'file': 'DarkConcordance-Handbuch', 'title': 'Handbuch', 'version': 'Version', 'contents': 'Inhalt',
           'facts': 'Drei Helden &middot; ein Prolog und vier Akte &middot; drei Enden &middot; die Tiefen',
           'controls': ('', 'Tastatur und Maus', 'Gamepad, Steam Deck'),
           'difficulty': ('Schwierigkeit', 'Zähigkeit der Gegner', 'Ihre Treffer', 'Der Ort liegt höchstens',
                          'Funde'),
           'below': '{} Stufen unter dem Helden', 'below1': 'eine Stufe unter dem Helden', 'below0': 'auf der Stufe des Helden',
           'finds': {'NIGHTMARE': 'vier Stufen höher, seltene öfter'},
           'names': {'EASY': 'Leicht', 'NORMAL': 'Normal', 'HARD': 'Schwer', 'NIGHTMARE': 'Albtraum (nach dem ersten Durchlauf)'},
           'actions': {'up': 'Laufen', 'fire': 'Schießen, zuschlagen, Schwarm schicken', 'jump': 'Springen',
                       'dodge': 'Ausweichrolle', 'special': 'Fähigkeit 1 (Überladung, Sprengbolzen, Übernahme)',
                       'skill2': 'Fähigkeit 2', 'skill3': 'Fähigkeit 3', 'stim': 'Stim spritzen',
                       'use': 'Benutzen, öffnen, reden', 'strafe': 'Ausrichtung halten (Dorra: Schild, Ione: Schirm)',
                       'map': 'Karte', 'inventory': 'Inventar', 'character': 'Charakter', 'camreset': 'Kamera zurück',
                       'photo': 'Fotomodus', 'salvage': 'Im Inventar: zerlegen', 'store': 'Im Inventar: in den Spind'},
           'extra': [('Kamera drehen', 'rechte Maustaste halten', 'rechter Stick'),
                     ('Zoomen', 'Mausrad', 'rechter Stick auf und ab'), ('Pause', 'Esc', 'Start'),
                     ('Bildrate, Bildschirmfoto', 'F1, F12', '')],
           'walk': 'WASD oder Pfeiltasten'},
    'en': {'file': 'DarkConcordance-Manual', 'title': 'Manual', 'version': 'version', 'contents': 'Contents',
           'facts': 'Three heroes &middot; a prologue and four acts &middot; three endings &middot; the Depths',
           'controls': ('', 'Keyboard and mouse', 'Gamepad, Steam Deck'),
           'difficulty': ('Difficulty', 'Foes\' toughness', 'Their blows', 'A place lies at most', 'Finds'),
           'below': '{} levels below the hero', 'below1': 'one level below the hero', 'below0': 'at the hero\'s level',
           'finds': {'NIGHTMARE': 'four levels higher, rare ones more often'},
           'names': {'EASY': 'Easy (Leicht)', 'NORMAL': 'Normal', 'HARD': 'Hard (Schwer)',
                     'NIGHTMARE': 'Nightmare (Albtraum, after the first run)'},
           'actions': {'up': 'Move', 'fire': 'Shoot, strike, send the swarm', 'jump': 'Jump', 'dodge': 'Dodge roll',
                       'special': 'Skill 1 (overcharge, blast bolts, takeover)', 'skill2': 'Skill 2',
                       'skill3': 'Skill 3', 'stim': 'Inject a stim', 'use': 'Use, open, talk',
                       'strafe': 'Hold the facing (Dorra: shield, Ione: screen)', 'map': 'Map',
                       'inventory': 'Inventory', 'character': 'Character', 'camreset': 'Camera back',
                       'photo': 'Photo mode', 'salvage': 'In the inventory: salvage',
                       'store': 'In the inventory: into the locker'},
           'extra': [('Turn the camera', 'hold the right mouse button', 'right stick'),
                     ('Zoom', 'mouse wheel', 'right stick up and down'), ('Pause', 'Esc', 'Start'),
                     ('Frame rate, screenshot', 'F1, F12', '')],
           'walk': 'WASD or the arrows'},
}

KEYS = {'KEY_LEFT_CONTROL': 'Ctrl', 'KEY_LEFT_SHIFT': 'Shift', 'KEY_SPACE': 'Space', 'KEY_BACKSPACE': 'Backspace',
        'MouseLeft': 'Mouse 1'}
PAD = {'GAMEPAD_BUTTON_RIGHT_TRIGGER_2': 'RT', 'GAMEPAD_BUTTON_LEFT_TRIGGER_2': 'LT',
       'GAMEPAD_BUTTON_RIGHT_TRIGGER_1': 'RB', 'GAMEPAD_BUTTON_LEFT_TRIGGER_1': 'LB',
       'GAMEPAD_BUTTON_RIGHT_FACE_DOWN': 'A', 'GAMEPAD_BUTTON_RIGHT_FACE_RIGHT': 'B',
       'GAMEPAD_BUTTON_RIGHT_FACE_LEFT': 'X', 'GAMEPAD_BUTTON_RIGHT_FACE_UP': 'Y',
       'GAMEPAD_BUTTON_LEFT_THUMB': 'L3', 'GAMEPAD_BUTTON_RIGHT_THUMB': 'R3', 'GAMEPAD_BUTTON_LEFT_FACE_UP': 'D-pad up',
       'GAMEPAD_BUTTON_MIDDLE_LEFT': 'View', '0': ''}

# The keys' and buttons' names as each language writes them.
WORDS = {'de': {'Mouse 1': 'linke Maustaste', 'Space': 'Leertaste', 'Ctrl': 'Strg', 'Shift': 'Umschalt',
                'Backspace': 'Rücktaste', 'D-pad up': 'Steuerkreuz hoch', 'L3': 'linken Stick drücken (L3)',
                'R3': 'rechten Stick drücken (R3)'},
         'en': {'Mouse 1': 'left mouse button', 'D-pad up': 'd-pad up', 'L3': 'left stick click (L3)',
                'R3': 'right stick click (R3)'}}


def read(path):
    return open(os.path.join(ROOT, path), encoding='utf-8').read()


def table(head, rows):
    out = ['<table><tr>' + ''.join(f'<th>{html.escape(h)}</th>' for h in head) + '</tr>']
    for r in rows:
        out.append('<tr>' + ''.join(f'<td>{inline(str(c))}</td>' for c in r) + '</tr>')
    return '\n'.join(out) + '</table>'


def table_controls(lang):
    """The game's own bindings (src/Bindings.cpp's Defaults), in the order it lists them."""
    L = LANG[lang]
    src = read('src/Bindings.cpp')
    names = re.findall(r'"(\w+)"', src[src.index('Names[(int)BindE::COUNT]'):src.index('};', src.index('Names['))])
    block = src[src.index('Defaults[(int)BindE::COUNT]'):]
    pairs = re.findall(r'\{\s*(\w+),\s*(\w+)\s*\}', block[:block.index('};')])
    rows = []
    for name, (key, pad) in zip(names, pairs):
        if name in ('down', 'left', 'right'):
            continue
        k = WORDS[lang].get(KEYS.get(key, key.replace('KEY_', '')), KEYS.get(key, key.replace('KEY_', '')))
        p = WORDS[lang].get(PAD.get(pad, pad), PAD.get(pad, pad))
        if name == 'up':
            k, p = L['walk'], 'left stick' if lang == 'en' else 'linker Stick'
        if name == 'use' and not p:
            p = 'A (in reach)' if lang == 'en' else 'A (in Reichweite)'
        rows.append((L['actions'].get(name, name), k, p))
    return table(L['controls'], rows + L['extra'])


def table_difficulty(lang):
    """Toughness and harm from src/GameShared.h, the places' floor from src/Game.cpp."""
    L = LANG[lang]
    src = read('src/GameShared.h')

    def factors(fn):
        body = src[src.index('inline float %s(' % fn):]
        body = body[:body.index('}')]
        easy = re.search(r'EASY \? ([0-9.]+)f', body).group(1)
        hard = re.search(r'HARD \? ([0-9.]+)f', body).group(1)
        night = re.search(r'NIGHTMARE \? ([0-9.]+)f', body).group(1)
        rest = re.search(r': ([0-9.]+)f;', body).group(1)
        return {'EASY': easy, 'NORMAL': rest, 'HARD': hard, 'NIGHTMARE': night}

    tough, harm = factors('toughness'), factors('harm')
    behind = [int(x) for x in re.search(r'behind\[\(int\)DifficultyE::COUNT\] = \{ ([0-9, ]+) \}',
                                        read('src/Game.cpp')).group(1).split(',')]
    rows = []
    for i, d in enumerate(('EASY', 'NORMAL', 'HARD', 'NIGHTMARE')):
        b = behind[i]
        below = L['below0'] if b == 0 else L['below1'] if b == 1 else L['below'].format(b)
        pct = lambda v: '%d %%' % round(float(v) * 100)
        rows.append((L['names'][d], pct(tough[d]), pct(harm[d]), below, L['finds'].get(d, '')))
    return table(L['difficulty'], rows)


TABLES = {'controls': table_controls, 'difficulty': table_difficulty}


def inline(text):
    t = html.escape(text)
    t = re.sub(r'\*\*(.+?)\*\*', r'<b>\1</b>', t)
    t = re.sub(r'`([^`]+)`', r'<code>\1</code>', t)
    return t


def picture(lang, name, width=1400):
    path = os.path.join(OUT, 'shots', lang, name + '.png')
    if not os.path.exists(path):
        print('missing picture', lang, name)
        return ''
    im = Image.open(path).convert('RGB')
    if im.width > width:
        im = im.resize((width, round(im.height * width / im.width)), Image.LANCZOS)
    buf = io.BytesIO()
    im.save(buf, 'JPEG', quality=84, optimize=True)
    return 'data:image/jpeg;base64,' + base64.b64encode(buf.getvalue()).decode('ascii')


def logo():
    """The program's icon (res/darkconcordance.png, tools/make_icon.py)."""
    data = open(os.path.join(ROOT, 'res', 'darkconcordance.png'), 'rb').read()
    return 'data:image/png;base64,' + base64.b64encode(data).decode('ascii')


def version():
    m = re.search(r'project\(\s*SpaceOpera\s+VERSION\s+([0-9.]+)', read('CMakeLists.txt'))
    return m.group(1) if m else ''


def render(lang, chapters):
    body, toc = [], []
    open_chapter = False
    for block in re.split(r'\n\s*\n', chapters.strip()):
        lines = block.strip().split('\n')
        first = lines[0]
        if first.startswith('# '):
            if open_chapter:
                body.append('</div>')
            title = first[2:].strip()
            anchor = re.sub(r'\W+', '-', title.lower()).strip('-')
            toc.append(f'<li><a href="#{anchor}">{html.escape(title)}</a></li>')
            body.append(f'<div class="chapter" id="{anchor}"><h2>{html.escape(title)}</h2>')
            open_chapter = True
            lines = lines[1:]
            if not lines:
                continue
            first = lines[0]
        if first.startswith('## '):
            body.append(f'<h3>{html.escape(first[3:].strip())}</h3>')
            lines = lines[1:]
            if not lines:
                continue
            first = lines[0]
        if first.startswith('[shot '):
            for line in lines:
                m = re.match(r'\[shot (\w+)\]\s*(.*)', line)
                if m:
                    src = picture(lang, m.group(1))
                    if src:
                        body.append(f'<figure><img src="{src}" alt="{html.escape(m.group(2))}">'
                                    f'<figcaption>{inline(m.group(2))}</figcaption></figure>')
        elif first.startswith('[table '):
            body.append(TABLES[first[7:-1].strip()](lang))
        elif first.startswith('|'):
            rows = [[c.strip() for c in l.strip().strip('|').split('|')] for l in lines if not re.match(r'\|\s*-', l)]
            body.append(table(rows[0], rows[1:]))
        elif first.startswith('- '):
            items = []
            for line in lines:
                if line.startswith('- '):
                    items.append(line[2:])
                else:
                    items[-1] += ' ' + line.strip()
            body.append('<ul>' + ''.join(f'<li>{inline(i)}</li>' for i in items) + '</ul>')
        else:
            body.append(f'<p>{inline(" ".join(l.strip() for l in lines))}</p>')
    if open_chapter:
        body.append('</div>')
    return toc, body


def find_browser():
    for p in (r'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe',
              r'C:\Program Files\Microsoft\Edge\Application\msedge.exe',
              r'C:\Program Files\Google\Chrome\Application\chrome.exe'):
        if os.path.exists(p):
            return p
    return None


def build(lang):
    L = LANG[lang]
    toc, body = render(lang, read('tools/manual/chapters_%s.txt' % lang))
    cover = f"""<div class="cover"><img class="logo" src="{logo()}" alt=""><h1>DARK CONCORDANCE</h1><p class="sub">{L['title']} &middot; {L['version']} {version()}</p>
<img class="hero" src="{picture(lang, 'cover', 1600)}" alt="">
<p class="facts">{L['facts']}</p></div>
<div class="toc"><h2>{L['contents']}</h2><ol>{''.join(toc)}</ol></div>"""
    page = f"""<!DOCTYPE html><html lang="{lang}"><head><meta charset="utf-8"><title>DARK CONCORDANCE {L['title']}</title>
<style>{CSS}</style></head><body>{cover}{''.join(body)}</body></html>"""
    os.makedirs(OUT, exist_ok=True)
    out_html = os.path.join(OUT, L['file'] + '.html')
    open(out_html, 'w', encoding='utf-8', newline='\n').write(page)
    print('wrote', out_html, f'{os.path.getsize(out_html) // 1024} KB')
    browser = find_browser()
    if not browser:
        print('no browser: print the HTML by hand')
        return
    pdf = os.path.join(OUT, L['file'] + '.pdf')
    if os.path.exists(pdf):
        os.remove(pdf)
    url = 'file:///' + out_html.replace('\\', '/')
    # A profile of its own, or a browser already running takes the call over and prints nothing.
    profile = os.path.join(ROOT, 'work', 'edge-profile')
    subprocess.run([browser, '--headless=new', '--disable-gpu', '--no-pdf-header-footer', '--user-data-dir=' + profile,
                    '--print-to-pdf=' + pdf, url], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                   timeout=300)
    # The launcher returns before its child has printed: wait until the file stops growing.
    size = -1
    for _ in range(240):
        time.sleep(0.5)
        now = os.path.getsize(pdf) if os.path.exists(pdf) else -1
        if now > 0 and now == size:
            break
        size = now
    print('wrote', pdf, f'{os.path.getsize(pdf) // 1024} KB' if os.path.exists(pdf) else '(failed)')


for language in ('de', 'en'):
    build(language)
