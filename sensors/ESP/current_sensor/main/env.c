#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <freertos/semphr.h>
#include "stdint.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "led.h"
#include "env.h"

#define I2C_MASTER_NUM              0                          /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
#define I2C_MASTER_FREQ_HZ          400000                     /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       10

static const char *TAG = "env";

uint16_t current_val;
static volatile unsigned int sum_val;
static SemaphoreHandle_t env_semaphore;

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

esp_err_t ads1115_register_write(uint8_t *data, size_t len)
{
  return i2c_master_write_to_device(I2C_MASTER_NUM, ADS1115_ADDR, data, len,
                                    I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

int16_t ads1115_read(void)
{
  union {
    uint8_t data[2];
    int16_t idata;
  } d, d2;

  d2.data[0] = 0;
  i2c_master_write_read_device(I2C_MASTER_NUM, ADS1115_ADDR, d2.data,
                              1, d.data, 2, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
  d2.data[0] = d.data[1];
  d2.data[1] = d.data[0];
  return d2.idata;
}

static esp_err_t ads1115_start(void)
{
  // set config register and start conversion
  // differential AIN0-AIN1, 2.048v, 860SPS
  uint8_t data[3];
  data[0] = 1;
  data[1] = 0b10000101;
  data[2] = 0b11100011;
  return ads1115_register_write(data, 3);
}

static void periodic_timer_callback(void* arg)
{
  BaseType_t mustYield = pdFALSE;
  int v = ads1115_read() >> 2;
  ads1115_start();
  xSemaphoreTakeFromISR(env_semaphore, &mustYield);
  sum_val += abs(v);
  xSemaphoreGive(env_semaphore);
}

void post_init_env(void) {}

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
  rc = ads1115_start();
  if (rc != ESP_OK)
  {
    ESP_LOGE(TAG, "ads1115_start error %d", rc);
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

  const esp_timer_create_args_t periodic_timer_args = {
      .callback = &periodic_timer_callback,
      /* name is optional, but may help identify the timer when debugging */
      .name = "periodic"
  };

  esp_timer_handle_t periodic_timer;
  ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
  // 500 hz timer
  ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 2000));
}

int get_env(void)
{
  unsigned int val;
  xSemaphoreTake(env_semaphore, portMAX_DELAY);
  val = sum_val;
  sum_val = 0;
  xSemaphoreGive(env_semaphore);
  current_val = val / PWR_DIVIDER / (SEND_INTERVAL / 1000);
  ESP_LOGI(TAG, "pwr = %u", current_val);
  return 0;
}

