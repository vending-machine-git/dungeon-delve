"""Master the owner's narration without changing its performance or timing."""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import wave

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--input', type=Path)
    parser.add_argument('--floor', type=int, choices=(1, 2, 3), default=1)
    parser.add_argument('--ffmpeg', default=str(Path.home()/'.local/bin/ffmpeg.exe'))
    args = parser.parse_args()
    stem = 'dungeon-intro' if args.floor == 1 else f'dungeon-floor-{args.floor}'
    args.input = args.input or ROOT/'framework/audio/source'/(stem+'-original.wav')
    with wave.open(str(args.input)) as recording:
        duration = recording.getnframes()/recording.getframerate()
    if duration <= .1:
        raise ValueError('Narration is too short')
    output = ROOT/'framework/audio/masters'
    evidence = ROOT/'modding-evidence/intro-audio'
    if args.floor != 1:
        evidence = evidence/f'floor-{args.floor}'
    delivery = ROOT/'mods/extraction-raid/music/rmf'/(stem+'.wav')
    for folder in (output, evidence, delivery.parent):
        folder.mkdir(parents=True, exist_ok=True)
    filters = f'adeclip,highpass=f=65,afftdn=nr=6:nf=-48:tn=0,equalizer=f=220:t=q:w=0.8:g=-1.5,equalizer=f=2600:t=q:w=0.8:g=2,deesser=i=0.12:m=0.25:f=0.5,acompressor=threshold=0.18:ratio=2:attack=12:release=120:makeup=1,atrim=start=0.025,asetpts=PTS-STARTPTS,apad=pad_dur=0.025,afade=t=in:st=0:d=0.025,afade=t=out:st={duration - .08:.6f}:d=0.08'
    def run(arguments):
        result = subprocess.run([args.ffmpeg, '-hide_banner', '-y', '-i', str(args.input), *arguments], capture_output=True, text=True, check=True)
        return result.stderr
    analysis = run(['-af', filters+',loudnorm=I=-16:TP=-1.5:LRA=7:print_format=json', '-f', 'null', 'NUL'])
    measurement = json.loads(analysis[analysis.rfind('{'):analysis.rfind('}')+1])
    normalize = ',loudnorm=I=-16:TP=-1.5:LRA=7:linear=true:measured_I={input_i}:measured_TP={input_tp}:measured_LRA={input_lra}:measured_thresh={input_thresh}:offset={target_offset}:print_format=json'.format(**measurement)
    master = output/(stem+'-master.wav')
    render_log = run(['-af', filters+normalize, '-ar', '48000', '-ac', '2', '-c:a', 'pcm_s24le', str(master)])
    subprocess.run([args.ffmpeg, '-hide_banner', '-y', '-i', str(master), '-ar', '48000', '-ac', '2', '-c:a', 'pcm_s16le', str(delivery)], capture_output=True, check=True)
    preview = output/(stem+'-cleaned.ogg')
    subprocess.run([args.ffmpeg, '-hide_banner', '-y', '-i', str(master), '-c:a', 'libvorbis', '-q:a', '6', str(preview)], capture_output=True, check=True)
    verification = subprocess.run([args.ffmpeg, '-hide_banner', '-i', str(delivery), '-af', 'loudnorm=I=-16:TP=-1.5:LRA=7:print_format=json', '-f', 'null', 'NUL'], capture_output=True, text=True, check=True).stderr
    final = json.loads(verification[verification.rfind('{'):verification.rfind('}')+1])
    report = dict(input=str(args.input), original_sha256=hashlib.sha256(args.input.read_bytes()).hexdigest(), filters=filters, normalization=normalize, measured_source_after_restoration=measurement, measured_delivery=final, master=str(master), delivery=str(delivery), preview=str(preview), preserves_performance=True)
    (evidence/'mastering.json').write_text(json.dumps(report, indent=2)+'\n')
    (evidence/'ffmpeg-analysis.log').write_text(analysis+'\n'+render_log+'\n'+verification)
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
