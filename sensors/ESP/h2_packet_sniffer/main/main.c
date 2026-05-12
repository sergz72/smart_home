#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "led.h"
#include <getstring.h>
#include <shell.h>
#include "common.h"
#include "esp_ieee802154.h"

#define LOG_TAG "main"

static int channel_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data);
static const ShellCommandItem channel_command_items[] = {
  {NULL, param_handler, NULL},
  {NULL, NULL, channel_handler}
};
static const ShellCommand channel_command = {
  channel_command_items,
  "channel",
  "channel num",
  NULL,
  NULL
};

typedef struct
{
  esp_ieee802154_frame_info_t frame_info;
  uint8_t message[128];
} received_frame_t;

typedef struct __attribute__((packed))
{
  uint16_t frame_type : 3;
  uint16_t security_enabled : 1;
  uint16_t frame_pending : 1;
  uint16_t ack_request : 1;
  uint16_t pan_id_compression : 1;
  uint16_t reserved : 1;
  uint16_t sequence_number_suppression : 1;
  uint16_t ie_list_present : 1;
  uint16_t dest_addr_mode : 2;
  uint16_t frame_version : 2;
  uint16_t src_addr_mode : 2;
} ieee802154_frame_control_field_t;

static QueueHandle_t ieee802154_receive_queue;
static received_frame_t received_frame;

static char command_line[200];

static int getch_(void)
{
  int c = getchar();
  if (c == '\n')
    c = '\r';
  return c;
}

static char* gets_(void)
{
  getstring_next();
  return command_line;
}

static void puts_(const char* s)
{
  while (*s)
    putchar(*s++);
}

static esp_err_t set_channel(uint8_t channel)
{
  esp_err_t err = esp_ieee802154_set_rx_when_idle(false);
  if (err != ESP_OK)
  {
    ESP_LOGE(LOG_TAG, "esp_ieee802154_set_rx_when_idle error %d", err);
    return err;
  }
  err = esp_ieee802154_sleep();
  if (err != ESP_OK)
  {
    ESP_LOGE(LOG_TAG, "esp_ieee802154_sleep error %d", err);
    return err;
  }
  err = esp_ieee802154_set_channel(channel);
  if (err != ESP_OK)
  {
    ESP_LOGE(LOG_TAG, "esp_ieee802154_set_channel error %d", err);
    return err;
  }
  err = esp_ieee802154_set_rx_when_idle(true);
  if (err != ESP_OK)
  {
    ESP_LOGE(LOG_TAG, "esp_ieee802154_set_rx_when_idle error %d", err);
    return err;
  }
  // 4. Start receiving
  err = esp_ieee802154_receive();
  if (err != ESP_OK)
  {
    ESP_LOGE(LOG_TAG, "esp_ieee802154_receive error %d", err);
    return err;
  }
  ESP_LOGI(LOG_TAG, "Listening on channel %d", channel);
  return ESP_OK;
}

static esp_err_t radio_init(void)
{
  // 1. Initialize the 802.15.4 radio
  esp_err_t err = esp_ieee802154_enable();
  if (err != ESP_OK)
  {
    ESP_LOGE(LOG_TAG, "esp_ieee802154_enable error %d", err);
    return err;
  }

  // 2. Enable Promiscuous Mode (receives ALL packets, no filtering)
  err = esp_ieee802154_set_promiscuous(true);
  if (err != ESP_OK)
  {
    ESP_LOGE(LOG_TAG, "esp_ieee802154_set_promiscuous error %d", err);
    return err;
  }
  err = esp_ieee802154_set_coordinator(false);
  if (err != ESP_OK)
  {
    ESP_LOGE(LOG_TAG, "esp_ieee802154_set_coordinator error %d", err);
    return err;
  }

  return set_channel(IEEE802154_CHANNEL);
}

// Callback triggered when a packet is received
void esp_ieee802154_receive_done(uint8_t* frame, esp_ieee802154_frame_info_t* frame_info)
{
  if (frame[0] > 127)
  {
    esp_ieee802154_receive_handle_done(frame);
    return;
  }

  memcpy(&received_frame.frame_info, frame_info, sizeof(esp_ieee802154_frame_info_t));
  memcpy(received_frame.message, frame, frame[0]);
  esp_ieee802154_receive_handle_done(frame);

  auto xHigherPriorityTaskWoken = pdFALSE;
  auto result = xQueueSendFromISR(ieee802154_receive_queue, &received_frame, &xHigherPriorityTaskWoken);
  if (result != pdFAIL && xHigherPriorityTaskWoken == pdTRUE)
    portYIELD_FROM_ISR();
}

static const char* get_frame_type(uint16_t frame_type)
{
  switch (frame_type)
  {
  case 0:
    return "Beacon";
  case 1:
    return "Data";
  case 2:
    return "Ack";
  case 3:
    return "MAC Command";
  case 4:
    return "type 4";
  case 5:
    return "type 5";
  case 6:
    return "type 6";
  case 7:
    return "type 7";
  default:
    return "Unknown";
  }
}

