Shared context:
Repo: arfipod/pocket_synth.
Target: M5Stack Cardputer ADV / ESP32-S3 / ESP-IDF via PlatformIO.
Cardputer ADV has 8 MB flash. Current repo incorrectly forces 2 MB; update to 8 MB.
Main priority order:
1. stable audio
2. safe wireless dev infrastructure
3. USB MIDI host
4. musical expressiveness

Hard constraints:
- Do not put logs, malloc/new, WiFi, USB, I2C, display, or blocking waits in the audio render path.
- AudioTask must remain real-time safe.
- WiFi Dev Mode must be optional.
- OTA must update application slots only, not bootloader or partition table.
- Keep serial recovery possible.
- USB MIDI must be optional and isolated.
- Cardputer keyboard must keep working.
Validation baseline:
- pio run -e cardputer_adv
- Document if hardware tests cannot be run.