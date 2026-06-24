# Komplete M32 USB MIDI Integration

This document describes how `pocketsynth` uses a Native Instruments Komplete M32
as a class-compliant USB MIDI input device.

The goal is not to implement Native Instruments Komplete Kontrol, NKS, DAW
integration, browser integration, or proprietary behavior.

The goal is:

```text
Komplete M32 -> USB MIDI packets -> MIDI parser -> SynthEvent -> synth engine
```

## Current Scope

Supported or intended in the current development phase:

- USB Host runtime.
- Powered hub test setup.
- USB device enumeration.
- MIDI Streaming interface detection.
- MIDI IN endpoint detection.
- Raw USB MIDI packet logging.
- Note On.
- Note Off.
- Note On with velocity 0 treated as Note Off.
- Control Change parsing.
- Pitch Bend parsing.
- Note On/Off conversion to `SynthEvent`.
- Per-note velocity gain.
- Sustain CC64.
- Pitch bend scaling.
- Modulation strip and general CC state caching.
- Optional M32 OLED feedback for MIDI events in WiFi Dev builds.

Not yet fully implemented:

- all-notes-off recovery;
- advanced CC mappings (LFOs, Filters);
- Native Instruments proprietary controls.
- proprietary HID input reports for non-MIDI buttons.

## Required Hardware Setup

Use the full MIDI host setup:

```text
Cardputer ADV USB-C -> powered USB-C hub with PD
Hub PD input -> charger or power bank
Hub USB port -> Komplete M32
PC and Cardputer ADV -> same WiFi or Cardputer AP
Optional: Cardputer audio out -> Focusrite -> PC
```

Why this setup is needed:

- The M32 is USB bus-powered.
- The Cardputer USB-C is used as USB Host.
- USB serial should not be assumed available while USB Host is active.
- WiFi Dev Mode provides `/status`, `/logs`, and OTA upload.

## Recommended Test Builds

Use the forced WiFi Dev Mode build for MIDI tests:

```powershell
pio run -e cardputer_adv_wifi_dev
```

Initial upload by serial:

```powershell
pio run -e cardputer_adv_wifi_dev -t upload
```

After WiFi Dev Mode works:

```powershell
python tools/ota_upload.py --host 192.168.4.1 --bin .pio/build/cardputer_adv_wifi_dev/firmware.bin
```

## Diagnostics

Read status:

```powershell
python tools/pocketsynth_status.py --host 192.168.4.1
```

Read logs:

```powershell
python tools/pocketsynth_logs.py --host 192.168.4.1
```

Expected log examples:

```text
usb_host: library ready
usb_midi: client ready
usb_midi: MIDI IN addr=3 vid=0x17cc pid=0x1860 intf=1 ep=0x82
usb_midi: pkt #1 09 90 3c 40
usb_midi: pkt #2 08 80 3c 10
```

When `POCKETSYNTH_ENABLE_M32_OLED=1`, expected OLED transport logs include:

```text
usb_midi: M32 OLED using direct OUT endpoint intf=2 ep=0x01
usb_midi: M32 OLED direct addr=3 intf=2 ep=0x01
```

## Observed Komplete M32 Data

A real hardware capture over a powered hub observed:

```text
USB MIDI enumeration:
usb_midi: MIDI IN addr=3 vid=0x17cc pid=0x1860 intf=1 ep=0x82
```

Example packets:

| Action | Raw USB MIDI event packet | Meaning |
| --- | --- | --- |
| Key press | `09 90 3c 0f` | Note On, channel 1, note 60, velocity 15 |
| Key release | `08 80 3c 13` | Note Off, channel 1, note 60, release velocity 19 |
| Key press | `09 90 37 40` | Note On, channel 1, note 55, velocity 64 |
| Key release | `08 80 37 10` | Note Off, channel 1, note 55, release velocity 16 |
| Key press | `09 90 34 38` | Note On, channel 1, note 52, velocity 56 |
| Key release | `08 80 34 32` | Note Off, channel 1, note 52, release velocity 50 |
| Control movement | `0b b0 01 xx` | CC 1 stream, likely mod strip/control |
| Control movement | `0b b0 0e xx` | CC 14 observed |
| Control movement | `0b b0 10 xx` | CC 16 observed |
| Control movement | `0b b0 13 xx` | CC 19 observed |
| Control movement | `0b b0 14 xx` | CC 20 observed |
| Control movement | `0b b0 15 xx` | CC 21 observed |

