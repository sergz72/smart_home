#include "common.h"
#include "env.h"
#ifndef NO_ENV
#include "bh1750.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "stdint.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "cc1101_func.h"
#include "led.h"
#include "hal.h"
#include "laCrosseDecoder.h"

static const char *TAG = "env";
volatile static int currentTime, prevLevel;
static gptimer_handle_t gptimer;
scd30_result result_scd30;

int16_t temp_val = 2510;
uint16_t humi_val = 4020;
int16_t ext_temp_val = 0;
uint32_t co2_level = 0;
uint32_t luminocity = 0;

void post_init_env(void) {}

void init_env(void)
{
  esp_err_t rc;

  gpio_init();

  rc = i2c_master_init();
  if (rc != ESP_OK)
  {
    ESP_LOGE(TAG, "i2c_master_init error %d", rc);
    set_led_red();
    while (1){}
  }

  rc = spi_master_init();
  if (rc != ESP_OK)
  {
    ESP_LOGE(TAG, "spi_master_init error %d", rc);
    set_led_red();
    while (1){}
  }

#ifdef USE_CC1101
  if (cc1101Init())
  {
    ESP_LOGE(TAG, "cc1101Init failed");
    set_led_red();
    while (1){}
  }
#endif

  scd30_init_sensor(60);
  bh1750_set_measurement_time(254);
}

static void get_ext_env(void)
{
  int i, flags;
  unsigned char *laCrossePacket;

  currentTime = prevLevel = flags = 0;

  // 200 us timer
  gptimer_start(gptimer);

  cc1101ReceiveStart();
  for (i = 0; i < 240; i++)
  {
    laCrossePacket = cc1101Receive();
    if (laCrossePacket)
    {
      ESP_LOGI(TAG, "PACKET RECEIVED!!! %d %d %d %d %d", laCrossePacket[0], laCrossePacket[1], laCrossePacket[2],
               laCrossePacket[3], laCrossePacket[4]);
      const char *message = laCrosseDecode(8, laCrossePacket, &ext_temp_val, NULL);
      if (!message)
      {
        ext_temp_val *= 10;
        ESP_LOGI(TAG, "PACKET DECODE SUCCESS, t = %d", ext_temp_val);
        cc1101ReceiveStop();
        return;
      }
      ESP_LOGE(TAG, "PACKET DECODE FAILURE %s", message);
      cc1101ReceiveStop();
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      cc1101ReceiveStart();
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  cc1101ReceiveStop();
  if (i == 240)
    ESP_LOGE(TAG, "External temperature receiver failure");
}

int get_env(void)
{
  unsigned short result;

  int rc = bh1750_measure(BH1750_ONE_TIME_HIGH_RES_MODE_2, &result);
  if (rc)
  {
    ESP_LOGE(TAG, "bh1750_measure error %d", rc);
    return rc;
  }
  luminocity = result * 11;
  ESP_LOGI(TAG, "Luminocity: %d", luminocity);
  rc = scd30_measure(&result_scd30);
  if (rc)
  {
    ESP_LOGE(TAG, "scd30_measure error %d", rc);
    return rc;
  }
  result_scd30.temperature += TEMPERATURE_OFFSET;
  temp_val = (int16_t)(result_scd30.temperature * 100);
  humi_val = (int16_t)(result_scd30.humidity * 100);
  co2_level = (uint16_t)(result_scd30.co2 * 100);

  get_ext_env();
  return 0;
}
#else
int16_t temp_val = 2510;
uint16_t humi_val = 4020;
int16_t ext_temp_val = 0;
uint16_t co2_level = 0;
uint32_t luminocity = 0;

void init_env(void)
{
}

void post_init_env(void) {}

int get_env(void)
{
  return 0;
}
#endif
