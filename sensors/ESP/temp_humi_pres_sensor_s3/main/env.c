#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "stdint.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "PrologueDecoder.h"
#include "driver/gptimer.h"
#include "cc1101_func.h"
#include "common.h"
#include "led.h"
#include "env.h"
#include "hal.h"
#include "laCrosseDecoder.h"
#ifdef USE_BME280
#include "bme280.h"
#endif

static const char *TAG = "env";
static char prologueBuffer[DECODER_BUFFER_SIZE * sizeof(ProloguePacket)];
static SemaphoreHandle_t env_semaphore;
volatile static int currentTime, prevLevel;
static gptimer_handle_t gptimer;

int16_t temp_val = 2510;
uint16_t humi_val = 4020;
#ifdef SEND_PRESSURE
uint32_t pres_val = 10003;
#endif
int16_t ext_temp_val = 0;
uint16_t ext_humi_val = 0;
int16_t ext_temp_val2 = 0;

void post_init_env(void) {}

/* Timer interrupt service routine */
static bool IRAM_ATTR on_timer_alarm_cb(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_data)
{
  BaseType_t mustYield = pdFALSE;
  currentTime += 200;
  int level = gpio_get_level(GPIO_RECEIVER_DATA_IO);
  if (level != prevLevel)
  {
    prevLevel = level;
    xSemaphoreTakeFromISR(env_semaphore, &mustYield);
    PrologueDecoder(level, currentTime);
    xSemaphoreGive(env_semaphore);
  }
  return false;
}

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
#ifdef USE_BME280
  if (bme280_readCalibrationData())
  {
    ESP_LOGE(TAG, "bme280_readCalibrationData error %d", rc);
    set_led_red();
    while (1){}
  }
#endif

  if (!cc1101Init())
  {
    ESP_LOGE(TAG, "cc1101Init failed");
    set_led_red();
    while (1){}
  }

  env_semaphore = xSemaphoreCreateBinary();
  if (!env_semaphore) {
    ESP_LOGE(TAG, "semaphore create fail.");
    set_led_red();
    while (1);
  }
  xSemaphoreGive(env_semaphore);

  PrologueDecoderInit(DECODER_BUFFER_SIZE, prologueBuffer);

  gptimer_config_t timer_config = {
      .clk_src = GPTIMER_CLK_SRC_DEFAULT,
      .direction = GPTIMER_COUNT_UP,
      .resolution_hz = 1000000, // 1MHz, 1 tick = 1us
  };
  ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

  // 200 us timer
  gptimer_alarm_config_t alarm_config = {
      .reload_count = 0,
      .alarm_count = 200,
      .flags.auto_reload_on_alarm = true,
  };
  gptimer_event_callbacks_t cbs = {
      .on_alarm = on_timer_alarm_cb,
  };
  ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, NULL));
  ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));
  ESP_ERROR_CHECK(gptimer_enable(gptimer));
}

#define LA_CROSSE_FLAG 1
#define PROLOGUE_FLAG 2
#define ALL_DONE 3

static int get_ext_env(void)
{
  int i, flags;
  ProloguePacket *p = NULL;
  unsigned char *laCrossePacket;

  currentTime = prevLevel = flags = 0;

  // 200 us timer
  gptimer_start(gptimer);

  for (i = 0; i < 240; i++)
  {
    if (!(flags & LA_CROSSE_FLAG))
    {
      if ((i % 10) == 0)
      {
//      ESP_LOGI(TAG, "cc1101ReceiveStart");
        cc1101ReceiveStart();
      } else if ((i % 10) == 9)
      {
//      ESP_LOGI(TAG, "cc1101ReceiveStop");
        cc1101ReceiveStop();
      } else
      {
        laCrossePacket = cc1101Receive();
        if (laCrossePacket)
        {
          ESP_LOGI(TAG, "PACKET RECEIVED!!! %d %d %d %d %d", laCrossePacket[0], laCrossePacket[1], laCrossePacket[2],
                   laCrossePacket[3], laCrossePacket[4]);
          laCrossePacket = laCrosseDecode(8, laCrossePacket, &ext_temp_val2, NULL);
          if (!laCrossePacket)
          {
            ext_temp_val2 *= 10;
            ESP_LOGI(TAG, "PACKET DECODE SUCCESS, t = %d", ext_temp_val2);
            cc1101ReceiveStop();
            if (flags & PROLOGUE_FLAG)
              break;
            flags |= LA_CROSSE_FLAG;
          }
          else
            ESP_LOGE(TAG, "PACKET DECODE FAILURE %s", laCrossePacket);
        }
      }
    }
    if (!(flags & PROLOGUE_FLAG))
    {
      xSemaphoreTake(env_semaphore, portMAX_DELAY);
      p = queue_peek(&prologueDecoderQueue);
      if (p)
      {
        queue_pop(&prologueDecoderQueue);
        xSemaphoreGive(env_semaphore);
        if (p->id == SENSOR_ID && p->type == PACKET_TYPE)
        {
          ext_temp_val = (int16_t) (p->temperature * 10);
          ext_humi_val = (uint16_t) p->humidity * 100;
          ESP_LOGI(TAG, "External sensor id: %d, packet type: %d, temperature: %d.%d, humidity %d.%d", p->id, p->type,
                   ext_temp_val / 100, ext_temp_val % 100, ext_humi_val / 100, ext_humi_val % 100);
          gptimer_stop(gptimer);
          PrologueDecoderReset();
          if (flags & LA_CROSSE_FLAG)
            break;
          flags |= PROLOGUE_FLAG;
        }
      } else
        xSemaphoreGive(env_semaphore);
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  if (i == 240)
  {
    ESP_LOGE(TAG, "External temperature receiver failure");
    gptimer_stop(gptimer);
    PrologueDecoderReset();
    cc1101ReceiveStop();
  }
  return 0;
}

int get_env(void)
{
#ifdef USE_BME280
  bme280_data data;
#endif

#ifdef USE_BME280
#ifdef SEND_PRESSURE
  unsigned int rc = bme280_get_data(&data, 1);
#else
  unsigned int rc = bme280_get_data(&data, 0);
#endif
  if (rc != ESP_OK)
    return 1;
  temp_val = (int16_t)data.temperature;
  humi_val = (int16_t)data.humidity;
#ifdef SEND_PRESSURE
  pres_val = data.pressure;
  ESP_LOGI(TAG, "BME280 temperature: %d.%d, humidity %d.%d, pressure: %d.%d", temp_val / 100, temp_val % 100, humi_val / 100, humi_val % 100,
           (int)(pres_val / 10), (int)(pres_val % 10));
#else
  ESP_LOGI(TAG, "BME280 temperature: %d.%d, humidity: %d.%d", temp_val / 100, temp_val % 100, humi_val / 100, humi_val % 100);
#endif
#endif
  return get_ext_env();
}
