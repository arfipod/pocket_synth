#include "dev_mode.h"

#include "esp_log.h"

#if POCKETSYNTH_ENABLE_WIFI_DEV_MODE
#include <cstring>

#include "boot_diagnostics.h"
#include "m32_oled.h"
#include "usb_host_diag.h"
#include "usb_midi_host.h"
#include "wifi_credentials.h"

#include "esp_app_format.h"
#include "esp_flash.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#endif

namespace pocketsynth {
namespace {

constexpr const char* TAG = "pocketsynth-dev";

#if POCKETSYNTH_ENABLE_WIFI_DEV_MODE
constexpr const char* DEV_AP_SSID = "pocketsynth-dev";
constexpr const char* DEV_AP_PASSWORD = "pocketsynth";
constexpr const char* DEV_OTA_TOKEN = "pocketsynth-dev";
constexpr uint8_t DEV_AP_CHANNEL = 6;
constexpr uint8_t DEV_AP_MAX_CONNECTIONS = 2;
#if defined(WIFI_SSID2) && defined(WIFI_PASSWORD_2)
constexpr const char* DEV_STA_SSID = WIFI_SSID2;
constexpr const char* DEV_STA_PASSWORD = WIFI_PASSWORD_2;
#else
constexpr const char* DEV_STA_SSID = WIFI_SSID;
constexpr const char* DEV_STA_PASSWORD = WIFI_PASSWORD;
#endif
constexpr size_t OTA_RECV_BUFFER_SIZE = 1024;
constexpr TickType_t OTA_REBOOT_DELAY = pdMS_TO_TICKS(800);
constexpr size_t STATUS_RESPONSE_SIZE = 12288;
constexpr size_t LOG_RESPONSE_SIZE = 2048;

char gStatusResponse[STATUS_RESPONSE_SIZE] = {};

const char* otaStateToString(esp_err_t stateErr, esp_ota_img_states_t state);

void rebootAfterOtaTask(void*) {
  vTaskDelay(OTA_REBOOT_DELAY);
  ESP_LOGI(TAG, "Rebooting into updated firmware");
  esp_restart();
}

esp_err_t statusHandler(httpd_req_t* req) {
  const esp_app_desc_t* appDescription = esp_app_get_description();
  uint32_t flashSize = 0;
  esp_err_t flashErr = esp_flash_get_size(nullptr, &flashSize);
  if (flashErr != ESP_OK) {
    flashSize = 0;
  }

  const esp_partition_t* runningPartition = esp_ota_get_running_partition();
  const char* partitionLabel = runningPartition != nullptr ? runningPartition->label : "unknown";
  esp_ota_img_states_t otaState = ESP_OTA_IMG_UNDEFINED;
  esp_err_t otaStateErr = runningPartition != nullptr ? esp_ota_get_state_partition(runningPartition, &otaState) : ESP_FAIL;

  BootDiagnosticsSnapshot diagnostics = {};
  copyBootDiagnostics(&diagnostics);

  char m32OledStatus[2048] = {};
  writeM32OledStatusJson(m32OledStatus, sizeof(m32OledStatus));

  char* response = gStatusResponse;
  response[0] = '\0';
  const int written = snprintf(
      response,
      STATUS_RESPONSE_SIZE,
      "{"
      "\"app\":\"pocketsynth\","
      "\"firmware\":{\"project\":\"%s\",\"version\":\"%s\",\"built\":\"%s %s\"},"
      "\"dev_mode\":true,"
      "\"ota\":true,"
      "\"flash_size_bytes\":%lu,"
      "\"heap_free_bytes\":%lu,"
      "\"active_partition\":\"%s\","
      "\"ota_state\":\"%s\","
      "\"ota_state_result\":\"%s\","
      "\"audio_init\":{"
      "\"app_state\":%s,"
      "\"self_test\":%s,"
      "\"task_probe\":%s,"
      "\"i2c\":\"%s\","
      "\"i2s\":\"%s\","
      "\"codec\":\"%s\""
      "},"
      "\"usb_midi_enabled\":%s,"
      "\"usb_host_diag_enabled\":%s,"
      "\"m32_oled_enabled\":%s,"
      "\"m32_oled\":%s"
      "}\n",
      appDescription != nullptr ? appDescription->project_name : "unknown",
      appDescription != nullptr ? appDescription->version : "unknown",
      appDescription != nullptr ? appDescription->date : "unknown",
      appDescription != nullptr ? appDescription->time : "unknown",
      static_cast<unsigned long>(flashSize),
      static_cast<unsigned long>(esp_get_free_heap_size()),
      partitionLabel,
      otaStateToString(otaStateErr, otaState),
      esp_err_to_name(otaStateErr),
      diagnostics.appStateReady ? "true" : "false",
      diagnostics.coreSelfTestPassed ? "true" : "false",
      diagnostics.taskProbeReady ? "true" : "false",
      esp_err_to_name(diagnostics.i2cResult),
      esp_err_to_name(diagnostics.i2sResult),
      esp_err_to_name(diagnostics.codecResult),
      isUsbMidiHostBuildEnabled() ? "true" : "false",
      isUsbHostDiagnosticsBuildEnabled() ? "true" : "false",
      isM32OledBuildEnabled() ? "true" : "false",
      m32OledStatus);

  if (written <= 0 || written >= static_cast<int>(STATUS_RESPONSE_SIZE)) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    return httpd_resp_sendstr(req, "status response too large\n");
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

const char* otaStateToString(esp_err_t stateErr, esp_ota_img_states_t state) {
  if (stateErr == ESP_ERR_NOT_SUPPORTED) {
    return "not_supported";
  }
  if (stateErr == ESP_ERR_NOT_FOUND) {
    return "not_found";
  }
  if (stateErr != ESP_OK) {
    return "unknown";
  }

  switch (state) {
    case ESP_OTA_IMG_NEW:
      return "new";
    case ESP_OTA_IMG_PENDING_VERIFY:
      return "pending_verify";
    case ESP_OTA_IMG_VALID:
      return "valid";
    case ESP_OTA_IMG_INVALID:
      return "invalid";
    case ESP_OTA_IMG_ABORTED:
      return "aborted";
    case ESP_OTA_IMG_UNDEFINED:
      return "undefined";
    default:
      return "unknown";
  }
}

esp_err_t logsHandler(httpd_req_t* req) {
  char response[LOG_RESPONSE_SIZE] = {};
  size_t length = copyDiagnosticLogs(response, sizeof(response));
  if (length == 0) {
    length = snprintf(response, sizeof(response), "no diagnostic logs buffered\n");
  }

  httpd_resp_set_type(req, "text/plain");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

bool requestHasValidOtaToken(httpd_req_t* req) {
  char token[32] = {};
  esp_err_t err = httpd_req_get_hdr_value_str(req, "X-PocketSynth-Token", token, sizeof(token));
  return err == ESP_OK && std::strcmp(token, DEV_OTA_TOKEN) == 0;
}

esp_err_t otaHandler(httpd_req_t* req) {
  if (!requestHasValidOtaToken(req)) {
    addDiagnosticLog("W", TAG, "OTA upload rejected: invalid token");
    httpd_resp_set_status(req, "401 Unauthorized");
    return httpd_resp_sendstr(req, "missing or invalid OTA token\n");
  }

  if (req->content_len <= 0) {
    addDiagnosticLog("W", TAG, "OTA upload rejected: empty body");
    httpd_resp_set_status(req, "400 Bad Request");
    return httpd_resp_sendstr(req, "empty firmware upload\n");
  }

  const esp_partition_t* updatePartition = esp_ota_get_next_update_partition(nullptr);
  if (updatePartition == nullptr) {
    ESP_LOGE(TAG, "OTA upload rejected: no inactive OTA app partition");
    addDiagnosticLog("E", TAG, "OTA rejected: no inactive app partition");
    httpd_resp_set_status(req, "500 Internal Server Error");
    return httpd_resp_sendstr(req, "no inactive OTA partition\n");
  }

  if (static_cast<size_t>(req->content_len) > updatePartition->size) {
    ESP_LOGE(TAG,
             "OTA upload rejected: image %d bytes exceeds partition %lu bytes",
             req->content_len,
             static_cast<unsigned long>(updatePartition->size));
    addDiagnosticLog("E", TAG, "OTA rejected: image too large (%d bytes)", req->content_len);
    httpd_resp_set_status(req, "413 Payload Too Large");
    return httpd_resp_sendstr(req, "firmware image too large for OTA slot\n");
  }

  ESP_LOGW(TAG,
           "Starting OTA upload to %s at 0x%lx, %d bytes",
           updatePartition->label,
           static_cast<unsigned long>(updatePartition->address),
           req->content_len);
  addDiagnosticLog("W", TAG, "OTA upload started to %s (%d bytes)", updatePartition->label, req->content_len);

  esp_ota_handle_t otaHandle = 0;
  esp_err_t err = esp_ota_begin(updatePartition, req->content_len, &otaHandle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
    addDiagnosticLog("E", TAG, "OTA begin failed: %s", esp_err_to_name(err));
    httpd_resp_set_status(req, "500 Internal Server Error");
    return httpd_resp_sendstr(req, "ota begin failed\n");
  }

  char buffer[OTA_RECV_BUFFER_SIZE];
  int remaining = req->content_len;
  while (remaining > 0) {
    const int requested = remaining < static_cast<int>(sizeof(buffer)) ? remaining : sizeof(buffer);
    const int received = httpd_req_recv(req, buffer, requested);
    if (received == HTTPD_SOCK_ERR_TIMEOUT) {
      continue;
    }
    if (received <= 0) {
      ESP_LOGE(TAG, "OTA upload receive failed: %d", received);
      addDiagnosticLog("E", TAG, "OTA receive failed: %d", received);
      esp_ota_abort(otaHandle);
      httpd_resp_set_status(req, "400 Bad Request");
      return httpd_resp_sendstr(req, "firmware upload interrupted\n");
    }

    err = esp_ota_write(otaHandle, buffer, received);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
      addDiagnosticLog("E", TAG, "OTA write failed: %s", esp_err_to_name(err));
      esp_ota_abort(otaHandle);
      httpd_resp_set_status(req, "500 Internal Server Error");
      return httpd_resp_sendstr(req, "ota write failed\n");
    }

    remaining -= received;
  }

  err = esp_ota_end(otaHandle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ota_end image validation failed: %s", esp_err_to_name(err));
    addDiagnosticLog("E", TAG, "OTA image validation failed: %s", esp_err_to_name(err));
    httpd_resp_set_status(req, "400 Bad Request");
    return httpd_resp_sendstr(req, "firmware image validation failed\n");
  }

  esp_app_desc_t appDescription = {};
  err = esp_ota_get_partition_description(updatePartition, &appDescription);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "OTA image description read failed: %s", esp_err_to_name(err));
    addDiagnosticLog("E", TAG, "OTA image description invalid: %s", esp_err_to_name(err));
    httpd_resp_set_status(req, "400 Bad Request");
    return httpd_resp_sendstr(req, "firmware image description invalid\n");
  }

