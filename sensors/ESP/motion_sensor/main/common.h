#ifndef TEMP_HUMI_PRES_SENSOR_COMMON_H
#define TEMP_HUMI_PRES_SENSOR_COMMON_H

//#define TEST

#define DEVICE_ID 2

#ifdef TEST
#define HOST_IP_ADDR "192.168.178.200"
#define SEND_INTERVAL 5000 // 5 seconds
#else
#define SEND_INTERVAL 300000 // 5 minutes
#define HOST_IP_ADDR ""
#endif

#define NOTIFICATION_HOST_IP_ADDR "192.168.178.202"
#define NOTIFICATION_SEND_INTERVAL 20 // 2 seconds

#define PORT 0
#define NOTIFICATION_PORT 0

#define MOTION_DELAY 10

#define ST_IMPL

#define BLE_ENV

#if DEVICE_ID == 2
#define M5STAMP_C3U
#define DEVICE_NAME "env_sensor_2"
#endif

#ifdef M5STAMP_C3U
#define I2C_MASTER_SCL_IO           10                         /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           8                          /*!< GPIO number used for I2C master data  */

#define SHT30_SENSOR_ADDR           0x44
#define VL53L1_SENSOR_ADDR          0x29

#define LED_GPIO 2
#define LED_STRIP

#define BUTTON_GPIO 9
#define TRIG_GPIO 7
#define ECHO_GPIO 6
#define VL_GPIO 21
#define VL_XSCHUT 20
#define MOTION_GPIO 0

#endif

#ifdef WEMOS_C3
#define I2C_MASTER_SCL_IO           10                         /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           8                          /*!< GPIO number used for I2C master data  */
#define SHT30_SENSOR_ADDR           0x45
#define LED_GPIO 7
#define WIFI_POWER 3 // db

#define BUTTON_GPIO 9

#endif

#endif //TEMP_HUMI_PRES_SENSOR_COMMON_H
