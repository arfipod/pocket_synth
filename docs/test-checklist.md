# pocketsynth Test Checklist

This checklist defines the manual and semi-manual validation flow for
`pocketsynth`.

The project involves real hardware, audio output, WiFi, OTA, USB Host, and a
Komplete M32 MIDI keyboard. Not every test can run in CI. When a test cannot be
performed, document it explicitly.

## Setup Names

### SETUP A - Serial Basic

```text
PC <-> USB-C <-> Cardputer ADV
```

Use for serial flashing, monitor logs, and recovery.

### SETUP B - Audio Capture

```text
PC <-> USB-C <-> Cardputer ADV
Cardputer audio out / minijack -> Focusrite input -> PC
```

Use for recording and inspecting audio output.

### SETUP C - WiFi Dev

```text
PC and Cardputer ADV on the same WiFi
or
PC connected to Cardputer AP `pocketsynth-dev`
```

Use for `/status`, `/logs`, and `/ota`.

### SETUP D - MIDI Host Real

```text
Cardputer ADV USB-C -> powered USB-C hub with PD
Hub PD input -> charger or power bank
Hub USB port -> Komplete M32
PC and Cardputer ADV -> same WiFi or Cardputer AP
Optional: Cardputer audio out -> Focusrite -> PC
```

Use for USB Host, USB MIDI, Komplete M32, velocity, and sustain.

### SETUP E - Recovery

```text
PC <-> USB-C <-> Cardputer ADV
No hub
No Komplete M32
```

Use for recovery after a bad OTA image or broken Dev Mode build.

## 1. Build Validation

### 1.1 Normal Build

Setup: none or SETUP A.

Command:

```powershell
pio run -e cardputer_adv
```

Expected:

- build succeeds;
- no WiFi Dev Mode required;
- no local `include/wifi_credentials.h` required;
- no USB MIDI device required.

### 1.2 OTA Build

Setup: none or SETUP A.

Command:

```powershell
pio run -e cardputer_adv_ota
```

Expected:

- build succeeds;
- image fits inside the 3 MB OTA app slot;
- partition table is `partitions_ota_8mb.csv`.

### 1.3 WiFi Dev Mode Build

Setup: none or SETUP A.

Command:

```powershell
pio run -e cardputer_adv_wifi_dev
```

Expected:

- build succeeds;
- build should not require committed WiFi secrets;
- if local credentials are absent, AP-only mode should still be possible;
- USB Host and USB MIDI code compile.

### 1.4 UI Test Bench

Setup: optional hardware.

Command:

```powershell
pio run -d lib/firmware/cardputer_adv_ui_test -e cardputer_adv
```

Expected:

- build succeeds;
- generated UI test bench remains independent from the main firmware.

## 2. Serial Boot Validation

Setup: SETUP A.

Command:

```powershell
pio run -e cardputer_adv -t upload
```

Expected serial observations:

- firmware boots;
- detected flash size is 8 MB;
- WiFi Dev Mode is inactive;
- USB Host diagnostics are inactive unless explicitly enabled;
- USB MIDI host is inactive unless explicitly enabled;
- I2C and I2S initialization are logged;
- firmware reaches normal task creation.

Pass criteria:

- device does not reboot repeatedly;
- UI starts;
- keyboard input does not crash;
- audio task starts.

## 3. Audio Sanity Validation

Setup: SETUP B.

Recommended checks:

1. Boot normal firmware.
2. Select sine waveform.
3. Play one note.
4. Record audio through Focusrite.
5. Repeat with square, rectangular, and saw.
6. Play 3-note and 5-note chords.
7. Check for obvious clipping or dropouts.

Expected:

- sine is clean and stable;
- square/rectangular/saw are louder/brighter but not broken;
- chords do not cause extreme clipping;
- no dropouts during UI updates.

Failure indicators:

- silence;
- heavy distortion on one note;
- periodic clicks;
- buffer underruns;
- notes stuck after release.

Known technical debt to verify:

- current audio buffer/I2S packing should be reviewed;
- waveform gain and `PER_NOTE_GAIN` should be checked on real recordings.

## 4. WiFi Dev Mode Validation

Setup: SETUP A for first flash, then SETUP C.

First flash:

```powershell
pio run -e cardputer_adv_wifi_dev -t upload
```

Expected:

- Dev Mode logs show active;
- AP `pocketsynth-dev` appears;
- `/status`, `/logs`, and `/ota` are available.

Connect to AP:

```text
SSID: pocketsynth-dev
Password: pocketsynth
```

Status check:

```powershell
python tools/pocketsynth_status.py --host 192.168.4.1
```

Logs check:

```powershell
python tools/pocketsynth_logs.py --host 192.168.4.1
```

Expected `/status` includes:

- app name;
- firmware version/build date;
- flash size;
- heap free;
- active partition;
- OTA state;
- boot diagnostics;
- USB feature flags.

Expected `/logs` includes:

- boot messages;
- Dev Mode startup;
- I2C/I2S diagnostic messages;
- no audio render spam.

Pass criteria:

