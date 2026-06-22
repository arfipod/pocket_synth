# Komplete M32 USB MIDI Observations

Date: 2026-06-22

## Firmware

- USB MIDI host is enabled in `cardputer_adv_wifi_dev`.
- Raw USB MIDI event packets are logged to `GET /logs` as:

```text
usb_midi: pkt #N xx xx xx xx
```

- `GET /status` includes lightweight USB MIDI enablement flags.
- `GET /logs` includes USB MIDI connection state, VID/PID, claimed interface,
  IN endpoint, packet count, and raw packets.

## Hardware Capture

Hardware capture with a Native Instruments Komplete M32 was performed over a
powered USB hub.

USB MIDI enumeration:

```text
usb_midi: MIDI IN addr=3 vid=0x17cc pid=0x1860 intf=1 ep=0x82
```

Observed raw packets:

| Action | Raw USB MIDI event packet | Notes |
| --- | --- | --- |
| Key press | `09 90 3c 0f` | Note On, channel 1, note 60, velocity 15. |
| Key release | `08 80 3c 13` | Note Off, channel 1, note 60, release velocity 19. |
| Key press | `09 90 37 40` | Note On, channel 1, note 55, velocity 64. |
| Key release | `08 80 37 10` | Note Off, channel 1, note 55, release velocity 16. |
| Key press | `09 90 34 38` | Note On, channel 1, note 52, velocity 56. |
| Key release | `08 80 34 32` | Note Off, channel 1, note 52, release velocity 50. |
| Control movement | `0b b0 01 25` ... `0b b0 01 38` | CC 1 value stream, likely mod strip/control. |
| Control movement | `0b b0 0e 05` / `0b b0 0e 04` | CC 14 values observed. |
| Control movement | `0b b0 10 17` / `0b b0 10 18` | CC 16 values observed. |
| Control movement | `0b b0 13 01` / `0b b0 13 02` | CC 19 values observed. |
| Control movement | `0b b0 14 12` | CC 20 value observed. |
| Control movement | `0b b0 15 00` | CC 21 value observed. |

Many zero packets (`00 00 00 00`) were observed after real events. The firmware
now ignores these before logging or event conversion.

## Notes

- Only class-compliant USB MIDI streaming descriptors are parsed.
- No Native Instruments proprietary/NKS behavior is implemented.
- Note On/Off packets are converted to `SynthEvent` note events.
- Control Change and Pitch Bend packets are parsed and logged, but they are not
  mapped to synth controls yet
- Komplete M32 is actually able to trigger note on and off!
