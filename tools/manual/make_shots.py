"""Takes the manual's pictures (tools/manual/shots.txt) with the game itself: each line a run of bin/msvc's program,
muted, at below normal priority, two at a time, once per language, into docs/manual/shots/<de|en>/NAME.png.

    py -3 tools/manual/make_shots.py [NAME ...]
"""
import os
import shutil
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
EXE = os.path.join(ROOT, 'bin', 'msvc', 'SpaceOpera.exe')
OUT = os.path.join(ROOT, 'docs', 'manual', 'shots')
WORKERS = 2   # game instances at once (more next to a GPU job crashed the PC)

only = set(sys.argv[1:])
jobs = []
for line in open(os.path.join(HERE, 'shots.txt'), encoding='utf-8'):
    if not line.strip() or line.lstrip().startswith('#'):
        continue
    name, args = (s.strip() for s in line.split('|', 1))
    if only and name not in only:
        continue
    for lang in ('de', 'en'):
        jobs.append((lang, name, args.split()))

running = []
while jobs or running:
    while jobs and len(running) < WORKERS:
        lang, name, args = jobs.pop(0)
        os.makedirs(os.path.join(OUT, lang), exist_ok=True)
        # Each run from a copy of its own (the log goes beside the program).
        work = tempfile.mkdtemp(prefix='shot_', dir=os.path.join(ROOT, 'work'))
        exe = os.path.join(work, 'SpaceOpera.exe')
        shutil.copy2(EXE, exe)
        cmd = [exe, '--shot', os.path.join(OUT, lang, name + '.png'), '--mute', '--lang', lang, '--w', '1600',
               '--h', '900'] + args
        p = subprocess.Popen(cmd, cwd=work, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                             creationflags=0x00004000)
        running.append((p, work))
    time.sleep(0.4)
    for p, work in list(running):
        if p.poll() is not None:
            running.remove((p, work))
            shutil.rmtree(work, ignore_errors=True)
print('pictures in', OUT)