- AP path works even without router credentials;
- status tool prints valid JSON;
- logs tool prints bounded diagnostic text;
- normal audio path is not spam-logged.

## 5. OTA Upload Validation

Setup: SETUP C, with SETUP E ready.

Build an OTA-capable image:

```powershell
pio run -e cardputer_adv_ota
```

Upload:

```powershell
python tools/ota_upload.py --host 192.168.4.1 --bin .pio/build/cardputer_adv_ota/firmware.bin
```

Expected:

- upload starts;
- HTTP 200 response says OTA succeeded or equivalent;
- device reboots;
- new partition boots;
- boot validation runs;
- app is marked valid if rollback support is active.

Negative test:

```powershell
python tools/ota_upload.py --host 192.168.4.1 --bin README.md
```

Expected:

- upload is rejected;
- boot partition does not switch;
- device remains on current firmware.

Pass criteria:

- valid firmware uploads successfully;
- invalid firmware is rejected;
- failed upload does not break current firmware;
- serial recovery remains available.

## 6. Rollback Validation

Setup: SETUP C, with SETUP E ready.

Prerequisite:

- OTA rollback is enabled in `sdkconfig.cardputer_adv_ota` and
  `sdkconfig.cardputer_adv_wifi_dev`.

Create a temporary failing build using:

```ini
-DPOCKETSYNTH_FORCE_BOOT_SELF_TEST_FAIL=1
```

Steps:

1. Start from a known-good OTA firmware.
2. Upload failing OTA image.
3. Wait for reboot.
4. Observe boot self-test failure.
5. Confirm rollback request.
6. Confirm previous valid image boots again.

Pass criteria:

- bad image does not become permanent;
- previous valid image returns;
- serial recovery is not needed.

If rollback support is disabled in a future build, document that the test is
blocked.

## 7. USB Host Enumeration Validation

Setup: SETUP D.

Steps:

1. Boot `cardputer_adv_wifi_dev`.
2. Connect powered hub.
3. Connect Komplete M32 to hub.
4. Query logs:

   ```powershell
   python tools/pocketsynth_logs.py --host 192.168.4.1
   ```

5. Query status:

   ```powershell
   python tools/pocketsynth_status.py --host 192.168.4.1
   ```

Expected:

- USB Host library ready;
- USB diagnostic client ready only if enabled;
- hub support enabled;
- device connected;
- VID/PID shown;
- interfaces/endpoints visible in logs or diagnostics.

Expected Komplete M32 observations:

```text
vid=0x17cc
pid=0x1860
```

Pass criteria:

- connecting the M32 does not crash;
- disconnecting the M32 does not crash;
- reconnecting can be attempted;
- diagnostics remain readable over WiFi.

## 8. USB MIDI Raw Packet Validation

Setup: SETUP D.

Steps:

1. Boot `cardputer_adv_wifi_dev`.
2. Connect powered hub and M32.
3. Read logs.
4. Press middle C or another key.
5. Release the key.
6. Move mod/pitch controls if desired.

Expected logs:

```text
usb_midi: MIDI IN addr=... vid=0x17cc pid=0x1860 intf=... ep=...
usb_midi: pkt #N 09 90 3c xx
usb_midi: pkt #N 08 80 3c xx
```

Expected:

- non-zero packets logged;
- zero packets ignored;
- Note On packet appears on press;
- Note Off packet appears on release;
- CC packets appear when controls move.

Pass criteria:

- raw packets are captured reliably;
- no crash on repeated key presses;
- no crash on disconnect.

## 9. M32 OLED Feedback Validation

Setup: SETUP D.

Steps:

1. Boot `cardputer_adv_wifi_dev`.
2. Connect powered hub and M32.
3. Read logs and status.
4. Confirm the M32 OLED shows the `POCKETSYNTH` splash.
5. Move knobs or touch strips.
6. Press and release several keys.
7. Read status again.

Expected logs:

```text
usb_midi: M32 OLED using direct OUT endpoint intf=2 ep=0x01
usb_midi: M32 OLED direct addr=... intf=2 ep=0x01
usb_midi: MIDI IN addr=... vid=0x17cc pid=0x1860 intf=1 ep=0x82
```

Expected `/status`:

- `m32_oled.enabled` is true;
- `m32_oled.device_connected` is true;
- `m32_oled.endpoint_address` is `0x01`;
- `m32_oled.frame_count` increases after splash and input feedback;
- with the ownership heartbeat enabled, `m32_oled.frame_count` continues to
  increase while the M32 remains connected;
- `m32_oled.feedback_count` increases after key, knob, CC, or pitch activity;
- `usb_midi.hid_in_endpoint_allocated` is reported explicitly. On current
  SETUP D hardware it is expected to be false because direct HID IN `0x81`
  returns `ESP_ERR_NOT_SUPPORTED` when MIDI IN and OLED OUT are active.

Pass criteria:

- splash appears on the M32 OLED;
- knob/CC feedback appears on the M32 OLED;
- after pressing or moving knobs, the built-in `Template 1` / `CC ##` screen may
  flash briefly, but the PocketSynth ownership heartbeat should repaint and keep
  the PocketSynth UI visible;
