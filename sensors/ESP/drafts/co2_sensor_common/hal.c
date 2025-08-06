#include "board.h"
#include "led_strip.h"
#include "driver/i2c.h"
#include <sht4x.h>
#include <scd30.h>
#include <env.h>

#define I2C_MASTER_TX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */

static unsigned int s_led_state;

static led_strip_handle_t led_strip;

static void configure_led(void)
{
  /* LED strip initialization with the GPIO and pixels number*/
  led_strip_config_t strip_config = {
      .strip_gpio_num = PIN_LED,
      .max_leds = 1, // at least one LED on board
  };
  led_strip_rmt_config_t rmt_config = {
      .resolution_hz = 10 * 1000 * 1000, // 10MHz
  };
  ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
  /* Set all LED off to clear all pixels */
  led_strip_clear(led_strip);

  s_led_state = 0;
}

static void set_led(uint32_t red, uint32_t green, uint32_t blue)
{
  /* Set the LED pixel using RGB from 0 (0%) to 255 (100%) for each color */
  led_strip_set_pixel(led_strip, 0, red, green, blue);
  /* Refresh the strip to send data */
  led_strip_refresh(led_strip);
}

void blink_led(void)
{
  s_led_state = s_led_state ? 0 : 8;
  set_led(0, s_led_state, 0);
}

static esp_err_t i2c_master_init(void)
{
  i2c_config_t conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = PIN_SDA,
    .scl_io_num = PIN_SCL,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = I2C_MASTER_FREQ_HZ,
  };

  i2c_param_config(I2C_MASTER_NUM, &conf);

  return i2c_driver_install(I2C_MASTER_NUM, conf.mode, I2C_MASTER_RX_BUF_DISABLE,
                            I2C_MASTER_TX_BUF_DISABLE, 0);
}

esp_err_t sht40_register_read(uint8_t *rdata, size_t rlen, uint8_t *data, size_t len)
{
  return i2c_master_write_read_device(I2C_MASTER_NUM, SHT40_SENSOR_ADDR, rdata,
                                      rlen, data, len, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

esp_err_t sht40_read(uint8_t *data, size_t len)
{
  return i2c_master_read_from_device(I2C_MASTER_NUM, SHT40_SENSOR_ADDR, data, len,
                                    I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

esp_err_t sht40_register_write(uint8_t *data, size_t len)
{
  return i2c_master_write_to_device(I2C_MASTER_NUM, SHT40_SENSOR_ADDR, data, len,
                                    I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

esp_err_t scd30_write(uint8_t *data, size_t len)
{
  return i2c_master_write_to_device(I2C_MASTER_NUM, SCD30_SENSOR_ADDR, data, len,
                                    I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

esp_err_t scd30_read(uint8_t *data, size_t len)
{
  return i2c_master_read_from_device(I2C_MASTER_NUM, SCD30_SENSOR_ADDR, data, len,
                                    I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

esp_err_t scd30_command(uint8_t *wdata, size_t wlen, uint8_t *rdata, size_t rlen)
{
  return i2c_master_write_read_device(I2C_MASTER_NUM, SCD30_SENSOR_ADDR, wdata,
                                      wlen, rdata, rlen, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

void configure_hal(void)
{
  configure_led();
  i2c_master_init();
}
