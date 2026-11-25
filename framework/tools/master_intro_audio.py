"""Restore the owner's takes and render an original Herx-inspired voice effect.

Run with framework/audio-tools/Scripts/python.exe. No stock audio is sampled.
The centered lead, sample count and subtitle timeline retain the performance.
"""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import tempfile
import wave

import numpy as np
from scipy import ndimage, signal
import soundfile as sf

ROOT = Path(__file__).resolve().parents[2]
RATE = 48000
REVISION = 'depop-dungeon-1'


def run(ffmpeg, *arguments):
    return subprocess.run([ffmpeg, '-hide_banner', '-y', *map(str, arguments)],
                          capture_output=True, check=True)


def decode(ffmpeg, path):
    raw = run(ffmpeg, '-i', path, '-ar', RATE, '-ac', '2', '-f', 'f32le', '-').stdout
    return np.frombuffer(raw, dtype='<f4').reshape(-1, 2).astype(np.float64)


def fit(data, frames):
    return np.pad(data[:frames], (0, max(0, frames-len(data))))


def depop(lead):
    """Turn down transient low-band overloads while retaining the vocal body."""
    low = signal.sosfiltfilt(signal.butter(4, 180, fs=RATE, output='sos'), lead)
    body = signal.sosfiltfilt(signal.butter(3, (200, 3500), btype='bandpass',
                                        fs=RATE, output='sos'), lead)
    step = 240
    positions = np.arange(0, len(lead), step)
    low_rms = np.sqrt(ndimage.uniform_filter1d(low*low, 1440)[positions]+1e-12)
    body_rms = np.sqrt(ndimage.uniform_filter1d(body*body, 1440)[positions]+1e-12)
    baseline = ndimage.gaussian_filter1d(low_rms, 40)
    excess = np.minimum(low_rms/(body_rms*1.65+.008),
                        low_rms/(baseline*1.35+.008))
    reduction = np.clip(20*np.log10(np.maximum(excess, 1)), 0, 12)
    reduction = ndimage.maximum_filter1d(reduction, 7)
    reduction = ndimage.gaussian_filter1d(reduction, 3)
    gain = np.interp(np.arange(len(lead)), positions, 10**(-reduction/20))
    restored = lead-low+low*gain
    report = dict(max_low_band_reduction_db=round(float(np.max(reduction)), 3),
                  treated_seconds=round(float(np.sum(reduction>.5)*step/RATE), 3))
    return restored, report


def level_phrases(lead, cues):
    """Ride whole spoken phrases smoothly, without boosting gaps or breaths."""
    levels = []
    for cue in cues:
        phrase = lead[int(cue['start']*RATE):int(cue['end']*RATE)]
        rms = np.sqrt(np.mean(phrase*phrase)+1e-12)
        levels.append(20*np.log10(rms))
    corrections = np.clip((np.median(levels)-np.asarray(levels))*.9, -6, 6)
    times, gains = [], []
    for cue, correction in zip(cues, corrections):
        times.extend((cue['start'], cue['end']))
        gains.extend((correction, correction))
    positions = np.arange(0, len(lead), 240)
    gain_db = np.interp(positions/RATE, times, gains)
    gain_db = ndimage.gaussian_filter1d(gain_db, 20)
    gain = np.interp(np.arange(len(lead)), positions, 10**(gain_db/20))
    return lead*gain, dict(cue_gain_db=[round(float(x), 3) for x in corrections],
                           transition_seconds=.1)


