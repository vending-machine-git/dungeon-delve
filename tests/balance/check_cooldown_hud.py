"""Verify native cooldown rings from the isolated cooldowns smoke scenario."""
import argparse
import json
import math
from pathlib import Path
from PIL import Image
from check_delve_hud import captured_geometry

BRASS = (216, 179, 106)
CYAN = (133, 205, 214)
IVORY = (231, 223, 202)


def validate(profile):
    report = dict(status='RUNNING', checks=[])

    def check(condition, description):
        assert condition, description
        report['checks'].append(description)

    screenshots = {}
    try:
        for phase in range(5):
            with Image.open(profile / 'cooldowns' / str(phase) / 'host.png') as source:
                screenshots[phase] = source.convert('RGB')
        scale, origin_x, origin_y = captured_geometry(screenshots[0])

        def pixels(phase, left, top, width, height):
            screenshot = screenshots[phase]
            bounds = (round(left * scale + origin_x), round(top * scale + origin_y),
                      round((left + width) * scale + origin_x), round((top + height) * scale + origin_y))
            return screenshot.crop(bounds)

        def matches(pixel, color):
            return max(abs(component - expected) for component, expected in zip(pixel, color)) <= 8

        def ring_count(phase, left, top, color):
            image = pixels(phase, left, top, 52, 52)
            total = 0
            for row in range(image.height):
                for column in range(image.width):
                    radius = math.hypot((column + .5) / image.width - .5, (row + .5) / image.height - .5)
                    if .425 < radius < .485 and matches(image.getpixel((column, row)), color):
                        total += 1
            return total

        for phase in range(5):
            for slot in range(2):
                check(ring_count(phase, 48, 164 + slot * 82, BRASS) < 8 * scale * scale,
                      f'ability {slot + 1}: no duplicate left cooldown ring in phase {phase}')
        for passive, color in [(True, CYAN)]:
            left = 190 if passive else 48
            kind = 'passive' if passive else 'ability'
            for slot in range(2):
                top = 164 + slot * 82
                full = ring_count(0, left, top, color)
                half = ring_count(1, left, top, color)
                check(full > 35 * scale * scale, f'{kind} {slot + 1}: round colored ring appears on cooldown')
                check(full * .3 < half < full * .7, f'{kind} {slot + 1}: radial meter reflects half recovery')
                check(ring_count(2, left, top, color) < full * .1, f'{kind} {slot + 1}: ready skills disappear')
                check(ring_count(3, left, top, color) < full * .1, f'{kind} {slot + 1}: inventory hides meter')
                check(ring_count(4, left, top, color) > full * .3, f'{kind} {slot + 1}: meter returns after closing inventory')
                timer = pixels(0, left + 4, top + 23, 44, 22)
                values = timer.get_flattened_data() if hasattr(timer, 'get_flattened_data') else timer.getdata()
                check(sum(matches(pixel, IVORY) for pixel in values) > 8 * scale * scale,
                      f'{kind} {slot + 1}: remaining seconds are readable')
                check(top > 152, f'{kind} {slot + 1}: clears match card and trap warning')
        report.update(status='PASS', scale=scale)
    except BaseException as error:
        report.update(status='FAIL', error=repr(error))
        raise
    finally:
        (profile / 'cooldown-layout.json').write_text(json.dumps(report, indent=2) + '\n')
        print(report['status'], len(report['checks']), 'cooldown HUD checks;', profile, flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('profiles', type=Path, nargs='+')
    for profile in parser.parse_args().profiles:
        validate(profile.resolve())


if __name__ == '__main__':
    main()
