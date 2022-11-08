#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "stdint.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "led.h"
#include "env.h"
#include "sht3x.h"
#include "qmp6988.h"
#ifdef USE_BME280
#include "bme280.h"
#endif

#define I2C_MASTER_NUM              0                          /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
#define I2C_MASTER_FREQ_HZ          100000                     /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       1000

static const char *TAG = "env";

int16_t temp_val = 2510;
uint16_t humi_val = 4020;
#ifdef SEND_PRESSURE
uint32_t pres_val = 10003;
#endif

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

#ifdef SHT30_SENSOR_ADDR
esp_err_t sht30_register_read(uint8_t *rdata, size_t rlen, uint8_t *data, size_t len)
{
  return i2c_master_write_read_device(I2C_MASTER_NUM, SHT30_SENSOR_ADDR, rdata,
                                      rlen, data, len, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

esp_err_t sht30_read(uint8_t *data, size_t len)
{
  return i2c_master_read_from_device(I2C_MASTER_NUM, SHT30_SENSOR_ADDR, data, len,
                                    I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

esp_err_t sht30_register_write(uint8_t *data, size_t len)
{
  return i2c_master_write_to_device(I2C_MASTER_NUM, SHT30_SENSOR_ADDR, data, len,
                                    I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}
#endif

#if defined(SEND_PRESSURE) && defined(QMP6988_SENSOR_ADDR)
esp_err_t qmp6988_register_read(uint8_t *rdata, size_t rlen, uint8_t *data, size_t len)
{
  return i2c_master_write_read_device(I2C_MASTER_NUM, QMP6988_SENSOR_ADDR, rdata,
                                      rlen, data, len, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

esp_err_t qmp6988_register_write(uint8_t *data, size_t len)
{
  return i2c_master_write_to_device(I2C_MASTER_NUM, QMP6988_SENSOR_ADDR, data, len,
                                    I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}
#endif

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
#ifdef SHT30_SENSOR_ADDR
  if (sht3x_init_sensor())
  {
    set_led_red();
    while (1){}
  }
#endif
#if defined(SEND_PRESSURE) && defined(QMP6988_SENSOR_ADDR)
  if (qmp6988_init_sensor())
  {
    set_led_red();
    while (1){}
  }
#endif
#ifdef USE_BME280
  if (bme280_readCalibrationData())
  {
    ESP_LOGE(TAG, "bme280_readCalibrationData error %d", rc);
    set_led_red();
    while (1){}
  }
  get_env();
#endif
}

int get_env(void)
{
#if defined(SEND_PRESSURE) && defined(QMP6988_SENSOR_ADDR)
  uint16_t t;
#endif
#ifdef USE_BME280
  bme280_data data;
#endif

#ifdef SHT30_SENSOR_ADDR
  esp_err_t rc = sht3x_measure(&temp_val, &humi_val);
  if (rc != ESP_OK)
    return 1;
  ESP_LOGI(TAG, "SHT30 temperature: %d.%d, humidity: %d.%d", temp_val / 100, temp_val % 100, humi_val / 100, humi_val % 100);
#endif
#if defined(SEND_PRESSURE) && defined(QMP6988_SENSOR_ADDR)
  rc = qmp6988_calcPressure(&pres_val, &t);
  if (rc != ESP_OK)
    return 1;
  ESP_LOGI(TAG, "QMP6988 temperature: %d.%d, pressure: %d.%d", t / 100, t % 100, (int)(pres_val / 10), (int)(pres_val % 10));
#endif
#ifdef USE_BME280
#ifdef SEND_PRESSURE
  unsigned int rc = bme280_get_data(&data, 1);
#else
  unsigned int rc = bme280_get_data(&data, 0);
#endif
  if (rc != ESP_OK)
    return 1;
  temp_val = (int16_t)data.temperature * 10;
  humi_val = (int16_t)data.humidity * 100;
#ifdef SEND_PRESSURE
  pres_val = data.pressure * 10;
  ESP_LOGI(TAG, "BME280 temperature: %d.%d, humidity %d.%d, pressure: %d.%d", temp_val / 100, temp_val % 100, humi_val / 100, humi_val % 100,
           (int)(pres_val / 10), (int)(pres_val % 10));
#else
  ESP_LOGI(TAG, "BME280 temperature: %d.%d, humidity: %d.%d", temp_val / 100, temp_val % 100, humi_val / 100, humi_val % 100);
#endif
#endif
  return 0;
}

