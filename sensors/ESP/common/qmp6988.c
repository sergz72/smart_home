#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "qmp6988.h"
#include "env.h"

#if defined(SEND_PRESSURE) && defined(QMP6988_SENSOR_ADDR)

#define QMP6988_CHIP_ID 0x5C

#define QMP6988_CHIP_ID_REG     0xD1
#define QMP6988_RESET_REG       0xE0 /* Device reset register */
#define QMP6988_CTRLMEAS_REG    0xF4 /* Measurement Condition Control Register */
#define QMP6988_CONFIG_REG      0xF1 /*IIR filter co-efficient setting Register*/
/* data */
#define QMP6988_PRESSURE_MSB_REG    0xF7 /* Pressure MSB Register */
#define QMP6988_TEMPERATURE_MSB_REG 0xFA /* Temperature MSB Reg */

#define QMP6988_SLAVE_ADDRESS   0x70

#define QMP6988_CALIBRATION_DATA_START  0xA0 /* QMP6988 compensation coefficients */
#define QMP6988_CALIBRATION_DATA_LENGTH 25

#define QMP6988_NORMAL_MODE          0x03
#define QMP6988_FILTERCOEFF_4        0x02
#define QMP6988_OVERSAMPLING_1X      0x01
#define QMP6988_OVERSAMPLING_2X      0x02
#define QMP6988_OVERSAMPLING_4X      0x03
#define QMP6988_OVERSAMPLING_8X      0x04

#define SUBTRACTOR 8388608

typedef struct
{
  double b00, bt1, bt2, bp1, b11, bp2, b12, b21, bp3, a0, a1, a2;
} qmp6988_cali_data_t;

static qmp6988_cali_data_t qmp6988_cali;

static const char *TAG = "qmp6988";

static esp_err_t writeReg(uint8_t reg_add, uint8_t reg_dat)
{
  uint8_t data[2] = {reg_add, reg_dat};
  return qmp6988_register_write(data, 2);
}

static esp_err_t readData(uint8_t reg_add, unsigned char *Read, uint8_t num)
{
  return qmp6988_register_read(&reg_add, 1, Read, num);
}

