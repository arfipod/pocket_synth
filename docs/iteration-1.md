# Iteration 1: One Polyphonic Voice

## Goal

Build a real-time synthesizer on the M5Stack Cardputer ADV. Notes are generated
mathematically on the device and sent to the audio system.

The first iteration is not a complete synthesizer. It validates the base chain:

```text
physical keyboard
-> note on/off events
-> synthesis engine
-> mathematical oscillators
-> mix up to 8 notes
-> master volume
-> audio buffer
-> I2S / speaker output
```

## Included Scope

- One voice/instrument.
- Maximum polyphony of 8 simultaneous notes.
- Mathematical waveform generation.
- Waveforms: sine, square, fixed-duty rectangular pulse, and sawtooth.
- Cardputer keyboard reading.
- Physical key to musical note mapping.
- Waveform changes with `Fn + 1`, `Fn + 2`, `Fn + 3`, and `Fn + 4`.
- Master volume.
- Visual feedback for pressed keys.
- Current polyphony indicator.
- Basic chord detection.
- Compact single-screen UI.
- Visible app name: small `pocketsynth` text in the upper area.
- Small waveform icons next to selectors.

## Out of Scope

- ADSR.
- LFO.
- Filters.
- Multiple channels or sound layers.
- Presets.
- Effects.
- Sequencer.
- Recording.
- Arpeggiator.
- Deep menus.
- Advanced parameter editing.

These exclusions protect the real-time stability of the engine.

## Sound Concept

In this iteration, "voice" means one instrument or sound layer. That voice can
hold up to 8 simultaneous notes:

```text
Voice 1
|- active note 1
|- active note 2
|- active note 3
|- ...
`- active note 8
```

Each active note keeps its own phase. There must not be a single global time
variable shared by all notes.

## Per-Note State

```cpp
struct ActiveNote {
  bool active;
  float frequency;
  float phase;
  float phaseIncrement;
};
```

Phase uses a normalized range:

```text
phase = 0.0 ... 1.0
phaseIncrement = frequency / sampleRate
```

Advance per sample:

```cpp
phase += phaseIncrement;
if (phase >= 1.0f) {
  phase -= 1.0f;
}
```

Example: A4 at 440 Hz with `sampleRate = 22050` gives
`phaseIncrement = 440 / 22050 = 0.01995`.

## Waveforms

Sine:

```cpp
sample = sinf(phase * 2.0f * PI);
```

Square:

```cpp
sample = phase < 0.5f ? 1.0f : -1.0f;
```

Fixed-duty rectangular pulse, initially 25%:

```cpp
sample = phase < pulseWidth ? 1.0f : -1.0f;
```

Sawtooth:

```cpp
sample = 2.0f * phase - 1.0f;
```

## Mixing And Normalization

Recommended internal range: `-1.0 ... +1.0`.

Initial strategy:

```text
mixed = sum(activeNotes)
mixed *= perNoteGain
mixed /= sqrt(activeNoteCount)
mixed *= masterVolume
mixed = clamp(mixed, -1.0f, 1.0f)
```

Initial constants:

```cpp
constexpr int MAX_POLYPHONY = 8;
constexpr float PER_NOTE_GAIN = 0.45f;
float masterVolume = 0.70f;
```

Conceptual render:

```cpp
float renderSample(SynthState& state) {
  float mixed = 0.0f;
  int activeCount = 0;

  for (auto& note : state.notes) {
    if (!note.active) continue;

    float s = oscillatorSample(note.phase, state.waveform);
    mixed += s * PER_NOTE_GAIN;

    note.phase += note.phaseIncrement;
    if (note.phase >= 1.0f) {
      note.phase -= 1.0f;
    }

    activeCount++;
  }

  if (activeCount > 1) {
    mixed /= sqrtf((float)activeCount);
  }

  mixed *= state.masterVolume;
  if (mixed > 1.0f) mixed = 1.0f;
  if (mixed < -1.0f) mixed = -1.0f;
  return mixed;
}
```

## Sample Rate And Buffer

Starting values:

```cpp
constexpr int SAMPLE_RATE = 22050;
constexpr int AUDIO_BUFFER_FRAMES = 128;
constexpr int MAX_POLYPHONY = 8;
```

Reasons:

- 22050 Hz reduces CPU load.
- 128 frames gives reasonable latency with stability margin.
- 8 notes cover complex Cardputer keyboard chords.

After the system is stable, `SAMPLE_RATE = 32000` can be evaluated.

## Controls

White keys:

| Key | Note |
| --- | --- |
| z | C4 |
| x | D4 |
| c | E4 |
| v | F4 |
| b | G4 |
| n | A4 |
| m | B4 |
| q | C5 |
| w | D5 |
| e | E5 |
| r | F5 |
| t | G5 |
| y | A5 |
| u | B5 |
| i | C6 |

Black keys:

| Key | Note |
| --- | --- |
| s | C#4 / Db4 |
| d | D#4 / Eb4 |
| g | F#4 / Gb4 |
| h | G#4 / Ab4 |
| j | A#4 / Bb4 |
| 2 | C#5 / Db5 |
| 3 | D#5 / Eb5 |
| 5 | F#5 / Gb5 |
| 6 | G#5 / Ab5 |
| 7 | A#5 / Bb5 |

Waveforms:

| Combination | Waveform |
| --- | --- |
| Fn + 1 | Sine |
| Fn + 2 | Square |
| Fn + 3 | Rectangular pulse |
| Fn + 4 | Sawtooth |

Initial volume controls:

| Combination | Action |
| --- | --- |
| Fn + Up | Increase volume |
| Fn + Down | Decrease volume |

The implementation must use the real mapping exposed by the keyboard driver.

## Expected UI

The screen should only show what exists in this phase:

- `pocketsynth`.
- `0/8`.
- `CHORD --`.
- `VOL`.
- `F1`, `F2`, `F3`, and `F4` selectors with waveform icons.
- Selected waveform preview.
- Output preview.
- Piano.

It must not show mixer, multiple voices, ADSR, LFO, filters, or presets.

Active key visual feedback:

| Type | Fill | Stroke |
| --- | --- | --- |
| White | `#d8ecff` | `#7cc7ff` |
| Black | `#25415f` | `#7cc7ff` |