def dungeon_effect(ffmpeg, clean_path, clean, work):
    """Centered natural lead, dark pitched doubles and a diffuse stereo room."""
    layers = []
    for channel, semitones in enumerate((-3.0, -3.17)):
        path = work/f'double-{channel}.wav'
        pitch = 2**(semitones/12)
        run(ffmpeg, '-i', clean_path, '-af',
            'pan=mono|c0=0.5*c0+0.5*c1,'
            f'rubberband=pitch={pitch:.9f}:tempo=1:formant=preserved:pitchq=quality,'
            'highpass=f=150,lowpass=f=4300',
            '-ar', RATE, '-ac', '1', '-c:a', 'pcm_f32le', path)
        layer, _ = sf.read(path, dtype='float64')
        delay = int(RATE*(.011 if channel == 0 else .019))
        layers.append(fit(np.pad(layer, (delay, 0)), len(clean)))

    rendered = np.column_stack([clean+.24*layer for layer in layers])
    for channel in range(2):
        # Original deterministic impulse; purchased audio is never sampled.
        rng = np.random.default_rng(731970+channel)
        length = int(RATE*1.25)
        t = np.arange(length)/RATE
        impulse = rng.standard_normal(length)*np.exp(-np.log(1000)*t/.9)
        impulse = signal.sosfilt(signal.butter(2, (220, 4800), btype='bandpass',
                                              fs=RATE, output='sos'), impulse)
        impulse /= np.sqrt(np.sum(impulse*impulse))
        impulse = np.pad(impulse, (int(RATE*.027), 0))
        wet = signal.fftconvolve(clean, impulse)[:len(clean)]
        rendered[:, channel] += .20*wet
        for delay, gain in ((.061, .065), (.113, .035), (.181, .020)):
            offset = int(RATE*(delay+channel*.007))
            rendered[offset:, channel] += gain*clean[:-offset]
    return rendered


def measurement(ffmpeg, path):
    log = run(ffmpeg, '-i', path, '-af',
              'loudnorm=I=-16:TP=-1.5:LRA=5:print_format=json',
              '-f', 'null', '-').stderr.decode('utf-8', errors='replace')
    return json.loads(log[log.rfind('{'):log.rfind('}')+1]), log


def normalize(ffmpeg, input_path, output_path, frames):
    measured, analysis = measurement(ffmpeg, input_path)
    gain_db = -16-float(measured['input_i'])
    for _ in range(3):
        # Fixed gain preserves the phrase balance. Oversampled, linked limiting
        # catches remaining peaks without a moving loudness target in short takes.
        normalization = (f'volume={gain_db:.6f}dB,aresample=192000,'
                         'alimiter=limit=0.82224:level=false:attack=5:release=80:latency=true,'
                         f'aresample={RATE},apad,atrim=end_sample={frames}')
        result = run(ffmpeg, '-i', input_path, '-af', normalization,
                     '-ar', RATE, '-ac', '2', '-c:a', 'pcm_s24le', output_path)
        final, log = measurement(ffmpeg, output_path)
        analysis += result.stderr.decode(errors='replace')+log
        difference = -16-float(final['input_i'])
        if abs(difference) <= .1:
            break
        gain_db += difference
    return measured, normalization, analysis