- note this as a known limitation, not as final desired behavior. Future work
  should remove the brief `Template 1` flash entirely if a reliable host/takeover
  command or HID coexistence path is found;
- key press/release feedback appears on the M32 OLED;
- MIDI packets still arrive while OLED frames are sent;
- no audio task logging or blocking is introduced.

Validated on 2026-06-24 with SETUP D over WiFi at `192.168.31.147`:

```text
frame_count=23
transfer_count=46
feedback_count=37
transfer_active=false
```

Validated on 2026-06-25 with SETUP D over WiFi at `192.168.31.147`:

```text
frame_count=94
transfer_count=190
usb_midi.hid_in_endpoint_allocated=false
usb_midi.hid_in_report_count=0
```

## 10. MIDI Parser Validation

Setup: no hardware required.

Validation vectors:

| Packet | Expected |
| --- | --- |
| `09 90 3C 64` | NoteOn, note 60, velocity 100 |
| `08 80 3C 00` | NoteOff, note 60 |
| `09 90 3C 00` | NoteOff, note 60 |
| `0B B0 40 7F` | ControlChange, CC64, value 127 |
| `0E E0 00 40` | PitchBend, center |

Pass criteria:

- static asserts or tests pass;
- parser does not depend on USB Host runtime;
- parser does not depend on synth engine.

## 11. MIDI to SynthEvent Validation

Setup: SETUP D. Use SETUP B optionally for audio capture.

Steps:

1. Boot Dev Mode.
2. Connect M32.
3. Press C4.
4. Confirm Note On packet appears.
5. Confirm synth note starts.
6. Release C4.
7. Confirm Note Off packet appears.
8. Confirm synth note stops.
9. Play a 3-note chord.
10. Confirm active count changes and no crash occurs.

Pass criteria:

- M32 can trigger notes;
- NoteOff stops notes;
- Cardputer keyboard still works;
- waveform selection still works;
- volume controls still work;
- max polyphony remains bounded at 8.

Failure indicators:

- notes stuck after release;
- duplicated notes for repeated NoteOn;
- active count incorrect;
- Cardputer keyboard broken after MIDI integration.

## 12. Velocity Validation

Setup: SETUP D + SETUP B.

Prerequisite:

- per-note velocity gain implemented.

Steps:

1. Connect audio output to Focusrite.
2. Press a key softly on the M32.
3. Record or observe level.
4. Press the same key hard.
5. Record or observe level.
6. Compare amplitude.
7. Test Cardputer keyboard note level.

Expected:

- hard press is louder than soft press;
- Cardputer keyboard uses fixed default velocity;
- master volume still affects all notes;
- velocity does not replace master volume;
- chords do not clip badly.

Pass criteria:

- velocity is audible and measurable;
- no severe clipping on high-velocity chords;
- no regressions in NoteOff behavior.

## 13. Sustain CC64 Validation

Setup: SETUP D + SETUP B. Add sustain pedal if available.

If no pedal is available, simulate CC64 through a test harness or another MIDI
controller.

Steps:

1. Send CC64 value 127.
2. Press and release a note.
3. Confirm note remains sounding.
4. Send CC64 value 0.
5. Confirm note stops.
6. Repeat with a chord.
7. Confirm no stuck notes.

Expected:

- CC64 >= 64 enables sustain;
- CC64 < 64 disables sustain;
- if `POCKETSYNTH_INVERT_SUSTAIN_PEDAL=1` is enabled, the expected polarity is
  reversed: CC64 >= 64 disables sustain and CC64 < 64 enables sustain;
- released notes are held only while sustain is on;
- turning sustain off clears released notes.

Pass criteria:

- sustain works;
- the observed pedal polarity matches the build flag. The current
  `cardputer_adv_wifi_dev` M32 test build enables inverted sustain polarity;
- no stuck notes;
- active count returns to zero after release and sustain off.

## 14. Regression Checklist Before Merging

Before merging any feature branch, verify:

- `pio run -e cardputer_adv`
- `pio run -e cardputer_adv_ota`
- `pio run -e cardputer_adv_wifi_dev`
- no real WiFi credentials committed;
- `include/wifi_credentials.h` remains ignored;
- no logs added to audio render;
- no dynamic allocation added to audio render;
- no WiFi/USB work added to AudioTask;
- serial recovery path documented;
- setup used for manual tests documented;
- known untested hardware items listed.

## 15. Current Known Open Items

Track these until closed:

- WiFi credentials should be persisted in NVS or provisioned.
- OTA rollback is enabled in OTA sdkconfigs, but manual rollback behavior should
  be revalidated after boot-flow changes.
- Audio buffer / I2S PCM packing should be simplified or justified.
- Waveform gain and `PER_NOTE_GAIN` need final real-recording confirmation.
- All-notes-off recovery should be added for stuck MIDI notes.
- Advanced CC mapping remains future work.
- Native Instruments proprietary features are intentionally out of scope.
