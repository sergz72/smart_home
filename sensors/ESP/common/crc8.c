#include <crc8.h>

static const unsigned char g_polynom = 0x31;

unsigned char crc8 (const unsigned char data[], int len)
{
  // initialization value
  unsigned char crc = 0xff;
  int i, j;

  // iterate over all bytes
  for (i = 0; i < len; i++)
  {
    crc ^= data[i];

    for (j = 0; j < 8; j++)
    {
      unsigned char xor = crc & 0x80;
      crc = crc << 1;
      crc = xor ? crc ^ g_polynom : crc;
    }
  }

  return crc;
}
