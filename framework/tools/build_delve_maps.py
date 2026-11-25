"""Build original empty canvases for Barony's installed native room generator.

No official rooms, maps or assets are copied into the package. At runtime the
owner's biome room sets supply the normal Barony architecture and decoration.
"""
from pathlib import Path
import struct,json
ROOT=Path(__file__).resolve().parents[2]
FLOORS=[('Delve Mines','mine',90,(56,20,8),36,52,56),
        ('Delve Jungle','swamp',90,(14,37,0),30,44,56),
        ('Delve Crystal Caves','caves',90,(155,156,156),24,36,56)]
def main():
 p=ROOT/'mods/extraction-raid/data/rmf/raid.json'
 config=json.loads(p.read_text());config['layout_version']=4;config['layouts']=[]
 for i,(name,theme,size,tiles,chests,drops,guards) in enumerate(FLOORS,1):
  data=b'BARONY'+name.encode().ljust(32,b'\0')+b'vending_machine'.ljust(32,b'\0')
  data+=struct.pack('<II',size,size)+struct.pack('<iii',*tiles)*(size*size)+struct.pack('<I',0)
  (ROOT/f'mods/extraction-raid/maps/delve{i}.lmp').write_bytes(data)
  config['layouts'].append(dict(name=name,generation='native',theme=theme,size=size,chest_target=chests,drop_target=drops,guard_target=guards))
 p.write_text(json.dumps(config,indent=2)+'\n')
 print(json.dumps(config['layouts'],indent=2))
if __name__=='__main__':main()
