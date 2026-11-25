from pathlib import Path
from math import exp, sin, cos
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
image = Image.new('RGBA', (96, 96))
for vertical in range(96):
    for horizontal in range(96):
        horizontal_offset = (horizontal - 47.5) / 48
        vertical_offset = (vertical - 47.5) / 48
        distance = horizontal_offset ** 2 + vertical_offset ** 2
        billow = .78 + .12 * sin(horizontal * .21 + vertical * .13) + .10 * cos(vertical * .27 - horizontal * .11)
        alpha = round(210 * exp(-distance * 3.8) * max(0, 1 - distance) * billow)
        image.putpixel((horizontal, vertical), (255, 255, 255, alpha))
image.save(ROOT / 'mods/extraction-raid/images/rmf/rarity-smoke.png')
