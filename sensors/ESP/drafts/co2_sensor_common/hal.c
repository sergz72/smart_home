#include "board.h"
#include "led_strip.h"
#include "driver/i2c.h"
#include <sht4x.h>
#include <scd30.h>
#include <env.h>
#include <epd_ssd1680.h>
#include "esp_log.h"

#define I2C_MASTER_TX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */

static const char *TAG = "hal";

static unsigned int s_led_state;

static led_strip_handle_t led_strip;
static spi_device_handle_t spi_handle;

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

esp_err_t spi_master_init(void)
{
  esp_err_t err;

  gpio_reset_pin(PIN_CS);
  SSD1680_CS_SET;
  gpio_set_direction(PIN_CS, GPIO_MODE_OUTPUT);
  gpio_reset_pin(PIN_DC);
  gpio_set_direction(PIN_DC, GPIO_MODE_OUTPUT);
  gpio_reset_pin(PIN_RES);
  SSD1680_RES_CLR;
  gpio_set_direction(PIN_RES, GPIO_MODE_OUTPUT);
  gpio_reset_pin(PIN_BUSY);
  gpio_set_direction(PIN_BUSY, GPIO_MODE_INPUT);
  gpio_set_pull_mode(PIN_BUSY, GPIO_PULLDOWN_ONLY);

  spi_bus_config_t buscfg={
    .miso_io_num = -1,
    .mosi_io_num = PIN_MOSI,
    .sclk_io_num = PIN_SCK,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1
};
  //Initialize the SPI bus
  err = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
  if (err != ESP_OK)
    return err;

  spi_device_interface_config_t devcfg={
    .clock_speed_hz = 5000000,
    .mode = 0,          //SPI mode 0
    .spics_io_num = -1,
    .queue_size = 1,
  };
  return spi_bus_add_device(SPI2_HOST, &devcfg, &spi_handle);
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

static void configure_button(void)
{
  gpio_config_t io_conf = {0};
  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.pin_bit_mask = BIT64(PIN_BTN);
  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.pull_up_en = 1;
  io_conf.pull_down_en = 0;
  gpio_config(&io_conf);
}

void configure_hal(void)
{
  configure_led();
  configure_button();

  esp_err_t rc = i2c_master_init();
  if (rc != ESP_OK)
  {
    ESP_LOGE(TAG, "i2c_master_init error %d", rc);
    set_led(8, 0, 0);
    while (1){}
  }

  rc = spi_master_init();
  if (rc != ESP_OK)
  {
    ESP_LOGE(TAG, "spi_master_init error %d", rc);
    set_led(8, 0, 0);
    while (1){}
  }
}

void delayms(unsigned int ms)
{
  ms /= portTICK_PERIOD_MS;
  if (!ms)
    ms = 1;
  vTaskDelay(ms);
}

void ssd1680_command(unsigned char command, unsigned char *data, unsigned int data_length)
{
  esp_err_t err = spi_device_acquire_bus(spi_handle, portMAX_DELAY);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "spi_device_acquire_bus error %d", err);
    return;
  }
  SSD1680_CS_CLR;
  SSD1680_DC_CLR;

  spi_transaction_t t = {
    .length = 8,
    .tx_buffer = &command,
    .rx_buffer = NULL,
    .rxlength = 0
  };
  err = spi_device_polling_transmit(spi_handle, &t);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "spi command transmit error %d", err);
    spi_device_release_bus(spi_handle);
    SSD1680_CS_SET;
    return;
  }

  SSD1680_DC_SET;

  t.length = 8 * data_length;
  t.rxlength = 0;
  t.tx_buffer = data;
  err = spi_device_polling_transmit(spi_handle, &t);
  if (err != ESP_OK)
    ESP_LOGE(TAG, "spi data transmit error %d", err);

  spi_device_release_bus(spi_handle);

  SSD1680_CS_SET;
}
