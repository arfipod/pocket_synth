Import("env")

OTA_0_OFFSET = "0x20000"

env.Replace(ESP32_APP_OFFSET=OTA_0_OFFSET)
env["INTEGRATION_EXTRA_DATA"].update({"application_offset": OTA_0_OFFSET})
