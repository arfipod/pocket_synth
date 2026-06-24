# PocketSynth M32 OLED library patch

Copy these files into the root of `arfipod/pocket_synth`:

```text
include/m32_oled.h
src/m32_oled.cpp
docs/komplete-m32-oled.md
include/synth_config.h
src/usb_host_runtime.cpp
src/main.cpp
platformio.ini
```

Then build:

```powershell
pio run -e cardputer_adv_wifi_dev
```

Initial upload:

```powershell
pio run -e cardputer_adv_wifi_dev -t upload
```

The OLED feature is behind:

```ini
-DPOCKETSYNTH_ENABLE_M32_OLED=1
```

Expected behavior: when the M32 is detected, the Cardputer sends a splash frame to the M32 OLED:

```text
POCKETSYNTH
M32 OLED READY
USB HID OK
```

Important: this patch is designed from the validated WebHID proof of concept and repo inspection. It still needs real ESP32-S3 + M32 hardware validation because this environment cannot run PlatformIO/ESP-IDF builds.
