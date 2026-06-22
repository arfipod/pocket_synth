#include "dev_mode.h"

#include "esp_log.h"

#if POCKETSYNTH_ENABLE_WIFI_DEV_MODE
#include <cstring>

#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#endif

namespace pocketsynth {
namespace {

constexpr const char* TAG = "pocketsynth-dev";

#if POCKETSYNTH_ENABLE_WIFI_DEV_MODE
constexpr const char* DEV_AP_SSID = "pocketsynth-dev";
constexpr const char* DEV_AP_PASSWORD = "pocketsynth";
constexpr uint8_t DEV_AP_CHANNEL = 6;
constexpr uint8_t DEV_AP_MAX_CONNECTIONS = 2;

esp_err_t statusHandler(httpd_req_t* req) {
  static constexpr const char* RESPONSE =
      "{\"app\":\"pocketsynth\",\"dev_mode\":true,\"ota\":false}\n";
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, RESPONSE, HTTPD_RESP_USE_STRLEN);
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

esp_err_t startDevAccessPoint() {
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

  wifi_init_config_t initConfig = WIFI_INIT_CONFIG_DEFAULT();
  err = esp_wifi_init(&initConfig);
  if (err != ESP_OK) {
    return err;
  }

  wifi_config_t wifiConfig = {};
  std::strncpy(reinterpret_cast<char*>(wifiConfig.ap.ssid),
               DEV_AP_SSID,
               sizeof(wifiConfig.ap.ssid));
  std::strncpy(reinterpret_cast<char*>(wifiConfig.ap.password),
               DEV_AP_PASSWORD,
               sizeof(wifiConfig.ap.password));
  wifiConfig.ap.ssid_len = std::strlen(DEV_AP_SSID);
  wifiConfig.ap.channel = DEV_AP_CHANNEL;
  wifiConfig.ap.max_connection = DEV_AP_MAX_CONNECTIONS;
  wifiConfig.ap.authmode = WIFI_AUTH_WPA2_PSK;
  wifiConfig.ap.pmf_cfg.required = false;

  err = esp_wifi_set_mode(WIFI_MODE_AP);
  if (err != ESP_OK) {
    return err;
  }

  err = esp_wifi_set_config(WIFI_IF_AP, &wifiConfig);
  if (err != ESP_OK) {
    return err;
  }

  return esp_wifi_start();
}

esp_err_t startStatusServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;

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
  return httpd_register_uri_handler(server, &statusUri);
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

  ESP_LOGW(TAG, "WiFi Dev Mode active; OTA upload is not implemented");

  esp_err_t err = startDevAccessPoint();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "WiFi Dev Mode AP init failed: %s", esp_err_to_name(err));
    return err;
  }

  err = startStatusServer();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "WiFi Dev Mode status server failed: %s", esp_err_to_name(err));
    return err;
  }

  ESP_LOGI(TAG, "WiFi Dev Mode status endpoint ready at http://192.168.4.1/status");
  ESP_LOGI(TAG, "WiFi Dev Mode AP SSID=%s password=%s", DEV_AP_SSID, DEV_AP_PASSWORD);
  return ESP_OK;
#else
  ESP_LOGI(TAG, "WiFi Dev Mode not compiled");
  return ESP_OK;
#endif
}

}  // namespace pocketsynth