Many zero packets were observed:

```text
00 00 00 00
```

These should be ignored before logging or event conversion.

## USB MIDI Packet Format

USB MIDI Event Packets are 4 bytes:

```text
byte 0: cable number + CIN
byte 1: MIDI status
byte 2: data byte 1
byte 3: data byte 2
```

Example:

```text
09 90 3c 40
```

Meaning:

```text
09 = Code Index Number: Note On
90 = MIDI Note On, channel 1
3c = note 60
40 = velocity 64
```

## Parser Behavior

The MIDI parser should normalize these messages:

### Note On

```text
09 90 nn vv
```

where:

- `nn` is MIDI note number;
- `vv` is velocity.

If `vv > 0`, generate `MidiMessageType::NoteOn`.

### Note On with Velocity 0

```text
09 90 nn 00
```

This must be treated as Note Off.

### Note Off

```text
08 80 nn vv
```

Generate `MidiMessageType::NoteOff`.

### Control Change

```text
0b b0 cc vv
```

Generate `MidiMessageType::ControlChange`.

### Pitch Bend

```text
0e e0 lsb msb
```

Generate `MidiMessageType::PitchBend`.

Normalize pitch bend around center:

```text
raw = lsb | (msb << 7)
pitchBend = raw - 8192
```

## Synth Integration

The synth engine should not know whether a note came from:

- Cardputer keyboard;
- USB MIDI;
- future BLE MIDI;
- future sequencer.

All note sources should eventually become `SynthEvent`.

Current intended path:

```text
USB transfer callback
  -> raw 4-byte packets
  -> parseUsbMidiPacket()
  -> MidiMessage
  -> sendMidiNoteEvent()
  -> SynthEvent queue
  -> SynthControlTask
  -> SynthAudioState
  -> AudioTask
```

## Note Identity

Cardputer keyboard notes have a physical `noteIndex` and can update `pressedMask`
for UI feedback.

External MIDI notes do not have a Cardputer UI key index.

Use a special marker for MIDI notes:

```cpp
SYNTH_NO_UI_NOTE_INDEX = 0xFF
```

For external notes, note identity should primarily be MIDI note number. Future
work may need a better voice identity if duplicate Note On messages for the same
MIDI note should be supported.

## Velocity Roadmap

Velocity is already parsed and fully wired into the synth engine.

Implemented behavior:

- Added `velocity` to `SynthEvent`.
- Added `velocity` and `velocityGain` to `ActiveNote`.
- Uses a default fixed velocity (127) for Cardputer keyboard notes.
- Multiplies each oscillator output by `velocityGain` in audio render.

Velocity curve used:

```cpp
float velocityToGain(uint8_t velocity) {
  const float v = static_cast<float>(velocity) / 127.0f;
  return 0.10f + 0.90f * sqrtf(v);
}
```

Expected result:

- soft key press sounds quieter;
- hard key press sounds louder;
- Cardputer keyboard remains consistent.

## Sustain Roadmap

Sustain uses MIDI CC64:

```text
CC64 value >= 64 -> sustain on
CC64 value < 64  -> sustain off
```

Some sustain pedals or adapter chains expose the opposite polarity. The firmware
supports a build-time polarity flag:

```ini
-DPOCKETSYNTH_INVERT_SUSTAIN_PEDAL=1
```

When this flag is enabled, CC64 values `>= 64` are treated as sustain off and
values `< 64` are treated as sustain on. The raw CC value is still stored in
`SynthAudioState::cc[64]`; only the derived `sustainPedal` state is inverted.
The WiFi Dev M32 test environment currently enables this flag for the connected
pedal setup.

Implemented behavior:

