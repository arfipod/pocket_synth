# Komplete M32 USB MIDI Observations

Date: 2026-06-22

## Firmware

- USB MIDI host is enabled in `cardputer_adv_wifi_dev`.
- Raw USB MIDI event packets are logged to `GET /logs` as:

```text
usb_midi: pkt #N xx xx xx xx
```

- `GET /status` includes `usb_midi` connection state, VID/PID, claimed
  interface, IN endpoint, packet count, and last raw packet.

## Hardware Capture

Hardware capture with a Native Instruments Komplete M32 is still pending in
this workspace.

Expected observations to record:

| Action | Raw USB MIDI event packet |
| --- | --- |
| Key press | Pending hardware test |
| Key release | Pending hardware test |
| Pitch control | Pending hardware test |
| Mod control | Pending hardware test |

## Notes

- Only class-compliant USB MIDI streaming descriptors are parsed.
- No Native Instruments proprietary/NKS behavior is implemented.
- No packets are converted to `SynthEvent` yet.
