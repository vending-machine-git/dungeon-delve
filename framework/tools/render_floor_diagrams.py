"""Render captured smoke-runtime terrain as a themed, interactive floor atlas."""
import argparse
import base64
import json
from pathlib import Path

TEMPLATE = r"""<!doctype html><html><head><meta charset="utf-8"><style>
*{box-sizing:border-box}body{margin:0;color:var(--foreground);font-family:var(--font-sans)}
.note{font-size:13px;color:var(--muted-foreground);line-height:1.5;margin:0 0 12px}
.key{display:flex;flex-wrap:wrap;gap:8px 17px;font-size:12px;align-items:center;margin-bottom:16px}
.key b{font-family:var(--font-mono);margin-right:5px}.spawn{color:var(--success)}.down{color:var(--info)}.escape{color:var(--chart-4)}.boss{color:var(--destructive)}.cache{color:var(--warning)}
.options{display:flex;gap:16px;margin:0 0 16px;font-size:12px;color:var(--muted-foreground)}input{accent-color:var(--primary);vertical-align:middle}
section{padding:16px;background:var(--card);border-radius:var(--radius);margin-bottom:16px}
header{display:flex;justify-content:space-between;gap:12px;align-items:baseline;margin-bottom:12px}h2{font-size:17px;margin:0;font-weight:650}header span{font-size:12px;color:var(--muted-foreground);white-space:nowrap}
.layout{display:grid;grid-template-columns:minmax(0,1fr) 125px;gap:16px;align-items:start}
canvas{display:block;width:100%;height:auto;border:1px solid var(--border);border-radius:6px;image-rendering:pixelated}
aside{font-size:12px;line-height:1.5;color:var(--muted-foreground)}aside strong{font-size:20px;display:block;color:var(--foreground);line-height:1.2;margin-top:13px}aside strong:first-child{margin-top:0}.bossname{color:var(--destructive);font-weight:600;margin:14px 0 0}.route{font-size:12px;color:var(--muted-foreground);margin:10px 0 0}.seed{font-family:var(--font-mono);font-size:10px;margin-top:15px;overflow-wrap:anywhere}
@media(max-width:520px){.layout{grid-template-columns:1fr}aside{display:flex;gap:14px;flex-wrap:wrap}aside strong{font-size:17px;margin-top:0}.seed{width:100%;margin:0}.bossname{margin:0}h2{font-size:15px}}
</style></head><body>
<p class="note">Real generated terrain from one captured match?not fixed map templates. Corridors and placements change each match. North is up; the separate staging strip is omitted. Doors and decorative props are not drawn.</p>
<div class="key"><span class="spawn"><b>S</b>Player start</span><span class="down"><b>D</b>Down to next floor</span><span class="escape"><b>E</b>Extraction</span><span class="boss"><b>B</b>Boss</span><span class="cache"><b>?</b>Loot cache</span><span><b>?</b>Loose loot</span></div>
<div class="options"><label><input id="loot" type="checkbox" checked> Show loot</label><label><input id="guards" type="checkbox"> Show guard positions</label><span>Water: blue ? Lava: orange</span></div>
<main id="atlas"></main><script>
const floors=FLOOR_DATA;
const root=document.getElementById('atlas');
const color=name=>getComputedStyle(document.documentElement).getPropertyValue('--'+name).trim();
floors.forEach(f=>{
 const section=document.createElement('section');
 const stairs=f.descents.length;
 section.innerHTML=`<header><h2>Floor ${f.floor} ? ${f.name}</h2><span>${f.size} ? ${f.size} tiles</span></header><div class="layout"><canvas role="img" aria-label="Terrain diagram of ${f.name}, with player starts, loot, exits and boss"></canvas><aside><div><strong>${f.chests.length}</strong>loot caches</div><div><strong>${f.loot.length}</strong>loose-loot stacks</div><div><strong>${f.guards.length}</strong>guards + boss</div><div><strong>${stairs ? stairs : 'Final'}</strong>${stairs ? 'routes deeper' : 'floor ? no stairs'}</div><div><strong>${f.exits.length}</strong>extraction ${f.exits.length===1?'point':'points'}</div><p class="bossname">${['Skeleton','Vampire','Crystal golem'][f.floor-1]} boss</p><p class="seed">Seed ${f.seed}</p></aside></div><p class="route">${stairs?'Descend for better loot?or use an extraction point to leave the match.':'No deeper floor. Extract from here to bank your loot.'}</p>`;
 root.appendChild(section);f.canvas=section.querySelector('canvas');
});
function draw(f){
 const unit=8,canvas=f.canvas;canvas.width=f.size*unit;canvas.height=(f.size-9)*unit;
 const ctx=canvas.getContext('2d'),bytes=atob(f.terrain);
 ctx.fillStyle=color('code-background');ctx.fillRect(0,0,canvas.width,canvas.height);
 const colors=[color('code-background'),color('secondary'),color('info'),color('warning')];
 for(let index=0;index<f.size*(f.size-9);index++){
  const tile=(bytes.charCodeAt(index>>2)>>((index%4)*2))&3;
  if(!tile)continue;
  ctx.fillStyle=colors[tile];ctx.fillRect((index%f.size)*unit,Math.floor(index/f.size)*unit,unit,unit);
  if(tile===1){ctx.globalAlpha=.22;ctx.fillStyle=color('chart-'+f.floor);ctx.fillRect((index%f.size)*unit,Math.floor(index/f.size)*unit,unit,unit);ctx.globalAlpha=1;}
 }
 const center=point=>[(point[0]+.5)*unit,(point[1]-8.5)*unit];
 if(document.getElementById('loot').checked){
  ctx.fillStyle=color('muted-foreground');for(const point of f.loot){const [x,y]=center(point);ctx.beginPath();ctx.arc(x,y,1.7,0,Math.PI*2);ctx.fill();}
  ctx.fillStyle=color('warning');for(const point of f.chests){const [x,y]=center(point);ctx.fillRect(x-2.2,y-2.2,4.4,4.4);}
 }
 if(document.getElementById('guards').checked){ctx.strokeStyle=color('destructive');ctx.lineWidth=1.2;for(const point of f.guards){const [x,y]=center(point);ctx.beginPath();ctx.moveTo(x-2,y-2);ctx.lineTo(x+2,y+2);ctx.moveTo(x+2,y-2);ctx.lineTo(x-2,y+2);ctx.stroke();}}
 function marker(point,label,tone){const [x,y]=center(point);ctx.fillStyle=color(tone);ctx.strokeStyle=color('background');ctx.lineWidth=1.5;ctx.beginPath();ctx.arc(x,y,6.7,0,Math.PI*2);ctx.fill();ctx.stroke();ctx.fillStyle=color('background');ctx.font='bold 9px sans-serif';ctx.textAlign='center';ctx.textBaseline='middle';ctx.fillText(label,x,y+.5);}
 for(const point of f.spawns)marker(point,'S','success');
 for(const point of f.descents)marker(point,'D','info');
 for(const point of f.exits)marker(point,'E','chart-4');
 marker(f.boss,'B','destructive');
}
function redraw(){floors.forEach(draw)}
document.getElementById('loot').onchange=redraw;document.getElementById('guards').onchange=redraw;redraw();
</script></body></html>"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('profile', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    floors = []
    for floor in range(1, 4):
        source = json.loads((args.profile / f'delve-generated-floor{floor}.json').read_text())
        tiles = [{'#': 0, ' ': 0, '.': 1, '~': 2, '^': 3}[tile]
                 for row in source['diagram_tiles'][9:] for tile in row]
        packed = bytes(sum(value << (offset * 2) for offset, value in enumerate(tiles[start:start + 4]))
                       for start in range(0, len(tiles), 4))
        diagram = {key: source[key] for key in ('name', 'size', 'seed')}
        diagram.update(floor=floor, terrain=base64.b64encode(packed).decode())
        for target, key in [('spawns', 'spawns'), ('descents', 'descents'), ('exits', 'portals'),
                            ('chests', 'chests'), ('loot', 'drops'), ('guards', 'encounters')]:
            diagram[target] = [[point['x'], point['y']] for point in source[key]]
        diagram['boss'] = [source['boss']['x'], source['boss']['y']]
        floors.append(diagram)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(TEMPLATE.replace('FLOOR_DATA', json.dumps(floors, separators=(',', ':'))), encoding='utf-8')
    print(args.output.resolve())


if __name__ == '__main__':
    main()
