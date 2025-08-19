#ifndef TEMP_HUMI_PRES_SENSOR_COMMON_H
#define TEMP_HUMI_PRES_SENSOR_COMMON_H

//#define TEST
//#define NO_ENV

#define DEVICE_ID 1

#ifdef TEST
#define CONFIG_SERVER_ADDRESS_SUFFIX 200
#define HOST_IP_ADDR "192.168.178.200"
#define SEND_INTERVAL 5000 // 5 seconds
#else
#define CONFIG_SERVER_ADDRESS_SUFFIX 254
#define SEND_INTERVAL 270000 // 4.5 minutes
#define HOST_IP_ADDR "152.67.67.107"
#endif

#define PORT 60002

#define BLE_ENV

#define DECODER_BUFFER_SIZE 100

#if DEVICE_ID == 1
#define WEMOS_S3
#define DEVICE_NAME "env_sensor_1"
#define SENSOR_ID 185
#define PACKET_TYPE 5
#define BSENSOR_ID 255
#endif

#ifdef WEMOS_S3
#define BUTTON_GPIO 0
#define I2C_MASTER_SCL_IO           14                         /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           21                         /*!< GPIO number used for I2C master data  */
#define I2C_MASTER2_SCL_IO          5                          /*!< GPIO number used for I2C master clock */
#define I2C_MASTER2_SDA_IO          4                          /*!< GPIO number used for I2C master data  */
//#define PIN_NUM_MISO 13
//#define PIN_NUM_MOSI 11
//#define PIN_NUM_CLK  12
//#define PIN_NUM_CS   10
//#define PIN_NUM_GDO0 15
//#define PIN_NUM_GDO2 16
#define LED_GPIO 38
#define LED_STRIP
#define GPIO_RECEIVER_DATA_IO 2
//#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_RECEIVER_DATA_IO) | (1ULL<<PIN_NUM_GDO0) | (1ULL<<PIN_NUM_GDO2))
#define GPIO_INPUT_PIN_SEL  (1ULL<<GPIO_RECEIVER_DATA_IO)
//#define GPIO_OUTPUT_PIN_SEL (1ULL<<PIN_NUM_CS)
//#define WIFI_POWER 3 // db
#endif

#endif //TEMP_HUMI_PRES_SENSOR_COMMON_H
