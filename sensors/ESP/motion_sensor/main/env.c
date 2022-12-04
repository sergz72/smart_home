#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "stdint.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "led.h"
#include "env.h"
#include "sht3x.h"
#include "esp_timer.h"
#if defined(SEND_PRESSURE) && defined(QMP6988_SENSOR_ADDR)
#include "qmp6988.h"
#endif
#ifdef USE_BME280
#include "bme280.h"
#endif
#include "vl53l1x_api.h"
#include "lwip/sockets.h"

#define I2C_MASTER_NUM              0                          /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
#define I2C_MASTER_FREQ_HZ          100000                     /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       1000
#define ESP_INTR_FLAG_DEFAULT       0

#define SOUND_SPEED 343 // meter/second

static const char *TAG = "env";
static QueueHandle_t gpio_evt_queue;
static SemaphoreHandle_t env_semaphore;

int16_t temp_val = 2510;
uint16_t humi_val = 4020;
#ifdef SEND_PRESSURE
uint32_t pres_val = 10003;
#endif

typedef struct {
  int64_t time;
  int gpio;
  int level;
} queue_item;

typedef struct {
  uint16_t hc_distance, vl_distance;
} notification;

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

static struct sockaddr_in dest_addr;

static void net_client_init(void)
{
  dest_addr.sin_addr.s_addr = inet_addr(NOTIFICATION_HOST_IP_ADDR);
  dest_addr.sin_family = AF_INET;
  dest_addr.sin_port = htons(NOTIFICATION_PORT);
}

static void send_data(int hcDistance, uint16_t vl_distance)
{
  notification n;

  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
  if (sock < 0) {
    ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
    return;
  }
  n.hc_distance = (uint16_t)hcDistance;
  n.vl_distance = vl_distance;
  ESP_LOGI(TAG, "Socket created, sending to %s:%d", NOTIFICATION_HOST_IP_ADDR, NOTIFICATION_PORT);
  int err = sendto(sock, &n, sizeof(n), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
  if (err < 0)
    ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
  else
    ESP_LOGI(TAG, "Notification sent");
  shutdown(sock, 0);
  close(sock);
}

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
  queue_item qi;
  qi.time = esp_timer_get_time();
  qi.gpio = (int)arg;
  qi.level = gpio_get_level(qi.gpio);
  xQueueSendFromISR(gpio_evt_queue, &qi, NULL);
}

static int calculateDistance(int delay)
{
  return delay * SOUND_SPEED / 20000;
}

static uint16_t VLMesaureDistance(void)
{
  uint16_t Distance;
  uint16_t SignalRate;
  uint16_t AmbientRate;
  uint16_t SpadNum;
  uint8_t RangeStatus;

  VL53L1X_GetRangeStatus(0, &RangeStatus);
  VL53L1X_GetDistance(0, &Distance);
  VL53L1X_GetSignalRate(0, &SignalRate);
  VL53L1X_GetAmbientRate(0, &AmbientRate);
  VL53L1X_GetSpadNb(0, &SpadNum);
  ESP_LOGI(TAG, "VL53L1: RangeStatus: %u, Distance: %u, SignalRate: %u, AmbientRate: %u, SpadNum: %u", RangeStatus,
           Distance, SignalRate, AmbientRate, SpadNum);
  return (RangeStatus == 0) && (SignalRate >= 6000 || Distance > 1000) ? Distance / 10 : 0 ;
}

static void gpio_task(void* arg)
{
  queue_item qi;
  int64_t start_time, send_data_until;
  int counter, delay, distance;
  uint8_t dataReady;
  uint16_t Distance = 0;

  start_time = send_data_until = 0;
  counter = delay = distance = 0;

  for(;;)
  {
    while (xQueueReceive(gpio_evt_queue, &qi, 0))
    {
      switch (qi.gpio)
      {
        case MOTION_GPIO:
          if (qi.level)
          {
            set_led_green();
            send_data_until = esp_timer_get_time() + MOTION_DELAY * 1000000;
            counter = NOTIFICATION_SEND_INTERVAL - 1;
          }
          else
          {
            led_off();
          }
          break;
        case ECHO_GPIO:
          if (qi.level)
            start_time = qi.time;
          else {
            delay = (int)(qi.time - start_time);
            distance = calculateDistance(delay);
            ESP_LOGI(TAG, "HC-SR04: delay is %d us, distance is %d cm", delay, distance);
          }
          break;
      }
    }
    // wait 100 ms
    vTaskDelay(100 / portTICK_PERIOD_MS);
    if (esp_timer_get_time() < send_data_until)
    {
      counter++;
      if (counter == 3)
      {
        if (delay == -1)
          ESP_LOGE(TAG, "HC-SR04 timeout");
        send_data(distance, Distance);
      }
      if (counter == NOTIFICATION_SEND_INTERVAL) // 2 seconds
      {
        // HC-SR04
        gpio_set_level(TRIG_GPIO, 1);
        esp_rom_delay_us(10);
        gpio_set_level(TRIG_GPIO, 0);
        counter = 0;
        delay = -1;
        distance = 0;
        ESP_LOGI(TAG, "HC-SR04, VL53L1: start");
        // VL53L1
        xSemaphoreTake(env_semaphore, portMAX_DELAY);
        VL53L1X_StartRanging(0);   /* This function has to be called to enable the ranging */
        dataReady = 0;
        while (!dataReady)
        {
          VL53L1X_CheckForDataReady(0, &dataReady);
          vTaskDelay(2 / portTICK_PERIOD_MS);
        }
        Distance = VLMesaureDistance();
        VL53L1X_ClearInterrupt(0);
        VL53L1X_StopRanging(0);
        xSemaphoreGive(env_semaphore);
      }
    }
  }
}

