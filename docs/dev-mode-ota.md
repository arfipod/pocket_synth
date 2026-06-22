# WiFi Dev Mode OTA

WiFi OTA is only available in the forced Dev Mode build:

```powershell
pio run -e cardputer_adv_wifi_dev -t upload
```

The normal `cardputer_adv` build does not start WiFi and does not expose `/ota`,
`/status`, or `/logs`.

## Upload

Build the firmware image you want to send, connect the PC to the
`pocketsynth-dev` WiFi network, then run:

```powershell
pio run -e cardputer_adv_ota
python tools/ota_upload.py --host 192.168.4.1 --bin .pio/build/cardputer_adv_ota/firmware.bin
```

Use `.pio/build/cardputer_adv_wifi_dev/firmware.bin` instead when the updated
app should keep WiFi Dev Mode available. Do not use the plain `cardputer_adv`
image for OTA rollback testing because it is the serial/single-app build.

The endpoint accepts a raw `firmware.bin` body at `POST /ota`. It requires the
placeholder header `X-PocketSynth-Token: pocketsynth-dev`; the upload tool sends
that token by default.

OTA writes only the inactive app partition. It does not update the bootloader or
partition table. If receiving, writing, image validation, or boot partition
selection fails, the firmware returns an error and keeps the current boot
partition.

After a successful upload the device responds `ota ok; rebooting` and restarts.
On first boot after OTA, the app runs a fast self-test and marks itself valid
only after the core runtime checks pass. WiFi and MIDI are not required for app
validity.

The self-test requires app state/event queue initialization and task allocation
viability. I2C and I2S initialization are attempted and logged, but peripheral
failures are treated as diagnostics rather than rollback triggers. Display init
is left to the UI task because it owns the display device.

## WiFi Diagnostics

Dev Mode exposes:

```text
GET /status
GET /logs
```

`/status` returns firmware version, flash size, free heap, active partition,
OTA state, boot/audio initialization results, USB MIDI raw-reader state, and
USB Host diagnostics when the Dev Mode diagnostics build flag is enabled. USB
Host diagnostics report connection state, VID/PID, device class triplet,
configurations, interfaces, and endpoints without emitting synth events.

`/logs` returns a bounded in-memory diagnostic ring buffer. It is not a full
serial log mirror and intentionally does not log from the audio render path.

## Manual Checklist

1. Serial flash the OTA-capable Dev Mode image:

   ```powershell
   pio run -e cardputer_adv_wifi_dev -t upload
   ```

2. Confirm serial boot logs show Dev Mode active and `/ota` ready.
3. Connect to SSID `pocketsynth-dev` with password `pocketsynth`.
4. Confirm status:

   ```powershell
   curl http://192.168.4.1/status
   ```

   Or use:

   ```powershell
   python tools/pocketsynth_status.py --host 192.168.4.1
   python tools/pocketsynth_logs.py --host 192.168.4.1
   ```

5. Build and upload a rollback-capable firmware image:

   ```powershell
   pio run -e cardputer_adv_ota
   python tools/ota_upload.py --host 192.168.4.1 --bin .pio/build/cardputer_adv_ota/firmware.bin
   ```

6. Confirm the tool prints `HTTP 200: ota ok; rebooting`.
7. Watch serial logs for reboot, boot self-test, and `OTA app marked valid`.
8. For a failure test, send a non-firmware file and confirm the device does not
   switch boot partitions.

## Manual Rollback Test

For a deliberate rollback test, build an OTA-capable image with:

```ini
-DPOCKETSYNTH_FORCE_BOOT_SELF_TEST_FAIL=1
```

Upload that image from Dev Mode. The first boot should log the forced self-test
failure, call ESP-IDF rollback, reboot, and return to the previous valid app.

## Serial Recovery

Keep USB serial flashing as the recovery path. Use serial whenever the partition
table changes, the OTA image cannot boot, or the device is no longer reachable
over WiFi:

```powershell
pio run -e cardputer_adv_ota -t upload
```

If needed, hold the Cardputer ADV boot button while resetting or plugging in,
then flash again from PlatformIO.
