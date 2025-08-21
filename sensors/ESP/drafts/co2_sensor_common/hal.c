#include "board.h"
#include "led_strip.h"
#include "driver/i2c.h"
#include <sht4x.h>
#include <scd30.h>
#include <env.h>
#include <epd_ssd1680.h>
#include "esp_log.h"
#include <display.h>
#include <cc1101.h>
#include <vl6180.h>
#include "esp_timer.h"
#include <VL53L0X.h>
#include <veml7700.h>
#include <tsl2591.h>

#define I2C_MASTER_TX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */

static const char *TAG = "hal";

static unsigned int s_led_state;

static led_strip_handle_t led_strip;
static spi_device_handle_t spi_handle;

#ifdef PIN_LED
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
#else
static void set_led(uint32_t red, uint32_t green, uint32_t blue)
{
}
void blink_led(void)
{
}
#endif

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

#ifdef PIN_DISPLAY_CS
static void display_init(void)
{
  gpio_reset_pin(PIN_DISPLAY_CS);
  SSD1680_CS_SET;
  gpio_set_direction(PIN_DISPLAY_CS, GPIO_MODE_OUTPUT);
  gpio_reset_pin(PIN_DC);
  gpio_set_direction(PIN_DC, GPIO_MODE_OUTPUT);
  gpio_reset_pin(PIN_RES);
  SSD1680_RES_CLR;
  gpio_set_direction(PIN_RES, GPIO_MODE_OUTPUT);
  gpio_reset_pin(PIN_BUSY);
  gpio_set_direction(PIN_BUSY, GPIO_MODE_INPUT);
  gpio_set_pull_mode(PIN_BUSY, GPIO_PULLDOWN_ONLY);
}
#endif

#ifdef PIN_MOSI
esp_err_t spi_master_init(void)
{
  esp_err_t err;

  spi_bus_config_t buscfg={
    .miso_io_num = PIN_MISO,
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
    .clock_speed_hz = 1000000,
    .mode = 0,
    .spics_io_num = -1,
    .queue_size = 1,
  };
  return spi_bus_add_device(SPI2_HOST, &devcfg, &spi_handle);
}
#endif

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

