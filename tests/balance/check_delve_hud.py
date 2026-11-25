"""Check native tour screenshots captured with BARONY_SMOKE_DELVE_HUD=1."""
from pathlib import Path
import argparse
import json
import math
import re
from PIL import Image, ImageChops

BRASS = (216, 179, 106)
CYAN = (133, 205, 214)
DANGER = (242, 109, 85)


def captured_geometry(screenshot):
    region = screenshot.crop((0, 0, screenshot.width * 2 // 5, screenshot.height // 2))
    difference = ImageChops.difference(region, Image.new('RGB', region.size, BRASS))
    red, green, blue = difference.split()
    mask = ImageChops.lighter(ImageChops.lighter(red, green), blue).point(lambda value: 255 if value <= 16 else 0)
    pixels = mask.tobytes()
    runs = []
    for row in range(region.height):
        for match in re.finditer(b'\xff{100,}', pixels[row * region.width:(row + 1) * region.width]):
            runs.append((match.end() - match.start(), match.start(), row))
    assert runs, 'Full countdown meter missing from initial screenshot'
    length, left, top = max(runs, key=lambda entry: entry[0])
    scale = length / 270
    return scale, left - 15 * scale, top - 91 * scale


def validate(profile):
    report = dict(status='RUNNING', profile=str(profile), checks=[])

    def check(passed, name):
        assert passed, name
        report['checks'].append(name)

    try:
        config = json.loads((profile / 'host/config/config.json').read_text())
        ui_scale = config.get('ui_scale', 100) / 100
        with Image.open(profile / 'views/1-0/host.png') as source:
            initial = source.convert('RGB')
        native_scale = min(initial.width / 1280, initial.height / 720) / (3 - 2 * ui_scale)
        scale, origin_x, origin_y = captured_geometry(initial)
        expected_scale = max(1.0, native_scale)
        check(abs(scale - expected_scale) <= expected_scale * .05,
              'card follows native UI scale within window-capture tolerance')
        for floor in range(1, 4):
            for phase in range(3):
                path = profile / 'views' / f'{floor}-{phase}' / 'host.png'
                with Image.open(path) as source:
                    screenshot = source.convert('RGB')
                def screen(left, top):
                    return (math.floor(left * scale + origin_x + .5),
                            math.floor(top * scale + origin_y + .5))

                def crop(left, top, width, height):
                    return screenshot.crop((*screen(left, top), *screen(left + width, top + height)))

                def matches(value, color):
                    return max(abs(channel - expected) for channel, expected in zip(value, color)) <= 16

                def count(region, color):
                    values = region.get_flattened_data() if hasattr(region, 'get_flattened_data') else region.getdata()
                    return sum(matches(value, color) for value in values)

                frame = f'floor {floor}, phase {phase}'
                check(count(crop(36, 12, 204, 22), BRASS) > 60 * scale * scale,
                      frame + ': title stays in top-left panel')
                check(count(crop(14, 193, 200, 15), CYAN) < 15 * scale * scale,
                      frame + ': no permanent passive/build panel')
                check(matches(crop(100, 45, 1, 1).getpixel((0, 0)), (30, 34, 42)),
                      frame + ': enlarged card has an opaque readable background')
                check(screen(0, 0)[1] >= math.ceil(64 * native_scale),
                      frame + ': card clears the native top icon row')
                check(crop(0, 0, 300, 100).width >= 285,
                      frame + ': card never shrinks below its readable base size')
                title = crop(36, 12, 204, 22)
                title_rows = [row for row in range(title.height)
                              if any(matches(title.getpixel((column, row)), BRASS) for column in range(title.width))]
                check(title_rows and max(title_rows) - min(title_rows) >= 8 * scale,
                      frame + ': title glyphs render larger than the old tiny font')
                removed_panel = crop(24, 160, 200, 30)
                values = removed_panel.get_flattened_data() if hasattr(removed_panel, 'get_flattened_data') else removed_panel.getdata()
                check(sum(value == (30, 34, 42) for value in values) < removed_panel.width * removed_panel.height * .1,
                      frame + ': no extra ability panel beneath match card')
                color = BRASS if phase == 0 else DANGER
                check(count(crop(186, 44, 100, 28), color) > 60 * scale * scale,
                      frame + ': timer uses correct urgency color')
                meter = crop(15, 91, 270, 2)
                fill = count(meter, color) / (meter.width * meter.height)
                check(fill > .9 if phase == 0 else .05 < fill < .2 if phase == 1 else fill < .01,
                      frame + ': collapse meter matches remaining time')
                for depth in range(1, 4):
                    point = screen(20 + (depth - 1) * 16, 80)
                    expected = BRASS if depth <= floor else (78, 76, 69)
                    check(matches(screenshot.getpixel(point), expected),
                          frame + f': depth marker {depth} matches floor')
        report['status'] = 'PASS'
    except BaseException as error:
        report.update(status='FAIL', error=repr(error))
        raise
    finally:
        (profile / 'hud-layout.json').write_text(json.dumps(report, indent=2) + '\n')
        print(report['status'], len(report['checks']), 'HUD checks;', profile / 'hud-layout.json', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('profiles', type=Path, nargs='+')
    for profile in parser.parse_args().profiles:
        validate(profile.resolve())


if __name__ == '__main__':
    main()
