"""Seed only the separate demo profile with public Cinemeta catalogs, without an account."""
import json
from pathlib import Path
import sys
from urllib.request import urlopen

state = Path(sys.argv[1]) / 'state.json'
if not state.exists():
    url = 'https://v3-cinemeta.strem.io/manifest.json'
    with urlopen(url, timeout=30) as response:
        manifest = json.load(response)
    state.write_text(json.dumps({'addons': [{'transportUrl': url, 'manifest': manifest}]}), encoding='utf-8')
print('Demo profile: public Cinemeta catalogs; no Stremio account required')
