# WiFi Dev Mode and OTA

This document describes the current development infrastructure for updating and
diagnosing `pocketsynth` without relying on the USB serial link.

This is especially important when the Cardputer ADV USB-C port is used as USB
Host for a powered hub and a Komplete M32 MIDI keyboard.

## Purpose

WiFi Dev Mode exists to support:

- local OTA firmware upload;
- bounded diagnostic logs;
- runtime status inspection;
- USB Host and USB MIDI diagnostics while the USB-C port is occupied.

It is not a musical feature and must remain optional.

## Build Environments

Normal serial-oriented build:

```powershell
pio run -e cardputer_adv
```

OTA-capable build using the 8 MB OTA partition layout:

```powershell
pio run -e cardputer_adv_ota
```

Forced WiFi Dev Mode build:

```powershell
pio run -e cardputer_adv_wifi_dev
```

Upload the first WiFi Dev Mode firmware by serial:

```powershell
pio run -e cardputer_adv_wifi_dev -t upload
```

After that, firmware can be uploaded over WiFi using OTA, as long as the current
firmware still boots and Dev Mode is reachable.

## Hardware Setups

### Initial Flash or Recovery

```text
PC <-> USB-C <-> Cardputer ADV
```

Use this when:

- flashing Dev Mode for the first time;
- changing partition tables;
- recovering from a bad OTA image;
- the device is no longer reachable over WiFi.

### WiFi OTA Development

```text
PC and Cardputer ADV on the same WiFi
or
PC connected to the Cardputer AP `pocketsynth-dev`
```

Use this for:

- `/status`
- `/logs`
- `/ota`

### USB MIDI Development

```text
Cardputer ADV USB-C -> powered USB-C hub with PD
Hub PD input -> charger or power bank
Hub USB port -> Komplete M32
PC and Cardputer ADV -> same WiFi or Cardputer AP
Optional: Cardputer audio out -> Focusrite -> PC
```

Use this when testing USB Host or USB MIDI. In this setup the USB-C port is used
for host mode, so serial monitor should not be assumed available.

## Flash and Partition Layout

The Cardputer ADV target has 8 MB flash.

The OTA partition table is:

```csv
# Name,   Type, SubType, Offset,   Size,     Flags
nvs,      data, nvs,     0x9000,   0x6000,
otadata,  data, ota,     0xf000,   0x2000,
phy_init, data, phy,     0x11000,  0x1000,
ota_0,    app,  ota_0,   0x20000,  0x300000,
ota_1,    app,  ota_1,   0x320000, 0x300000,
storage,  data, spiffs,  0x620000, 0x1E0000,
```

Each OTA app slot is 3 MB.

OTA updates must only write application slots. Do not use OTA to update the
bootloader or partition table.

If the partition table changes, use serial flashing.

## Dev Mode Activation

The current forced Dev Mode build uses these compile-time flags:

```text
POCKETSYNTH_ENABLE_WIFI_DEV_MODE=1
POCKETSYNTH_FORCE_DEV_MODE=1
```

The normal build must not start WiFi.

Future improvement: replace forced activation with a physical boot gesture such
as `Fn + Esc`, or use a persistent development flag stored in NVS.

## Network Modes

Dev Mode should support two paths:

### AP Mode

The Cardputer starts a local AP:

```text
SSID: pocketsynth-dev
Password: pocketsynth
Default IP: 192.168.4.1
```

This path is useful when the device is not configured for the router WiFi.

### Station Mode

The Cardputer may also connect to a 2.4 GHz router network.

Current local-secret approach:

```text
include/wifi_credentials.h
```

This file must remain untracked and ignored by Git.

The firmware should compile even when this file is absent. If local credentials
are missing, AP-only Dev Mode should still work.

Recommended future behavior:

1. Use NVS-stored credentials when available.
2. Use a local non-tracked fallback header only for development.
3. Provide a Dev Mode-only setup endpoint to save/reset WiFi credentials.
4. Never commit real SSID or password.

## Suggested Local Credentials Header

Create this file locally only:

```text
include/wifi_credentials.h
```

Example:

