# Komplete M32 USB MIDI Integration

This document describes how `pocketsynth` uses a Native Instruments Komplete M32
as a class-compliant USB MIDI input device.

The goal is not to implement Native Instruments Komplete Kontrol, NKS, DAW
integration, OLED control, browser integration, or proprietary behavior.

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

Not yet fully implemented:

- per-note velocity gain;
- sustain CC64;
- pitch bend mapping;
- modulation strip mapping;
- general CC mapping;
- all-notes-off recovery;
- Native Instruments proprietary controls.

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

Velocity is already present in parsed MIDI messages, but must be wired into the
synth engine.

Required changes:

- Add `velocity` to `SynthEvent`.
- Add `velocity` and `velocityGain` to `ActiveNote`.
- Change `sendMidiNoteEvent()` to accept velocity.
- Use a default fixed velocity for Cardputer keyboard notes.
- Multiply each note by `velocityGain` in audio render.
- Review `PER_NOTE_GAIN` to avoid clipping.

Suggested initial velocity curve:

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

Sustain should use MIDI CC64:

```text
CC64 value >= 64 -> sustain on
CC64 value < 64  -> sustain off
```

Required behavior:

- While sustain is on, NoteOff marks the note as released-but-held.
- When sustain turns off, notes that are not physically held should stop.
- Avoid stuck notes.
- Add all-notes-off recovery if needed.

Do not implement ADSR release in the first sustain task. A held note can stop
immediately when sustain is released until envelopes exist.

## Pitch Bend Roadmap

Pitch bend is parsed but not mapped.

Future behavior:

- Store global or per-channel pitch bend state.
- Apply pitch bend to note frequency or phase increment.
- Keep pitch bend out of the audio render control path except as a copied state.
- Avoid expensive recomputation per sample if possible.

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

- No velocity gain yet.
- No sustain CC64 yet.
- No pitch bend mapping yet.
- No NKS/proprietary Native Instruments behavior.
- USB MIDI Host is experimental and hardware-dependent.
- USB Serial/JTAG should not be assumed available while using the same USB-C path
  as USB Host.
