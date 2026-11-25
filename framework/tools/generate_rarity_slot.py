from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
image = Image.new('RGBA', (48, 48))
for vertical in range(48):
    for horizontal in range(48):
        if (horizontal in (0, 47) and (vertical < 2 or vertical > 45)) or (vertical in (0, 47) and (horizontal < 2 or horizontal > 45)):
            continue
        edge = min(horizontal, vertical, 47 - horizontal, 47 - vertical)
        value = round(42 - vertical * .32) + ((horizontal * 7 + vertical * 11) % 3 - 1)
        if edge == 0:
            value = 18
        elif edge == 1:
            value = 70 if vertical < 24 and horizontal < 24 else 26
        elif edge == 2:
            value = 14
        image.putpixel((horizontal, vertical), (value, value, value, 255))
image.save(ROOT / 'mods/extraction-raid/images/rmf/rarity-slot.png')
