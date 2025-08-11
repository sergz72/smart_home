#include <cc1101.h>
#include "cc1101_func.h"
#include "board.h"
#include "common.h"
#include "driver/gpio.h"
//#include "esp_log.h"

static unsigned char rxdata[64];
static unsigned char txdata[64];

static const cc1101_cfg cfgDevice = {
    CC1101_RX_ATTENUATION_0DB,
    5,
    CC1101_PQT4,
    0,
    0,
    CC1101_NO_ADDRESS_CHECK,
    0,
    0,
    CC1101_PACKET_LENGTH_FIXED,
    0,
    0,
    0,
    868300,
    0,
    CC1101_MODE_LA_CROSSE,
    0,
    CC1101_SYNC_MODE_1616,
    0,
    CC1101_PREAMBLE_2,
    0,
    0,
    CC1101_RX_TIME_7,
    CC1101_CCA_MODE_RSSI_PACKET,
    CC1101_RXOFF_MODE_IDLE,
    CC1101_TXOFF_MODE_IDLE,
    CC1101_AUTOCAL_FROM_IDLE,
    CC1101_PO_TIMEOUT_40US,
    0,
    0,
    CC1101_TX_POWER_M30_868
};

unsigned int cc1101Init(void)
{
  unsigned char status;

  if (!cc1101_strobe(0, CC1101_STROBE_SRES, &status))
    return 0;

  if (!cc1101_Check(0))
    return 0;

  if (!cc1101_Init(0, &cfgDevice))
    return 0;

  return cc1101_strobe(0, CC1101_STROBE_SIDLE, &status);
}

unsigned int cc1101ReceiveStart(void)
{
  unsigned char status;
  return cc1101_strobe(0, CC1101_STROBE_SRX, &status);
}

unsigned int cc1101ReceiveStop(void)
{
  unsigned char status;
  return cc1101_strobe(0, CC1101_STROBE_SIDLE, &status);
}

unsigned char * cc1101Receive(void)
{
  unsigned char status;

//  ESP_LOGI("EEE", "cc1101Receive");

  if (!cc1101_GD0)
    return NULL;

//  ESP_LOGI("EEE", "GDO0 HIGH!!!");

  txdata[0] = CC1101_RXBYTES;
  if (!cc1101_RW(0, txdata, rxdata, 2) || rxdata[1] != 5)
  {
//    ESP_LOGI("EEE", "Wrong rxdata size: %d", rxdata[1]);
    cc1101_strobe(0, CC1101_STROBE_SFRX, &status);
    cc1101_strobe(0, CC1101_STROBE_SIDLE, &status);
    return NULL;
  }

//  ESP_LOGI("EEE", "RX data size OK");

  txdata[0] = CC1101_FIFO | CC1101_READ;
  if (!cc1101_RW(0, txdata, rxdata, 6))
  {
//    ESP_LOGI("EEE", "RX data read error");
    cc1101_strobe(0, CC1101_STROBE_SIDLE, &status);
    return NULL;
  }

//  ESP_LOGI("EEE", "RX data read OK");

  cc1101_strobe(0, CC1101_STROBE_SIDLE, &status);

//  ESP_LOGI("EEE", "STROBE_IDLE");

  return &rxdata[1];
}
