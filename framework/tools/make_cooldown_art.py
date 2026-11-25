"""Generate original geometric Delve cooldown masks; no game art is copied."""
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]


def main():
    destination = ROOT / 'mods/extraction-raid/images/rmf'
    destination.mkdir(parents=True, exist_ok=True)
    for name, inner in [('disc', False), ('ring', True)]:
        image = Image.new('RGBA', (512, 512))
        drawing = ImageDraw.Draw(image)
        drawing.ellipse((4, 4, 507, 507), fill='white')
        if inner:
            drawing.ellipse((40, 40, 471, 471), fill=(0, 0, 0, 0))
        image.resize((128, 128), Image.Resampling.LANCZOS).save(destination / f'cooldown-{name}.png')
    print('Generated two original cooldown masks')


if __name__ == '__main__':
    main()
