# Code Agent Guide

This repo implements `pocketsynth`, a real-time synthesizer for the M5Stack
Cardputer ADV. Before changing code, read `docs/iteration-1.md`,
`docs/architecture.md`, and `docs/ui.md`.

## Project Priorities

1. Stable audio.
2. Reliable keyboard interaction.
3. Compact UI that reflects real state.
4. Basic musical detection.
5. Advanced features only after iteration 1 is complete.

Do not introduce ADSR, LFO, filters, presets, sequencers, arpeggiators, or deep
menus during iteration 1 unless the user explicitly asks for them.

## Audio Path Rules

The task or function that renders audio must not do any of the following:

- Logs (`printf`, `ESP_LOG*`).
- `malloc`, `new`, dynamic `std::vector`, or `String`.
- SD reads.
- Display redraws.
- Slow I2C.
- Long mutex waits.
- UI work or heavy musical analysis.

Audio must be able to render buffers predictably and write them to I2S.

## Expected Architecture

- `AudioTask`: high priority, generates buffers and writes I2S.
- `InputTask`: medium priority, scans the keyboard and emits events.
- `SynthControlTask`: medium priority, applies note on/off, waveform, and
  volume changes.
- `UiTask`: low priority, redraws only when state changes.
- `ChordTask`: optional; it can be integrated into control at first.

When sharing state with audio, use small copies or short critical sections. Do
not block audio rendering on UI work.

## Repo Layout

- `src/` and `include/`: main synthesizer firmware.
- `lib/firmware/cardputer_adv_ui_test`: display, keyboard, widget, and generated
  UI runtime test bench.
- `docs/references/cardputer-ui-pocketsynth-main.cardputer-ui.json`: latest
  reference UI source.
- `docs/references/pocket_synth_iteration_1_design_v1_2.pdf`: original design
  document.

## Validation

For main firmware changes:

```powershell
pio run -e cardputer_adv
```

For UI test bench changes:

```powershell
pio run -d lib/firmware/cardputer_adv_ui_test -e cardputer_adv
```

Document in the final response if the build or hardware test could not be run.
