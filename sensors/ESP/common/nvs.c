#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "led.h"
#include "wifi.h"
#include "nvs.h"

static const char *TAG = "nvs";

void nvs_init(void)
{
  int rc;
  size_t l;
  nvs_handle_t my_handle;

  // Initialize NVS
  rc = nvs_flash_init();
  if (rc == ESP_ERR_NVS_NO_FREE_PAGES || rc == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS partition was truncated and needs to be erased
    // Retry nvs_flash_init
    nvs_flash_erase();
    rc = nvs_flash_init();
  }
  if (rc != 0)
  {
    ESP_LOGE(TAG, "nvs_flash_init failed");
    set_led_red();
    while (1){}
  }

  rc = nvs_open("storage", NVS_READONLY, &my_handle);
  if (rc != 0)
  {
    ESP_LOGE(TAG, "nvs_open failed");
    return;
  }
  l = WIFI_NAME_LENGTH;
  rc = nvs_get_str(my_handle, "wifi_name", (char*)wifi_config.sta.ssid, &l);
  if (rc != 0)
  {
    wifi_config.sta.ssid[0] = 0;
  }
  else
  {
    ESP_LOGI(TAG, "wifi_name: %s", wifi_config.sta.ssid);
  }
  l = WIFI_PASSWORD_LENGTH;
  rc = nvs_get_str(my_handle, "wifi_pwd", (char*)wifi_config.sta.password, &l);
  if (rc != 0)
  {
    wifi_config.sta.password[0] = 0;
  }
  else
  {
    ESP_LOGI(TAG, "wifi_pwd: %s", wifi_config.sta.password);
  }
  nvs_close(my_handle);
}

void nvs_set_wifi_name(void)
{
  int rc;
  nvs_handle_t my_handle;

  rc = nvs_open("storage", NVS_READWRITE, &my_handle);
  if (rc != 0)
  {
    ESP_LOGE(TAG, "nvs_open failed");
    return;
  }
  nvs_set_str(my_handle, "wifi_name", (const char *)wifi_config.sta.ssid);
  nvs_commit(my_handle);
  nvs_close(my_handle);
}

void nvs_set_wifi_pwd(void)
{
  int rc;
  nvs_handle_t my_handle;

  rc = nvs_open("storage", NVS_READWRITE, &my_handle);
  if (rc != 0)
  {
    ESP_LOGE(TAG, "nvs_open failed");
    return;
  }
  nvs_set_str(my_handle, "wifi_pwd", (const char *)wifi_config.sta.password);
  nvs_commit(my_handle);
  nvs_close(my_handle);
}