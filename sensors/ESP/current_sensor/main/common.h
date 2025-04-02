#ifndef CURRENT_SENSOR_COMMON_H
#define CURRENT_SENSOR_COMMON_H

#define TEST

#define DEVICE_ID 5

#ifdef TEST
#define CONFIG_SERVER_ADDRESS_SUFFIX 200
#define HOST_IP_ADDR "192.168.178.200"
#define SEND_INTERVAL 5000 // 5 seconds
#else
#define CONFIG_SERVER_ADDRESS_SUFFIX 254
#define SEND_INTERVAL 300000 // 5 minutes
#define HOST_IP_ADDR ""
#endif

#define PORT 0
#define ENCRYPTED_LENGTH 16
#define QUEUE_SIZE 1024

#if DEVICE_ID == 5
#define WEMOS_C3
#define DEVICE_NAME "current_sensor_5"
#define PWR_DIVIDER 311 // 30A sensor
#endif

#define ADS1115_ADDR 0x48

#ifdef M5STAMP_C3U
#define I2C_MASTER_SCL_IO           0                          /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           1                          /*!< GPIO number used for I2C master data  */
#define LED_GPIO 2
#define LED_STRIP

#define BUTTON_GPIO 9

#endif

#ifdef WEMOS_C3
#define I2C_MASTER_SCL_IO           0                          /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           1                          /*!< GPIO number used for I2C master data  */
#define LED_GPIO 7
#define WIFI_POWER 3 // db

#define BUTTON_GPIO 9

#endif

#endif //CURRENT_SENSOR_COMMON_H
