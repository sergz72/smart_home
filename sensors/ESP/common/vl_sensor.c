#include "vl_sensor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "common.h"
#include "esp_log.h"
#include "driver/gpio.h"
#ifdef ST_IMPL
#include "vl53l1x_api.h"
#else
#include "VL53L1X.h"
#endif

static const char *TAG = "vl";

#ifdef ST_IMPL
static unsigned short VLGetDistance(void)
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

unsigned short VLMeasureDistance(void)
{
  unsigned short Distance;
  uint8_t dataReady;

  VL53L1X_StartRanging(0);   /* This function has to be called to enable the ranging */
  dataReady = 0;
  while (!dataReady)
  {
    VL53L1X_CheckForDataReady(0, &dataReady);
    vTaskDelay(2 / portTICK_PERIOD_MS);
  }
  Distance = VLGetDistance();
  VL53L1X_ClearInterrupt(0);
  VL53L1X_StopRanging(0);
  return Distance;
}

int VLInit(void)
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
  return 0;
}
#else
unsigned short VLMeasureDistance(void)
{
  RangingData d;
  int rc = VL53L1X_readSingle(&d);
  ESP_LOGI(TAG, "VL53L1: rc: %d, RangeStatus: %s, Distance: %u, AmbientCountRate: %f, PeakSignalCountRate: %f", rc,
           VL53L1X_rangeStatusToString(d.range_status), d.range_mm, d.ambient_count_rate_MCPS, d.peak_signal_count_rate_MCPS);
  return rc != 0 ? 0 : d.range_mm / 10;
}

int VLInit(void)
{
#ifdef PIN_VL53L1_XSCHUT
  gpio_set_level(PIN_VL53L1_XSCHUT, 1);
  vTaskDelay(20 / portTICK_PERIOD_MS);
  return VL53L1X_init(1, Long, 500000, 5000);
#else
  return 1;
#endif
}

#endif
