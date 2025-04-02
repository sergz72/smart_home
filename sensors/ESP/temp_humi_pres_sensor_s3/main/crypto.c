#include <time.h>
#include "crypto.h"
#include "crypto_esp.h"
#include "env.h"
#include "common.h"

#define ENCRYPTED_LENGTH 48

unsigned char encrypted[ENCRYPTED_LENGTH];

static void encrypt(unsigned char *data)
{
  esp_aes_crypt_ecb(&aes_ctx, ESP_AES_ENCRYPT, data, encrypted);
  esp_aes_crypt_ecb(&aes_ctx, ESP_AES_ENCRYPT, data + 16, encrypted + 16);
  esp_aes_crypt_ecb(&aes_ctx, ESP_AES_ENCRYPT, data + 32, encrypted + 32);
}

int encrypt_env(void **data)
{
  time_t now;
  sensor_data sdata;
  uint32_t pval = pres_val * 10;

  time(&now);

  sdata.device_id = DEVICE_ID;
  sdata.time1 = (unsigned int)now;
  sdata.sensor_data1_1 = temp_val;
  sdata.sensor_data1_2 = 0;
  sdata.sensor_data2_1 = humi_val;
  sdata.sensor_data2_2 = 0;
  sdata.time2          = (unsigned int)now;
  sdata.sensor_data3_1 = (unsigned short)pval;
  sdata.sensor_data3_2 = (unsigned char)(pval >> 16);
  sdata.sensor_data4_1 = ext_temp_val;
  sdata.sensor_data4_2 = (ext_temp_val & 0x8000) ? 0xFF : 0;
  sdata.sensor_data5_1 = ext_humi_val;
  sdata.sensor_data5_2 = 0;
  sdata.sensor_data6_1 = ext_temp_val2;
  sdata.sensor_data6_2 = (ext_temp_val2 & 0x8000) ? 0xFF : 0;
  sdata.time3          = (unsigned int)now;
  sdata.sensor_data7_1 = ext_temp_val3;
  sdata.sensor_data7_2 = (ext_temp_val3 & 0x8000) ? 0xFF : 0;
  sdata.sensor_data8_1 = ext_humi_val3;
  sdata.sensor_data8_2 = 0;
  sdata.sensor_data9_1 = (unsigned short)co2_level;
  sdata.sensor_data9_2 = (unsigned char)(co2_level >> 16);
  sdata.sensor_data10_1 = (unsigned short)luminocity;
  sdata.sensor_data10_2 = (unsigned char)(luminocity >> 16);
  calculateCRC(&sdata);
  encrypt((unsigned char*)&sdata);
  *data = encrypted;
  return ENCRYPTED_LENGTH;
}