- While sustain is on, NoteOff marks the note as `keyReleased = true` instead of clearing it.
- When sustain turns off, notes that have `keyReleased == true` are cleared immediately.
- There is no ADSR release yet; notes stop instantly upon physical release or pedal lift.

## Pitch Bend Roadmap

Pitch bend is parsed and integrated.

Implemented behavior:

- Stores global `pitchBendMultiplier` in `SynthAudioState`.
- Applies the multiplier to each note's phase increment in the `audio_render` loop.
- Modifies pitch by +/- 2 semitones.
- Re-computed only when `PitchBend` events arrive, keeping the audio loop fast.

## Modulation / CC Roadmap

Observed CC streams include CC1 and several other controls.

Initial mapping suggestion:

| Control | Suggested Use |
| --- | --- |
| CC1 | modulation amount |
| CC64 | sustain |
| CC7 | master volume if observed |
| CC10 | pan, not initially used |
| other CCs | log only until mapped |

Do not map controls blindly. Capture observations first and document them.

## Troubleshooting

### No USB MIDI device appears

Check:

- powered hub is connected and powered;
- M32 lights up;
- Cardputer is running `cardputer_adv_wifi_dev`;
- USB Host diagnostics are enabled;
- `/logs` shows `usb_host: library ready`;
- hub support is enabled in sdkconfig;
- M32 appears in USB descriptor logs.

### Device enumerates but no MIDI packets arrive

Check:

- MIDI Streaming interface was found;
- endpoint address is IN;
- endpoint max packet size is supported;
- interface was claimed;
- transfer is active;
- key presses are not being filtered as zero packets.

### Note On works but Note Off does not

Check:

- Note Off packets appear in `/logs`;
- `parseUsbMidiPacket()` recognizes `08 80 nn vv`;
- Note On with velocity 0 is treated as Note Off;
- external note identity matches on NoteOff.

### Notes get stuck

Potential causes:

- dropped NoteOff;
- duplicate NoteOn for same MIDI note;
- sustain logic if implemented incorrectly;
- event queue overflow.

Mitigations:

- implement all-notes-off command;
- track physical held state separately from sustained state;
- consider special handling for duplicate NoteOn same MIDI note.

## Current Known Limitations

- No advanced ADSR release/decay yet.
- No NKS/proprietary Native Instruments behavior.
- USB MIDI Host is experimental and hardware-dependent.
- USB Serial/JTAG should not be assumed available while using the same USB-C path
  as USB Host.

## OLED Feedback Coexistence

The M32 OLED transport is documented in `docs/komplete-m32-oled.md`.

Important coexistence details:

- MIDI uses interface 1 with bulk endpoints `0x02` and `0x82`; firmware reads
  from `0x82`.
- OLED uses HID interface 2, interrupt OUT endpoint `0x01`.
- The M32 also exposes HID IN endpoint `0x81`; the PC host in the USBPcap
  capture uses it to detect non-MIDI control button activity.
- ESP-IDF public interface claiming cannot claim MIDI and the full HID interface
  together on the tested SETUP D because HID also has interrupt IN endpoint
  `0x81`.
- Firmware therefore keeps MIDI on the public USB Host API and allocates only
  OLED OUT endpoint `0x01` through the USBH layer.
- Direct HID IN allocation was also tested on SETUP D and returned
  `ESP_ERR_NOT_SUPPORTED`, so `/status.usb_midi.hid_in_endpoint_allocated` is
  expected to be false for now.
- HID OUT report `0x80` is a 22-byte LED/state report observed in the capture.
  The firmware can queue raw HID OUT reports and sends the baseline LED/state
  report on M32 connection.
- Since a proprietary command to disable the built-in `Template 1` screen was
  not observed, the firmware keeps ownership by sending immediate feedback
  frames plus an OLED heartbeat of the latest PocketSynth frame.
- WiFi Dev keeps `/status`, `/logs`, and OTA available; USB Host diagnostics are
  disabled in `cardputer_adv_wifi_dev` to leave resources for MIDI + OLED.
