"""Check native results screenshots and the dismissed side-summary layout."""
from pathlib import Path
import argparse
import json
from PIL import Image

BRASS = (216, 179, 106)
IVORY = (231, 223, 202)
DANGER = (242, 109, 85)


def pixels(image, color):
    channels = image.tobytes()
    return sum(max(abs(actual - wanted) for actual, wanted in zip(pixel, color)) <= 24
               for pixel in zip(channels[0::3], channels[1::3], channels[2::3]))


def validate(profile):
    checks = []
    config = json.loads((profile / 'host/config/config.json').read_text())
    for role in ('host', 'client1', 'client2', 'client3'):
        with Image.open(profile / (role + '.png')) as source:
            image = source.convert('RGB')
        scale = min(image.width / 1280, image.height / 720) / (3 - 2 * config['ui_scale'] / 100)
        left = image.width / 2 - 250 * scale
        top = image.height / 2 - 168 * scale

        def crop(horizontal, vertical, width, height):
            return image.crop((round(left + horizontal * scale), round(top + vertical * scale),
                               round(left + (horizontal + width) * scale), round(top + (vertical + height) * scale)))

        def check(condition, name):
            assert condition, (profile, role, name)
            checks.append(role + ': ' + name)

        check(pixels(crop(110, 90, 280, 20), (170, 114, 42)) > 25 * scale * scale,
              'gold heading renders in native banner')
        check(pixels(crop(94, 224, 312, 48), (170, 134, 102)) > 40 * scale * scale,
              'native placement footer remains readable')
        for row in range(4):
            check(pixels(crop(112, 141 + row * 21, 122, 21), DANGER) > 5 * scale * scale,
                  f'row {row + 1} outcome renders within its own row')
            check(pixels(crop(358, 134 + row * 21, 44, 14), IVORY) > 8 * scale * scale,
                  f'row {row + 1} monster total remains visible')
        check(pixels(image.crop((12, 80, 300, 110)), BRASS) < 20,
              'no duplicate side results while native window is open')
        with Image.open(profile / 'dismissed' / (role + '.png')) as source:
            dismissed = source.convert('RGB')
        hud_scale = max(1.0, scale)
        origin_left, origin_top = round(12 * scale), round(80 * scale)
        heading = dismissed.crop((origin_left + round(36 * hud_scale), origin_top + round(12 * hud_scale),
                                  origin_left + round(286 * hud_scale), origin_top + round(34 * hud_scale)))
        check(pixels(heading, BRASS) > 30 * hud_scale * hud_scale,
              'dismiss replaces centered window with compact side summary')
    (profile / 'results-ui-layout.json').write_text(json.dumps(dict(status='PASS', checks=checks), indent=2) + '\n')
    print('PASS', len(checks), 'results layout checks;', profile)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('profiles', type=Path, nargs='+')
    for profile in parser.parse_args().profiles:
        validate(profile.resolve())
