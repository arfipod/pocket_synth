# Initial Technical Decisions

| Decision | Value |
| --- | --- |
| Hardware | M5Stack Cardputer ADV |
| UI resolution | 240 x 135 px landscape |
| Voices/channels | 1 |
| Polyphony | 8 notes maximum |
| ADSR | Not in iteration 1 |
| LFO | Not in iteration 1 |
| Filter | Not in iteration 1 |
| Initial sample rate | 22050 Hz |
| Initial buffer | 128 frames |
| Internal format | `float` |
| Final output | `int16` |
| UI FPS | 15-20 FPS maximum |
| Visible name | `pocketsynth` |
| Waveform selectors | `Fn + 1..4` with compact icons |
| Normalization | `sqrt(activeNoteCount)` plus headroom |
| Initial per-note gain | `PER_NOTE_GAIN = 0.45f` |
| Initial master volume | `0.70f` |
| Rectangular pulse | Initial fixed 25% duty |
| Excess polyphony | Ignore new notes |

## Main Risks

### Audio Dropouts

Likely causes:

- `AudioTask` blocked.
- Buffer too small.
- UI too heavy.
- Logs inside the audio path.

Mitigations:

- High audio priority.
- No logs or dynamic allocation during render.
- Low-frequency UI.
- Adjustable buffer size.

### Clipping

Likely causes:

- Many notes summed together.
- Per-note gain too high.
- High master volume.

Mitigations:

- `PER_NOTE_GAIN`.
- Division by `sqrt(activeNoteCount)`.
- Final clamp.
- Future clipping indicator if needed.

### Keyboard Latency

Likely causes:

- Slow `InputTask`.
- Excessive debounce.
- Blocking reads.

Mitigations:

- Scan every 5-10 ms.
- Emit events only on changes.
- Keep input separate from UI.

### Crowded UI

Likely causes:

- Too much information in 240 x 135 px.
- Long labels.
- Graphs that are too large.

Mitigations:

- Compact UI.
- Abbreviations.
- Show only implemented features.
- Avoid future concepts on screen.
