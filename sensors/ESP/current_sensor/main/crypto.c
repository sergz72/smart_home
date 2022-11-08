#include <time.h>
#include <aes/esp_aes.h>
#include "crypto.h"
#include "env.h"
#include "common.h"

#define KEY_LENGTH 16
typedef struct __attribute__((__packed__)) __attribute__ ((aligned (sizeof(uint32_t)))) {
  unsigned int crc;
  unsigned short device_id;
  unsigned int time1;
  unsigned short sensor_data1_1;
  unsigned char sensor_data1_2;
  unsigned short sensor_data2_1;
  unsigned char sensor_data2_2;
} sensor_data;

static const unsigned char key[] =
    { };
static unsigned char encrypted[ENCRYPTED_LENGTH];
static esp_aes_context ctx;

void crypto_init(void)
{
  esp_aes_init(&ctx);
  esp_aes_setkey(&ctx, key, 128);
}

static void calculateCRC(sensor_data *data)
{
  unsigned int crc = 0;
  unsigned int k = 0, *pk = (unsigned int*)key;
  int i = KEY_LENGTH;
  unsigned int *idata = (unsigned int*)data;

  while (i > 0)
  {
    k += *pk++;
    i -= 4;
  }

  i = sizeof(sensor_data) - 4;
  while (i > 0)
  {
    crc += *++idata;
    i -= 4;
  }

  data->crc = crc * k;
}

static void encrypt(unsigned char *data)
{
  esp_aes_crypt_ecb(&ctx, ESP_AES_ENCRYPT, data, encrypted);
}

int encrypt_env(void **data)
{
  time_t now;
  sensor_data sdata;

  time(&now);

  sdata.device_id = DEVICE_ID;
  sdata.time1 = (unsigned int)now;
  sdata.sensor_data1_1 = current_val;
  sdata.sensor_data1_2 = 0;
  sdata.sensor_data2_1 = 0;
  sdata.sensor_data2_2 = 0;
  calculateCRC(&sdata);
  encrypt((unsigned char*)&sdata);
  *data = encrypted;
  return ENCRYPTED_LENGTH;
}
