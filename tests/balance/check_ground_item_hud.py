"""Check multiline native ground-card renders from the appraisal smoke scenarios."""
import argparse
import json
from pathlib import Path
from PIL import Image


def validate(profile):
    checks = []
    for role in ('host', 'client1'):
        for index in range(8):
            path = profile / role / f'hover-stats-{index}.png'
            with Image.open(path) as source:
                image = source.convert('RGB')
                assert image.width <= 400, (path, 'tooltip exceeds wrapped card width', image.size)
                assert image.height >= 115, (path, 'missing gameplay information', image.size)
                checks.append(f'{role} category {index}: bounded card includes gameplay information')
                if index == 0:
                    rows = []
                    for vertical in range(84, image.height - 10):
                        count = sum(red > 140 and 70 < green < red and blue < green
                                    for red, green, blue in (image.getpixel((horizontal, vertical))
                                    for horizontal in range(16, image.width - 16)))
                        rows.append(count > 4)
                    occupied = [offset for offset, value in enumerate(rows) if value]
                    groups = sum(index == 0 or offset - occupied[index - 1] > 5 for index, offset in enumerate(occupied))
                    assert groups == 2, (path, 'staff damage and slow effect must occupy separate text rows', groups)
                    checks.append(f'{role}: cold staff damage and slow effect render as two separate lines')
    (profile / 'ground-tooltip-layout.json').write_text(json.dumps(dict(status='PASS', checks=checks), indent=2) + '\n')
    print('PASS', len(checks), 'ground tooltip layout checks;', profile)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('profiles', type=Path, nargs='+')
    for profile in parser.parse_args().profiles:
        validate(profile.resolve())
