#include "board.h"
#include <cc1101_commands.h>
#include <cc1101_func.h>
#include <shell.h>
#include <laCrosseDecoder.h>

static int cc1101_init_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data);
static const ShellCommandItem cc1101_init_command_items[] = {
  {NULL, NULL, cc1101_init_handler}
};
static const ShellCommand cc1101_init_command = {
  cc1101_init_command_items,
  "cc1101_init",
  "cc1101_init",
  NULL,
  NULL
};

static int cc1101_receive_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data);
static const ShellCommandItem cc1101_receive_command_items[] = {
  {NULL, NULL, cc1101_receive_handler}
};
static const ShellCommand cc1101_receive_command = {
  cc1101_receive_command_items,
  "cc1101_receive",
  "cc1101_receive",
  NULL,
  NULL
};

static int cc1101_init_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data)
{
  int rc = cc1101Init();
  if (rc)
  {
    pfunc("cc1101 init failed\r\n");
    return rc;
  }
  return 0;
}

static int cc1101_receive_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data)
{
  unsigned char *laCrossePacket;

  cc1101ReceiveStart();
  int counter = 300;
  while (counter--)
  {
    laCrossePacket = cc1101Receive();
    if (laCrossePacket)
    {
      pfunc("PACKET RECEIVED!!! %d %d %d %d %d\r\n", laCrossePacket[0], laCrossePacket[1], laCrossePacket[2],
               laCrossePacket[3], laCrossePacket[4]);
      short ext_temp_val;
      const char *message = laCrosseDecode(LACROSSE_SENSOR_ID, laCrossePacket, &ext_temp_val, NULL);
      if (!message)
      {
        pfunc("PACKET DECODE SUCCESS, t = %d\r\n", ext_temp_val);
        cc1101ReceiveStop();
        return 0;
      }
      pfunc("PACKET DECODE FAILURE %s\r\n", message);
    }
    delayms(1000);
  }
  return 1;
}

void register_cc1101_commands(void)
{
  shell_register_command(&cc1101_init_command);
  shell_register_command(&cc1101_receive_command);
}

