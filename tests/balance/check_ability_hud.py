"""Compare dedicated ability keycaps against installed native art in real captures."""
import argparse
import json
from pathlib import Path
from PIL import Image, ImageChops

GAME = Path(r'F:\Steam\steamapps\common\Barony')


def glyph_template(key, scale, offset=None):
    folder = 'NamedKeys' if key == 'Space' else 'Alphabet'
    path = GAME / 'images/ui/Glyphs/KeyboardGlyphs/Rendered' / folder / f'Unpressed_Text_{key}_00.png'
    with Image.open(path) as source:
        source = source.convert('RGBA')
        size = (int(source.width * scale), int(source.height * scale))
        if offset is not None:
            return source.transform(size, Image.Transform.AFFINE,
                                    (1 / scale, 0, -offset[0] / scale, 0, 1 / scale, -offset[1] / scale),
                                    Image.Resampling.NEAREST)
        return source.resize(size, Image.Resampling.NEAREST)


def samples(template):
    result = []
    for row in range(3, template.height - 2, max(1, template.height // 6)):
        for column in range(3, template.width - 2, max(1, template.width // 6)):
            pixel = template.getpixel((column, row))
            if pixel[3] == 255 and min(pixel[:3]) > 30:
                result.append((column, row, pixel[:3]))
    assert len(result) >= 8
    return result


def matches(pixels, left, top, expected):
    return all(max(abs(actual - wanted) for actual, wanted in zip(pixels[left + column, top + row], color)) <= 9
               for column, row, color in expected)


def find_glyph(image, template):
    pixels = image.load()
    expected = samples(template)
    for top in range(round(image.height * .65), image.height - template.height):
        for left in range(0, image.width // 2 - template.width):
            if matches(pixels, left, top, expected):
                return left, top
    return None


def find_scaled_glyph(image, key, scale):
    template = glyph_template(key, scale)
    position = find_glyph(image, template)
    if position is not None:
        return template, position
    import numpy as np
    pixels = np.asarray(image, dtype=np.int16)
    top = round(image.height * .65)
    height = image.height - top - template.height
    width = image.width // 2 - template.width
    for vertical in range(8):
        for horizontal in range(8):
            template = glyph_template(key, scale, (horizontal / 8, vertical / 8))
            candidates = np.ones((height, width), dtype=bool)
            for column, row, color in samples(template):
                actual = pixels[top + row:top + row + height, column:column + width]
                candidates &= np.max(np.abs(actual - color), axis=2) <= 9
                if not candidates.any():
                    break
            if candidates.any():
                row, column = np.argwhere(candidates)[0]
                return template, (int(column), int(row) + top)
    raise AssertionError('Native keycap not found: ' + key)


def validate(profile):
    report = dict(status='RUNNING', checks=[])
    config = json.loads((profile / 'host/config/config.json').read_text())

    def check(condition, label):
        assert condition, label
        report['checks'].append(label)

    try:
        for role in ('host', 'client1'):
            captures = {}
            for phase in range(7):
                with Image.open(profile / 'abilities' / str(phase) / (role + '.png')) as source:
                    captures[phase] = source.convert('RGB')
            scale = min(captures[0].width / 1280, captures[0].height / 720) / (3 - 2 * config['ui_scale'] / 100)
            positions = {}
            for key in ('Q', 'E', 'Space'):
                template, (left, top) = find_scaled_glyph(captures[0], key, scale)
                positions[key] = (left, top, template.width, template.height)
                check(True, f'{role}: exact native {key} keycap is visible when ready')
                if key == 'Space':
                    continue
                expected = samples(template)
                for phase in (1, 2, 3, 4, 6):
                    check(matches(captures[phase].load(), left, top, expected),
                          f'{role}: {key} keycap stays visible in phase {phase}')
                check(matches(captures[5].load(), left, top, expected) == (role != 'host'),
                      f'{role}: only the player opening inventory hides the {key} prompt')
                frame_left = left + template.width / 2 - 22 * scale
                frame_top = top - 38 * scale
                bounds = tuple(round(value) for value in (frame_left, frame_top, frame_left + 44 * scale, frame_top + 44 * scale))
                image = captures[0].crop(bounds)
                pixels = image.get_flattened_data() if hasattr(image, 'get_flattened_data') else image.getdata()
                green = sum(green > 40 and green > red * 1.4 and green > blue * 1.3 for red, green, blue in pixels)
                check(green > 35 * scale * scale, f'{role}: {key} icon uses native green frame')
                check(frame_left >= 200 * scale or top + template.height < captures[0].height - 88 * scale,
                      f'{role}: {key} prompt clears health and mana bars')
                if role == 'host':
                    active_phase = 1 if key == 'Q' else 2
                    check(ImageChops.difference(captures[0].crop(bounds), captures[active_phase].crop(bounds)).getbbox() is not None,
                          f'{role}: {key} icon changes to cooldown state after real keypress')
            first, second, space = (positions[key] for key in ('Q', 'E', 'Space'))
            check(first[0] < second[0], f'{role}: Q and E read left to right')
            check(abs((second[0] - first[0]) - 60 * scale) < 3 * scale,
                  f'{role}: native action-prompt spacing is preserved')
            aligned = abs(second[1] - space[1]) < 8 * scale
            wrapped = second[1] < space[1] - 60 * scale and abs(second[0] + second[2] / 2 - space[0] - space[2] / 2) < 3 * scale
            check(aligned or wrapped, f'{role}: abilities sit beside Space or wrap directly above it at narrow resolution')
        report['status'] = 'PASS'
    except BaseException as error:
        report.update(status='FAIL', error=repr(error))
        raise
    finally:
        (profile / 'ability-layout.json').write_text(json.dumps(report, indent=2) + '\n')
        print(report['status'], len(report['checks']), 'ability prompt screenshot checks;', profile, flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('profiles', type=Path, nargs='+')
    for profile in parser.parse_args().profiles:
        validate(profile.resolve())
