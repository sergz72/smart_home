
/* 
* This file is part of VL53L1 Platform 
* 
* Copyright (c) 2016, STMicroelectronics - All Rights Reserved 
* 
* License terms: BSD 3-clause "New" or "Revised" License. 
* 
* Redistribution and use in source and binary forms, with or without 
* modification, are permitted provided that the following conditions are met: 
* 
* 1. Redistributions of source code must retain the above copyright notice, this 
* list of conditions and the following disclaimer. 
* 
* 2. Redistributions in binary form must reproduce the above copyright notice, 
* this list of conditions and the following disclaimer in the documentation 
* and/or other materials provided with the distribution. 
* 
* 3. Neither the name of the copyright holder nor the names of its contributors 
* may be used to endorse or promote products derived from this software 
* without specific prior written permission. 
* 
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE 
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
* 
*/

#include "vl53l1_platform.h"
#include "driver/i2c.h"
#include "common.h"

#define I2C_MASTER_NUM              0
#define I2C_MASTER_TIMEOUT_MS       1000

//int8_t VL53L1_WriteMulti( uint16_t dev, uint16_t index, uint8_t *pdata, uint32_t count) {
//	return 0; // to be implemented
//}

int8_t VL53L1_ReadMulti(uint16_t dev, uint16_t index, uint8_t *pdata, uint32_t count){
  unsigned char data[2];
  data[0] = index >> 8;
  data[1] = index & 0xFF;
  return (int8_t)i2c_master_write_read_device(I2C_MASTER_NUM, VL53L1_SENSOR_ADDR, data,
                                      2, pdata, count, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

int8_t VL53L1_WrByte(uint16_t dev, uint16_t index, uint8_t wdata) {
  unsigned char data[3];
  data[0] = index >> 8;
  data[1] = index & 0xFF;
  data[2] = wdata;
  return (int8_t)i2c_master_write_to_device(I2C_MASTER_NUM, VL53L1_SENSOR_ADDR, data, 3,
                                    I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

int8_t VL53L1_WrWord(uint16_t dev, uint16_t index, uint16_t wdata) {
  unsigned char data[4];
  data[0] = index >> 8;
  data[1] = index & 0xFF;
  data[2] = wdata >> 8;
  data[3] = wdata & 0xFF;
  return (int8_t)i2c_master_write_to_device(I2C_MASTER_NUM, VL53L1_SENSOR_ADDR, data, 4,
                                            I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

int8_t VL53L1_WrDWord(uint16_t dev, uint16_t index, uint32_t wdata) {
  unsigned char data[6];
  data[0] = index >> 8;
  data[1] = index & 0xFF;
  data[2] = wdata >> 24;
  data[3] = (wdata >> 16) & 0xFF;
  data[4] = (wdata >> 8) & 0xFF;
  data[5] = wdata & 0xFF;
  return (int8_t)i2c_master_write_to_device(I2C_MASTER_NUM, VL53L1_SENSOR_ADDR, data, 6,
                                            I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

int8_t VL53L1_RdByte(uint16_t dev, uint16_t index, uint8_t *rdata) {
  unsigned char data[2];
  data[0] = index >> 8;
  data[1] = index & 0xFF;
  return (int8_t)i2c_master_write_read_device(I2C_MASTER_NUM, VL53L1_SENSOR_ADDR, data,
                                              2, rdata, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

int8_t VL53L1_RdWord(uint16_t dev, uint16_t index, uint16_t *odata) {
  unsigned char data[2], rdata[2];
  int8_t rc;
  data[0] = index >> 8;
  data[1] = index & 0xFF;
  rc = (int8_t)i2c_master_write_read_device(I2C_MASTER_NUM, VL53L1_SENSOR_ADDR, data,
                                            2, rdata, 2, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
  if (!rc)
    *odata = ((uint16_t)rdata[0] << 8) | rdata[1];
  return rc;
}

int8_t VL53L1_RdDWord(uint16_t dev, uint16_t index, uint32_t *odata) {
  unsigned char data[2], rdata[4];
  int8_t rc;
  data[0] = index >> 8;
  data[1] = index & 0xFF;
  rc = (int8_t)i2c_master_write_read_device(I2C_MASTER_NUM, VL53L1_SENSOR_ADDR, data,
                                            2, rdata, 4, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
  if (!rc)
    *odata = ((uint32_t)rdata[0] << 24) | ((uint32_t)rdata[1] << 16) | ((uint32_t)rdata[2] << 8) | rdata[3];
  return rc;
}

//int8_t VL53L1_WaitMs(uint16_t dev, int32_t wait_ms){
//	return 0; // to be implemented
//}