  err = esp_ota_set_boot_partition(updatePartition);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
    addDiagnosticLog("E", TAG, "OTA boot partition switch failed: %s", esp_err_to_name(err));
    httpd_resp_set_status(req, "500 Internal Server Error");
    return httpd_resp_sendstr(req, "ota boot partition switch failed\n");
  }

  ESP_LOGW(TAG,
           "OTA upload complete; next boot partition=%s app=%s version=%s",
           updatePartition->label,
           appDescription.project_name,
           appDescription.version);
  addDiagnosticLog("W", TAG, "OTA complete; next partition=%s", updatePartition->label);
  httpd_resp_set_type(req, "text/plain");
  esp_err_t responseErr = httpd_resp_sendstr(req, "ota ok; rebooting\n");
  xTaskCreate(rebootAfterOtaTask, "OtaRebootTask", 2048, nullptr, 1, nullptr);
  return responseErr;
}

esp_err_t initializeNvsForWifi() {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGW(TAG, "NVS reset required before WiFi init: %s", esp_err_to_name(err));
    err = nvs_flash_erase();
    if (err != ESP_OK) {
      return err;
    }
    err = nvs_flash_init();
  }
  return err;
}

void wifiEventHandler(void*, esp_event_base_t eventBase, int32_t eventId, void* eventData) {
  if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_DISCONNECTED) {
    ESP_LOGW(TAG, "WiFi Dev Mode STA disconnected; reconnecting to %s", DEV_STA_SSID);
    addDiagnosticLog("W", TAG, "STA disconnected; reconnecting to %s", DEV_STA_SSID);
    esp_wifi_connect();
    return;
  }

  if (eventBase == IP_EVENT && eventId == IP_EVENT_STA_GOT_IP) {
    const auto* event = static_cast<ip_event_got_ip_t*>(eventData);
    ESP_LOGI(TAG, "WiFi Dev Mode STA IP " IPSTR, IP2STR(&event->ip_info.ip));
    addDiagnosticLog("I", TAG, "STA IP " IPSTR, IP2STR(&event->ip_info.ip));
  }
}

