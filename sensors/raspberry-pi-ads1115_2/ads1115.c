#include <stdio.h>
#include <sys/types.h> // open
#include <sys/stat.h>  // open
#include <fcntl.h>     // open
#include <unistd.h>    // read/write usleep
#include <stdlib.h>    // exit
#include <inttypes.h>  // uint8_t, etc
#include <linux/i2c-dev.h> // I2C bus definitions
#include <sys/ioctl.h>
#include <string.h>

#define ADDRESS 0x48

unsigned int measure_voltage(int fd, double koef, unsigned char config_byte) {
  unsigned char buffer[3];
  unsigned int val;
  
  ///////////////////////////////
  // set config register and start conversion
  // single-ended AIN2 or 3, 2.048v, 8SPS
  buffer[0] = 1;    // config register is 1
  buffer[1] = config_byte;
  buffer[2] = 0b00000011;
  if (write(fd, buffer, 3) != 3) {
    close(fd);
    perror("Error write to register 1");
    exit(-1);
  }
  //////////////////////////////
  // wait for conversion complete
  do {
        usleep(100000); // sleep 100 msec
	if (read(fd, buffer, 2) != 2) {
          close(fd);
	  perror("Read status error");
	  exit(-1);
	}
  } while ((buffer[0] & 0x80) == 0);
  //////////////////////////////
  // read conversion register
  buffer[0] = 0;   // conversion register is 0
  if (write(fd, buffer, 1) != 1) {
    close(fd);
    perror("Error write register select");
    exit(-1);
  }
  if (read(fd, buffer, 2) != 2) {
    close(fd);
    perror("Read conversion error");
    exit(-1);
  }
  //////////////////////////////
  // convert output and display results
  val = buffer[0];
  val = (val << 8) + buffer[1];

  return (int)(val * koef);
}

int main(int argc, char **argv) {
  int fd;
  unsigned char buffer[3];
  unsigned int val, uv, V;
  double i, r, temp, R;

  if (argc != 5 && argc != 4) {
    printf("Usage: ads1115 device_name [temperature voltage resistance][vcc koef]\n");
    return 1;
  }
  
  // open device on /dev/i2c-1 the default on Raspberry Pi B
  if ((fd = open(argv[1], O_RDWR)) < 0) {
    printf("Error: Couldn't open device! %d\n", fd);
    return 1;
  }

  // connect to ads1115 as i2c slave
  if (ioctl(fd, I2C_SLAVE, ADDRESS) < 0) {
    close(fd);
    printf("Error: Couldn't find device on address!\n");
    return 1;
  }

  if (!strcmp(argv[2], "vcc"))
  {
    i = atof(argv[3]);
    V  = measure_voltage(fd, i, 0b11100101); //AIN2
    uv = measure_voltage(fd, i, 0b11110101); //AIN3
    close(fd);
    printf("{\"vcc\":%d,\"vbat\":%d}", V, uv);
    return 0;
  }

  V = atoi(argv[3]) * 1000;
  R = atoi(argv[4]);
  
  ///////////////////////////////
  // set config register and start conversion
  // differential AIN0-AIN1, 0.256v, 8SPS
  buffer[0] = 1;    // config register is 1
  buffer[1] = 0b10001111;
  buffer[2] = 0b00000011;
  if (write(fd, buffer, 3) != 3) {
    close(fd);
    perror("Error write to register 1");
    exit(-1);
  }
  //////////////////////////////
  // wait for conversion complete
  do {
        usleep(100000); // sleep 100 msec
	if (read(fd, buffer, 2) != 2) {
          close(fd);
	  perror("Read status error");
	  exit(-1);
	}
  } while ((buffer[0] & 0x80) == 0);
  //////////////////////////////
  // read conversion register
  buffer[0] = 0;   // conversion register is 0
  if (write(fd, buffer, 1) != 1) {
    close(fd);
    perror("Error write register select");
    exit(-1);
  }
  if (read(fd, buffer, 2) != 2) {
    close(fd);
    perror("Read conversion error");
    exit(-1);
  }
  //////////////////////////////
  // convert output and display results
  val = buffer[0];
  val = (val << 8) + buffer[1];
  uv = val * 256000 / 32768;
  i = (V - uv) / R; //uA
  r = uv / i; //ohm
  temp = (r - 100) / 0.384;
  printf("{\"temp\":%.1f}", temp);

  close(fd);

  return 0;
}
