#include <time.h>
#include "crypto.h"
#include "crypto_esp.h"
#include "env.h"
#include "common.h"

#define ENCRYPTED_LENGTH 32

unsigned char encrypted[ENCRYPTED_LENGTH];

static void encrypt(unsigned char *data)
{
  esp_aes_crypt_ecb(&aes_ctx, ESP_AES_ENCRYPT, data, encrypted);
  esp_aes_crypt_ecb(&aes_ctx, ESP_AES_ENCRYPT, data + 16, encrypted + 16);
}

int encrypt_env(void **data)
{
  time_t now;
  sensor_data sdata;

  time(&now);

  sdata.device_id = DEVICE_ID;
  sdata.time1 = (unsigned int)now;
  sdata.sensor_data1_1 = temp_val;
  sdata.sensor_data1_2 = 0;
  sdata.sensor_data2_1 = humi_val;
  sdata.sensor_data2_2 = 0;
  sdata.time2          = (unsigned int)now;
  sdata.sensor_data3_1 = (unsigned short)co2_level;
  sdata.sensor_data3_2 = (unsigned char)(co2_level >> 16);
  sdata.sensor_data4_1 = (unsigned short)luminocity;
  sdata.sensor_data4_2 = (unsigned char)(luminocity >> 16);
  sdata.sensor_data5_1 = ext_temp_val;
  sdata.sensor_data5_2 = (ext_temp_val & 0x8000) ? 0xFF : 0;
  sdata.sensor_data6_1 = 0;
  sdata.sensor_data6_2 = 0;
  calculateCRC(&sdata, ENCRYPTED_LENGTH);
  encrypt((unsigned char*)&sdata);
  *data = encrypted;
  return ENCRYPTED_LENGTH;
}
