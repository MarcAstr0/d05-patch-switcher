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
f = TTFont(sys.argv[1], recalcTimestamp=False); f.flavor = 'woff2'; f.save(sys.argv[2])
" assets/fonts/Inter-$w.ttf web/public/fonts/Inter-$w.woff2
done
cp assets/fonts/OFL.txt web/public/fonts/OFL.txt
```

`recalcTimestamp=False` keeps the TTF's `head.modified`, so the output is deterministic.

| File | Bytes | sha256 |
|---|---|---|
| `Inter-Regular.woff2` | 22,664 | `a8a0bf4b3eecae1b2fe703cd2f7d04a6e997395310ab1d112df072dfa06d4375` |
| `Inter-Medium.woff2` | 23,084 | `9a5c338ed57e8636b4619de0e119776aeb7f3582f3e5da21a504571bc7741185` |
| `OFL.txt` | 4,380 | `262481e844521b326f5ecd053e59b98c8b2da78c8ee1bdbb6e8174305e54935a` |
