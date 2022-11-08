#ifndef TEMP_HUMI_PRES_SENSOR_COMMON_H
#define TEMP_HUMI_PRES_SENSOR_COMMON_H

//#define TEST

#define DEVICE_ID 4

#ifdef TEST
#define HOST_IP_ADDR "192.168.178.200"
#define SEND_INTERVAL 5000 // 5 seconds
#else
#define SEND_INTERVAL 300000 // 5 minutes
#define HOST_IP_ADDR ""
#endif

#define PORT ????

#define BLE_ENV

#if DEVICE_ID == 1
#define M5STAMP_C3U
#define SEND_PRESSURE
#define DEVICE_NAME "env_sensor_1"
#elif DEVICE_ID == 2
#define M5STAMP_C3U
#define DEVICE_NAME "env_sensor_2"
#elif DEVICE_ID == 3
#define WEMOS_C3
#define DEVICE_NAME "env_sensor_3"
#elif DEVICE_ID == 4
#define WEMOS_C3
#define DEVICE_NAME "env_sensor_4"
#endif
//#define USE_BME280
//#define SEND_PRESSURE

#ifdef M5STAMP_C3U
#define I2C_MASTER_SCL_IO           0                          /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           1                          /*!< GPIO number used for I2C master data  */

#define SHT30_SENSOR_ADDR           0x44
#define QMP6988_SENSOR_ADDR         0x70

#define LED_GPIO 2
#define LED_STRIP

#endif

#ifdef WEMOS_C3
#define I2C_MASTER_SCL_IO           10                         /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           8                          /*!< GPIO number used for I2C master data  */
#define SHT30_SENSOR_ADDR           0x45
#define LED_GPIO 7
#define WIFI_POWER 3 // db
#endif

#endif //TEMP_HUMI_PRES_SENSOR_COMMON_H
