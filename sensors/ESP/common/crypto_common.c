#include <crypto.h>
#include <string.h>

const unsigned char default_aes_key[] =
    {};

unsigned int key_sum;

void crc_init(const unsigned char* key)
{
  unsigned int ikey[AES_KEY_LENGTH / 4];

  memcpy(ikey, key, AES_KEY_LENGTH);

  key_sum = 0;

  for (int i = 0; i < AES_KEY_LENGTH / 4; i++)
    key_sum += ikey[i];
}

void calculateCRC(sensor_data *data, int length)
{
  unsigned int crc = 0;
  unsigned int *idata = (unsigned int*)data;

  int i = length - 4;
  while (i > 0)
  {
    crc += *++idata;
    i -= 4;
  }

  data->crc = crc * key_sum;
}
