# UI And Controls

## Reference Source

The shared UI is stored in:

```text
docs/references/cardputer-ui-pocketsynth-main.cardputer-ui.json
```

The existing UI runtime test bench is in:

```text
lib/firmware/cardputer_adv_ui_test
```

That project contains display, keyboard, widgets, input mapper, generated UI,
and serial debug commands. It is the practical base for validating or
integrating the `pocketsynth` interface.

Synchronization note: the JSON archived in `docs/references/` is the latest
shared version. The JSON inside `lib/firmware/cardputer_adv_ui_test` may have
small coordinate differences because it belongs to an earlier generated test
bench.

## Device

- M5Stack Cardputer ADV.
- Landscape orientation.
- Logical resolution: 240 x 135 px.
- Color: RGB565 / ST7789V2.

## Main Screen

Single screen: `screen-main`.

JSON element summary:

| Type | Count | Purpose |
| --- | ---: | --- |
| `text` | 35 | Titles, labels, notes, chord, and octave markers. |
| `progress` | 2 | Battery and master volume. |
| `button` | 29 | Waveform selectors and piano keys. |
| `sparkline` | 6 | Waveform icons and previews. |

Main elements:

- `text-title`: `pocketsynth`.
- `text-polyphony`: `0/8`.
- `text-chord-label`: `CHORD`.
- `text-current-chord`: `--`.
- `progress-battery`: battery.
- `progress-main-volume`: vertical volume.
- `sparkline-wave-preview`: selected waveform preview.
- `sparkline-output-preview`: polyphonic output preview.

The original design document mentions `VOICE: V1` as possible status text. The
latest JSON does not contain that element, so the current UI omits it as long as
the screen stays clear that only one voice exists.

## Waveform Selectors

| Element | Key | Waveform | Icon |
| --- | --- | --- | --- |
| `button-wave-sine` | `Fn+1` | Sine | `icon-wave-sine` |
| `button-wave-square` | `Fn+2` | Square | `icon-wave-square` |
| `button-wave-rectangular` | `Fn+3` | Rectangular pulse | `icon-wave-rectangular` |
| `button-wave-sawtooth` | `Fn+4` | Sawtooth | `icon-wave-sawtooth` |

Active state:

- Fill: `#193322`.
- Stroke: `#9bffb7`.

Inactive state:

- Fill: `#101823`.
- Stroke: `#34445d`.

## Piano

White keys:

| Physical key | Note | Element |
| --- | --- | --- |
| z | C4 | `button-piano-white-z` |
| x | D4 | `button-piano-white-x` |
| c | E4 | `button-piano-white-c` |
| v | F4 | `button-piano-white-v` |
| b | G4 | `button-piano-white-b` |
| n | A4 | `button-piano-white-n` |
| m | B4 | `button-piano-white-m` |
| q | C5 | `button-piano-white-q` |
| w | D5 | `button-piano-white-w` |
| e | E5 | `button-piano-white-e` |
| r | F5 | `button-piano-white-r` |
| t | G5 | `button-piano-white-t` |
| y | A5 | `button-piano-white-y` |
| u | B5 | `button-piano-white-u` |
| i | C6 | `button-piano-white-i` |

Black keys:

| Physical key | Note | Element |
| --- | --- | --- |
| s | C#4 / Db4 | `button-piano-black-s` |
| d | D#4 / Eb4 | `button-piano-black-d` |
| g | F#4 / Gb4 | `button-piano-black-g` |
| h | G#4 / Ab4 | `button-piano-black-h` |
| j | A#4 / Bb4 | `button-piano-black-j` |
| 2 | C#5 / Db5 | `button-piano-black-2` |
| 3 | D#5 / Eb5 | `button-piano-black-3` |
| 5 | F#5 / Gb5 | `button-piano-black-5` |
| 6 | G#5 / Ab5 | `button-piano-black-6` |
| 7 | A#5 / Bb5 | `button-piano-black-7` |

Pressed key feedback:

| Type | Fill | Stroke |
| --- | --- | --- |
| White | `#d8ecff` | `#7cc7ff` |
| Black | `#25415f` | `#7cc7ff` |

## Base Colors

| Use | Color |
| --- | --- |
| Dark technical background | `#0b1018` |
| Soft primary text | `#c7d7ef` |
| Secondary text | `#8fa0bb` |
| Green status/accent | `#9bffb7` |
| Blue status/accent | `#7cc7ff` |
| Subtle stroke | `#34445d` |
| Battery green | `#76bb40` |

## UI Test Bench

Build command:

```powershell
pio run -d lib/firmware/cardputer_adv_ui_test -e cardputer_adv
```

Serial commands implemented by the runtime:

| Command | Action |
| --- | --- |
| `fb` or `dump` | Dump framebuffer in native panel order. |
| `fb logical` | Dump framebuffer in logical order. |
| `widgets` or `gallery` | Switch to the widget gallery. |
| `ui` or `generated` | Return to the generated UI. |

The runtime header draws keyboard diagnostics at the top. When integrated into
`pocketsynth`, that layer should become musical state or remain debug-only.

## UI Generation

The smoke-test project documents this command, although the generator does not
live in this repo:

```powershell
npm run firmware:prepare -- path/to/project.cardputer-ui.json
```

If the UI is regenerated, keep these files synchronized:

- JSON source.
- `src/generated/cardputer_ui.*`.
- `src/generated/cardputer_ui_assets.*`.
- `src/generated/cardputer_ui_fonts.*`.

## Design Constraints

- One screen.
- Small, legible text.
- No controls for unimplemented features.
- Compact, technical, recognizable waveform icons.
- The UI must reflect real state, not transient events only.
- Redraw at 15-20 FPS maximum and only when important state changes.
