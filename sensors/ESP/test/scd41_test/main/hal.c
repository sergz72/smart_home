#include "board.h"
#include "driver/i2c_master.h"
#include <scd4x.h>
#include <rtc_ds1307.h>
#include <rtc_ds3231.h>

static i2c_master_bus_handle_t i2c_bus_handle;
static i2c_master_dev_handle_t scd41_handle;
static i2c_master_dev_handle_t rtc_handle;

esp_err_t i2c_probe(uint16_t address, int timeout_ms)
{
  return i2c_master_probe(i2c_bus_handle, address, timeout_ms);
}

esp_err_t i2c_master_init(void)
{
  i2c_master_bus_config_t conf = {
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .i2c_port = I2C_NUM_0,
    .sda_io_num = PIN_SDA,
    .scl_io_num = PIN_SCL
  };

  esp_err_t rc = i2c_new_master_bus(&conf, &i2c_bus_handle);
  if (rc != ESP_OK)
    return rc;

  i2c_device_config_t dev_cfg = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = SCD4X_SENSOR_ADDR,
    .scl_speed_hz = I2C_MASTER_FREQ_HZ
  };

  rc = i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &scd41_handle);
  if (rc != ESP_OK)
    return rc;

  dev_cfg.device_address = DS1307_I2C_ADDRESS >> 1;

  return i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &rtc_handle);
}

esp_err_t scd4x_write(uint8_t *data, size_t len)
{
  return i2c_master_transmit(scd41_handle, data, len, I2C_MASTER_TIMEOUT_MS);
}

esp_err_t scd4x_read(uint8_t *data, size_t len)
{
  return i2c_master_receive(scd41_handle, data, len, I2C_MASTER_TIMEOUT_MS);
}

esp_err_t scd4x_command(uint8_t *wdata, size_t wlen, uint8_t *rdata, size_t rlen)
{
  return i2c_master_transmit_receive(scd41_handle, wdata,
                                      wlen, rdata, rlen, I2C_MASTER_TIMEOUT_MS);
}

int i2c_ds1307_write(const unsigned char *data, int data_length)
{
  return i2c_master_transmit(rtc_handle, data, data_length, I2C_MASTER_TIMEOUT_MS);
}

int i2c_ds1307_transfer(const unsigned char *wdata, int wdata_length, unsigned char *rdata, int rdata_length)
{
  return i2c_master_transmit_receive(rtc_handle, wdata,
                                      wdata_length, rdata, rdata_length, I2C_MASTER_TIMEOUT_MS);
}

int i2c_ds3231_write(const unsigned char *data, int data_length)
{
  return i2c_master_transmit(rtc_handle, data, data_length, I2C_MASTER_TIMEOUT_MS);
}

int i2c_ds3231_transfer(const unsigned char *wdata, int wdata_length, unsigned char *rdata, int rdata_length)
{
  return i2c_master_transmit_receive(rtc_handle, wdata,
                                      wdata_length, rdata, rdata_length, I2C_MASTER_TIMEOUT_MS);
}