static esp_err_t device_check(void)
{
  unsigned char chip_id;
  esp_err_t rc = readData(QMP6988_CHIP_ID_REG, &chip_id, 1);
  if (rc == ESP_OK)
    return rc;
  ESP_LOGI(TAG, "qmp6988 read chip id = 0x%x", chip_id);
  if (chip_id == QMP6988_CHIP_ID)
  {
    return ESP_OK;
  }
  return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t softwareReset(void)
{
  esp_err_t ret;

  ret = writeReg(QMP6988_RESET_REG, 0xe6);
  if (ret != ESP_OK)
    ESP_LOGE(TAG, "qmp6988 softwareReset fail!!!");
  vTaskDelay(20 / portTICK_PERIOD_MS);
  ret = writeReg(QMP6988_RESET_REG, 0x00);
  if (ret != ESP_OK)
    ESP_LOGE(TAG, "qmp6988 softwareReset fail2!!!");
  return ret;
}

static esp_err_t getCalibrationData(void)
{
  esp_err_t status;
  uint8_t a_data_uint8_tr[QMP6988_CALIBRATION_DATA_LENGTH];
  uint8_t raddr, *addr;
  int len;

  raddr = QMP6988_CALIBRATION_DATA_START;
  addr = a_data_uint8_tr;
  for (len = 0; len < QMP6988_CALIBRATION_DATA_LENGTH; len++)
  {
    status = readData(raddr, addr++, 1);
    if (status != ESP_OK)
    {
      ESP_LOGE(TAG, "qmp6988 read 0x%x error!", raddr);
      return status;
    }
    raddr++;
  }

  qmp6988_cali.a0 = (((int)((a_data_uint8_tr[18] << 24) | (a_data_uint8_tr[19] << 16) |
      ((a_data_uint8_tr[24] & 0x0f) << 8))) >> 12) / 16.0;
  qmp6988_cali.a1 = -6.30E-03 + 4.30E-04 * (short)(((a_data_uint8_tr[20]) << 8) | a_data_uint8_tr[21]) / 32767.0;
  qmp6988_cali.a2 = -1.90E-11 + 1.20E-10 * (short)(((a_data_uint8_tr[22]) << 8) | a_data_uint8_tr[23]) / 32767.0;

  qmp6988_cali.b00 = (((int)((a_data_uint8_tr[0] << 24) | (a_data_uint8_tr[1] << 16) |
      ((a_data_uint8_tr[24] & 0xf0) << 4))) >> 12) / 16.0;

  qmp6988_cali.bt1 = 1.00E-01 + 9.10E-02 * (short)(((a_data_uint8_tr[2]) << 8) | a_data_uint8_tr[3]) / 32767.0;
  qmp6988_cali.bt2 = 1.20E-08 + 1.20E-06 * (short)(((a_data_uint8_tr[4]) << 8) | a_data_uint8_tr[5]) / 32767.0;
  qmp6988_cali.bp1 = 3.30E-02 + 1.90E-02 * (short)(((a_data_uint8_tr[6]) << 8) | a_data_uint8_tr[7]) / 32767.0;
  qmp6988_cali.b11 = 2.10E-07 + 1.40E-07 * (short)(((a_data_uint8_tr[8]) << 8) | a_data_uint8_tr[9]) / 32767.0;
  qmp6988_cali.bp2 = -6.30E-10 + 3.50E-10 * (short)(((a_data_uint8_tr[10]) << 8) | a_data_uint8_tr[11]) / 32767.0;
  qmp6988_cali.b12 = 2.90E-13 + 7.60E-13 * (short)(((a_data_uint8_tr[12]) << 8) | a_data_uint8_tr[13]) / 32767.0;
  qmp6988_cali.b21 = 2.10E-15 + 1.20E-14 * (short)(((a_data_uint8_tr[14]) << 8) | a_data_uint8_tr[15]) / 32767.0;
  qmp6988_cali.bp3 = 1.30E-16 + 7.90E-17 * (short)(((a_data_uint8_tr[16]) << 8) | a_data_uint8_tr[17]) / 32767.0;

  ESP_LOGI(TAG, "a0[%f]	a1[%f]	a2[%f]	b00[%f]",
           qmp6988_cali.a0, qmp6988_cali.a1,
           qmp6988_cali.a2, qmp6988_cali.b00);
  ESP_LOGI(TAG, "bt1[%f]	bt2[%f]	bp1[%f]	b11[%f]",
           qmp6988_cali.bt1, qmp6988_cali.bt2,
           qmp6988_cali.bp1, qmp6988_cali.b11);
  ESP_LOGI(TAG, "bp2[%f]	b12[%f]	b21[%f]	bp3[%f]",
           qmp6988_cali.bp2, qmp6988_cali.b12,
           qmp6988_cali.b21, qmp6988_cali.bp3);

  return ESP_OK;
}

static esp_err_t setPowermode(uint8_t power_mode)
{
  uint8_t data;
  esp_err_t rc;

  rc = readData(QMP6988_CTRLMEAS_REG, &data, 1);
  if (rc != ESP_OK)
  {
    ESP_LOGE(TAG, "qmp_set_powermode readData error");
    return rc;
  }
  data = (data & 0xfc) | power_mode;
  rc = writeReg(QMP6988_CTRLMEAS_REG, data);
  if (rc != ESP_OK)
  {
    ESP_LOGE(TAG, "qmp_set_powermode writeReg error");
    return rc;
  }

  ESP_LOGI(TAG, "qmp_set_powermode 0xf4=0x%x", data);

  vTaskDelay(20 / portTICK_PERIOD_MS);

  return ESP_OK;
}

static esp_err_t setFilter(unsigned char filter)
{
  uint8_t data;
  esp_err_t rc;

  data = (filter & 0x03);
  rc = writeReg(QMP6988_CONFIG_REG, data);
  if (rc != ESP_OK)
    ESP_LOGE(TAG, "setFilter writeReg error");

  vTaskDelay(20 / portTICK_PERIOD_MS);

  return rc;
}

static esp_err_t setOversamplingP(unsigned char oversampling_p)
{
  uint8_t data;
  esp_err_t rc;

  rc = readData(QMP6988_CTRLMEAS_REG, &data, 1);
  if (rc != ESP_OK)
  {
    ESP_LOGE(TAG, "setOversamplingP readData error");
    return rc;
  }
  data &= 0xe3;
  data |= (oversampling_p << 2);
  rc = writeReg(QMP6988_CTRLMEAS_REG, data);
  if (rc != ESP_OK)
  {
    ESP_LOGE(TAG, "setOversamplingP writeReg error");
    return rc;
  }
  vTaskDelay(20 / portTICK_PERIOD_MS);

  return ESP_OK;
}

static esp_err_t setOversamplingT(unsigned char oversampling_t)
{
  uint8_t data;
  esp_err_t rc;

  rc = readData(QMP6988_CTRLMEAS_REG, &data, 1);
  if (rc != ESP_OK)
  {
    ESP_LOGE(TAG, "setOversamplingT readData error");
    return rc;
  }
  data &= 0x1f;
  data |= (oversampling_t << 5);
  rc = writeReg(QMP6988_CTRLMEAS_REG, data);
  if (rc != ESP_OK)
  {
    ESP_LOGE(TAG, "setOversamplingT writeReg error");
    return rc;
  }
  vTaskDelay(20 / portTICK_PERIOD_MS);
  return ESP_OK;
}

static double convT(int T_raw)
{
  return qmp6988_cali.a0 + qmp6988_cali.a1 * T_raw + qmp6988_cali.a2 * T_raw * T_raw;
}

static double convP(int P_raw, double t)
{
  return qmp6988_cali.b00 + qmp6988_cali.bt1 * t + qmp6988_cali.bp1 * P_raw + qmp6988_cali.b11 * t * P_raw +
         qmp6988_cali.bt2 * t * t + qmp6988_cali.bp2 * P_raw * P_raw + qmp6988_cali.b12 * P_raw * t * t +
         qmp6988_cali.b21 * P_raw * P_raw * t + qmp6988_cali.bp3 * P_raw * P_raw * P_raw;
}

esp_err_t qmp6988_calcPressure(uint32_t *pressure, uint16_t *temperature)
{
  esp_err_t err;
  uint8_t a_data_uint8_tr[6] = {0};
  int P_raw, T_raw;
  double p, t;

  err = readData(QMP6988_PRESSURE_MSB_REG, a_data_uint8_tr, 6);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "qmp6988 read press raw error!");
    return err;
  }
  P_raw = ((a_data_uint8_tr[0] << 16) | (a_data_uint8_tr[1] << 8) | a_data_uint8_tr[2]) - SUBTRACTOR;
  T_raw = ((a_data_uint8_tr[3] << 16) | (a_data_uint8_tr[4] << 8) | a_data_uint8_tr[5]) - SUBTRACTOR;

  t = convT(T_raw);
  p = convP(P_raw, t);
  if (temperature != NULL)
    *temperature = (int)(t * 100 / 256);
  if (pressure != NULL)
    *pressure = (int)p / 10;

  return ESP_OK;
}

esp_err_t qmp6988_init_sensor(void)
{
  esp_err_t rc;

  rc = device_check();
  if (rc != ESP_OK)
  {
    ESP_LOGE(TAG, "qmp6988 device check failure %d", rc);
    return rc;
  }

  rc = softwareReset();
  if (rc != ESP_OK)
    return rc;
  rc = getCalibrationData();
  if (rc != ESP_OK)
    return rc;
  rc = setPowermode(QMP6988_NORMAL_MODE);
  if (rc != ESP_OK)
    return rc;
  rc = setFilter(QMP6988_FILTERCOEFF_4);
  if (rc != ESP_OK)
    return rc;
  rc = setOversamplingP(QMP6988_OVERSAMPLING_8X);
  if (rc != ESP_OK)
    return rc;
  return setOversamplingT(QMP6988_OVERSAMPLING_1X);
}

#endif