int bh1750_write(unsigned char command)
{
  return i2c_master_write_to_device(I2C_MASTER_NUM, BH1750_ADDR, &command, 1,
                                    I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

int bh1750_read(unsigned char *result)
{
  return i2c_master_read_from_device(I2C_MASTER_NUM, BH1750_ADDR, result, 2,
                                    I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

int vl6180ReadRegister(unsigned short registerId, unsigned char *pData, unsigned int size)
{
  return i2c_master_write_read_device(I2C_MASTER_NUM, VL6180_ADDRESS7, (unsigned char*)&registerId,
                                      2, pData, size, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

int vl6180WriteRegister(unsigned short registerId, unsigned char *pData, unsigned int size)
{
  unsigned int size2 = size += 2;
  unsigned char buffer[size2];
  buffer[0] = registerId;
  buffer[1] = registerId >> 8;
  memcpy(&buffer[2], pData, size);
  return i2c_master_write_to_device(I2C_MASTER_NUM, VL6180_ADDRESS7, buffer, size2,
                                    I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

static void configure_inputs(void)
{
  gpio_config_t io_conf = {0};
  io_conf.intr_type = GPIO_INTR_DISABLE;
#ifdef PIN_GDO0
  io_conf.pin_bit_mask = BIT64(PIN_BTN) | BIT64(PIN_GDO0) | BIT64(PIN_GDO2);
#else
  io_conf.pin_bit_mask = BIT64(PIN_BTN);
#endif
  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.pull_up_en = 1;
  io_conf.pull_down_en = 0;
  gpio_config(&io_conf);
}

#ifdef PIN_VL6180_IO0
static void configure_vl6180(void)
{
  gpio_reset_pin(PIN_VL6180_IO0);
  VL6180_IO0_LOW;
  gpio_set_direction(PIN_VL6180_IO0, GPIO_MODE_OUTPUT);
}
#endif

#ifdef PIN_CC1101_CS
static void configure_cc1101(void)
{
  gpio_reset_pin(PIN_CC1101_CS);
  cc1101_CSN_SET(0);
  gpio_set_direction(PIN_CC1101_CS, GPIO_MODE_OUTPUT);
}
#endif

#ifdef PIN_VL53L1_XSCHUT
static void configure_vl53l(void)
{
  gpio_reset_pin(PIN_VL53L1_XSCHUT);
  gpio_set_level(PIN_VL53L1_XSCHUT, 0);
  gpio_set_direction(PIN_VL53L1_XSCHUT, GPIO_MODE_OUTPUT);
}
#endif
#ifdef PIN_VL53L0_XSCHUT
static void configure_vl53l(void)
{
  gpio_reset_pin(PIN_VL53L0_XSCHUT);
  gpio_set_level(PIN_VL53L0_XSCHUT, 0);
  gpio_set_direction(PIN_VL53L0_XSCHUT, GPIO_MODE_OUTPUT);
}
#endif


void configure_hal(void)
{
#ifdef PIN_LED
  configure_led();
#endif
  configure_inputs();
#ifdef PIN_VL6180_IO0
  configure_vl6180();
#endif
#ifdef PIN_CC1101_CS
  configure_cc1101();
#endif
#if defined(PIN_VL53L1_XSCHUT) || defined(PIN_VL53L0_XSCHUT)
  configure_vl53l();
#endif

  esp_err_t rc = i2c_master_init();
  if (rc != ESP_OK)
  {
    ESP_LOGE(TAG, "i2c_master_init error %d", rc);
    set_led(8, 0, 0);
    while (1){}
  }

#ifdef PIN_DISPLAY_CS
  display_init();
#endif

#ifdef PIN_MOSI
  rc = spi_master_init();
  if (rc != ESP_OK)
  {
    ESP_LOGE(TAG, "spi_master_display_init error %d", rc);
    set_led(8, 0, 0);
    while (1){}
  }
#endif
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
#ifdef PIN_DISPLAY_CS
  //if (data)
  //  ESP_LOGI(TAG, "command: 0x%02x, data length %d, data: 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X",
  //            command, data_length, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
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
#endif
}

void Log(const char *name, int value)
{
  ESP_LOGI(TAG, "%s: %d", name, value);
}

void LcdDrawChar(unsigned short x, unsigned short y, char ch, const FONT_INFO *font, unsigned short textColor,
                  unsigned short bkColor)
{
  ssd1680_draw_char(x, y, ch, font, textColor, bkColor);
}

int cc1101_RW(unsigned int device_num, unsigned char *txdata, unsigned char *rxdata, unsigned int size)
{
#ifdef PIN_CC1101_CS
  esp_err_t err;
  unsigned int rc;

  if (size < 2)
    return 200;

  rc = CC1101_TIMEOUT;
  while (--rc && cc1101_GD2) // waiting for chip ready
    ;
  if (!rc)
    return 201; // timeout

  err = spi_device_acquire_bus(spi_handle, portMAX_DELAY);
  if (err != ESP_OK)
    return err;

  cc1101_CSN_CLR(0);

  if (size > 2)
    txdata[0] |= CC1101_BURST;

  spi_transaction_t t = {
    .length = 8 * size,
    .tx_buffer = txdata,
    .rx_buffer = rxdata
  };
  err = spi_device_polling_transmit(spi_handle, &t);

  cc1101_CSN_SET(0);

  spi_device_release_bus(spi_handle);

  return err;
#else
  return 1;
#endif
}

int cc1101_strobe(unsigned int device_num, unsigned char data, unsigned char *status)
{
#ifdef PIN_CC1101_CS
  esp_err_t err;
  unsigned int rc;

  if (data != CC1101_STROBE_SRES)
  {
    rc = CC1101_TIMEOUT;
    while (--rc && cc1101_GD2) // waiting for chip ready
      ;
    if (!rc)
      return 201; // timeout
  }

  err = spi_device_acquire_bus(spi_handle, portMAX_DELAY);
  if (err != ESP_OK)
    return err;

  cc1101_CSN_CLR(0);

  spi_transaction_t t = {
    .length = 8,
    .tx_buffer = &data,
    .rx_buffer = status
  };
  err = spi_device_polling_transmit(spi_handle, &t);

  cc1101_CSN_SET(0);

  spi_device_release_bus(spi_handle);

  return err;
#else
  return 1;
#endif
}

unsigned long long int get_time_ms(void)
{
  return esp_timer_get_time() / 1000;
}

// Write an 8-bit register
int VL53L0x_writeReg(unsigned char reg, unsigned char value)
{
  ESP_LOGI(TAG, "Writing reg 0x%02x, value 0x%02x", reg, value);
  unsigned char data[2] = {reg, value};
  return i2c_master_write_to_device(I2C_MASTER_NUM, VL53L0_SENSOR_ADDR, data, 2,
                                    I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

// Write a 16-bit register
int VL53L0x_writeReg16Bit(unsigned char reg, unsigned short value)
{
  ESP_LOGI(TAG, "Writing reg 0x%02x, value 0x%04x", reg, value);
  unsigned char data[3] = {reg, (unsigned char)(value >> 8), (unsigned char)value};
  return i2c_master_write_to_device(I2C_MASTER_NUM, VL53L0_SENSOR_ADDR, data, 3,
                                    I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

// Write a 32-bit register
int VL53L0x_writeReg32Bit(unsigned char reg, unsigned long value)
{
  ESP_LOGI(TAG, "Writing reg 0x%02x, value 0x%08x", reg, value);
  unsigned char data[5] = {reg, (unsigned char)(value >> 24), (unsigned char)(value >> 16),
                            (unsigned char)(value >> 8), (unsigned char)value};
  return i2c_master_write_to_device(I2C_MASTER_NUM, VL53L0_SENSOR_ADDR, data, 5,
                                    I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

// Read an 8-bit register
int VL53L0x_readReg(unsigned char reg, unsigned char *value)
{
  int rc = i2c_master_write_read_device(I2C_MASTER_NUM, VL53L0_SENSOR_ADDR, (unsigned char*)&reg,
                                      1, value, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
  if (rc)
    return rc;
  ESP_LOGI(TAG, "Read reg 0x%02x, value 0x%02x", reg, value[0]);
  return 0;
}

// Read a 16-bit register
int VL53L0x_readReg16Bit(unsigned char reg, unsigned short *value)
{
  unsigned char data[2];
  int rc = i2c_master_write_read_device(I2C_MASTER_NUM, VL53L0_SENSOR_ADDR, (unsigned char*)&reg,
                                      1, data, 2, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
  if (rc)
    return rc;
  *value = ((unsigned short)data[0] << 8) | (unsigned short)data[1];
  ESP_LOGI(TAG, "Read reg 0x%02x, value 0x%02x%02x", reg, data[0], data[1]);
  return 0;
}

// Read a 32-bit register
int VL53L0x_readReg32Bit(unsigned char reg, unsigned long *value)
{
  unsigned char data[4];
  int rc = i2c_master_write_read_device(I2C_MASTER_NUM, VL53L0_SENSOR_ADDR, (unsigned char*)&reg,
                                      1, data, 4, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
  if (rc)
    return rc;
  *value = ((unsigned long)data[0] << 24)
          | ((unsigned long)data[1] << 16)
          | ((unsigned long)data[2] << 8)
          | (unsigned long)data[3];
  ESP_LOGI(TAG, "Read reg 0x%02x, value 0x%02x%02x%02x%02x", reg, data[0], data[1], data[2], data[3]);
  return 0;
}

// Write an arbitrary number of bytes from the given array to the sensor,
// starting at the given register
int VL53L0x_writeMulti(unsigned char reg, unsigned char const *src, unsigned char count)
{
  ESP_LOGI(TAG, "Write multi reg 0x%02x, count %d", reg, count);
  return i2c_master_write_to_device(I2C_MASTER_NUM, VL53L0_SENSOR_ADDR, src, count,
                                    I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

// Read an arbitrary number of bytes from the sensor, starting at the given
// register, into the given array
int VL53L0x_readMulti(unsigned char reg, unsigned char * dst, unsigned char count)
{
  ESP_LOGI(TAG, "Read multi reg 0x%02x, count %d", reg, count);
  return i2c_master_write_read_device(I2C_MASTER_NUM, VL53L0_SENSOR_ADDR, (unsigned char*)&reg,
                                      1, dst, count, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

int veml7700_read(unsigned char reg, unsigned short *data)
{
  return i2c_master_write_read_device(I2C_MASTER_NUM, VEML7700_I2C_ADDRESS, (unsigned char*)&reg,
                                      1, (unsigned char*)data, 2, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

int veml7700_write(unsigned char reg, unsigned short value)
{
  unsigned char data[3] = {reg, (unsigned char)value, (unsigned char)(value >> 8)};
  return i2c_master_write_to_device(I2C_MASTER_NUM, VEML7700_I2C_ADDRESS, data, 3,
                                    I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

int tsl2591_read8(unsigned char reg, unsigned char *data)
{
  int rc = i2c_master_write_read_device(I2C_MASTER_NUM, TSL2591_I2C_ADDRESS, (unsigned char*)&reg,
                                      1, (unsigned char*)data, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
  ESP_LOGI(TAG, "Read reg 0x%02x, value %d rc %d", reg, data[0], rc);
  return rc;
}

int tsl2591_read16(unsigned char reg, unsigned short *data)
{
  int rc = i2c_master_write_read_device(I2C_MASTER_NUM, TSL2591_I2C_ADDRESS, (unsigned char*)&reg,
                                      1, (unsigned char*)data, 2, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
  ESP_LOGI(TAG, "Read reg 0x%02x, value %d rc %d", reg, data[0], rc);
  return rc;
}

int tsl2591_write(unsigned char reg, unsigned char value)
{
  unsigned char data[2] = {reg, value};
  int rc = i2c_master_write_to_device(I2C_MASTER_NUM, TSL2591_I2C_ADDRESS, data, 2,
                                    I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
  ESP_LOGI(TAG, "Write reg 0x%02x, value %d rc %d", reg, value, rc);
  return rc;
}

int tsl2591_command(unsigned char command)
{
  int rc = i2c_master_write_to_device(I2C_MASTER_NUM, TSL2591_I2C_ADDRESS, &command, 1,
                                    I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
  ESP_LOGI(TAG, "Command 0x%02x, rc %d", command, rc);
  return rc;
}