```cpp
#pragma once

#define WIFI_SSID "Your2GHzNetwork"
#define WIFI_PASSWORD "YourPassword"

// Optional preferred second network
#define WIFI_SSID2 "YourPreferred2GHzNetwork"
#define WIFI_PASSWORD_2 "YourPreferredPassword"
```

Do not commit this file.

## HTTP Endpoints

### GET /status

Example:

```powershell
curl http://192.168.4.1/status
```

Expected purpose:

- firmware metadata;
- flash size;
- heap;
- active partition;
- OTA state;
- boot diagnostics;
- audio initialization results;
- USB feature flags.

The status endpoint should stay small and stable. Large USB descriptor dumps
should use a separate endpoint or `/logs`.

### GET /logs

Example:

```powershell
curl http://192.168.4.1/logs
```

or:

```powershell
python tools/pocketsynth_logs.py --host 192.168.4.1
```

Expected purpose:

- bounded diagnostic ring buffer;
- boot diagnostics;
- WiFi Dev Mode state;
- USB Host connection events;
- USB MIDI packet summaries.

This is not a full serial mirror.

Do not log from the audio render path.

### POST /ota

Example:

```powershell
python tools/ota_upload.py --host 192.168.4.1 --bin .pio/build/cardputer_adv_ota/firmware.bin
```

The OTA upload tool sends the placeholder header:

```text
X-PocketSynth-Token: pocketsynth-dev
```

Current security is development-only. Do not expose the endpoint outside a trusted
local network.

## OTA Flow

1. Build an OTA-capable image:

   ```powershell
   pio run -e cardputer_adv_ota
   ```

2. Upload over WiFi:

   ```powershell
   python tools/ota_upload.py --host 192.168.4.1 --bin .pio/build/cardputer_adv_ota/firmware.bin
   ```

3. Device receives the image.
4. Device writes the inactive OTA app slot.
5. Device validates the image.
6. Device sets the inactive slot as the next boot partition.
7. Device responds with success.
8. Device reboots.
9. New app performs boot validation.
10. New app marks itself valid if rollback support is enabled and validation passes.

## Uploading a Dev Mode Image

Use this when the updated firmware must keep WiFi Dev Mode available:

```powershell
pio run -e cardputer_adv_wifi_dev
python tools/ota_upload.py --host 192.168.4.1 --bin .pio/build/cardputer_adv_wifi_dev/firmware.bin
```

Use this carefully. A broken Dev Mode image may require serial recovery.

## Rollback

Rollback should be enabled for OTA builds.

The boot validation flow should be:

1. Detect whether the running app is pending OTA verification.
2. Initialize core app state and queues.
3. Run critical lightweight diagnostics.
4. Mark the app valid if diagnostics pass.
5. Mark app invalid and request rollback if diagnostics fail.

Required checks:

- app state/event queue initialized;
- task allocation viability;
- no immediate fatal boot failure.

Diagnostic-only checks:

- I2C initialization result;
- I2S initialization result;
- codec initialization result.

Do not require the following for app validity:

- WiFi connection success;
- Komplete M32 connected;
- USB MIDI device present;
- router reachable.

## Manual Rollback Test

Create or use a temporary build flag:

```ini
-DPOCKETSYNTH_FORCE_BOOT_SELF_TEST_FAIL=1
```

Then:

1. Build OTA-capable failing image.
2. Upload it from a known-good Dev Mode firmware.
3. Confirm the new image boots, fails self-test, requests rollback, and reboots.
4. Confirm the previous valid image runs again.

Do not keep the forced-failure flag in normal builds.

## Recovery

Use serial recovery when:

- OTA image cannot boot;
- WiFi is unreachable;
- `/ota` is not available;
- partition table changed;
- bootloader or flash config changed;
- USB Host experiments break the dev image.

Recovery command:

```powershell
pio run -e cardputer_adv_ota -t upload
```

or:

```powershell
pio run -e cardputer_adv_wifi_dev -t upload
```

If needed, hold the Cardputer ADV boot button while resetting or plugging in.

## Known Gaps

The following items should be closed:

- Dev Mode should compile when `include/wifi_credentials.h` is missing.
- WiFi credentials should be stored in NVS or provisioned.
- Rollback config must be confirmed as enabled in the build.
- `/status` should not become a large JSON dump.
- OTA token is currently a development placeholder.
- WiFi password must never be logged.
