"""Check delivered narration timing, peak safety, transcript and subtitle assets."""
from pathlib import Path
import hashlib
import json
import re
import wave
import numpy as np

ROOT = Path(__file__).resolve().parents[2]
SOURCE_HASHES = {
    1: 'e86372b38dc9b452cc3715b6621f310522033bafc4a17b27008772cf32c85a35',
    2: 'b27ff90847bb828030902338010fc39765b5a7c29b8566a4b44e6af784596c0c',
    3: '3fa9c8e6307ab88cb347bc497f29616413f8fe6cae81717cbd736ea8a8628850',
}


def samples(path):
    with wave.open(str(path)) as audio:
        assert audio.getsampwidth() == 2
        values = np.frombuffer(audio.readframes(audio.getnframes()), dtype='<i2').reshape(-1, audio.getnchannels())
        return audio.getframerate(), values


def words(text):
    text = re.sub(r'\[[^\]]*\]', '', text.lower())
    text = re.sub(r'\b(?:ha){2,}\b', '', text)
    text = re.sub(r'\b2\b', 'two', text)
    return re.sub(r'[^a-z0-9]+', ' ', text).strip()


def check_floor(floor):
    stem = 'dungeon-intro' if floor == 1 else f'dungeon-floor-{floor}'
    asset = 'intro' if floor == 1 else f'intro-floor-{floor}'
    evidence = ROOT/'modding-evidence/intro-audio'
    if floor != 1:
        evidence = evidence/f'floor-{floor}'
    source = ROOT/'framework/audio/source'/(stem+'-original.wav')
    delivery = ROOT/'mods/extraction-raid/music/rmf'/(stem+'.wav')
    rate, original = samples(source)
    final_rate, rendered = samples(delivery)
    assert rate == final_rate == 48000 and rendered.shape == original.shape
    assert rendered.shape[1] == 2
    # The effect now has stereo doubles/room, with a strongly centered dry lead.
    stereo_correlation = float(np.corrcoef(rendered.astype(float).T)[0, 1])
    assert .75 < stereo_correlation < .99, stereo_correlation
    assert hashlib.sha256(source.read_bytes()).hexdigest() == SOURCE_HASHES[floor]
    assert not np.any(np.abs(rendered.astype(np.int32)) >= 32767)
    fft_size = 1 << (len(original)*2-1).bit_length()
    correlation = np.fft.irfft(np.fft.rfft(rendered.mean(axis=1), fft_size)*np.conj(np.fft.rfft(original[:, 0], fft_size)), fft_size)
    lag = int(np.argmax(correlation))
    if lag > fft_size//2:
        lag -= fft_size
    alignment_ms = lag/rate*1000
    assert abs(alignment_ms) < 1, alignment_ms
    story = json.loads((ROOT/'mods/extraction-raid/data/rmf'/(asset+'.json')).read_text())
    assert abs(story['duration_seconds']-len(rendered)/rate) < 1/rate
    assert 'images' not in story and 'text' not in story
    menu = (ROOT/'src/ui/MainMenu.cpp').read_text(encoding='utf-8')
    assert 'DelveIntro::' not in menu
    raid = (ROOT/'src/raid.cpp').read_text()
    assert '!narrationStarted[floorNumber - 1]' in raid and 'DelveIntro::prepare(floorNumber)' in raid
    assert 'DelveIntro::stop(); narrationStarted.fill(false); narrationCue = -1;' in raid
    assert 'DelveIntro::showSubtitle()' in raid
    narration = (ROOT/'src/delve_intro.cpp').read_text()
    assert 'messageLocalPlayersColor(color, MESSAGE_WORLD' in narration
    assert 'Language::get(537)' in narration and 'makeColorRGB(255, 0, 255)' in narration
    assert 'Text::get' not in narration and 'drawColor' not in narration
    player = (ROOT/'src/actplayer.cpp').read_text(encoding='utf-8')
    assert 'PLAYER_NUM == clientnum && !Raid::active()' in player
    previous = 0
    for cue in story['cues']:
        assert previous <= cue['start'] < cue['end'] <= story['duration_seconds']
        assert 'image' not in cue
        assert len(cue['text'].splitlines()) <= 2 and max(map(len, cue['text'].splitlines())) <= 42
        previous = cue['end']
    transcript = (ROOT/'framework/audio/masters'/(stem+'-transcript.txt')).read_text()
    assert words(transcript) == words(' '.join(cue['text'] for cue in story['cues']))
    asr = json.loads((evidence/('transcription-cleaned.json' if floor == 1 else 'transcription-medium.json')).read_text())
    assert words(transcript) == words(' '.join(segment['text'] for segment in asr['segments']))
    srt = (ROOT/'mods/extraction-raid/data/rmf'/(asset+'.srt')).read_text()
    assert srt.count(' --> ') == len(story['cues']) == (5 if floor == 2 else 7)
    assert srt == (ROOT/'framework/audio/masters'/(stem+'-subtitles.srt')).read_text()
    assert not (ROOT/'mods/extraction-raid/data/story/intro.json').exists()
    mastering = json.loads((evidence/'mastering.json').read_text())
    assert mastering['revision'] == 'depop-dungeon-1'
    assert not mastering['effect']['stock_audio_sampled']
    loudness = float(mastering['measured_delivery']['input_i'])
    true_peak = float(mastering['measured_delivery']['input_tp'])
    assert abs(loudness+16) < .5 and true_peak <= -1.4
    def phrase_levels(audio):
        values = audio.astype(float)/32768
        return [float(20*np.log10(np.sqrt(np.mean(
            values[int(cue['start']*rate):int(cue['end']*rate)]**2))+1e-12))
                for cue in story['cues']]
    source_levels, delivered_levels = phrase_levels(original), phrase_levels(rendered)
    source_spread = max(source_levels)-min(source_levels)
    delivered_spread = max(delivered_levels)-min(delivered_levels)
    assert delivered_spread < 1.5 and delivered_spread < source_spread
    report = dict(status='PASS', floor=floor, duration_seconds=len(rendered)/rate, sample_rate=rate,
                  channels=rendered.shape[1], pcm_bits=16, alignment_ms=alignment_ms,
                  loudness_lufs=loudness, true_peak_dbtp=true_peak, subtitle_cards=len(story['cues']),
                  stereo_correlation=stereo_correlation,
                  source_phrase_spread_db=source_spread, delivered_phrase_spread_db=delivered_spread,
                  original_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                  delivery_sha256=hashlib.sha256(delivery.read_bytes()).hexdigest(),
                  original_peak_samples=int(np.count_nonzero(np.abs(original.astype(np.int32)) >= 32767)),
                  delivered_peak_samples=0, authored_transcript_matches_reference_asr=True)
    (evidence/'audio-qa.json').write_text(json.dumps(report, indent=2)+'\n')
    return report


def main():
    results = [check_floor(floor) for floor in (1, 2, 3)]
    (ROOT/'modding-evidence/intro-audio/floor-narration-qa.json').write_text(json.dumps(results, indent=2)+'\n')
    print(json.dumps(results, indent=2))


if __name__ == '__main__':
    main()
