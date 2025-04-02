#ifndef COMMON_CRYPTO_H
#define COMMON_CRYPTO_H

#define AES_KEY_LENGTH 16

typedef struct __attribute__((__packed__)) __attribute__ ((aligned (sizeof(unsigned int)))) {
  unsigned int crc;
  unsigned short device_id;
  unsigned int time1;
  unsigned short sensor_data1_1;
  unsigned char sensor_data1_2;
  unsigned short sensor_data2_1;
  unsigned char sensor_data2_2;
  unsigned int time2;
  unsigned short sensor_data3_1;
  unsigned char sensor_data3_2;
  unsigned short sensor_data4_1;
  unsigned char sensor_data4_2;
  unsigned short sensor_data5_1;
  unsigned char sensor_data5_2;
  unsigned short sensor_data6_1;
  unsigned char sensor_data6_2;
  unsigned int time3;
  unsigned short sensor_data7_1;
  unsigned char sensor_data7_2;
  unsigned short sensor_data8_1;
  unsigned char sensor_data8_2;
  unsigned short sensor_data9_1;
  unsigned char sensor_data9_2;
  unsigned short sensor_data10_1;
  unsigned char sensor_data10_2;
} sensor_data;

int encrypt_env(void **data);
void crypto_init(void);
void calculateCRC(sensor_data *data, int length);
void crc_init(const unsigned char* key);

extern const unsigned char default_aes_key[];

#endif
