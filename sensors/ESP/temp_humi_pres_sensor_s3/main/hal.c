#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "hal.h"
#include "driver/gpio.h"
#include "common.h"
#include "board.h"
#include <cc1101.h>
#include "bme280.h"
#include "mh_z19b.h"
#include "driver/uart.h"
#include "esp_adc/adc_oneshot.h"

#define I2C_MASTER_NUM              0                          /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
#define I2C_MASTER_FREQ_HZ          100000                     /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       1000

#define PIN_UART1_TXD 17
#define PIN_UART1_RXD 18
#define UART1_BAUD_RATE   9600
#define UART1_BUFFER_SIZE 1024

#define ADC_VREF 3300

static spi_device_handle_t spi_handle;
static adc_oneshot_unit_handle_t adc_handle;

esp_err_t i2c_master_init(void)
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

esp_err_t spi_master_init(void)
{
  esp_err_t err;

  spi_bus_config_t buscfg={
      .miso_io_num = PIN_NUM_MISO,
      .mosi_io_num = PIN_NUM_MOSI,
      .sclk_io_num = PIN_NUM_CLK,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
  };
  //Initialize the SPI bus
  err = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
  if (err != ESP_OK)
    return err;

  spi_device_interface_config_t devcfg={
      .clock_speed_hz = 1000000,
      .mode = 0,          //SPI mode 0
      .spics_io_num = PIN_NUM_CS,
      .queue_size = 1,
  };
  return spi_bus_add_device(SPI2_HOST, &devcfg, &spi_handle);
}

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

void uart1_init(void)
{
  uart_config_t uart_config = {
    .baud_rate = UART1_BAUD_RATE,
    .data_bits = UART_DATA_8_BITS,
    .parity    = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT,
  };

  uart_driver_install(1, UART1_BUFFER_SIZE, UART1_BUFFER_SIZE, 0, NULL, ESP_INTR_FLAG_IRAM);
  uart_param_config(1, &uart_config);
  uart_set_pin(1, PIN_UART1_TXD, PIN_UART1_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

void adc_init(void)
{
  adc_oneshot_unit_init_cfg_t init_config2 = {
    .unit_id = ADC_UNIT_1,
    .ulp_mode = ADC_ULP_MODE_DISABLE,
    .clk_src = ADC_RTC_CLK_SRC_DEFAULT
  };
  adc_oneshot_new_unit(&init_config2, &adc_handle);

  adc_oneshot_chan_cfg_t config = {
    .atten = ADC_ATTEN_DB_0,
    .bitwidth = ADC_BITWIDTH_12
  };
  adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_3, &config);
}

void gpio_init(void)
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
  //io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
  //io_conf.mode = GPIO_MODE_OUTPUT;
  //gpio_config(&io_conf);
}

unsigned int cc1101_RW(unsigned int device_num, unsigned char *txdata, unsigned char *rxdata, unsigned int size)
{
  esp_err_t err;
  unsigned int rc;

  if (size < 2)
    return 0;

  rc = CC1101_TIMEOUT;
  while (--rc && cc1101_GD2) // waiting for chip ready
    ;
  if (!rc)
    return 0; // timeout

  err = spi_device_acquire_bus(spi_handle, portMAX_DELAY);
  if (err != ESP_OK)
    return 0;

  if (size > 2)
    txdata[0] |= CC1101_BURST;

  spi_transaction_t t = {
      .length = 8 * size,
      .tx_buffer = txdata,
      .rx_buffer = rxdata
  };
  err = spi_device_polling_transmit(spi_handle, &t);

  spi_device_release_bus(spi_handle);

  return err == ESP_OK ? 1 : 0;
}

unsigned int cc1101_strobe(unsigned int device_num, unsigned char data, unsigned char *status)
{
  esp_err_t err;
  unsigned int rc;

  rc = CC1101_TIMEOUT;
  while (--rc && cc1101_GD2) // waiting for chip ready
    ;
  if (!rc)
    return 0; // timeout

  err = spi_device_acquire_bus(spi_handle, portMAX_DELAY);
  if (err != ESP_OK)
    return 0;

  spi_transaction_t t = {
      .length = 8,
      .tx_buffer = &data,
      .rx_buffer = status
  };
  err = spi_device_polling_transmit(spi_handle, &t);

  spi_device_release_bus(spi_handle);

  return err == ESP_OK ? 1 : 0;
}

void mh_z19b_send(unsigned char *data, int len)
{
  uart_write_bytes(1, data, len);
}

int mh_z19b_read(unsigned char *data, int data_size)
{
  return uart_read_bytes(1, data, data_size, 100 / portTICK_PERIOD_MS);
}

unsigned int temt6000_get_mv(void)
{
  int value;
  adc_oneshot_read(adc_handle, ADC_CHANNEL_3, &value);
  return (value * ADC_VREF) >> 12;
}
