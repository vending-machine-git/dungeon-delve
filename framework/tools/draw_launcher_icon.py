"""Draw an original stone portal and descending steps; no purchased game art."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / 'framework/launcher'


def draw():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    canvas = Image.new('RGBA', (1024, 1024))
    ink = ImageDraw.Draw(canvas)
    ink.rounded_rectangle((30, 30, 994, 994), radius=208, fill='#11191e', outline='#ab8950', width=22)
    ink.rounded_rectangle((60, 60, 964, 964), radius=184, outline='#36444a', width=9)
    glow = Image.new('RGBA', canvas.size)
    aura = ImageDraw.Draw(glow)
    aura.ellipse((210, 135, 810, 915), fill=(45, 202, 182, 105))
    canvas = Image.alpha_composite(canvas, glow.filter(ImageFilter.GaussianBlur(65)))
    ink = ImageDraw.Draw(canvas)
    # The arch's broad silhouette stays readable at desktop-icon sizes.
    ink.rounded_rectangle((210, 135, 814, 900), radius=290, fill='#c1a773')
    ink.rectangle((210, 470, 814, 900), fill='#c1a773')
    ink.rounded_rectangle((314, 238, 710, 912), radius=194, fill='#143f42')
    ink.rectangle((314, 445, 710, 915), fill='#143f42')
    ink.arc((326, 249, 698, 632), 180, 360, fill='#68e7cc', width=17)
    ink.line((327, 440, 327, 818), fill='#68e7cc', width=15)
    ink.line((697, 440, 697, 818), fill='#68e7cc', width=15)
    for y in (486, 627, 768):
        ink.line((212, y, 310, y), fill='#6e6552', width=15)
        ink.line((712, y, 812, y), fill='#6e6552', width=15)
    ink.line((363, 173, 400, 262), fill='#6e6552', width=14)
    ink.line((661, 173, 624, 262), fill='#6e6552', width=14)
    ink.polygon([(467, 139), (557, 139), (545, 259), (479, 259)], fill='#e8ce95')
    # Stair treads lead into the portal, forming a compact descending mark.
    for x1, x2, top, bottom in ((455, 569, 557, 625), (418, 606, 645, 717),
                                  (374, 650, 737, 813), (309, 715, 833, 913)):
        ink.rectangle((x1, top, x2, bottom), fill='#b79963')
        ink.rectangle((x1, top, x2, top + 17), fill='#f1d49c')
        ink.rectangle((x1, bottom - 12, x2, bottom), fill='#685942')
    canvas = canvas.resize((512, 512), Image.Resampling.LANCZOS)
    canvas.save(OUTPUT / 'DungeonDelve.png')
    canvas.save(OUTPUT / 'DungeonDelve.ico', sizes=[(size, size) for size in (16, 24, 32, 48, 64, 128, 256)])
    print(OUTPUT / 'DungeonDelve.ico')


if __name__ == '__main__':
    draw()
