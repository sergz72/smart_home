#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <crypto.h>
#include <errno.h>
#include <time.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <aes128.h>

#define MAX_SENSORS 10
#define FIRST_SENSOR_VALUE_ARGUMENT 4

static sensor_data sdata;
static struct sockaddr_in dest_addr;
static unsigned char encrypted[sizeof(sensor_data)];

static void usage(void)
{
  puts("Usage: device_emulator [udp|tcp] port_number device_id sensor_values");
}

static void set_sensor_value(int sensor_idx, int value)
{
  switch (sensor_idx)
  {
    case 0:
      sdata.sensor_data1_1 = value;
      sdata.sensor_data1_2 = value >> 16;
      break;
    case 1:
      sdata.sensor_data2_1 = value;
      sdata.sensor_data2_2 = value >> 16;
      break;
    case 2:
      sdata.sensor_data3_1 = value;
      sdata.sensor_data3_2 = value >> 16;
      break;
    case 3:
      sdata.sensor_data4_1 = value;
      sdata.sensor_data4_2 = value >> 16;
      break;
    case 4:
      sdata.sensor_data5_1 = value;
      sdata.sensor_data5_2 = value >> 16;
      break;
    case 5:
      sdata.sensor_data6_1 = value;
      sdata.sensor_data6_2 = value >> 16;
      break;
    case 6:
      sdata.sensor_data7_1 = value;
      sdata.sensor_data7_2 = value >> 16;
      break;
    case 7:
      sdata.sensor_data8_1 = value;
      sdata.sensor_data8_2 = value >> 16;
      break;
    case 8:
      sdata.sensor_data9_1 = value;
      sdata.sensor_data9_2 = value >> 16;
      break;
    case 9:
      sdata.sensor_data10_1 = value;
      sdata.sensor_data10_2 = value >> 16;
      break;
    default:
      break;
  }
}

static void prepare_sensor_data(int length)
{
  time_t t;
  time(&t);
  sdata.time1 = t;
  sdata.time2 = t;
  sdata.time3 = t;
  calculateCRC(&sdata, length);
}

static int send_sensor_data_udp(int length)
{
  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
  if (sock < 0) {
    printf("Unable to create socket: errno %d\n", errno);
    return 1;
  }
  int err = sendto(sock, encrypted, length, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
  if (err < 0)
  {
    shutdown(sock, 0);
    close(sock);
    printf("Error occurred during sending: errno %d\n", errno);
    return 2;
  }
  shutdown(sock, 0);
  close(sock);
  puts("Message sent");
  return 0;
}

static int send_sensor_data_tcp(int length)
{
  int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  if (sock < 0) {
    printf("Unable to create socket: errno %d\n", errno);
    return sock;
  }
  // Marking the socket as non-blocking
  int flags = fcntl(sock, F_GETFL);
  int err = fcntl(sock, F_SETFL, flags | O_NONBLOCK);
  if (err < 0)
  {
    puts("Unable to set socket non blocking");
    shutdown(sock, 0);
    close(sock);
    return err;
  }
  err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
  if (err < 0) {
    if (errno == EINPROGRESS)
    {
      puts("Connection in progress...");
      fd_set fdset;
      FD_ZERO(&fdset);
      FD_SET(sock, &fdset);

      // Connection in progress -> have to wait until the connecting socket is marked as writable, i.e. connection completes
      err = select(sock+1, NULL, &fdset, NULL, NULL);
      if (err < 0)
      {
        puts("Error during connection: select for socket to be writable");
        shutdown(sock, 0);
        close(sock);
        return err;
      } else if (err == 0)
      {
        puts("Connection timeout: select for socket to be writable");
        shutdown(sock, 0);
        close(sock);
        return -1;
      }
      else
      {
        int sockerr;
        socklen_t len = (socklen_t)sizeof(int);

        if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (void*)(&sockerr), &len) < 0)
        {
          puts("Error when getting socket error using getsockopt()");
          shutdown(sock, 0);
          close(sock);
          return -1;
        }
        if (sockerr)
        {
          puts("Connection error");
          shutdown(sock, 0);
          close(sock);
          return -1;
        }
      }
    }
    else
    {
      printf("Socket unable to connect: errno %d\n", errno);
      shutdown(sock, 0);
      close(sock);
      return err;
    }
  }
  puts("Successfully connected, sending data...");
  err = send(sock, encrypted, length, 0);
  if (err < 0)
    printf("Error occurred during sending: errno %d\n", errno);
  else
    puts("Message sent");
  shutdown(sock, 0);
  close(sock);
  return err;
}

static void encrypt_sensor_data(int length)
{
  unsigned char *encrypted_p = encrypted;
  unsigned char *plain_p = (unsigned char*)&sdata;
  while (length > 0)
  {
    aes128_encrypt(encrypted_p, plain_p);
    length -= 16;
    encrypted_p += 16;
    plain_p += 16;
  }
}

int main(int argc, char **argv)
{
  if (argc < FIRST_SENSOR_VALUE_ARGUMENT + 1)
  {
    usage();
    return 1;
  }
  int udp = !strcmp(argv[1], "udp");
  int tcp = !strcmp(argv[1], "tcp");
  if (!udp && !tcp)
  {
    usage();
    return 2;
  }
  int port = atoi(argv[2]);
  if (port <= 0 || port > 65535)
  {
    puts("incorrect port number");
    return 3;
  }
  int device_id = atoi(argv[3]);
  if (device_id <= 0 || device_id > 255)
  {
    puts("incorrect device id");
    return 4;
  }
  int sensor_count = argc - FIRST_SENSOR_VALUE_ARGUMENT;
  if (sensor_count > MAX_SENSORS)
  {
    puts("too many sensor values");
    return 5;
  }

  crc_init(default_aes_key);
  aes128_set_key(default_aes_key);

  dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  dest_addr.sin_family = AF_INET;
  dest_addr.sin_port = htons(port);

  printf("Sending %s packet to port %d device_id %d sensor count %d\nSensor values: ", udp ? "udp" : "tcp", port,
          device_id, sensor_count);
  memset(&sdata, 0, sizeof(sdata));
  sdata.device_id = device_id;
  for (int i = FIRST_SENSOR_VALUE_ARGUMENT; i < argc; i++)
  {
    int value = i < argc ? atoi(argv[i]) : 0;
    set_sensor_value(i - FIRST_SENSOR_VALUE_ARGUMENT, value);
    printf("%d ", value);
  }
  putchar('\n');
  int length = sensor_count > 6 ? 48 : sensor_count > 2 ? 32 : 16;
  prepare_sensor_data(length);
  encrypt_sensor_data(length);
  if (udp)
    return send_sensor_data_udp(length);
  return send_sensor_data_tcp(length);
}