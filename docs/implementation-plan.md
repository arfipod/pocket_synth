# Implementation Plan

The first iteration is split into small, measurable phases. Each phase must
preserve audio stability before the next one begins.

## Phase 1A: Minimal Audio

Goal:

- Initialize audio output.
- Generate one fixed note.
- Confirm the buffer does not underrun.

Acceptance criteria:

- One continuous note plays without audible dropouts for at least 60 seconds.

Implementation notes:

- Use `SAMPLE_RATE = 22050`.
- Use `AUDIO_BUFFER_FRAMES = 128`.
- No logs in the audio path.
- Convert internal `float` to `int16` before I2S.

## Phase 1B: Monophonic Keyboard

Goal:

- Read the physical keyboard.
- Map one key to one note.
- Generate note on/off.

Acceptance criteria:

- Pressing `z` plays C4.
- Releasing `z` stops the note.
- Audio does not block.

## Phase 1C: 8-Note Polyphony

Goal:

- Allow up to 8 simultaneous notes.
- Keep independent phase per note.
- Normalize the sum.

Acceptance criteria:

- 3, 4, and 5 note chords can be played without obvious clipping.
- The polyphony counter shows `n/8`.
- Exceeding the maximum does not break the system.

Initial policy when more than 8 notes are requested:

- Ignore new notes until one active note is released.

## Phase 1D: Waveform Selection

Goal:

- `Fn + 1`: sine.
- `Fn + 2`: square.
- `Fn + 3`: rectangular pulse.
- `Fn + 4`: sawtooth.
- Show small waveform icons.

Acceptance criteria:

- The waveform can be changed while a note or chord is sounding.
- The UI reflects the selected waveform through active state and icon.

## Phase 1E: Compact UI

Goal:

- Show minimal state.
- Draw active keys.
- Show waveform and output previews.
- Show volume and polyphony.

Acceptance criteria:

- The UI updates without causing audio dropouts.
- Refresh rate is low but stable.

Recommendation:

- 15-20 FPS maximum.
- Redraw only when state changes.

## Phase 1F: Chord Detection

Goal:

- Identify basic chords from active notes.
- Show the chord name on screen.

Acceptance criteria:

- Major and minor triads are detected correctly.
- Basic seventh chords are detected correctly.
- Inversions show the bass note when needed.

Initial patterns:

- Major.
- Minor.
- Diminished.
- Augmented.
- Sus2.
- Sus4.
- 7.
- Maj7.
- m7.

## Phase 1G: Stabilization And Profiling

Goal:

- Measure CPU load.
- Check underruns.
- Check latency.
- Adjust buffer size.
- Adjust sample rate if appropriate.

Acceptance criteria:

- The system can be played for several minutes without dropouts, lockups, or
  degradation.

## Final Checklist

- `pocketsynth` boots and shows the compact UI.
- `z`, `x`, and `c` produce C4, D4, and E4.
- 3-5 note chords sound without obvious clipping.
- `Fn + 1..4` changes waveform.
- Master volume increases and decreases.
- The polyphony counter reflects active notes.
- Detected chord appears on screen.
- No logs or dynamic allocations happen during audio render.
- Long play sessions remain stable for several minutes.
