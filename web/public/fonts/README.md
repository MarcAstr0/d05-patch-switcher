# Fonts served by the web version

Inter 4.1 (Rasmus Andersson), SIL Open Font License 1.1, no Reserved Font Name (`OFL.txt`).
These are `assets/fonts/Inter-Regular.ttf` and `Inter-Medium.ttf` (the same subset as the desktop
app) converted to WOFF2 with nothing else changed (docs/design/DESIGN-WEB.md 3.1):

```sh
python3 -m venv /tmp/venv-fonts
/tmp/venv-fonts/bin/pip install fonttools==4.67.0 brotli==1.2.1
for w in Regular Medium; do
  /tmp/venv-fonts/bin/python -I -c "
import sys
from fontTools.ttLib import TTFont
f = TTFont(sys.argv[1]); f.flavor = 'woff2'; f.save(sys.argv[2])
" assets/fonts/Inter-$w.ttf web/public/fonts/Inter-$w.woff2
done
cp assets/fonts/OFL.txt web/public/fonts/OFL.txt
```

| File | Bytes | sha256 |
|---|---|---|
| `Inter-Regular.woff2` | 22,744 | `3d47ddbc0cb77e0edf2f5c5067a93d85d65c27e341ecff66355c6fc3131940dc` |
| `Inter-Medium.woff2` | 23,060 | `fb207b907e4554637e008a4f194a781963ce7cd4da47743e192c652b2e272b56` |
| `OFL.txt` | 4,380 | `262481e844521b326f5ecd053e59b98c8b2da78c8ee1bdbb6e8174305e54935a` |

The sizes and hashes differ slightly from the ones in DESIGN-WEB 3.1 (22,644 and 23,052 bytes):
the command above ran here with the pinned versions but produced these bytes (another Python or
brotli build). Glyphs and tables are the same input; the service worker precaches both files.
