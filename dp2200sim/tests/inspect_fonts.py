"""Compare fonts loaded by the real 5500 ROM and installed DOS.D; export glyphs."""
import argparse
import hashlib
import json
from pathlib import Path
from install_dos_d import machine, Session


def svg(glyphs, codes, columns, scale=3):
    rows = (len(codes) + columns - 1) // columns
    width, height = columns*7, rows*9
    dots = []
    for cell, code in enumerate(codes):
        for x, bits in enumerate(glyphs[code & 127]):
            for y in range(7):
                if bits & (64 >> y):
                    dots.append(f'<rect x="{cell%columns*7+x+1}" y="{cell//columns*9+y+1}" width="1" height="1"/>')
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{width*scale}" height="{height*scale}" '
            f'viewBox="0 0 {width} {height}" shape-rendering="crispEdges">'
            f'<rect width="{width}" height="{height}" fill="black"/>'
            '<g fill="#00ff00">' + ''.join(dots) + '</g></svg>\n')


def investigate(installation, output):
    output.mkdir(parents=True, exist_ok=True)
    with machine(installation) as sim:
        rom = sim.command('font-state')['font']
        # Follow the real ROM cassette bootstrap, rather than jumping to tape code.
        sim.command(f'tape 0 {installation / "boot.tap"}')
        sim.command(f'pc {0o175724}')
        session = Session(sim, output, 'dos-font-boot')
        session.until('READY', budget=30000000)
        dos = sim.command('font-state')['font']
        flags = sim.command(f'memory {0o1377} 1')['memory'][0]
        assert flags & 2, 'DOS did not detect the RAM display'
        assert dos['writes'] - rom['writes'] == 640, 'DOS did not load all 128 glyphs'
        for name, font in [('rom', rom), ('dos', dos)]:
            (output / f'{name}-font.json').write_text(json.dumps(font, indent=2)+'\n')
            (output / f'{name}-glyphs.svg').write_text(svg(font['glyphs'], list(range(128)), 16, 5))
        codes = [ord(c) for c in session.state['screen'] if c != '\n']
        (output / 'dos-ready.svg').write_text(svg(dos['glyphs'], codes, 80))
        (output / 'dos-ready.frame').write_bytes(bytes(sum(dos['glyphs'], [])) + bytes(codes))
        # A synthetic example of tile graphics; these are custom glyphs, not
        # a screenshot from a historical Datapoint application.
        demo_font = [glyph[:] for glyph in dos['glyphs']]
        demo_font[1:8] = [[15,8,8,8,8], [8,8,8,8,15],
                          [120,8,8,8,8], [8,8,8,8,120],
                          [8]*5, [127,0,0,0,0], [0,0,0,0,127]]
        demo = bytearray(b' '*960)
        left,right,top,bottom=5,73,2,9
        for x in range(left+1,right):
            demo[top*80+x]=demo[bottom*80+x]=5
        for y in range(top+1,bottom):
            demo[y*80+left]=6;demo[y*80+right]=7
        for x,y,code in [(left,top,1),(right,top,2),(left,bottom,3),(right,bottom,4)]:
            demo[y*80+x]=code
        for y,label in [(4,b'5500 PROGRAMMABLE FONT'),(6,b'Custom glyphs make screen forms'),(7,b'Every cell sharing a code uses the same glyph')]:
            start=y*80+10;demo[start:start+len(label)]=label
        (output / 'custom-form.svg').write_text(svg(demo_font, demo, 80))
        (output / 'custom-form.frame').write_bytes(bytes(sum(demo_font, []))+demo)
        session.keys('CAT :D0\n')
        session.until('READY', after='SYSTEM0/SYS', budget=10000000)
        assert sim.command('font-state')['font']['glyphs'] == dos['glyphs'], 'CAT changed font'
        changed = [f'{i:03o}' for i in range(128) if rom['glyphs'][i] != dos['glyphs'][i]]
        summary = {'rom_writes': rom['writes'], 'dos_total_writes': dos['writes'],
                   'dos_added_writes': dos['writes']-rom['writes'],
                   'changed_glyphs': changed, 'changed_count': len(changed),
                   'dos_flags_octal': f'{flags:03o}',
                   'dos_font_sha256': hashlib.sha256(bytes(sum(dos['glyphs'], []))).hexdigest(),
                   'catalogue_keeps_font': True}
        (output / 'comparison.json').write_text(json.dumps(summary, indent=2)+'\n')
        return summary


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--installation', type=Path, default=Path(__file__).resolve().parents[1]/'dos-d-results/dos-d-2.6')
    parser.add_argument('--output', type=Path, default=Path(__file__).resolve().parents[1]/'font-results')
    args = parser.parse_args()
    print(json.dumps(investigate(args.installation.resolve(), args.output.resolve()), indent=2))