static void gpio_init(void)
{
  gpio_config_t io_conf;
  //interrupt of rising edge
  io_conf.intr_type = GPIO_INTR_ANYEDGE;
  //set as input mode
  io_conf.mode = GPIO_MODE_INPUT;
  //bit mask of the pins
  io_conf.pin_bit_mask = (1ULL<<MOTION_GPIO) | (1ULL<<ECHO_GPIO);
  //enable pull-down mode
  io_conf.pull_down_en = 1;
  //disable pull-up mode
  io_conf.pull_up_en = 0;
  //configure GPIO with the given settings
  gpio_config(&io_conf);

  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pull_down_en = 0;
  io_conf.pull_up_en = 0;
  io_conf.pin_bit_mask = (1ULL<<TRIG_GPIO) | (1ULL<<VL_XSCHUT);
  gpio_config(&io_conf);
}

void post_init_env(void)
{
  //create a queue to handle gpio event from isr
  gpio_evt_queue = xQueueCreate(10, sizeof(queue_item));
  //start gpio task
  xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 10, NULL);

  //install gpio isr service
  gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
  //hook isr handler for specific gpio pin
  gpio_isr_handler_add(MOTION_GPIO, gpio_isr_handler, (void*)MOTION_GPIO);
  gpio_isr_handler_add(ECHO_GPIO, gpio_isr_handler, (void*)ECHO_GPIO);
}

void boot_vl_sensor(void)
{
  uint8_t byteData, sensorState=0;
  uint16_t wordData;

  gpio_set_level(VL_XSCHUT, 1);
  vTaskDelay(2 / portTICK_PERIOD_MS);

  ESP_LOGI(TAG, "VL53L1 chip init");
  VL53L1_RdByte(0, 0x010F, &byteData);
  ESP_LOGI(TAG, "VL53L1X Model_ID: %X", byteData);
  VL53L1_RdByte(0, 0x0110, &byteData);
  ESP_LOGI(TAG, "VL53L1X Module_Type: %X", byteData);
  VL53L1_RdWord(0, 0x010F, &wordData);
  ESP_LOGI(TAG, "VL53L1X: %X", wordData);
  while(!sensorState)
  {
    VL53L1X_BootState(0, &sensorState);
    vTaskDelay(2 / portTICK_PERIOD_MS);
  }
  ESP_LOGI(TAG, "VL53L1 chip booted");
  VL53L1X_SensorInit(0);
  /* Optional functions to be used to change the main ranging parameters according the application requirements to get the best ranging performances */
  VL53L1X_SetDistanceMode(0, 2); /* 1=short, 2=long */
  VL53L1X_SetTimingBudgetInMs(0, 500); /* in ms possible values [20, 50, 100, 200, 500] */
  VL53L1X_SetInterMeasurementInMs(0, 5000); /* in ms, IM must be > = TB */
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
#endif
  gpio_init();
  boot_vl_sensor();
  net_client_init();

  env_semaphore = xSemaphoreCreateBinary();
  if (!env_semaphore) {
    ESP_LOGE(TAG, "semaphore create fail.");
    set_led_red();
    while (1);
  }
  xSemaphoreGive(env_semaphore);
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
  xSemaphoreTake(env_semaphore, portMAX_DELAY);
  esp_err_t rc = sht3x_measure(&temp_val, &humi_val);
  xSemaphoreGive(env_semaphore);
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

/*int get_env(void)
{
  return 1;
}*/