def master(args, floor):
    stem = 'dungeon-intro' if floor == 1 else f'dungeon-floor-{floor}'
    source = args.input or ROOT/'framework/audio/source'/(stem+'-original.wav')
    with wave.open(str(source)) as recording:
        if recording.getframerate() != RATE or recording.getsampwidth() != 2:
            raise ValueError('Expected the preserved 48 kHz, 16-bit source take')
        frames = recording.getnframes()
    duration = frames/RATE
    if duration <= .1:
        raise ValueError('Narration is too short')
    output = ROOT/'framework/audio/masters'
    evidence = ROOT/'modding-evidence/intro-audio'
    if floor != 1:
        evidence = evidence/f'floor-{floor}'
    delivery = ROOT/'mods/extraction-raid/music/rmf'/(stem+'.wav')
    for folder in (output, evidence, delivery.parent):
        folder.mkdir(parents=True, exist_ok=True)
    # afftdn buffers 25 ms. Compensate once before dynamics and effects.
    restoration = (
        'adeclip,highpass=f=85:p=2,afftdn=nr=8:nf=-48:tn=0,'
        'equalizer=f=240:t=q:w=0.8:g=-2,equalizer=f=2800:t=q:w=0.8:g=2.5,'
        'deesser=i=0.15:m=0.3:f=0.5,atrim=start_sample=1200,'
        f'asetpts=PTS-STARTPTS,apad,atrim=end_sample={frames}'
    )
    dynamics = (
        'acompressor=threshold=0.125:ratio=3:attack=4:release=110:'
        'makeup=1:knee=3:detection=rms,'
        f'apad,atrim=end_sample={frames},afade=t=in:st=0:d=0.012,'
        f'afade=t=out:st={duration-.08:.6f}:d=0.08'
    )
    with tempfile.TemporaryDirectory(prefix=stem+'-') as temporary:
        work = Path(temporary)
        restored = work/'restored.wav'
        run(args.ffmpeg, '-i', source, '-af', restoration, '-ar', RATE,
            '-ac', '2', '-c:a', 'pcm_f32le', restored)
        lead = decode(args.ffmpeg, restored).mean(axis=1)
        lead, depop_report = depop(lead)
        depopped = work/'depopped.wav'
        sf.write(depopped, np.column_stack((lead, lead)), RATE, subtype='FLOAT')
        controlled = work/'controlled.wav'
        run(args.ffmpeg, '-i', depopped, '-af', dynamics, '-ar', RATE,
            '-ac', '2', '-c:a', 'pcm_f32le', controlled)
        asset = 'intro' if floor == 1 else f'intro-floor-{floor}'
        cues = json.loads((ROOT/'mods/extraction-raid/data/rmf'/(asset+'.json')).read_text())['cues']
        lead, level_report = level_phrases(decode(args.ffmpeg, controlled).mean(axis=1), cues)
        leveled = work/'leveled.wav'
        sf.write(leveled, np.column_stack((lead, lead)), RATE, subtype='FLOAT')
        clean_master = output/(stem+'-clean-master.wav')
        normalize(args.ffmpeg, leveled, clean_master, frames)
        clean = decode(args.ffmpeg, clean_master).mean(axis=1)
        effected = dungeon_effect(args.ffmpeg, clean_master, clean, work)
        fade_frames = min(int(.08*RATE), frames)
        effected[-fade_frames:] *= np.linspace(1, 0, fade_frames)[:, None]
        effect_path = work/'dungeon.wav'
        sf.write(effect_path, effected, RATE, subtype='FLOAT')
        master_path = output/(stem+'-master.wav')
        measured, normalization, analysis = normalize(args.ffmpeg, effect_path,
                                                     master_path, frames)
        run(args.ffmpeg, '-i', master_path, '-ar', RATE, '-ac', '2',
            '-c:a', 'pcm_s16le', delivery)
        preview = output/(stem+'-dungeon.ogg')
        clean_preview = output/(stem+'-cleaned.ogg')
        for path, target in ((master_path, preview), (clean_master, clean_preview)):
            run(args.ffmpeg, '-i', path, '-c:a', 'libvorbis', '-q:a', '6', target)
        final, verification = measurement(args.ffmpeg, delivery)
    report = dict(revision=REVISION, input=str(source),
                  original_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                  filters=restoration, dynamics=dynamics, depop=depop_report,
                  phrase_leveling=level_report,
                  effect=dict(name='Herx-inspired dungeon voice', centered_lead=True,
                              double_semitones=[-3.0, -3.17], double_gain=.24,
                              reverb_gain=.20, reverb_decay_seconds=.9,
                              reverb_predelay_seconds=.027, stock_audio_sampled=False),
                  normalization=normalization,
                  measured_source_after_restoration=measured, measured_delivery=final,
                  master=str(master_path), clean_master=str(clean_master),
                  delivery=str(delivery), preview=str(preview), clean_preview=str(clean_preview),
                  preserves_performance=True, frames=frames, duration_seconds=duration)
    (evidence/'mastering.json').write_text(json.dumps(report, indent=2)+'\n')
    (evidence/'ffmpeg-analysis.log').write_text(analysis+'\n'+verification)
    return report


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--input', type=Path)
    parser.add_argument('--floor', type=int, choices=(1, 2, 3), default=1)
    parser.add_argument('--all', action='store_true', help='master all three source takes')
    parser.add_argument('--ffmpeg', default=str(Path.home()/'.local/bin/ffmpeg.exe'))
    args = parser.parse_args()
    if args.all and args.input:
        parser.error('--input applies to one floor only')
    results = [master(args, floor) for floor in ((1, 2, 3) if args.all else (args.floor,))]
    print(json.dumps(results, indent=2))


if __name__ == '__main__':
    main()