esp_err_t startDevWifi() {
  esp_err_t err = initializeNvsForWifi();
  if (err != ESP_OK) {
    return err;
  }

  err = esp_netif_init();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    return err;
  }

  err = esp_event_loop_create_default();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    return err;
  }

  esp_netif_t* apNetif = esp_netif_create_default_wifi_ap();
  if (apNetif == nullptr) {
    return ESP_FAIL;
  }

  esp_netif_t* staNetif = esp_netif_create_default_wifi_sta();
  if (staNetif == nullptr) {
    return ESP_FAIL;
  }

  wifi_init_config_t initConfig = WIFI_INIT_CONFIG_DEFAULT();
  err = esp_wifi_init(&initConfig);
  if (err != ESP_OK) {
    return err;
  }

  err = esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, wifiEventHandler, nullptr, nullptr);
  if (err != ESP_OK) {
    return err;
  }

  err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifiEventHandler, nullptr, nullptr);
  if (err != ESP_OK) {
    return err;
  }

  wifi_config_t apConfig = {};
  std::strncpy(reinterpret_cast<char*>(apConfig.ap.ssid),
               DEV_AP_SSID,
               sizeof(apConfig.ap.ssid));
  std::strncpy(reinterpret_cast<char*>(apConfig.ap.password),
               DEV_AP_PASSWORD,
               sizeof(apConfig.ap.password));
  apConfig.ap.ssid_len = std::strlen(DEV_AP_SSID);
  apConfig.ap.channel = DEV_AP_CHANNEL;
  apConfig.ap.max_connection = DEV_AP_MAX_CONNECTIONS;
  apConfig.ap.authmode = WIFI_AUTH_WPA2_PSK;
  apConfig.ap.pmf_cfg.required = false;

  wifi_config_t staConfig = {};
  std::strncpy(reinterpret_cast<char*>(staConfig.sta.ssid),
               DEV_STA_SSID,
               sizeof(staConfig.sta.ssid));
  std::strncpy(reinterpret_cast<char*>(staConfig.sta.password),
               DEV_STA_PASSWORD,
               sizeof(staConfig.sta.password));
  staConfig.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
  staConfig.sta.pmf_cfg.capable = true;
  staConfig.sta.pmf_cfg.required = false;

  err = esp_wifi_set_mode(WIFI_MODE_APSTA);
  if (err != ESP_OK) {
    return err;
  }

  err = esp_wifi_set_config(WIFI_IF_AP, &apConfig);
  if (err != ESP_OK) {
    return err;
  }

  err = esp_wifi_set_config(WIFI_IF_STA, &staConfig);
  if (err != ESP_OK) {
    return err;
  }

  err = esp_wifi_start();
  if (err != ESP_OK) {
    return err;
  }

  err = esp_wifi_connect();
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "WiFi Dev Mode STA connect start failed: %s", esp_err_to_name(err));
    addDiagnosticLog("W", TAG, "STA connect failed: %s", esp_err_to_name(err));
  }

  return ESP_OK;
}