This feedback must depend on the real active note state.

## Chord Detection

Chord detection must not block audio.

Input: active note set.

Output: chord name, for example:

- `C`
- `Cm`
- `CMaj7`
- `CMaj7#5/C`
- `Gsus4/D`

Recommended first version:

1. Convert active notes to pitch classes.
2. Calculate the bass note.
3. Try patterns: major, minor, diminished, augmented, sus2, sus4, 7, Maj7, and
   m7.
4. Show inversion if the bass note does not match the root.

Inversion format: `CMaj7/E`.

## Global Success Criteria

Iteration 1 ends when the Cardputer ADV acts as a basic real-time synthesizer:
one polyphonic voice with up to 8 notes, waveform selection, master volume,
visual feedback, chord detection, and stable audio.

Final test:

1. Power on `pocketsynth`.
2. Press `z`, `x`, and `c` and hear individual notes.
3. Play 3 to 5 note chords.
4. Change between `Fn + 1`, `Fn + 2`, `Fn + 3`, and `Fn + 4` while sound is
   playing.
5. Verify the selector and waveform icon change.
6. Increase and decrease volume.
7. Verify the polyphony counter.
8. Verify chord detection.
9. Keep playing for several minutes without dropouts.

## Later Roadmap

- Iteration 2: multiple channels/voices, waveform and volume per channel, mixer.
- Iteration 3: ADSR per note.
- Iteration 4: LFO for vibrato, tremolo, PWM, and modulation.
- Iteration 5: basic low-pass, high-pass, band-pass filters and resonance.
- Iteration 6: presets, patch save/load, possible SD use.
