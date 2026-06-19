# cardputer_adv_ui_test

UI test bench for the M5Stack Cardputer ADV.

This project validates:

- ST7789V2 display initialization.
- RGB565 framebuffer.
- Basic widgets.
- Physical keyboard and modifiers.
- Generated UI runtime.
- Framebuffer dumps over serial.

## Build

```powershell
pio run -d lib/firmware/cardputer_adv_ui_test -e cardputer_adv
```

## UI Source

The project includes a UI JSON file at:

```text
generated-project.cardputer-ui.json
```

The latest shared reference for `pocketsynth` is archived at:

```text
docs/references/cardputer-ui-pocketsynth-main.cardputer-ui.json
```

The local JSON in this project may have small coordinate differences compared
with that archived reference. Treat `docs/references/` as the latest design
source and this project as the runtime/test bench.

If the UI is regenerated, keep the source JSON and `src/generated/` files in
sync.

## Serial Commands

| Command | Action |
| --- | --- |
| `fb` or `dump` | Dump framebuffer in native panel order. |
| `fb logical` | Dump framebuffer in logical order. |
| `widgets` or `gallery` | Show the widget gallery. |
| `ui` or `generated` | Return to the generated UI. |

## Future Integration

For the main `pocketsynth` firmware, this project should be treated as the UI
runtime base and laboratory. Final integration should connect widgets to real
synthesizer state: active notes, waveform, volume, polyphony, and detected
chord.
