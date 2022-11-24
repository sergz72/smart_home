#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "stdint.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "led.h"
#include "env.h"
#ifdef USE_BME280
#include "bme280.h"
#endif
#include "PrologueDecoder.h"
#include "common.h"
#include "driver/gptimer.h"

#define I2C_MASTER_NUM              0                          /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
#define I2C_MASTER_FREQ_HZ          100000                     /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       1000

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

static esp_err_t i2c_master_init(void)
{
  i2c_config_t conf = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = I2C_MASTER_SDA_IO,
      .scl_io_num = I2C_MASTER_SCL_IO,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master.clk_speed = I2C_MASTER_FREQ_HZ,
  };

  i2c_param_config(I2C_MASTER_NUM, &conf);

  return i2c_driver_install(I2C_MASTER_NUM, conf.mode, I2C_MASTER_RX_BUF_DISABLE,
                            I2C_MASTER_TX_BUF_DISABLE, 0);
}

#ifdef USE_BME280
unsigned int bme280Read(unsigned char register_no, unsigned char *pData, int size)
{
  return i2c_master_write_read_device(I2C_MASTER_NUM, BME280_ADDRESS >> 1, &register_no,
                                      1, pData, size, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

unsigned int bme280Write(unsigned char register_no, unsigned char value)
{
  unsigned char data[2];
  data[0] = register_no;
  data[1] = value;
  return i2c_master_write_to_device(I2C_MASTER_NUM, BME280_ADDRESS >> 1, data, 2,
                                    I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

void delayms(unsigned int ms)
{
  vTaskDelay (ms / portTICK_PERIOD_MS);
}
#endif

static void gpio_init(void)
{
  //zero-initialize the config structure.
  gpio_config_t io_conf;
  //disable interrupt
  io_conf.intr_type = GPIO_INTR_DISABLE;
  //disable pull-down mode
  io_conf.pull_down_en = 0;
  //disable pull-up mode
  io_conf.pull_up_en = 0;
  //bit mask of the pins
  io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
  //set as input mode
  io_conf.mode = GPIO_MODE_INPUT;
  gpio_config(&io_conf);
}

/* Timer interrupt service routine */
static bool IRAM_ATTR on_timer_alarm_cb(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_data)
{
  BaseType_t mustYield = pdFALSE;
  currentTime += 200;
  int level = gpio_get_level(GPIO_INPUT_IO);
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

  rc = i2c_master_init();
  if (rc != ESP_OK)
  {
    ESP_LOGE(TAG, "i2c_master_init error %d", rc);
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

  env_semaphore = xSemaphoreCreateBinary();
  if (!env_semaphore) {
    ESP_LOGE(TAG, "semaphore create fail.");
    set_led_red();
    while (1);
  }
  xSemaphoreGive(env_semaphore);

  gpio_init();
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

static int get_ext_env(void)
{
  int i;
  ProloguePacket *p = NULL;

  currentTime = prevLevel = 0;

  // 200 us timer
  gptimer_start(gptimer);

  for (i = 0; i < 240; i++)
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
        break;
      }
    }
    else
      xSemaphoreGive(env_semaphore);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  if (i == 240)
    ESP_LOGE(TAG, "External temperature receiver failure");
  gptimer_stop(gptimer);
  PrologueDecoderReset();
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
