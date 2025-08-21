#include "common.h"
#include "env.h"
#ifndef NO_ENV
#include "board.h"
#ifdef USE_BH1750
#include "bh1750.h"
#endif
#ifdef USE_VEML7700
#include "veml7700.h"
#endif
#ifdef PIN_TSL2591_INT
#include "tsl2591.h"
#endif
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "stdint.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "cc1101_func.h"
#include "led.h"
#include "hal.h"
#include "laCrosseDecoder.h"

static const char *TAG = "env";

static const tsl2591_config tsl_config = {
  .integration_time_ms = 600,
  .als_interrupt_enable = 0,
  .als_thresholds = {0,65535},
  .no_persist_interrupt_enable = 0,
  .no_persist_als_thresholds = {0, 65535},
  .persistence_filter = AnyOutOfRange,
  .sleep_after_interrupt = 0,
};

volatile static int currentTime, prevLevel;
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
#ifdef USE_BH1750
  if (bh1750_set_measurement_time(254))
  {
    ESP_LOGE(TAG, "bh1750_set_measurement_time failed");
    set_led_red();
    while (1){}
  }
#endif
#ifdef USE_VEML7700
  if (veml7700_init())
  {
    ESP_LOGE(TAG, "veml7700_init failed");
    set_led_red();
    while (1){}
  }
#endif
#ifdef PIN_TSL2591_INT
  if (tsl2591_init(&tsl_config))
  {
    ESP_LOGE(TAG, "tsl2591_init failed");
    set_led_red();
    while (1){}
  }
#endif
}

#ifdef USE_CC1101
static void get_ext_env(void)
{
  int i, flags;
  unsigned char *laCrossePacket;

  currentTime = prevLevel = flags = 0;

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
#endif

#ifdef USE_BH1750
static int measure_luminocity(void)
{
  unsigned short result;

  int rc = bh1750_measure(BH1750_ONE_TIME_HIGH_RES_MODE_2, &result);
  if (rc)
  {
    ESP_LOGE(TAG, "bh1750_measure error %d", rc);
    return rc;
  }
  luminocity = (uint32_t)result * 11;
  ESP_LOGI(TAG, "Luminocity: %d", luminocity);
  return 0;
}
#endif
#ifdef USE_VEML7700
static int measure_luminocity(void)
{
  veml7700_result result;

  int rc = veml7700_measure(&result);
  if (rc)
  {
    ESP_LOGE(TAG, "veml7700_measure error %d", rc);
    return rc;
  }
  ESP_LOGI(TAG, "luminocity: %f gainx8 %d tries %d", result.lux, result.gainx8, result.tries);
  luminocity = (uint32_t)(result.lux * 100);
  return 0;
}
#endif
#ifdef PIN_TSL2591_INT
static int measure_luminocity(void)
{
  tsl2591_result result;

  int rc = tsl2591_measure(&result);
  if (rc)
  {
    ESP_LOGE(TAG, "tsl2591_measure error %d", rc);
    return rc;
  }
  ESP_LOGI(TAG, "luminocity: %f gain %d tries %d", result.lux, result.gain, result.tries);
  luminocity = (uint32_t)(result.lux * 100);
  return 0;
}
#endif

int get_env(void)
{
  int rc = measure_luminocity();
  if (rc)
    return rc;
  rc = scd30_measure(&result_scd30);
  if (rc)
  {
    ESP_LOGE(TAG, "scd30_measure error %d", rc);
    return rc;
  }
  result_scd30.temperature += TEMPERATURE_OFFSET;
  temp_val = (int16_t)(result_scd30.temperature * 100);
  humi_val = (int16_t)(result_scd30.humidity * 100);
  co2_level = (uint32_t)(result_scd30.co2 * 100);

#ifdef USE_CC1101
  get_ext_env();
#endif
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
