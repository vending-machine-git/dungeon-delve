"""Original Last Hearth score, rendered with acoustic GeneralUser GS instruments.

Requires Python mido/numpy and WSL fluidsynth. SoundFont and stems stay outside
Workshop content. GeneralUser GS permits unrestricted use in music recordings:
https://www.schristiancollins.com/generaluser
"""
from pathlib import Path
import hashlib
import json
import subprocess
import wave
import mido
import numpy as np

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT/'framework/assets/audio'
RATE = 44100
TEMPO = 1_000_000  # quarter=60; a gentle 6/8, dotted-quarter=40
BAR = 3.0
BARS = 48
LENGTH = BARS*BAR


def linux(path):
    path = path.resolve()
    return '/mnt/'+path.drive[0].lower()+path.as_posix()[2:]


def compose():
    WORK.mkdir(parents=True, exist_ok=True)
    font = WORK/'GeneralUser-GS.sf2'
    if not font.exists():
        raise FileNotFoundError('Place GeneralUser GS v2.0.3 in '+str(font))
    rng = np.random.default_rng(37197020)
    midi = mido.MidiFile(ticks_per_beat=960)
    conductor = mido.MidiTrack(); midi.tracks.append(conductor)
    conductor.append(mido.MetaMessage('set_tempo', tempo=TEMPO))
    conductor.append(mido.MetaMessage('time_signature', numerator=6, denominator=8))
    conductor.append(mido.MetaMessage('key_signature', key='Dm'))
    events = [[] for _ in range(4)]
    # Guitar accompaniment, guitar melody, solo cello, very quiet bowed strings.
    instruments = [(24, 86, 44), (24, 90, 80), (42, 61, 61), (48, 29, 72)]

    def event(channel, seconds, message):
        events[channel].append((max(0, round(seconds*960)), message))

    def note(channel, start, pitch, duration, velocity):
        start += float(rng.uniform(-.013, .013))
        velocity = int(np.clip(velocity+rng.integers(-4, 5), 1, 100))
        event(channel, start, mido.Message('note_on',channel=channel,note=pitch,velocity=velocity))
        event(channel, start+duration, mido.Message('note_off',channel=channel,note=pitch,velocity=0))

    # Open guitar voicings with economical movement; no borrowed melodies.
    a = [([50,57,62,65],38), ([50,57,62,64],38),
         ([46,53,57,62],34), ([46,53,58,62],34),
         ([45,53,57,60],33), ([43,55,60,64],36),
         ([43,50,58,62],31), ([45,52,57,62],33),
         ([50,57,62,65],38), ([48,55,60,64],36),
         ([46,53,57,62],34), ([46,53,58,65],34),
         ([43,50,58,62],31), ([46,53,57,62],34),
         ([45,52,57,62],33), ([45,52,57,61],33)]
    b = [([43,50,58,62],31), ([43,50,57,62],31),
         ([50,57,62,65],38), ([48,55,60,64],36),
         ([46,53,57,62],34), ([43,50,58,62],31),
         ([45,52,57,62],33), ([45,52,57,61],33),
         ([50,57,62,65],38), ([50,57,62,64],38),
         ([46,53,58,62],34), ([45,53,57,60],33),
         ([43,50,58,62],31), ([43,50,58,65],31),
         ([45,52,57,62],33), ([45,52,57,61],33)]
    # Sparse answering phrases, leaving whole bars to the accompaniment.
    phrases = {
        0:[(0,69,1.0),(1.5,65,.8),(2.5,64,.4)],
        1:[(0,62,1.7)],
        3:[(.5,65,.9),(2,62,.8)],
        5:[(0,64,1.3),(1.5,62,1.0)],
        6:[(.5,62,1.4)],
        7:[(1.5,61,.9)],
        8:[(0,62,.85),(1,65,.75),(2,69,.8)],
        9:[(.5,67,1.3)],
        11:[(0,65,.9),(1.5,62,1.1)],
        13:[(.5,65,.8),(1.5,62,.9)],
        14:[(0,64,1.5)],
        15:[(.5,61,1.3),(2,57,.6)]}

    # Three cycles let the second one be cut with the previous room tail intact.
    for cycle in range(3):
        for bar, (chord, bass) in enumerate(a+b+a):
            start = cycle*LENGTH+bar*BAR
            section = bar//16
            # Humanized thumb/fingers in 6/8, with rests and alternate bass notes.
            pattern = [0,2,3,1,2,3] if bar%4!=3 else [0,2,3,1,None,2]
            for eighth, index in enumerate(pattern):
                if index is None:continue
                velocity = [58,41,46,51,39,43][eighth]
                pitch = chord[index]
                if eighth==3 and bar%4==2:pitch = chord[0]-5
                note(0,start+eighth*.5,pitch,.82 if eighth in (0,3) else .66,velocity)
            # Melody is warm nylon guitar, never a synthetic bell/lead tone.
            phrase = phrases.get(bar%16, [])
            if section==1 and bar%4==1:phrase=[]
            for offset, pitch, duration in phrase:
                if section==1:pitch -= 12
                note(1,start+offset+.035,pitch,duration,58 if section!=1 else 51)
            # Bowed bass enters only after the first four bars and breathes between phrases.
            if bar>=4:
                note(2,start+.08,bass,2.66,42 if section!=1 else 48)
                event(2,start,mido.Message('control_change',channel=2,control=11,value=42))
                event(2,start+.5,mido.Message('control_change',channel=2,control=11,value=70))
                event(2,start+2.3,mido.Message('control_change',channel=2,control=11,value=48))
            if bar>=8 and bar%2==0:
                for pitch in (chord[1],chord[2]):
                    note(3,start+.25,pitch,5.15,30 if section!=1 else 36)

    for channel, (program, volume, pan) in enumerate(instruments):
        track = mido.MidiTrack(); midi.tracks.append(track)
        track.append(mido.Message('program_change',channel=channel,program=program))
        for control,value in ((7,volume),(10,pan),(91,45 if channel<2 else 62),(93,0)):
            track.append(mido.Message('control_change',channel=channel,control=control,value=value))
        previous=0
        for tick,message in sorted(events[channel],key=lambda entry:entry[0]):
            track.append(message.copy(time=tick-previous));previous=tick
    score = WORK/'embers-v2-score.mid'; midi.save(score)
    rendered = WORK/'embers-v2-three-cycles.wav'
    cache='/root/.cache/reyes-mod-framework/audio'
    subprocess.run(['wsl.exe','-d','Ubuntu-24.04','--exec','mkdir','-p',cache],check=True)
    command = ['wsl.exe','-d','Ubuntu-24.04','--exec','fluidsynth','-ni','-q',
        '-C','0','-R','1','-r',str(RATE),'-g','.65',
        '-o','synth.reverb.room-size=.76','-o','synth.reverb.damp=.58',
        '-o','synth.reverb.width=65','-o','synth.reverb.level=.24',
        '-F',cache+'/embers-v2-three-cycles.wav','-T','wav','-O','s16',linux(font),linux(score)]
    subprocess.run(command,check=True)
    subprocess.run(['wsl.exe','-d','Ubuntu-24.04','--exec','cp',cache+'/embers-v2-three-cycles.wav',linux(rendered)],check=True)
    with wave.open(str(rendered),'rb') as wav:
        assert wav.getnchannels()==2 and wav.getsampwidth()==2
        wav.setpos(round(LENGTH*RATE))
        signal=np.frombuffer(wav.readframes(round(LENGTH*RATE)),dtype='<i2').reshape(-1,2).astype(np.float64)/32768
    # Soft bandwidth shaping takes brittle MIDI treble and subsonics out of the room.
    spectrum=np.fft.rfft(signal,axis=0)
    frequency=np.fft.rfftfreq(len(signal),1/RATE)
    highpass=1-np.exp(-(frequency/48)**4)
    lowpass=1/np.sqrt(1+(frequency/6700)**6)
    signal=np.fft.irfft(spectrum*(highpass*lowpass)[:,None],n=len(signal),axis=0)
    rms=float(np.sqrt(np.mean(signal**2)))
    signal *= min(.085/max(rms,1e-9), .74/max(abs(signal).max(),1e-9))
    pcm=np.clip(np.round(signal*32767),-32768,32767).astype('<i2')
    output=ROOT/'mods/extraction-raid/music/rmf/embers-before-the-delve.wav'
    with wave.open(str(output),'wb') as wav:
        wav.setnchannels(2);wav.setsampwidth(2);wav.setframerate(RATE);wav.writeframes(pcm.tobytes())
    evidence=dict(version=2,seconds=LENGTH,rate=RATE,channels=2,
        composition='Original 48-bar D minor 6/8 score, humanized nylon guitar and bowed strings',
        font='GeneralUser GS v2.0.3, S. Christian Collins',
        font_sha256=hashlib.sha256(font.read_bytes()).hexdigest(),
        output_sha256=hashlib.sha256(output.read_bytes()).hexdigest(),
        rms=float(np.sqrt(np.mean((pcm.astype(float)/32768)**2))),
        peak=float(abs(pcm.astype(float)).max()/32768),
        seam_delta=(abs(pcm[0].astype(int)-pcm[-1].astype(int))/32768).tolist())
    (WORK/'embers-v2-render.json').write_text(json.dumps(evidence,indent=2)+'\n')
    print(json.dumps(evidence,indent=2))
    print(output)


if __name__=='__main__':compose()