esp_err_t startStatusServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.stack_size = 8192;

  httpd_handle_t server = nullptr;
  esp_err_t err = httpd_start(&server, &config);
  if (err != ESP_OK) {
    return err;
  }

  httpd_uri_t statusUri = {};
  statusUri.uri = "/status";
  statusUri.method = HTTP_GET;
  statusUri.handler = statusHandler;
  statusUri.user_ctx = nullptr;
  err = httpd_register_uri_handler(server, &statusUri);
  if (err != ESP_OK) {
    return err;
  }

  httpd_uri_t otaUri = {};
  otaUri.uri = "/ota";
  otaUri.method = HTTP_POST;
  otaUri.handler = otaHandler;
  otaUri.user_ctx = nullptr;
  err = httpd_register_uri_handler(server, &otaUri);
  if (err != ESP_OK) {
    return err;
  }

  httpd_uri_t logsUri = {};
  logsUri.uri = "/logs";
  logsUri.method = HTTP_GET;
  logsUri.handler = logsHandler;
  logsUri.user_ctx = nullptr;
  return httpd_register_uri_handler(server, &logsUri);
}
#endif

}  // namespace

bool isDevModeBuildEnabled() {
#if POCKETSYNTH_ENABLE_WIFI_DEV_MODE
  return true;
#else
  return false;
#endif
}

