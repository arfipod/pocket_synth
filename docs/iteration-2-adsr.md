# Iteration 2: Per-Note ADSR Envelope

## Goal

Iteration 2 adds a real amplitude ADSR envelope to every active note in
`pocketsynth`.

The audio chain is now:

```text
note on/off
-> ActiveNote
-> oscillator sample
-> per-note ADSR envelope gain
-> velocity gain
-> per-note gain
-> mix
-> normalization
-> master volume
-> I2S / audio output
```

ADSR is not a global effect on the final mix. Each active note owns its own
envelope state, so a chord can contain one note in Attack, another in Sustain,
and another in Release.

## Envelope Model

The envelope module lives in:

```text
include/synth_envelope.h
src/synth_envelope.cpp
```

It exposes:

```cpp
enum class EnvelopeStage : uint8_t {
  Off,
  Attack,
  Decay,
  Sustain,
  Release
};

struct EnvelopeParams {
  float attackMs;
  float decayMs;
  float sustainLevel;
  float releaseMs;
};

struct EnvelopeState {
  EnvelopeStage stage;
  float level;
  float releaseStartLevel;
  uint32_t sampleInStage;
};
```

The render loop calls `envelopeNextGain()` for each active note before mixing
that note. The function is real-time safe: it does not allocate, log, use
strings, touch WiFi/USB/display state, or block.

## ActiveNote Mapping

`ActiveNote` now keeps:

- oscillator identity and phase;
- MIDI velocity and computed `velocityGain`;
- `EnvelopeState envelope`;
- `uint32_t ageSamples` for simple voice stealing;
- `keyReleased`, which separates physical release from envelope lifetime.

The old fixed anti-click `attackSamples` ramp has been removed from the audio
path. Click prevention is now part of the Attack stage.

## Note Lifecycle

On NoteOn:

- an inactive slot is used when available;
- otherwise a voice already in Release is stolen, preferring the quietest one;
- otherwise the oldest voice is stolen;
- duplicate held notes are ignored;
- a released-but-still-fading duplicate note is reused for retriggering;
- the note phase starts at zero;
- `envelopeNoteOn()` puts the per-note envelope into Attack.

On NoteOff:

- the note is not cleared immediately;
- `keyReleased` is set;
- if sustain is off, `envelopeNoteOff()` starts Release from the current level;
- the slot is cleared only after the envelope reaches Off.

When the sustain pedal is on, NoteOff only marks `keyReleased`. When CC64 turns
sustain off, released notes enter Release instead of being cleared abruptly.

## Parameters

Initial amplitude envelope:

| Parameter | Initial | Range |
| --- | ---: | ---: |
| Attack | 5 ms | 0 to 3000 ms |
| Decay | 80 ms | 0 to 3000 ms |
| Sustain | 0.65 | 0.0 to 1.0 |
| Release | 120 ms | 0 to 5000 ms |

The current keyboard step size is intentionally simple:

| Parameter | Step |
| --- | ---: |
| Attack | 10 ms |
| Decay | 10 ms |
| Sustain | 0.05 |
| Release | 10 ms |

All values are clamped in the synth engine when adjustment events are applied.

## Cardputer Controls

ADSR controls use Fn and do not replace the existing waveform or volume
controls.

| Combination | Action |
| --- | --- |
| `Fn + W` | Increase Attack |
| `Fn + A` | Decrease Attack |
| `Fn + E` | Increase Decay |
| `Fn + S` | Decrease Decay |
| `Fn + R` | Increase Sustain |
| `Fn + D` | Decrease Sustain |
| `Fn + T` | Increase Release |
| `Fn + F` | Decrease Release |

Existing controls remain:

| Combination | Action |
| --- | --- |
| `Fn + 1` | Sine |
| `Fn + 2` | Square |
| `Fn + 3` | Rectangular pulse |
| `Fn + 4` | Sawtooth |
| `Fn + Up` | Increase volume |
| `Fn + Down` | Decrease volume |

The input task only converts key gestures into synth events. ADSR state changes
are applied by the synth engine, and audio rendering remains isolated.

## UI

The main screen now shows a compact ADSR line:

```text
A005 D080 S0.65 R120
```

The UI state mirrors `SynthAudioState::ampEnvelope`, so the display updates when
an ADSR event is applied.

## Tests

Envelope unit tests live in:

```text
test/test_synth_envelope/test_synth_envelope.cpp
```

They cover:

- Attack rising to full level;
- Decay reaching sustain;
- Sustain holding constant;
- Release reaching Off;
- zero-time Attack, Decay, and Release;
- output gain clamping;
- NoteOff from Attack, Decay, and Sustain preserving the current release start
  level.

## Remaining Limitations

- ADSR parameters are not saved to flash.
- There are no named presets yet.
- There is no ADSR editing from the Komplete M32.
- Time-step behavior is simple and may later become adaptive for coarse/fine
  editing.
- No filters, LFOs, effects, arpeggiators, sequencers, or deep menus are part of
  this iteration.