static const char* get_addr_mode(uint16_t addr_mode)
{
  switch (addr_mode)
  {
  case 0:
    return "PAN identifier and address fields are not present";
  case 1:
    return "Reserved";
  case 2:
    return "Address field contains a 16-bit short address";
  case 3:
    return "Address field contains a 64-bit extended address";
  default:
    return "Unknown";
  }
}

static int print_address(const char *name, uint16_t addr_mode, int idx)
{
  if (addr_mode == 2)
  {
    uint16_t short_addr = received_frame.message[idx + 1] << 8 | received_frame.message[idx];
    ESP_LOGI(LOG_TAG, "%s address: %04X", name, short_addr);
    idx += 2;
  }
  else if (addr_mode == 3)
  {
    uint64_t addr;
    memcpy(&addr, received_frame.message + idx, 8);
    ESP_LOGI(LOG_TAG, "%s address: %016llX", name, addr);
    idx += 8;
  }
  return idx;
}

static void decode_frame(const uint8_t* frame)
{
  int length = frame[0];
  ieee802154_frame_control_field_t frame_control;
  memcpy(&frame_control, frame + 1, sizeof(ieee802154_frame_control_field_t));
  ESP_LOGI(LOG_TAG, "Frame type: %s, security enabled: %d, frame pending: %d, ack request: %d, pan id compression: %d",
    get_frame_type(frame_control.frame_type), frame_control.security_enabled, frame_control.frame_pending, frame_control.ack_request, frame_control.pan_id_compression);
  ESP_LOGI(LOG_TAG, "Sequence number suppression: %d, ie list present: %d, dest addr mode: %s, frame version: %d, src addr mode: %s",
    frame_control.sequence_number_suppression, frame_control.ie_list_present, get_addr_mode(frame_control.dest_addr_mode), frame_control.frame_version, get_addr_mode(frame_control.src_addr_mode));
  int idx = 3;
  if (!frame_control.sequence_number_suppression)
    ESP_LOGI(LOG_TAG, "Sequence number: %d", frame[idx++]);
  uint16_t pan_id = frame[idx + 1] << 8 | frame[idx];
  ESP_LOGI(LOG_TAG, "Destination Pan Id: %04X", pan_id);
  idx += 2;
  idx = print_address("Destination", frame_control.dest_addr_mode, idx);
  if (!frame_control.pan_id_compression)
  {
    pan_id = frame[idx + 1] << 8 | frame[idx];
    ESP_LOGI(LOG_TAG, "Source Pan Id: %04X", pan_id);
    idx += 2;
  }
  idx = print_address("Source", frame_control.src_addr_mode, idx);
  while (idx < length)
    ESP_LOGI(LOG_TAG, "Payload: %02X", frame[idx++]);
}

static int channel_handler(printf_func pfunc, gets_func gfunc, int argc, char **argv, void *data)
{
  int channel = atoi(argv[0]);
  if (channel < 11 || channel > 26)
    return 1;
  return set_channel(channel);
}

void app_main(void) {
  configure_led();

  shell_init(printf, gets_);
  shell_register_command(&channel_command);

  getstring_init(command_line, sizeof(command_line), getch_, puts_);

  ieee802154_receive_queue = xQueueCreate(10, sizeof(received_frame_t));

  esp_err_t err = radio_init();
  if (err != ESP_OK)
  {
    set_led_red();
    while (true)
      vTaskDelay(1000 / portTICK_PERIOD_MS);
  }

  int rc;

  for (;;)
  {
    if (!getstring_next())
    {
      switch (command_line[0])
      {
      case SHELL_UP_KEY:
        puts_("\r\33[2K$ ");
        getstring_buffer_init(shell_get_prev_from_history());
        continue;
      case SHELL_DOWN_KEY:
        puts_("\r\33[2K$ ");
        getstring_buffer_init(shell_get_next_from_history());
        continue;
      default:
        rc = shell_execute(command_line);
        if (rc == 0)
          puts_("OK\r\n$ ");
        else if (rc < 0)
          puts_("Invalid command line\r\n$ ");
        else
          printf("shell_execute returned %d\n$ ", rc);
        break;
      }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
    BaseType_t rx_result;
    do
    {
      received_frame_t received_message;
      rx_result = xQueueReceive(ieee802154_receive_queue, &received_message, 0);
      if (rx_result == pdPASS)
      {
        ESP_LOGI(LOG_TAG, "Packet received! lqi=%d rssi=%d length=%d channel %d", received_message.frame_info.lqi,
          received_message.frame_info.rssi, received_message.message[0], received_message.frame_info.channel);
        decode_frame(received_message.message);
      }
    } while (rx_result == pdPASS);
  }
}