bool isDevModeForced() {
#if POCKETSYNTH_FORCE_DEV_MODE
  return true;
#else
  return false;
#endif
}

bool isDevModeActive() {
  return isDevModeBuildEnabled() && isDevModeForced();
}

esp_err_t initializeDevMode() {
#if POCKETSYNTH_ENABLE_WIFI_DEV_MODE
  if (!isDevModeActive()) {
    ESP_LOGI(TAG, "WiFi Dev Mode compiled in but inactive");
    return ESP_OK;
  }

  ESP_LOGW(TAG, "WiFi Dev Mode active; local OTA upload enabled");

  esp_err_t err = startDevWifi();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "WiFi Dev Mode WiFi init failed: %s", esp_err_to_name(err));
    return err;
  }

  err = startStatusServer();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "WiFi Dev Mode status server failed: %s", esp_err_to_name(err));
    return err;
  }

  ESP_LOGI(TAG, "WiFi Dev Mode status endpoint ready at http://192.168.4.1/status");
  ESP_LOGI(TAG, "WiFi Dev Mode OTA endpoint ready at http://192.168.4.1/ota");
  ESP_LOGI(TAG, "WiFi Dev Mode logs endpoint ready at http://192.168.4.1/logs");
  ESP_LOGI(TAG, "WiFi Dev Mode AP SSID=%s password=%s", DEV_AP_SSID, DEV_AP_PASSWORD);
  ESP_LOGI(TAG, "WiFi Dev Mode STA connecting to SSID=%s", DEV_STA_SSID);
  addDiagnosticLog("I", TAG, "WiFi Dev Mode endpoints ready");
  return ESP_OK;
#else
  ESP_LOGI(TAG, "WiFi Dev Mode not compiled");
  return ESP_OK;
#endif
}

}  // namespace pocketsynth
